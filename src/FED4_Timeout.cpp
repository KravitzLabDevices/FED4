#include "FED4.h"

void FED4::timeout(uint16_t min, uint16_t max)
{
    // Generate random delay between min and max seconds
    uint16_t delaySeconds = random(min, max + 1);
    uint32_t delayTime = delaySeconds * 1000;
    
    // Print the word "TIMEOUT" with countdown timer
    for (int remaining = delaySeconds; remaining > 0; remaining--) {
        // Clear a larger area to ensure no black rectangles cover the text
        fillRect(0, 0, 144, 17, DISPLAY_BLACK);
        setFont(&FreeSans9pt7b);
        setTextSize(1);
        setTextColor(DISPLAY_WHITE);
        setCursor(5, 12);
        print("Timeout: ");
        print(remaining);
        print("s");
        refresh();
   
        // Sleep for 1 second using light sleep for power efficiency
        esp_sleep_enable_timer_wakeup(1000000); // 1 second in microseconds
        esp_light_sleep_start();
   }
} 