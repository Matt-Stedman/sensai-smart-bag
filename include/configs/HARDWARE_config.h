#pragma once

// BOARD VERSION
#define BETAV4 // Options: BETAV4, BETAV3, BETAV2

// LEDS
#define SYSTEM_LED_PIN_R 16 // Used to show the status of the system
#define SYSTEM_LED_PIN_G 17 // Used to show the status of the system
#define SYSTEM_LED_PIN_B 4  // Used to show the status of the system
#define LED_ON 0
#define LED_OFF 255
#define SYSTEM_UI_BUTTON_1 14 // Used to control whether or not we will bother uploading this data
#define SYSTEM_UI_BUTTON_2 13 // Used to control whether or not we will bother uploading this data

// SWITCHES
#define SYSTEM_UI_SWITCH 15 // Used to control whether or not we will bother uploading this data

// SIMCOM modules
#define SIM_RX_pin 32
#define SIM_TX_pin 33
#define SIM_POWER_PIN 25
#define SIM7600x // alternatives: SIM7070G, A7672x, SIM7000x, SIM7600x
#define RESERVED_NOISE_PIN GPIO_NUM_12

// NMEA CAN converter
#define ESP32_CAN_RX_PIN GPIO_NUM_26 // Set CAN RX port to 4
#define ESP32_CAN_TX_PIN GPIO_NUM_27 // Set CAN TX port to 5

// I2C (Accel and RTC)
#define I2C_SDA 21
#define I2C_SCL 22
#define ACCEL_INT1 34
#define ACCEL_INT2 35

// SPI (SD Card)
#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_SS 5
#define SPI_SPEED 80000000

// RTC
#define RTC_Interrupt 21

// Board
#define Serial_Mon_Baud 115200 // Baud rate for communicating between Arduino and PC
