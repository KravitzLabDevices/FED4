#include "lis3dh-motion-detection.h"
#include "Wire.h"

#define LOW_POWER // Accelerometer provides different Power modes by changing output bit resolution, LOW power works for our use case.
// #define NORMAL_MODE
// #define HIGH_RESOLUTION
#define accel_int 45		// int pin, which doesnt work yet. this will change in FED V1.4
#define LIS3DH_DEBUG Serial // Enable Serial debbug on Serial UART to see registers wrote

uint16_t sampleRate = 50; // HZ - Samples per second - 1, 10, 25, 50, 100, 200, 400, 1600, 5000
uint8_t accelRange = 2;	  // Accelerometer range = 2, 4, 8, 16g
uint16_t errorsAndWarnings = 0;

LIS3DH myIMU(0x19); // Default address is 0x19.

void setup()
{
	// put your setup code here, to run once:

	Serial.begin(115200);
	delay(1000); // wait until serial is open...
	pinMode(accel_int, INPUT);

	if (myIMU.begin(sampleRate, 1, 1, 1, accelRange) != 0)
	{
		Serial.print("Failed to initialize IMU.\n");
	}
	else
	{
		Serial.print("IMU initialized.\n");
	}

	// Detection threshold can be from 1 to 127 and depends on the Range
	// chosen above, change it and test accordingly to your application
	// Duration = timeDur x Seconds / sampleRate
	myIMU.intConf(INT_1, DET_MOVE, 65, 3);

	uint8_t readData = 0;

	// Confirm configuration:
	myIMU.readRegister(&readData, LIS3DH_INT1_CFG);
}

void loop()
{
	/*

	  int16_t dataHighres = 0;

	  if( myIMU.readRegisterInt16( &dataHighres, LIS3DH_OUT_X_L ) != 0 )
	  {
		errorsAndWarnings++;
	  }
	  Serial.print(" Acceleration X RAW = ");
	  Serial.println(dataHighres);

	  if( myIMU.readRegisterInt16( &dataHighres, LIS3DH_OUT_Z_L ) != 0 )
	  {
		errorsAndWarnings++;
	  }
	  Serial.print(" Acceleration Z RAW = ");                                  // uncomment this block if you want to see the live XYZ axis data.
	  Serial.println(dataHighres);

	  // Read accelerometer data in mg as Float
	  Serial.print(" Acceleration Z float = ");
	  Serial.println( myIMU.axisAccel( Z ), 4);
	  delay(100);

	  */

	int accel = digitalRead(accel_int); // this just shows the interupt firing when the device is picked up.
										// mind that the device needs to be in the orientation that it will sit in the device to fire accurately.
	Serial.println(accel);
}