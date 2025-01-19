#include "FED4.h"

// Interrupt handler for BUTTON_1
void IRAM_ATTR FED4::onButton1WakeUp() {
}

// Interrupt handler for BUTTON_2
void IRAM_ATTR FED4::onButton2WakeUp() {
}

// Interrupt handler for BUTTON_3
void IRAM_ATTR FED4::onButton3WakeUp() {
}

bool FED4::initializeButtons() {
    // Configure BUTTON_1, BUTTON_2, and BUTTON_3 as inputs with internal pulldown
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_HIGH_LEVEL;
    io_conf.pin_bit_mask = (1ULL << BUTTON_1) | (1ULL << BUTTON_2) | (1ULL << BUTTON_3);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        Serial.println("Failed to configure GPIO");
        return false;
    }
    
    // Enable wake-up on GPIO for all buttons
    err = gpio_wakeup_enable((gpio_num_t)BUTTON_1, GPIO_INTR_HIGH_LEVEL);
    if (err != ESP_OK) {
        Serial.println("Failed to enable GPIO wakeup for Button 1");
        return false;
    }
    
    err = gpio_wakeup_enable((gpio_num_t)BUTTON_2, GPIO_INTR_HIGH_LEVEL);
    if (err != ESP_OK) {
        Serial.println("Failed to enable GPIO wakeup for Button 2");
        return false;
    }
    
    err = gpio_wakeup_enable((gpio_num_t)BUTTON_3, GPIO_INTR_HIGH_LEVEL);
    if (err != ESP_OK) {
        Serial.println("Failed to enable GPIO wakeup for Button 3");
        return false;
    }
    
    err = esp_sleep_enable_gpio_wakeup();
    if (err != ESP_OK) {
        Serial.println("Failed to enable GPIO wakeup in sleep");
        return false;
    }
    
    Serial.println("Button wake-up interrupts have been set");
    
    return true;
}

