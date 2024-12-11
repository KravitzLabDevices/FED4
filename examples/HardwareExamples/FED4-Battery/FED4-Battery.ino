#include <Arduino.h>
#include <esp_adc_cal.h>
#include <driver/adc.h>

#define VBAT_ADC_CHANNEL ADC1_CHANNEL_6

esp_adc_cal_characteristics_t *adc_cal;
uint32_t millivolts = 0;

float getBatteryVoltage()
{
	const uint32_t upper_divider = 442;
	const uint32_t lower_divider = 160;
	return (float)(upper_divider + lower_divider) / lower_divider / 1000 * millivolts;
}

void setup()
{
	Serial.begin(115200);

	// Allocate memory for ADC characteristics
	adc_cal = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));

	// Characterize ADC
	esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 0, adc_cal);

	// Configure ADC channel
	adc1_config_width(ADC_WIDTH_BIT_12);
	adc1_config_channel_atten((adc1_channel_t)VBAT_ADC_CHANNEL, ADC_ATTEN_DB_11);
}

void loop()
{
	// Read raw ADC value
	uint32_t raw = adc1_get_raw((adc1_channel_t)VBAT_ADC_CHANNEL);

	// Convert ADC value to millivolts
	millivolts = esp_adc_cal_raw_to_voltage(raw, adc_cal);

	float bat = getBatteryVoltage();

	// Print battery voltage
	Serial.print("Battery voltage: ");
	Serial.print(bat);
	Serial.println(" V");

	delay(1000);
}
