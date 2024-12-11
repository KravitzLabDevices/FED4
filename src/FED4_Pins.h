#ifndef FED4_PINS_h
#define FED4_PINS_h

// Motor Control Pins
#define MOTOR_MSBY 15  // MSBY motor
#define MOTOR_PIN_1 46 // A2 motor
#define MOTOR_PIN_2 37 // A3 motor
#define MOTOR_PIN_3 21 // A4 motor
#define MOTOR_PIN_4 38 // A5 motor

// User Buttons
#define BUTTON_1 14 // User Button 1 - bottom
#define BUTTON_2 39 // User Button 2 - middle
#define BUTTON_3 40 // User Button 3 - top

// Touch Sensor Pins
#define TOUCH_PAD_CENTER TOUCH_PAD_NUM5 // Touch pad - Center
#define TOUCH_PAD_RIGHT TOUCH_PAD_NUM1  // Touch pad - Right
#define TOUCH_PAD_LEFT TOUCH_PAD_NUM6   // Touch pad - Left

// Audio Interface
#define AUDIO_TRRS_1 4
#define AUDIO_TRRS_2 3
#define AUDIO_TRRS_3 2
#define AUDIO_LRCLK 48
#define AUDIO_BCLK 45
#define AUDIO_DIN 43
#define AUDIO_SD 42

// SPI Interface
#define SD_CS 10
#define DISPLAY_CS 17
#define SPI_MOSI 11
#define SPI_SCK 12
#define SPI_MISO 13

// I2C Interface
#define SDA 8
#define SCL 9
#define SDA_2 20
#define SCL_2 19

// Power Management
#define LDO2_ENABLE 47
#define BAT_VOLTAGE 7
#define USER_PIN_18 18

// LED Control
#define NEOPIXEL_PIN 35
#define RGB_STRIP_PIN 36

// ADC Configuration
#define VBAT_ADC_CHANNEL ADC1_CHANNEL_6

// GPIO Expander Pins
#define EXP_PHOTOGATE_1 12 // Center
#define EXP_PHOTOGATE_2 13 // Left
#define EXP_PHOTOGATE_3 0  // Right
#define EXP_PHOTOGATE_4 11 // Pellet Detector
#define EXP_XSHUT_1 2      // controls timing for ToF sensors
#define EXP_XSHUT_2 15     // controls timing for ToF sensors
#define EXP_XSHUT_3 3      // controls timing for ToF sensors
#define EXP_LDO3 14        // enables LDO3
#define EXP_HAPTIC 8       // Turns on/off haptic motor

#endif
