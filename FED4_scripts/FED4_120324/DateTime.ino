void dateTime(uint16_t* date, uint16_t* time) {
  DateTime rnow = rtc.now();
  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(rnow.year(), rnow.month(), rnow.day());

  // return time using FAT_TIME macro to format fields
  *time = FAT_TIME(rnow.hour(), rnow.minute(), rnow.second());
}
