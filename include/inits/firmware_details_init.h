#pragma once

// configs
#include <configs/HARDWARE_config.h>
#include <configs/OPERATIONS_config.h>

namespace Firmware
{
        bool initialized = false;
        String version = "unknown";
        String board_version = "uknown";
        String write_date = "unknown";

        /**
         * @brief Print the firmware details to console (no status logging)
         */
        void printFirmwareDetails()
        {
                Serial.println();
                Serial.println("-- FIRMWARE DETAILS --");
                Serial.println("VMU ID: " + String(MONITOR_BOX_ID));
                Serial.println("Firmware version: " + version);
                Serial.println("Board version: " + board_version);
                Serial.println("Firmware write date: " + write_date);
                Serial.println("-- END OF FIRMWARE DETAILS --");
                Serial.println();
        }

        /**
         * @brief Initialize the Firmware preferences brick
         */
        void init()
        {
#ifdef FIRMWARE_VERSION
                version = FIRMWARE_VERSION;
#endif
#ifdef BETAV4
                board_version = "BETAV4";
#else
                board_version = "BETAV3";
#endif
#ifdef WRITE_DATE
                write_date = WRITE_DATE;
#endif
                printFirmwareDetails();
                initialized = true;
        }
}