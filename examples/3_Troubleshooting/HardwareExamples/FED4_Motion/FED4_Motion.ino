/*
Example for using the STHS34PF80 presence/motion sensor on FED4
*/

#include "SparkFun_STHS34PF80_Arduino_Library.h"
#include <Wire.h>

STHS34PF80_I2C mySensor;
TwoWire I2C_2 = TwoWire(1);
int16_t motionVal = 0;

void setup() {
  Serial.begin(115200);

  pinMode(47, OUTPUT);
  digitalWrite(47, HIGH);

  if (I2C_2.begin(20, 19) == 0) {
    Serial.println("I2C_2 Error - check I2C Address");
    while (1)
      ;
  }
  delay(10);

  // Establish communication with second device
  mySensor.begin(0x5A, I2C_2);
  Serial.println("STHS34PF80 Initialized");

  // Set data rate for motion sensing
  mySensor.setTmosODR(STHS34PF80_TMOS_ODR_AT_30Hz);

  // Settings
  mySensor.setGainMode(STHS34PF80_GAIN_DEFAULT_MODE);
  mySensor.setLpfMotionBandwidth(STHS34PF80_LPF_ODR_DIV_20);
  mySensor.setMotionThreshold(200);
  mySensor.setMotionHysteresis(10);
}

void loop() {
  sths34pf80_tmos_drdy_status_t dataReady;
  mySensor.getDataReady(&dataReady);
  // Check whether sensor has new data - run through loop if data is ready
  if (dataReady.drdy == 1) {
    Serial.print(millis());
    sths34pf80_tmos_func_status_t status;
    mySensor.getStatus(&status);

    // Motion detected or not
    if (status.mot_flag == 1) {
      Serial.println(", Motion detected! ");

    } else {
      Serial.println(", None");
    }
    delay(10);  // Add a slight delay 
  }
}