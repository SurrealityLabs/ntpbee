static char GPSBuffer[128];

void gen_nmea(time_t timeUNIX)
{
  struct tm * timeinfo;
  
  timeinfo = gmtime(&timeUNIX);

  snprintf(GPSBuffer, 128, "$GPZDA,%02d%02d%02d.%03d,%02d,%02d,%04d,,*", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, 0, timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
  nmea_checksum(GPSBuffer);
  Serial.println(GPSBuffer);  
  
  snprintf(GPSBuffer, 128, "$GPGGA,%02d%02d%02d.%03d,4315.652,N,07955.198,W,1,12,1.0,0.0,M,0.0,M,,*", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, 0);
  nmea_checksum(GPSBuffer);
  Serial.println(GPSBuffer);
  
  snprintf(GPSBuffer, 128, "$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*");
  nmea_checksum(GPSBuffer);
  Serial.println(GPSBuffer);
  
  snprintf(GPSBuffer, 128, "$GPRMC,%02d%02d%02d.%03d,A,4315.652,N,07955.198,W,,,%02d%02d%02d,000.0,W*", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, 0, timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year % 100);
  nmea_checksum(GPSBuffer);
  Serial.println(GPSBuffer);
}

static void nmea_checksum(char *inp)
{
  uint8_t tmp = 0;
  char *nmea = inp;
  char tmpstr[128];
  
  while (*nmea++)
  {
    if (*nmea == '$')
    {
      // skip
    }
    else if (*nmea == '*')
    {
      // append checksum
      snprintf(tmpstr, 128, "%s%02X", inp, tmp);
      strncpy(inp, tmpstr, 128);
      return;
    }
    else
    {
      tmp ^= *nmea;
    }
  }
}
