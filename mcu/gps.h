void gps_wake() {
  if (gps_enabled == 0) {
    //digitalWrite(gps_power, LOW);                                           // This is controlled by Air530 library?
    //Air530.begin(9600);
    //Air530.setPPS(3, 200);                                                  // Doesn't seem to do anything
    //Air530.setmode(MODE_GPS);
    //Air530.sendcmd("$PGKC115,1,1,1,1*2B\r\n"); // mode: gps, glonass, beidou, galileo [0: off, 1: on]
    //Air530.setNMEA(NMEA_GGA | NMEA_RMC | NMEA_VTG);
    //Air530.setNMEA(NMEA_GST);
    //Air530.setNMEA(NMEA_GGA);
  }
  gps_enabled = 1;
  //sleep_inhibit = 1;                                                        // Keep CPU awake if GPS is in use
}



void gps_sleep() {
  if (gps_enabled == 1) {
    Air530.end();
  }
  gps_enabled = 0;
  //sleep_inhibit = 0;                                                        // Allow CPU to go to sleep if GPS is not in use
}



void update_gps() {

  feedInnerWdt();           // Pet the watchdog

  while (Air530.available() > 0)
  {
    Air530.encode(Air530.read());
  }

  //String NMEA = Air530.getNMEA();
  //String NMEA = Air530.getGSA();
  String NMEA = Air530.getGGA();
  if (NMEA != "0") {
    if (debug) Serial.println(NMEA);

    int commas[15];
    commas[0] = NMEA.indexOf(',');
    for (int i = 1; i < 10; i++) {
      commas[i] = NMEA.indexOf(',', commas[i - 1] + 1 );
    }

    String gps_time_string = NMEA.substring(commas[0] + 1, commas[1]);
    String gps_latitude_string = NMEA.substring(commas[1] + 1, commas[2]);

    String gps_north_south_string = NMEA.substring(commas[2] + 1, commas[3]);
    String gps_longitude_string = NMEA.substring(commas[3] + 1, commas[4]);
    String gps_east_west_string = NMEA.substring(commas[4] + 1, commas[5]);
    String gps_fix_string = NMEA.substring(commas[5] + 1, commas[6]);
    String gps_satellites_string = NMEA.substring(commas[6] + 1, commas[7]);
    String gps_hdop_string = NMEA.substring(commas[7] + 1, commas[8]);
    String gps_altitude_string = NMEA.substring(commas[8] + 1, commas[9]);

    gps_time = gps_time_string.toInt();
    gps_time_hours = gps_time / 10000;
    gps_time_minutes = (gps_time - (gps_time_hours * 10000) ) / 100;

    gps_latitude = gps_latitude_string.toFloat();
    if (gps_north_south_string == "S") gps_latitude = -gps_latitude;
    gps_latitude = gps_latitude / 100;
    int gps_latitude_degrees = (int)gps_latitude;
    float gps_latitude_minutes = 100 * ( (gps_latitude - gps_latitude_degrees) / 60 );
    gps_latitude = (float)gps_latitude_degrees + gps_latitude_minutes;

    gps_longitude = gps_longitude_string.toFloat();
    if (gps_east_west_string == "W") gps_longitude = -gps_longitude;
    gps_longitude = gps_longitude / 100;
    int gps_longitude_degrees = (int)gps_longitude;
    float gps_longitude_minutes = 100 * ( (gps_longitude - gps_longitude_degrees) / 60 );
    gps_longitude = (float)gps_longitude_degrees + gps_longitude_minutes;


    float gps_hdop = gps_hdop_string.toFloat();

    lora_packet_len = sprintf(lora_tx_packet, "%05d, %f, %f, %i%%\n", unit_id, gps_latitude, gps_longitude, battery_percent);

    //sprintf(gps_latitude_temp_string, "%s %f", "Lat:", gps_latitude);

    //snprintf(txpacket, sizeof txpacket, "%s%s%s%s", str1, str2, str3, str4);
    if (debug) {
      Serial.printf("GPS Time: %02d:%02d", gps_time_hours, gps_time_minutes);
      Serial.printf(" Lat: ");
      Serial.print(gps_latitude, 6);
      Serial.print(" Long: ");
      Serial.print(gps_longitude, 6);
      Serial.print(" HDOP: ");
      Serial.print(gps_hdop, 2);
      Serial.println();
      Serial.println(lora_tx_packet);
    }


    if ( (gps_hdop > 0.1 ) && (gps_hdop < 1.5) ) last_gps_fix = InternalClock();                                           // record time if we get a good GPS fix

  }

  if (debug) Serial.println();

}
