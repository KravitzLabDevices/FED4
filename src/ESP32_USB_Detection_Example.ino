/*
 * FED4 ESP32-S3 USB Detection Example
 * 
 * This example demonstrates the improved USB detection using ESP32-S3's
 * hardware USB detection capabilities combined with voltage-based fallback.
 * 
 * Features:
 * - Hardware-based USB detection via GPIO pin
 * - Voltage-based fallback detection
 * - Real-time charging status with red LED indicator
 * - Detailed debug output showing both detection methods
 */

#include "FED4.h"

FED4 fed;

void setup() {
  // Initialize the FED4
  if (!fed.begin("ESP32_USB_Detection_Example")) {
    Serial.println("FED4 initialization failed!");
    while(1) delay(1000);
  }
  
  Serial.println("FED4 ESP32-S3 USB Detection Example Started");
  Serial.println("===========================================");
  Serial.println("Hardware USB Detection: GPIO pin monitoring");
  Serial.println("Fallback Detection: Voltage-based analysis");
  Serial.println("LED Indicator: Red LED shows charging status");
  Serial.println();
}

void loop() {
  // The charging indicator and USB detection are automatically updated
  // by the FED4 library during sensor polling, but you can also check manually:
  
  bool usbConnected = fed.isUSBPowered();
  bool isCharging = fed.isCharging();
  float voltage = fed.getBatteryVoltage();
  float percentage = fed.getBatteryPercentage();
  
  // Print detailed status
  Serial.printf("Status - Voltage: %.3fV (%.1f%%) | USB: %s | Charging: %s\n", 
                voltage, percentage, 
                usbConnected ? "YES" : "NO",
                isCharging ? "YES" : "NO");
  
  // Run the main FED4 loop (this calls pollSensors() and updateChargingIndicator() internally)
  fed.run();
  
  delay(2000); // Check every 2 seconds
}

/*
 * Hardware Setup Notes:
 * 
 * For proper USB detection, the FED4 hardware should connect the USB VBUS line
 * to GPIO pin 1 (USB_DETECT_PIN) through a voltage divider or level shifter.
 * 
 * Typical circuit:
 * USB VBUS (5V) -> Voltage Divider (5V to 3.3V) -> GPIO 1
 * 
 * The ESP32-S3 will read this pin to detect USB connection status.
 * If the hardware connection is not available, the system falls back
 * to voltage-based detection using the battery voltage readings.
 */
