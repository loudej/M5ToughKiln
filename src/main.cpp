#include "lvgl_includes.h"
#include "ui/ui.h"
#include "ui/ui_main_screen.h"
#include "model/app_state.h"
#include "hardware/kiln_hardware.h"
#include "control/power_output.h"
#include "control/firing_controller.h"
#include "service/preferences_persistence.h"

// LVGL display and touch driver
static lv_display_t *disp;
static lv_indev_t *indev;
static lv_color_t* buf;

// Core Engine Components
static KMeterISOHardware hardware;
static PowerOutput       powerOutput(&hardware);
static FiringController  controller(&hardware, &powerOutput);
PreferencesPersistence persistence;

// LVGL display flush callback
void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    M5.Display.pushImage(area->x1, area->y1, w, h, (const uint16_t*)px_map);
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
    M5.begin();

    M5.Log.println("setup() started");
    lv_init();

    // Allocate buffer for display
    auto bufSize = M5.Display.width() * 20 * sizeof(lv_color_t); // Buffer for 20 lines
    buf = (lv_color_t*)heap_caps_malloc(bufSize, MALLOC_CAP_DMA);
    assert(buf);

    // Initialize display driver
    disp = lv_display_create(M5.Display.width(), M5.Display.height());
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, buf, NULL, bufSize, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);

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
    persistence.loadCustomPrograms(appState.customPrograms);

    while (!hardware.init()) {
        M5.Log.println("KMeter ISO: retrying in 500 ms ...");
        delay(500);
    }

    // Initialize application UI
    ui_init();

    M5.Log.println("setup() finished");
}

void loop()
{
    static uint32_t last_tick_ms = 0;
    static uint32_t last_ui_update_ms = 0;
    uint32_t current_ms = millis();

    // 1. Run the core Firing Controller loop
    controller.update();

    // 2. Feed LVGL ticks
    lv_tick_inc(current_ms - last_tick_ms);
    last_tick_ms = current_ms;

    lv_timer_handler();

    // 3. Update UI at 2Hz to prevent redrawing too often
    if (current_ms - last_ui_update_ms > 500) {
        ui_main_screen_update();
        last_ui_update_ms = current_ms;
    }

    delay(5);
}
