// /********************************************************
//   log data to csv file in sd card
// ********************************************************/
// void LogData() {
//   Serial.print("Start logging");
//   GreenPix();
//   Serial.print("...green...");
//   WriteToSD();
//   Serial.print("...write done...");
//   logfile.flush();
//   Serial.println("...flush done");
//   NoPix();
// }

// /********************************************************
//   write FED4 data to sd
// ********************************************************/
// void WriteToSD() {
//   DateTime now = rtc.now();
//   logfile.print(now.month());
//   logfile.print("/");
//   logfile.print(now.day());
//   logfile.print("/");
//   logfile.print(now.year());
//   logfile.print(" ");
//   logfile.print(now.hour());
//   logfile.print(":");
//   if (now.minute() < 10) {
//     logfile.print('0');
//   }  // Trick to add leading zero for formatting
//   logfile.print(now.minute());
//   logfile.print(":");
//   if (now.second() < 10) {
//     logfile.print('0');
//   }  // Trick to add leading zero for formatting
//   logfile.print(now.second());
//   logfile.print(",");

//   float elapsedSec = millis() / 1000.000;
//   logfile.print(elapsedSec, 3);  // print elapsed time in s
//   logfile.print(",");

//   logfile.print(FED4);  // print elapsed time in s
//   logfile.print(",");

//   Serial.print("... log'd time ...");


//   float rtctemp = rtc.getTemperature();
//   logfile.print(rtctemp, 1);
//   logfile.print(",");

//   Serial.print("... log'd rtctemp ...");

//   sensors_event_t humidity, temp;
//   aht.getEvent(&humidity, &temp);  // populate temp and humidity objects with fresh data


//   logfile.print(temp.temperature, 1);
//   logfile.print(",");

//   logfile.print(humidity.relative_humidity, 1);
//   logfile.print(",");

//   Serial.print("... log'd aht ...");

//   uint32_t raw = adc1_get_raw((adc1_channel_t)VBAT_ADC_CHANNEL);
//   millivolts = esp_adc_cal_raw_to_voltage(raw, adc_cal);
//   float bat = getBatteryVoltage();
//   logfile.print(bat);
//   logfile.print(",");

//   Serial.print("... log'd vbat...");

//   logfile.print(Event);
//   logfile.print(",");


//   logfile.print(PelletCount);
//   logfile.print(",");
//   logfile.print(CenterCount);
//   logfile.print(",");
//   logfile.print(LeftCount);
//   logfile.print(",");
//   logfile.print(RightCount);
//   logfile.print(",");
//   if (RetrievalTime > 0) {
//     logfile.println(RetrievalTime);
//   } else {
//     logfile.println(sqrt(-1));  // print NaN if it's not a pellet Event
//   }
//   Serial.print("... logging");
// }

// /********************************************************
//   create new sd file when FED4 is restarted
// ********************************************************/
// void CreateFile() {
//   // see if the card is present and can be initialized:
//   if (!SD.begin(10)) {
//     Serial.println("SD error");
//   }

//   // Name filename in format F###_MMDDYYNN, where MM is month, DD is day, YY is year, and NN is an incrementing number for the number of files initialized each day
//   strcpy(filename, "FED4______________.CSV");  // placeholder filename
//   getFilename(filename);
//   Serial.print("new filename: ");
//   Serial.println(filename);
//   logfile = SD.open(filename, FILE_WRITE);

//   //write header
//   WriteHeader();
//   logfile.flush();
// }

// /********************************************************
//   write column headers to sd
// ********************************************************/
// void WriteHeader() {
//   logfile.println("Timestamp,ElapsedSeconds,Device,RTCTemperature,AHTTemperature,Humidity,vBat,Event,PelletCount,CenterCount,LeftCount,RightCount,RetrievalTime");
// }

// /********************************************************
//   Generate unique filename based on device number and date 
// ********************************************************/
// void getFilename(char *filename) {
//   DateTime now = rtc.now();
//   filename[5] = FED4 / 100 + '0';
//   filename[6] = FED4 / 10 + '0';
//   filename[7] = FED4 % 10 + '0';
//   filename[9] = now.month() / 10 + '0';
//   filename[10] = now.month() % 10 + '0';
//   filename[11] = now.day() / 10 + '0';
//   filename[12] = now.day() % 10 + '0';
//   filename[13] = (now.year() - 2000) / 10 + '0';
//   filename[14] = (now.year() - 2000) % 10 + '0';
//   for (uint8_t i = 0; i < 100; i++) {
//     filename[16] = '0' + i / 10;
//     filename[17] = '0' + i % 10;
//     if (!SD.exists(filename)) {
//       break;
//     }
//   }
//   return;
// }
