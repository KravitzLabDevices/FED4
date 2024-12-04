#ifndef FED4_PINS_h
#define FED4_PINS_h

// I2C Bus 2 Pins
#define SDA_2 20
#define SCL_2 19

// Sharp Memory Display Pins
#define SHARP_SCK 12
#define SHARP_MOSI 11
#define SHARP_SS 17

// Motor Control Pins
#define MOTOR_PIN_1 46
#define MOTOR_PIN_2 37
#define MOTOR_PIN_3 21
#define MOTOR_PIN_4 38
#define STEPS 512

// Power Management Pins
#define LDO3 14
#define POWER_PIN_1 47
#define POWER_PIN_2 36
#define PG1 12

// Touch Sensor Pins
#define TOUCH_PIN_1 TOUCH_PAD_NUM1
#define TOUCH_PIN_5 TOUCH_PAD_NUM5
#define TOUCH_PIN_6 TOUCH_PAD_NUM6

// LED & Haptic Feedback
#define NEOPIXEL_PIN 35 // NeoPixel enable pin
#define NUMPIXELS 1     // Number of NeoPixels
#define HAPTIC_PIN 8    // Haptic feedback pin

// General Purpose I/O
#define GPIO_PIN_1 2
#define GPIO_PIN_2 3
#define GPIO_PIN_3 4

// ADC Configuration
#define VBAT_ADC_CHANNEL ADC1_CHANNEL_6

// Display Colors
#define BLACK 0
#define WHITE 1

#endif