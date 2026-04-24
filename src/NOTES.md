
- [x] when choosing program, top of screen should have dropdown, then button with check mark, then button with X
  - that's so back and save/ok don't use a whole chunk of vertical space on their own at the bottom
- [x] put the text box to the left of the label
  - it's less standard, but as it stands the floating keyboard covers the input text
- [x] candle and soak are both in minutes
- [x] choosing bisque and glaze should default cone to 08 and 5
  - also change candle and soak to empty string
- [x] the empty string should show "min" as the prompt text instead of being in parenthesis on the label
- [x] in the custom segment list, the rows should be buttons
  - the text is the summary and tapping them brings up the edit popup
- [x] the edit popup of a segment should have ok (checkmark) cancel (x) and red DEL
- [x] clicking select program should have the current program selected in the dropdown
- [x] if that current program is predefined, then the fields should also be filled in
- [x] when powering on the selected program should be restored

[x] add segment followed by (x) cancel should not leave an added segment
[x] add segment button should be last
[x] the predefined program input fields are actually off-screen (too far left)
[x] the spacing between program input fields, and between the top and the input fields, seems larger than it needs to be
[x] the inputs in the segment popup are slightly too short (the text bounces up and down non-stop)
[x] the inputs in the segment popup should be to the left of the label text
[x] the label text should not have units - that's what the empty fields should show
[x] don't prepopulate values in a new segment

### Found Bugs / To-Do
- [x] **Popup Memory Leak on Parent Destruction**: In `ui_edit_segment_popup.cpp`, the `PopupContext* ctx` is allocated via `new`. The context is currently only deleted inside the explicit button callbacks (`on_save`, `on_cancel`, `on_delete`). However, if the user interacts with a hardware back button (or if the system forces the parent `program_config_screen` to close), LVGL will automatically destroy the popup overlay, but `ctx` will be permanently leaked. This needs an `LV_EVENT_DELETE` handler attached to the popup to reliably invoke `delete ctx`.
  - *How to fix*: Add `lv_obj_add_event_cb(overlay, [](lv_event_t *e){ delete (PopupContext*)lv_event_get_user_data(e); }, LV_EVENT_DELETE, ctx);` and remove `delete ctx;` from the button callbacks.
- [x] **Main Screen State Stagnation**: In `ui.cpp`, when `ui_switch_to_main_screen()` is called, it correctly destroys the config screen and loads the main screen using `lv_scr_load()`. However, it completely fails to call `ui_main_screen_update()`. Because the main screen isn't being recreated, any changes the user just made (like the newly selected program) will not visibly update on the main screen until a state change (like pressing START) triggers a re-render.
  - *How to fix*: Call `ui_main_screen_update();` inside `ui_switch_to_main_screen()` right after `lv_scr_load(ui_main_screen_get());`.
- [x] **Dangling Pointer Risk for Program Name**: In `btn_save_event_cb` (and `app_state_init`), `appState.status.activeProgramName` is assigned the raw pointer `.name.c_str()` from the `appState.customPrograms` vector. If the user later selects "Add New Custom", the `std::vector` may be forced to reallocate its internal buffer. This reallocation will invalidate the `c_str()` pointer, causing the main screen to read freed memory resulting in garbage text or crashes.
  - *How to fix*: Change `activeProgramName` in `KilnStatus` from `const char*` to `std::string` to safely store a copy of the name, avoiding dangling pointer risks when the vector reallocates.
- [x] **Main Screen Boot Initialization Desync**: In `app_state_init()`, the active program name is hardcoded to fallback to `appState.predefinedPrograms[0].name.c_str()` ("Fast Bisque"). However, when `PreferencesPersistence::loadCustomPrograms()` later runs and loads the user's previously saved `activeProgramIndex`, it *does not* update the `activeProgramName` pointer. The UI will boot up displaying "Program: Fast Bisque" even if "Custom 2" is the active memory index.
  - *How to fix*: At the end of `PreferencesPersistence::loadCustomPrograms()`, update `appState.status.activeProgramName` using the logic that checks if the loaded index is `<= 3` (predefined) or `> 3` (custom program).
- [x] **Edits to Predefined Programs are Never Saved**: In `btn_save_event_cb` (`ui_program_config_screen.cpp`), the critical call to `persistence.saveCustomPrograms(...)` (which ironically also saves the predefined parameters and active program index) is trapped inside the `else` block intended exclusively for custom programs. If a user edits a predefined program (e.g., changes the Cone or Soak time) and clicks the "Checkmark" button, the changes are applied in volatile memory but **never** written to NVM.
  - *How to fix*: Move the call to `persistence.saveCustomPrograms(appState.customPrograms);` to execute unconditionally at the end of `btn_save_event_cb`, so the active index and predefined fields are always saved.
- [x] **Active Program Selection is Lost (if predefined)**: Because of the exact same bug mentioned above, if the user simply changes the active program dropdown to "Slow Glaze" and hits "Checkmark", the new active index is updated in memory but not saved to the preferences. On reboot, the system will revert to the last saved state.
  - *How to fix*: Covered by the fix above. Moving `persistence.saveCustomPrograms()` outside the `else` block will fix this issue as well.
- [x] **Predefined Programs Do Not Regenerate on Boot**: During startup, `PreferencesPersistence::loadCustomPrograms()` correctly loads `origCone`, `origCandle`, and `origSoak` from memory into the struct fields. However, it never actually calls `ProfileGenerator::generate...()` to process those inputs into firing segments. If a user configured a predefined program to use "Cone 08" instead of the default "Cone 04", the system will load the "08" string for the UI, but the underlying firing sequence logic will silently execute the hardcoded defaults. 
  - *How to fix*: In `PreferencesPersistence::loadCustomPrograms()`, after loading the three original values for each predefined index, invoke the matching `ProfileGenerator::generate...()` function and overwrite the array item (just like it happens in `btn_save_event_cb`).
- [x] **Partial State Application for Custom Edits (Save/Cancel Conflict)**: If a user opens the segment edit popup and clicks "Save" inside the popup, `appState.customPrograms` is directly modified in volatile memory. If they then click the red "X" (Cancel) button on the top program config screen, their intention was to discard changes. However, the segment was already edited in the active vector. Those edits remain "live" for the current session but will vanish on reboot since `saveCustomPrograms` wasn't called.
  - *How to fix*: When editing a custom program, work on a deep copy of the program in volatile memory. When the user clicks the green "Save" button on the config screen, copy the modified program back into `appState.customPrograms` and save it. If they hit "Cancel", simply discard the copy.