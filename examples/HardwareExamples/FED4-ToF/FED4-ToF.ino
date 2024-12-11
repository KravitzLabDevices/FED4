#include <Adafruit_VL6180X.h>
#include <Adafruit_MCP23X17.h>
Adafruit_MCP23X17 mcp;

// address we will assign if dual sensor is present
#define LOX1_ADDRESS 0x30
#define LOX2_ADDRESS 0x31
#define LOX3_ADDRESS 0x32

// set the pins to shutdown
#define SHT_LOX1 1
#define SHT_LOX2 2
#define SHT_LOX3 15

// objects for the VL6180X
Adafruit_VL6180X lox1 = Adafruit_VL6180X();
Adafruit_VL6180X lox2 = Adafruit_VL6180X();
Adafruit_VL6180X lox3 = Adafruit_VL6180X();

void setID()
{
	// all reset
	mcp.digitalWrite(SHT_LOX1, LOW);
	mcp.digitalWrite(SHT_LOX2, LOW);
	mcp.digitalWrite(SHT_LOX3, LOW);
	delay(10);

	// all unreset
	mcp.digitalWrite(SHT_LOX1, HIGH);
	mcp.digitalWrite(SHT_LOX2, HIGH);
	mcp.digitalWrite(SHT_LOX3, HIGH);
	delay(10);

	// activating LOX1 and reseting LOX2
	mcp.digitalWrite(SHT_LOX1, HIGH);
	mcp.digitalWrite(SHT_LOX2, LOW);
	mcp.digitalWrite(SHT_LOX3, LOW);

	// initing LOX1
	if (!lox1.begin())
	{
		Serial.println(F("Failed to boot first VL6180X"));
		while (1)
			;
	}
	lox1.setAddress(LOX1_ADDRESS);
	delay(10);

	// activating LOX2
	mcp.digitalWrite(SHT_LOX2, HIGH);
	delay(10);

	// initing LOX2
	if (!lox2.begin())
	{
		Serial.println(F("Failed to boot second VL6180X"));
		while (1)
			;
	}
	lox2.setAddress(LOX2_ADDRESS);

	// activating LOX3
	mcp.digitalWrite(SHT_LOX3, HIGH);
	delay(10);

	// initing LOX3
	if (!lox3.begin())
	{
		Serial.println(F("Failed to boot third VL6180X"));
		while (1)
			;
	}
	lox3.setAddress(LOX3_ADDRESS);
}

void readSensor(Adafruit_VL6180X &vl, const char *sensorName)
{
	uint8_t range = vl.readRange();
	uint8_t status = vl.readRangeStatus();

	Serial.print(sensorName);
	Serial.print(": ");

	if (status == VL6180X_ERROR_NONE)
	{
		Serial.print(range);
		Serial.print(" mm ");
		Serial.print(" ");
	}
	else
	{
		Serial.print("0 ");
		Serial.print("mm ");
		Serial.print("  ");
	}
}

void read_sensors()
{
	readSensor(lox1, "Right");
	readSensor(lox3, "Middle");
	readSensor(lox2, "Left");
	Serial.println();
}

void setup()
{
	Serial.begin(115200);

	pinMode(47, OUTPUT);
	digitalWrite(47, HIGH);
	while (!Serial)
	{
		delay(1);
	}
	if (!mcp.begin_I2C())
	{
		Serial.println("Error initializing MCP23017.");
		while (1)
			;
	}

	mcp.pinMode(SHT_LOX1, OUTPUT);
	mcp.pinMode(SHT_LOX2, OUTPUT);
	mcp.pinMode(SHT_LOX3, OUTPUT);

	Serial.println("Shutdown pins inited...");

	mcp.digitalWrite(SHT_LOX1, LOW);
	mcp.digitalWrite(SHT_LOX2, LOW);
	mcp.digitalWrite(SHT_LOX3, LOW);
	Serial.println("All in reset mode...(pins are low)");

	Serial.println("Starting...");
	setID();
}

void loop()
{
	read_sensors();
	delay(100);
}
