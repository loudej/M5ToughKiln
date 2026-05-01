#include "lvgl_includes.h"
#include "ui/ui.h"
#include "ui/ui_main_screen.h"
#include "model/app_state.h"
#include "hardware/kiln_hardware.h"
#include "control/power_output.h"
#include "control/firing_controller.h"
#include "control/kiln_supervisor.h"
#include "service/preferences_persistence.h"
#include "server/kiln_wifi.h"
#include "server/kiln_http_server.h"
#include "server/kiln_arduino_ota.h"
#include "server/telnet_logger.h"
#include "ui/ui_settings_screen.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

namespace {

constexpr uint32_t kLoopSlowWarnMs = 150;

/// Experiment: LVGL buffering trade-off (RAM vs flush work). Core2 has PSRAM — large FBs usually go there.
enum class LvFramebufferMode : uint8_t {
    Partial20Lines,      ///< Small strip buffer (classic embedded default).
    FullSingleDirect,    ///< One full-screen framebuffer, DIRECT mode (dirty rects only).
    FullDoubleDirect,    ///< Two full-screen buffers — LVGL can render while another is shown (see LVGL docs).
};
constexpr LvFramebufferMode kLvFramebufferMode = LvFramebufferMode::FullSingleDirect;

constexpr int32_t kPartialStripLines = 20;

/// Prefer PSRAM for large allocations; fall back to internal DMA, then generic internal RAM.
static void* alloc_lv_buffer(size_t bytes, bool prefer_psram) {
    void* p = nullptr;
    if (prefer_psram)
        p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p)
        p = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!p)
        p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    return p;
}

static void lvgl_install_buffers(lv_display_t* display, int32_t w, int32_t h) {
    const size_t full_bytes =
        static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(lv_color_t);
    const size_t partial_bytes =
        static_cast<size_t>(w) * static_cast<size_t>(kPartialStripLines) * sizeof(lv_color_t);

    switch (kLvFramebufferMode) {
        case LvFramebufferMode::Partial20Lines: {
            void* fb = alloc_lv_buffer(partial_bytes, false);
            assert(fb);
            lv_display_set_buffers(display, fb, nullptr, partial_bytes,
                                   LV_DISPLAY_RENDER_MODE_PARTIAL);
            M5.Log.printf("LVGL: PARTIAL %u B (%d lines)\n", static_cast<unsigned>(partial_bytes),
                          kPartialStripLines);
            break;
        }
        case LvFramebufferMode::FullSingleDirect: {
            void* fb = alloc_lv_buffer(full_bytes, true);
            assert(fb);
            lv_display_set_buffers(display, fb, nullptr, full_bytes, LV_DISPLAY_RENDER_MODE_DIRECT);
            M5.Log.printf("LVGL: DIRECT single full FB %u B\n", static_cast<unsigned>(full_bytes));
            break;
        }
        case LvFramebufferMode::FullDoubleDirect: {
            void* a = alloc_lv_buffer(full_bytes, true);
            void* b = alloc_lv_buffer(full_bytes, true);
            if (a && b) {
                lv_display_set_buffers(display, a, b, full_bytes, LV_DISPLAY_RENDER_MODE_DIRECT);
                M5.Log.printf("LVGL: DIRECT double full FB %u B x2\n",
                              static_cast<unsigned>(full_bytes));
            } else {
                if (b) {
                    heap_caps_free(b);
                    b = nullptr;
                }
                if (a) {
                    heap_caps_free(a);
                    a = nullptr;
                }
                a = alloc_lv_buffer(full_bytes, true);
                assert(a);
                lv_display_set_buffers(display, a, nullptr, full_bytes,
                                       LV_DISPLAY_RENDER_MODE_DIRECT);
                M5.Log.printf("LVGL: DIRECT single (double OOM) %u B\n",
                              static_cast<unsigned>(full_bytes));
            }
            break;
        }
    }
}

}  // namespace

// LVGL display and touch driver
static lv_display_t *disp;
static lv_indev_t *indev;

// Core Engine Components
static KMeterISOHardware hardware;
static PowerOutput       powerOutput(&hardware);
static FiringController  controller(&hardware, &powerOutput);
static KilnSupervisor    supervisor;
PreferencesPersistence persistence;

static void controller_task(void* param) {
    (void)param;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        xSemaphoreTake(g_state_mutex, portMAX_DELAY);
        controller.update();
        xSemaphoreGive(g_state_mutex);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}

// LVGL display flush callback
void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    const int32_t x1      = area->x1;
    const int32_t y1      = area->y1;
    const int32_t w       = lv_area_get_width(area);
    const int32_t h       = lv_area_get_height(area);
    const lv_display_render_mode_t mode = lv_display_get_render_mode(display);

    // PARTIAL: draw_buf matches the invalidated rect only — pixels are tightly packed (w*h).
    // DIRECT / FULL: screen-sized framebuffer with stride = row width in *bytes*. `px_map` points at
    // buffer base; interpreting it as a compact w*h bitmap skews rows (classic diagonal raster tear).
    if (mode == LV_DISPLAY_RENDER_MODE_PARTIAL) {
        M5.Display.pushImage(x1, y1, static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                             reinterpret_cast<const uint16_t*>(px_map));
        lv_display_flush_ready(display);
        return;
    }

    lv_draw_buf_t* db = lv_display_get_buf_active(display);
    // LV_ASSERT_NULL(p) actually checks p != NULL (misleading name in lv_assert.h).
    LV_ASSERT(db != nullptr);
    auto* row_ptr =
        static_cast<uint8_t*>(lv_draw_buf_goto_xy(db, static_cast<uint32_t>(x1), static_cast<uint32_t>(y1)));
    const uint32_t stride_bytes = db->header.stride;
    const lv_color_format_t cf    = static_cast<lv_color_format_t>(db->header.cf);
    const uint32_t tight_row_bytes =
        static_cast<uint32_t>(w) * (lv_color_format_get_bpp(cf) / 8);

    if (stride_bytes == tight_row_bytes) {
        M5.Display.pushImage(x1, y1, static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                             reinterpret_cast<const uint16_t*>(row_ptr));
    } else {
        for (int32_t row = 0; row < h; ++row) {
            M5.Display.pushImage(x1, y1 + row, static_cast<uint32_t>(w), 1u,
                                 reinterpret_cast<const uint16_t*>(row_ptr));
            row_ptr += stride_bytes;
        }
    }

    lv_display_flush_ready(display);
}

// LVGL touch input callback
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{
    M5.update();
    auto t = M5.Touch.getDetail();

    if (t.isPressed()) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = t.x;
        data->point.y = t.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void setup()
{
    Serial.begin(115200);
    app_state_mutex_init();
    M5.begin();

    telnet_logger_setup();   // installs M5.Log callback; server opens when Wi-Fi connects
    M5.Log.println("setup() started");
    lv_init();

    const int32_t dw = M5.Display.width();
    const int32_t dh = M5.Display.height();

    disp = lv_display_create(dw, dh);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lvgl_install_buffers(disp, dw, dh);

    // Initialize touch driver
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // Adjust theme
    lv_display_set_theme(disp,
       lv_theme_default_init(disp,
        lv_palette_main(LV_PALETTE_BLUE_GREY),
        lv_palette_main(LV_PALETTE_RED),
        true,
        LV_FONT_DEFAULT)
      );

    // Initialize application state and load persisted data
    app_state_init();
    persistence.loadSettings();
    persistence.loadCustomPrograms();
    kiln_wifi_station_begin_from_nvs();

    while (!hardware.init()) {
        M5.Log.println("KMeter ISO: retrying in 500 ms ...");
        delay(500);
    }

    // Initialize application UI
    ui_init();

    xTaskCreatePinnedToCore(controller_task, "ctrl", 4096, nullptr, 2, nullptr, 1);

    M5.Log.println("setup() finished");
}

void loop()
{
    static uint32_t last_tick_ms = 0;
    static uint32_t last_ui_update_ms = 0;
    uint32_t current_ms = millis();

    // 1. Feed LVGL ticks
    lv_tick_inc(current_ms - last_tick_ms);
    last_tick_ms = current_ms;

    const uint32_t lv_t0 = millis();
    lv_timer_handler();
    const uint32_t lv_dt = millis() - lv_t0;
    if (lv_dt >= kLoopSlowWarnMs) {
        M5.Log.printf("[UI] lv_timer_handler slow: %lu ms\n", static_cast<unsigned long>(lv_dt));
    }

    // 3. Update UI at 2Hz to prevent redrawing too often
    if (current_ms - last_ui_update_ms > 500) {
        const uint32_t ui_t0 = millis();
        ui_main_screen_update();
        ui_settings_screen_update_status();
        const uint32_t ui_dt = millis() - ui_t0;
        if (ui_dt >= kLoopSlowWarnMs) {
            M5.Log.printf("[UI] screen updates slow: %lu ms\n", static_cast<unsigned long>(ui_dt));
        }
        last_ui_update_ms = current_ms;
    }

    kiln_wifi_service();
    ui_settings_screen_poll_wifi_scan();
    supervisor.service();

    telnet_logger_service();
    kiln_arduino_ota_service();

    const uint32_t http_t0 = millis();
    kiln_http_server_poll();
    const uint32_t http_dt = millis() - http_t0;
    if (http_dt >= kLoopSlowWarnMs) {
        M5.Log.printf("[HTTP] handleClient slow: %lu ms\n", static_cast<unsigned long>(http_dt));
    }

    delay(5);
}
