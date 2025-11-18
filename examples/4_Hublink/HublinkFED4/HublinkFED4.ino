#include <FED4.h> //Include the FED3 library
FED4 fed4;

#include <Hublink.h>
Hublink hublink(SD_CS); // Use default Hublink instance

// Hublink callback function to handle timestamp
void onTimestampReceived(uint32_t timestamp)
{
  Serial.println("Received timestamp: " + String(timestamp));
  fed4.adjustRTC(timestamp);
}

void setup()
{
  Serial.begin(9600);
  delay(1000);

  fed4.begin(); // inits SD card
  hublink.begin();
  hublink.onTimestampReceived(onTimestampReceived);
}

void loop()
{
  int batteryLevel = (int)fed4.getBatteryPercentage();
  if (batteryLevel < 20)
  {
    hublink.setAlert("Low Battery!");
  }
  hublink.setBatteryLevel(batteryLevel); // set before sync; consider 1s timer if loop delay ~= 0
  hublink.sync();                        // only blocks when ready
  // fed4.Feed();
  // fed4.sleep();
  delay(1000);
  Serial.println("I'm alive");
}