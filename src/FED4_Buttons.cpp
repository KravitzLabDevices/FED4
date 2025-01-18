#include "FED4.h"

// Interrupt handler for BUTTON_1
void IRAM_ATTR FED4::onButton1WakeUp() {
}

bool FED4::initializeButtons() {
    // Configure BUTTON_1 as input with internal pulldown
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_HIGH_LEVEL;
    io_conf.pin_bit_mask = (1ULL << BUTTON_1);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        Serial.println("Failed to configure GPIO");
        return false;
    }
    
    // Enable wake-up on GPIO
    err = gpio_wakeup_enable((gpio_num_t)BUTTON_1, GPIO_INTR_HIGH_LEVEL);
    if (err != ESP_OK) {
        Serial.println("Failed to enable GPIO wakeup");
        return false;
    }
    
    err = esp_sleep_enable_gpio_wakeup();
    if (err != ESP_OK) {
        Serial.println("Failed to enable GPIO wakeup in sleep");
        return false;
    }
    
    Serial.print("Button 1 current state: ");
    Serial.println(gpio_get_level((gpio_num_t)BUTTON_1));
    Serial.println("Button 1 wake-up interrupt has been set");
    
    return true;
}

