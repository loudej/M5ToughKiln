#include "lvgl_includes.h"
#include "ui/ui.h"
#include "ui/ui_main_screen.h"
#include "model/app_state.h"
#include "hardware/kiln_hardware.h"
#include "control/firing_controller.h"
#include "service/preferences_persistence.h"

// Diagnostic-only: minimum-viable KMeter ISO test at the top of setup() built
// against M5Unit-METER (M5UnitUnified) using the M5HAL software-I2C bus,
// which bypasses Arduino Wire entirely (Arduino Wire's endTransmission lies
// about NACK when the slave clock-stretches, even though bytes transfer
// correctly — proven empirically in the bare-Wire experiments below).
#include <Wire.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedMETER.h>
#include <M5HAL.hpp>
#include <driver/gpio.h>
#include <cmath>
static m5::unit::UnitUnified mvt_units;
static m5::unit::UnitKmeterISO mvt_unit;

// LVGL display and touch driver
static lv_display_t *disp;
static lv_indev_t *indev;
static lv_color_t* buf;

// Core Engine Components
static KMeterISOHardware hardware;
static FiringController controller(&hardware);
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
    uint16_t touchX, touchY;
    
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


// Button event handler
static void btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
    if(code == LV_EVENT_CLICKED) {
        static uint8_t cnt = 0;
        cnt++;

        lv_obj_t * label = lv_obj_get_child(btn, 0);
        lv_label_set_text_fmt(label, "Button: %d", cnt);
    }
}


void setup()
{
    Serial.begin(115200);

    // ── Minimum-viable KMeter ISO test (DIAGNOSTIC) ─────────────────────────
    // Pure M5Unit-METER (M5UnitUnified) path. Halts after printing a few
    // temperature samples so the rest of the app does not initialize and
    // muddy the bus. Remove once we trust the real hardware path.
    M5.begin();
    Serial.println("--- KMeter MVT (M5UnitUnified, Port A) ---");

    auto sda = M5.getPin(m5::pin_name_t::port_a_sda);
    auto scl = M5.getPin(m5::pin_name_t::port_a_scl);
    Serial.printf("Port A SDA=%u SCL=%u\n", sda, scl);

    Wire.begin(sda, scl, 100000U);

    for (int i = 0; i < 10; ++i) {
        Wire.beginTransmission(mvt_unit.address());
        auto err = Wire.endTransmission();
        Serial.printf("  ACK probe %d: err=%d\n", i + 1, err);
        if (err == 0) break;
        delay(10);
    }

    // ── Bare-Wire register-read experiments ────────────────────────────────
    // The library's begin() failed at: setClock(100k) + write(0xFE) + STOP,
    // returning err=2 (NACK on address) — even though the address probe just
    // succeeded. Replicate exactly that pattern and variations to isolate the
    // offending variable (setClock vs payload byte vs read transaction).
    constexpr uint8_t addr = 0x66;
    constexpr uint8_t reg_fw     = 0xFE;  // FIRMWARE_VERSION_REG
    constexpr uint8_t reg_status = 0x20;  // STATUS_REG (0 = data ready)
    constexpr uint8_t reg_temp_c = 0x00;  // TEMPERATURE_CELSIUS_REG (4 bytes, int32 LE * 0.01)

    delay(50);
    Wire.setClock(100000);
    Wire.beginTransmission(addr);
    Wire.write(reg_fw);
    auto e1 = Wire.endTransmission(true);
    Serial.printf("  [E1] setClock+write(0xFE)+STOP : err=%d\n", e1);

    delay(50);
    Wire.beginTransmission(addr);
    Wire.write(reg_fw);
    auto e2 = Wire.endTransmission(true);
    Serial.printf("  [E2] write(0xFE)+STOP (no setClock): err=%d\n", e2);

    delay(50);
    Wire.beginTransmission(addr);
    auto e3 = Wire.endTransmission();
    Serial.printf("  [E3] addr-only probe (post-writes) : err=%d\n", e3);

    delay(50);
    Wire.beginTransmission(addr);
    Wire.write(reg_fw);
    auto e4w = Wire.endTransmission(true);
    auto e4n = Wire.requestFrom(addr, (uint8_t)1);
    int e4val = Wire.available() ? Wire.read() : -1;
    Serial.printf("  [E4] read FW (0xFE) : write err=%d, requestFrom n=%u, value=0x%02X\n",
                  e4w, e4n, (unsigned)e4val);

    delay(50);
    Wire.beginTransmission(addr);
    Wire.write(reg_status);
    auto e5w = Wire.endTransmission(true);
    auto e5n = Wire.requestFrom(addr, (uint8_t)1);
    int e5val = Wire.available() ? Wire.read() : -1;
    Serial.printf("  [E5] read STATUS (0x20) : write err=%d, requestFrom n=%u, value=0x%02X\n",
                  e5w, e5n, (unsigned)e5val);

    delay(50);
    Wire.beginTransmission(addr);
    Wire.write(reg_temp_c);
    auto e6w = Wire.endTransmission(true);
    auto e6n = Wire.requestFrom(addr, (uint8_t)4);
    uint8_t e6buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    int e6got = 0;
    while (Wire.available() && e6got < 4) e6buf[e6got++] = Wire.read();
    int32_t e6raw = (int32_t)((uint32_t)e6buf[0] |
                              ((uint32_t)e6buf[1] << 8) |
                              ((uint32_t)e6buf[2] << 16) |
                              ((uint32_t)e6buf[3] << 24));
    float e6tempC = e6raw * 0.01f;
    Serial.printf("  [E6] read TEMP_C (0x00..03) : write err=%d, requestFrom n=%u, got %d bytes\n",
                  e6w, e6n, e6got);
    Serial.printf("       raw bytes = %02X %02X %02X %02X  -> %.2f C\n",
                  e6buf[0], e6buf[1], e6buf[2], e6buf[3], e6tempC);

    delay(50);

    // Release the ESP32 I2C peripheral from GPIO 32/33 so the bit-bang has
    // sole control of the pin matrix, and force-enable internal pull-ups
    // (Wire.end() drops them, and OUTPUT_OPEN_DRAIN alone leaves the line
    // floating when released — bit-bang then reads back low and times out).
    Wire.end();
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, INPUT_PULLUP);
    delay(5);

    // M5HAL software-I2C bus on the same Port A pins. SoftwareI2CBus bit-bangs
    // SCL/SDA via interface::gpio::Pin and explicitly waits for clock-stretch
    // release at every byte, so it does not depend on (or get lied to by)
    // the ESP32 hardware I2C peripheral / Arduino Wire driver.
    Serial.println("M5HAL: building software-I2C bus on Port A pins ...");
    auto sda_pin = m5::hal::gpio::getPin((m5::hal::types::gpio_number_t)sda);
    auto scl_pin = m5::hal::gpio::getPin((m5::hal::types::gpio_number_t)scl);
    Serial.printf("  pin handles: sda=%p scl=%p\n", (void*)sda_pin, (void*)scl_pin);

    m5::hal::bus::I2CBusConfig bus_cfg;
    bus_cfg.pin_sda = sda_pin;
    bus_cfg.pin_scl = scl_pin;
    auto bus_result = m5::hal::bus::i2c::getBus(bus_cfg);
    if (!bus_result.has_value()) {
        Serial.println("  -> getBus FAILED");
        for (;;) { delay(1000); }
    }
    auto* hal_bus = bus_result.value();
    Serial.printf("  -> getBus OK (bus=%p)\n", (void*)hal_bus);

    // SoftwareI2CBus::init() switched the pins to OUTPUT_OPEN_DRAIN, which on
    // Arduino-ESP32 does NOT enable internal pull-ups. Re-arm them now so the
    // released SCL/SDA actually float high enough for the bit-bang to read back.
    gpio_set_pull_mode((gpio_num_t)sda, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)scl, GPIO_PULLUP_ONLY);
    Serial.println("  pull-ups re-armed on SDA/SCL");

    Serial.println("Units.add(unit, hal_bus) ...");
    if (!mvt_units.add(mvt_unit, hal_bus)) {
        Serial.println("  -> add FAILED");
        for (;;) { delay(1000); }
    }
    Serial.println("  -> add OK");

    Serial.println("Units.begin() ...");
    if (!mvt_units.begin()) {
        Serial.println("  -> begin FAILED");
        for (;;) { delay(1000); }
    }
    Serial.println("  -> begin OK");

    for (int i = 0; i < 10; ++i) {
        mvt_units.update();
        float t = mvt_unit.temperature();
        if (std::isnan(t)) {
            Serial.printf("  [%d] (no sample yet)\n", i);
        } else {
            Serial.printf("  [%d] Temp = %.2f C\n", i, t);
        }
        delay(500);
    }

    Serial.println("--- MVT done. Halting. ---");
    for (;;) { delay(1000); }

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

    hardware.init();

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