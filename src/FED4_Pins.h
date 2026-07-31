#ifndef FED4_PINS_h
#define FED4_PINS_h

// Motor Control Pins
#define MOTOR_PIN_1 38 // A2 motor
#define MOTOR_PIN_2 45 // A3 motor
#define MOTOR_PIN_3 46 // A4 motor
#define MOTOR_PIN_4 47 // A5 motor

// User Buttons
#define BUTTON_1 15// User Button 1 - bottom
#define BUTTON_2 16 // User Button 2 - middle
#define BUTTON_3 39 // User Button 3 - top

// Touch Sensor Pins
#define TOUCH_PAD_CENTER TOUCH_PAD_NUM3 // Touch pad - Center
#define TOUCH_PAD_RIGHT TOUCH_PAD_NUM2  // Touch pad - Right
#define TOUCH_PAD_LEFT TOUCH_PAD_NUM1   // Touch pad - Left

// Photogate Pins
#define PHOTOGATE_1 14 // Center
#define PHOTOGATE_2 17 // Left
#define PHOTOGATE_3 18 // Right
#define PHOTOGATE_4 37 // Pellet Detector

// PIR Motion Sensor
#define PIR_MOTION 10 // PIR motion sensor

// Audio Interface
#define AUDIO_TRRS_1 4 // Audio TRRS pin 1
#define AUDIO_TRRS_2 5 // Audio TRRS pin 2
#define AUDIO_TRRS_3 6 // Audio TRRS pin 3

// Interrupt Logic
#define INT_OR 7 // Interrupt ORing logic

// Amplifier Interface
#define AMP_LRCLK 40 // Amplifier left right clock
#define AMP_BCLK 41 // Amplifier bit clock
#define AMP_DIN 42 // Amplifier data input
#define EXP_AMP_SD 4 // Amplifier SD line on GPIO expander

// SPI Interface
#define SPI_MOSI 13 // SPI master output, slave input
#define SPI_SCK 12 // SPI clock
#define SPI_MISO 11 // SPI master input, slave output
#define SD_CS 48 // SD card chip select

// Display Interface
#define DISPLAY_CS 44 // Display chip select
#define DISPLAY_VCOM 43 // Display VCOM
#define EXP_DISPLAY_RESET 6 // Display reset on GPIO expander
#define EXP_DISPLAY_LED 7 // Display LED on GPIO expander

// I2C Interface
#define SDA 8 // I2C data
#define SCL 9 // I2C clock

// I2C Device Addresses
#define I2C_ADDR_ACCEL       0x19 // LIS2DH12TR  (SA0=HIGH)
#define I2C_ADDR_BME680      0x76 // BME680       (SDO=LOW)
#define I2C_ADDR_MCP23017    0x20 // MCP23017     (A0/A1/A2=LOW)
#define I2C_ADDR_MAX17048    0x36 // MAX17048     (fixed)
#define I2C_ADDR_RTC         0x68 // DS3231       (fixed)
#define I2C_ADDR_TOF         0x29 // VL53L1X      (fixed)
#define I2C_ADDR_LIGHT       0x10 // VEML7700     (fixed)

// Power Management (TPS22917 load switch ~ON — active LOW: LOW = rail ON, HIGH = rail OFF)
#define EXP_PSV2_EN 13 // PSV2 enable on GPIO expander (MCP pin 13)
#define EXP_PSV3_EN 12 // PSV3 enable on GPIO expander (MCP pin 12)

// User Control Pins
#define USER_SRV_INT 21 // User Servo or interrupt
#define EXP_USER_GPX0 8 // User pin 8 on GPIO expander
#define EXP_USER_GPX1 9 // User pin 9 on GPIO expander
#define EXP_USER_GPX2 10 // Touch point 10 on GPIO expander
#define EXP_USER_GPX3 11 // Touch point 11 on GPIO expander

// Solenoid Control
#define EXP_SOL_1 2 // Solenoid 1 on GPIO expander
#define EXP_SOL_2 3 // Solenoid 2 on GPIO expander

// LED Control
#define STATUS_LED 35 // Status LED
#define RGB_STRIP 36 // front LEDs

// Haptic Motor
#define EXP_HAPTIC 5       // Turns on/off haptic motor

#endif
