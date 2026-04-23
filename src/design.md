# Kiln Controller Design

## 1. Hardware

- **MCU:** M5Stack Tough (ESP32-based)
- **Display:** Integrated TFT Touchscreen
- **Input:** On-screen numeric keypad and control buttons
- **Output:** Relay for controlling the kiln's heating element
- **Sensor:** MAX31855 K-type thermocouple amplifier for temperature readings

## 2. UI Pages

### Page 1: Main Status Screen

The main screen has two distinct modes depending on whether a firing program is active.

#### 1a. Idle / Ready View (Default)

- **Purpose:** To prepare for a firing cycle with clear, simple controls.
- **Elements:**
    - **Current Temperature:** Prominently displayed for safety.
    - **Status:** "Idle" or "Ready to Start".
    - **Selected Program:** Displays the name of the loaded firing schedule.
    - **START Button:** A large, clear button to begin the firing process.
    - **Select Program Button:** Navigates to the Program Selection page.

#### 1b. Active / Running View

- **Purpose:** To monitor the firing process and provide critical data at a glance.
- **Elements:**
    - **Current Temperature:** Very large, centered, real-time display.
    - **Target Temperature:** Shows the current setpoint.
    - **Program Status:** Details the current action (e.g., "Ramping to 800°C", "Soaking for 15 mins").
    - **Time Information:** Time elapsed in the current segment and/or total time remaining.
    - **STOP/ABORT Button:** A large, red button to immediately halt the firing process.
    - The "Select Program" button is hidden or disabled during an active run.

### Page 2: Program Configuration

- **Purpose:** To select and configure a firing schedule from a single screen.

- **Static Elements (Always Visible):**
    - **Program Dropdown:** Located at the top of the screen. Allows the user to select from:
        - A list of predefined programs (e.g., "Bisque Fire", "Glaze Fire").
        - A list of saved custom programs.
        - An option to "Add New" custom program.
    - **Back Button:** Returns to the Main Status Screen.
    - **OK/Save Button:** Confirms the selections and returns to the Main Status Screen.

- **Dynamic Content Area (Changes based on dropdown selection):**

    - **If a Predefined Program is selected:**
        - The area below the dropdown displays simple configuration options:
        - **Cone Temp:** Input field for the target cone temperature.
        - **Candle Time:** Input field for a pre-heat "candling" duration.
        - **Soak Time:** Input field for the hold time at peak temperature.

    - **If a Custom Program is selected:**
        - The area below the dropdown displays the full custom program editor:
        - A list of segments, where each segment has a ramp rate, target temperature, and soak time.
        - **Add Segment Button:** Adds a new segment to the schedule.
        - **Edit/Delete Segment Buttons:** For modifying the schedule.

### Page 3: Edit Segment Popup

- **Purpose:** To provide a focused interface for adding or editing a single segment of a custom firing schedule. This appears as a modal popup over the "Program Configuration" screen.
- **Elements:**
    - **Target Temperature:** Input field for the segment's target temperature.
    - **Ramp Rate:** Input field for the rate of temperature change (°C/hour).
    - **Soak Time:** Input field for the hold duration at the target temperature (in minutes).
    - **OK/Save Button:** Confirms the changes and closes the popup.
    - **Cancel Button:** Discards any changes and closes the popup.
    - The on-screen numeric keypad is used for all value entries.
