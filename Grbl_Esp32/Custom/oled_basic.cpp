/*
	oled_basic.cpp
	Part of Grbl_ESP32

	copyright (c) 2018 -	Bart Dring This file was modified for use on the ESP32
	CPU. Do not use this with Grbl for atMega328P

	Grbl is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Grbl is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Grbl.  If not, see <http://www.gnu.org/licenses/>.

	--------------------------------------------------------------

 	This is a very basic test of adding a little OLED

    https://github.com/ThingPulse/esp8266-oled-ssd1306

    platformio lib install 562

*/

// Include the correct display library

// For a connection via I2C using the Arduino Wire include:
// #include <Wire.h>         // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h"  // legacy: #include "SSD1306.h"
// OR #include "SH1106Wire.h"   // legacy: #include "SH1106.h"
#include "../src/WebUI/WebSettings.h"

// Initialize the OLED display using Arduino Wire:
//SSD1306Wire display(0x3c, GPIO_NUM_22, GPIO_NUM_16); // ADDRESS, SDA, SCL  -  SDA and SCL usually populate automatically based on your board's pins_arduino.h
SSD1306Wire display(0x3c, GPIO_NUM_14, GPIO_NUM_13, GEOMETRY_128_64);
// SSD1306Wire display(0x3c, D3, D5);  // ADDRESS, SDA, SCL  -  If not, they can be specified manually.
// SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32);  // ADDRESS, SDA, SCL, OLEDDISPLAY_GEOMETRY  -  Extra param required for 128x32 displays.
// SH1106 display(0x3c, SDA, SCL);     // ADDRESS, SDA, SCL

static TaskHandle_t displayUpdateTaskHandle = 0;

// returns the position of a machine axis
// wpos =true for corrected work postion
float getPosition(uint8_t axis, bool wpos = true) {
    float current_position = sys_position[axis] / axis_settings[axis]->steps_per_mm->get();
    return current_position;
}

String getStateText() {
    String str = "";

    switch (sys.state) {
        case State::Idle:
            str = "Idle";
            break;
        case State::Cycle:
            str = "Run";
            break;
        case State::Hold:
            if (!(sys.suspend.bit.jogCancel)) {
                str = "Hold:";
                sys.suspend.bit.holdComplete ? str += "0" : str += "1";  // Ready to resume
                break;
            }  // Continues to print jog state during jog cancel.
        case State::Jog:
            str = "Jog";
            break;
        case State::Homing:
            str = "Homing";
            break;
        case State::Alarm:
            str = "Alarm: ";
            str += String(static_cast<int>(sys_rt_exec_alarm));
            break;
        case State::CheckMode:
            str = "Check";
            break;
        case State::SafetyDoor:
            str = "Door:";
            if (sys.suspend.bit.initiateRestore) {
                str += "3";  // Restoring
            } else {
                if (sys.suspend.bit.retractComplete) {
                    sys.suspend.bit.safetyDoorAjar ? str += "1" : str += "0";  // Door ajar
                    // Door closed and ready to resume
                } else {
                    str += "2";  // Retracting
                }
            }
            break;
        case State::Sleep:
            str = "Sleep";
            break;
    }

    return str;
}

void displayRadioInfo() {
    String wifi_info = "";
    //     if (wifi_radio_mode->get() == ESP_BT) {
    // #ifdef ENABLE_BLUETOOTH

    // #endif
    //     } else {

    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_10);

    enum_opt_t foo = wifi_radio_mode->get();

#ifdef ENABLE_BLUETOOTH

#endif
#ifdef ENABLE_WIFI
        if ((WiFi.getMode() == WIFI_MODE_STA) || (WiFi.getMode() == WIFI_MODE_APSTA)) {
        wifi_info = "STA SSID: " + WiFi.SSID();
        display.drawString(0, 18, wifi_info);

        wifi_info = "IP: " + WiFi.localIP().toString();
        display.drawString(0, 30, wifi_info);

        wifi_info = "Status: ";
        (WiFi.status() == WL_CONNECTED) ? wifi_info += "Connected" : wifi_info += "Not connected";
        display.drawString(0, 42, wifi_info);
        //}
    }
    else if ((WiFi.getMode() == WIFI_MODE_AP) || (WiFi.getMode() == WIFI_MODE_APSTA)) {
        wifi_info = "AP SSID: Who knows?";

        display.drawString(0, 30, wifi_info);

        wifi_info = "IP: " + WiFi.softAPIP().toString();
        display.drawString(0, 47, wifi_info);
    }
#endif
}

void displayDRO() {
    char axisVal[20];

    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 13, String('X') + ":");
    display.drawString(0, 30, "Y:");
    display.drawString(0, 47, "Z:");

    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    snprintf(axisVal, 20 - 1, "%.3f", getPosition(X_AXIS, true));
    display.drawString(100, 13, axisVal);

    snprintf(axisVal, 20 - 1, "%.3f", getPosition(Y_AXIS, true));
    display.drawString(100, 30, axisVal);

    snprintf(axisVal, 20 - 1, "%.3f", getPosition(Z_AXIS, true));
    display.drawString(100, 47, axisVal);
}

void displayUpdate(void* pvParameters) {
    TickType_t       xLastWakeTime;
    const TickType_t xDisplayFrequency = 100;                  // in ticks (typically ms)
    xLastWakeTime                      = xTaskGetTickCount();  // Initialise the xLastWakeTime variable with the current time.

    vTaskDelay(2500);
    uint16_t sd_file_ticker = 0;

    display.init();
    display.flipScreenVertically();

    while (true) {
        display.clear();

        String state_string = getStateText();

        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.setFont(ArialMT_Plain_16);
        display.drawString(63, 0, state_string);

        if (get_sd_state(false) == SDState::BusyPrinting) {
            display.clear();
            display.setTextAlignment(TEXT_ALIGN_LEFT);
            display.setFont(ArialMT_Plain_16);
            state_string = "SD File";
            for (int i = 0; i < sd_file_ticker % 10; i++) {
                state_string += ".";
            }
            sd_file_ticker++;
            display.drawString(25, 0, state_string);

            int progress = sd_report_perc_complete();
            // draw the progress bar
            display.drawProgressBar(0, 45, 120, 10, progress);

            // draw the percentage as String
            display.setFont(ArialMT_Plain_16);
            display.setTextAlignment(TEXT_ALIGN_CENTER);
            display.drawString(64, 25, String(progress) + "%");

        } else if (sys.state == State::Alarm) {
            displayRadioInfo();
        } else {
            displayDRO();
        }
        display.display();

        vTaskDelayUntil(&xLastWakeTime, xDisplayFrequency);
    }
}

void display_init() {
    // Initialising the UI will init the display too.
    display.init();

    display.flipScreenVertically();

    display.clear();

    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(63, 0, MACHINE_NAME);

    display.display();

    xTaskCreatePinnedToCore(displayUpdate,        // task
                            "displayUpdateTask",  // name for task
                            4096,                 // size of task stack
                            NULL,                 // parameters
                            1,                    // priority
                            &displayUpdateTaskHandle,
                            CONFIG_ARDUINO_RUNNING_CORE  // must run the task on same core
                                                         // core
    );
}
