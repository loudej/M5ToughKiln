#define M5GFX_USING_REAL_LVGL
#include <lvgl.h>
#include <M5Unified.h>
#include "ui/ui.h"

// LVGL display and touch driver
static lv_display_t *disp;
static lv_indev_t *indev;
static lv_color_t* buf;

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
        lv_palette_main(LV_PALETTE_BROWN),
        lv_palette_main(LV_PALETTE_RED),
        true,
        LV_FONT_DEFAULT)
      );

    // Initialize application UI
    ui_init();

    /* Commenting out sample code
    // Create a text area on the left side
    lv_obj_t * ta = lv_textarea_create(lv_screen_active());
    lv_obj_set_size(ta, M5.Display.width() / 2 - 20, 40);
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_textarea_set_text(ta, "5"); // Set initial text
    lv_obj_add_state(ta, LV_STATE_FOCUSED);
    lv_textarea_set_text_selection(ta, true);
    lv_textarea_set_one_line(ta, true);
    lv_label_set_text_selection_start(lv_textarea_get_label(ta), 0);
    lv_label_set_text_selection_end(lv_textarea_get_label(ta), 1);

    // Create a button
    lv_obj_t * btn = lv_btn_create(lv_display_get_screen_active(disp));
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align_to(btn, ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20); // Align below text area
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);

    // Create a label for the button
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Button 1");
    lv_obj_center(label);

    // Create a second button
    lv_obj_t * btn2 = lv_btn_create(lv_screen_active());
    lv_obj_set_size(btn2, 120, 50);
    lv_obj_align_to(btn2, btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20); // Align below first button
    lv_obj_add_event_cb(btn2, btn_event_cb, LV_EVENT_ALL, NULL);

    // Create a label for the second button
    lv_obj_t * label2 = lv_label_create(btn2);
    lv_label_set_text(label2, "Button 2");
    lv_obj_center(label2);

    // Create a keyboard on the right side
    lv_obj_t * kb = lv_keyboard_create(lv_screen_active());
    lv_obj_set_size(kb, M5.Display.width() / 2, M5.Display.height() - 20);
    lv_obj_align(kb, LV_ALIGN_RIGHT_MID, -10, 0);

    // Define the new 3x4 numeric keypad layout
    static const char * kb_map[] = {
        "1", "2", "3", "\n",
        "4", "5", "6", "\n",
        "7", "8", "9", "\n",
        LV_SYMBOL_OK, "0", LV_SYMBOL_BACKSPACE, NULL
    };

    // Define the button control map for the new layout
    static const lv_btnmatrix_ctrl_t kb_ctrl[] = {
        LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
        LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
        LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
        LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT
    };

    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, kb_map, kb_ctrl);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
    lv_keyboard_set_textarea(kb, ta);
    */

    M5.Log.println("setup() finished");
}

void loop()
{
    static uint32_t last_tick_ms = 0;
    uint32_t current_ms = millis();

    // Calculate elapsed time and feed it to LVGL
    lv_tick_inc(current_ms - last_tick_ms);
    last_tick_ms = current_ms;

    lv_timer_handler();
    delay(5);
}