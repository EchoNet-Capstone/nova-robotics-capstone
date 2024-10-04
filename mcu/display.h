// Sleep display for power saving
void oled_sleep() {
  oled.sleep();//OLED sleep
  //VextOFF();
}


// Wake up display
void oled_wake() {
  VextON();
  oled.wakeup();
}


void update_display() {
  //long time_until_release;

  // Time until release
  time_until_release = release_timer - InternalClock();
  if (time_until_release < 0) time_until_release = 0;

  int release_days = time_until_release / 86400;
  int release_hours = (time_until_release - (release_days * 86400) ) / 3600;
  int release_minutes = (time_until_release - (release_days * 86400) - (release_hours * 3600) ) / 60;
  int release_seconds = (time_until_release - (release_days * 86400) - (release_hours * 3600) - (release_minutes * 60) );

  // 9 pixel height works well for line spacing
  oled_wake();
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.drawString(0, 0, "Battery: " + (String)battery_percent + "%");
  //oled.drawString(10, 0, "Battery: " + (String)(battery_volts) + "mV  " + (String)battery_percent + "%");

  if (gps_lock == 1) {
    oled.drawString(85, 0, "GPS Lock");
  }
  else if (gps_enabled == 1) {
    oled.drawString(85, 0, "GPS On");
  }
  else {
    oled.drawString(85, 0, "GPS Off");
  }


  if (time_until_release > 0) {
    int reset_countdown = 0;

    if (reed_switch1 || reed_switch2 ) reset_countdown = reed_switch_long_press - (InternalClock() - reed_switch_first_press);
    else reset_countdown = reed_switch_long_press;

    oled.drawString(5, 15, "Hold magnet " + (String)reset_countdown + " seconds");
    oled.drawString(14, 24, "to reset timer to zero.");

    //oled.drawString(0, 31, "Release Countdown:");
    //oled.drawString(0, 31, "Uptime: " + (String)InternalClock() );

    oled.setFont(ArialMT_Plain_16);

    // Draw Days
    if (release_days < 10) {
      oled.drawString(0, 40, "  " + (String)release_days);
      oled.drawString(1, 40, "  " + (String)release_days);
    }
    else if (release_days < 100) {
      oled.drawString(0, 40, " " + (String)release_days);
      oled.drawString(1, 40, " " + (String)release_days);
    }
    else {
      oled.drawString(0, 40, (String)release_days);
      oled.drawString(1, 40, (String)release_days);
    }

    // Draw Hours
    if (release_hours < 10) {
      oled.drawString(42, 40, " " + (String)release_hours);
      oled.drawString(43, 40, " " + (String)release_hours);
    }
    else {
      oled.drawString(42, 40, (String)release_hours);
      oled.drawString(43, 40, (String)release_hours);
    }

    // Draw Minutes
    if (release_minutes < 10) {
      oled.drawString(74, 40, "0" + (String)release_minutes);
      oled.drawString(75, 40, "0" + (String)release_minutes);
    }
    else {
      oled.drawString(74, 40, (String)release_minutes);
      oled.drawString(75, 40, (String)release_minutes);
    }

    // Draw Seconds
    if (release_seconds < 10) {
      oled.drawString(107, 40, "0" + (String)release_seconds);
      oled.drawString(108, 40, "0" + (String)release_seconds);
    }
    else {
      oled.drawString(107, 40, (String)release_seconds);
      oled.drawString(108, 40, (String)release_seconds);
    }

    oled.setFont(ArialMT_Plain_10);                                       // Reset font back to small
    oled.drawString(0, 54, "DAYS    HRS    MIN     SEC");
  }

  else {
    oled.drawString(8, 16, "Hold magnet for at least ");
    oled.drawString(12, 25, (String)reed_switch_short_press + " second to set timer.");

    oled.setFont(ArialMT_Plain_16);
    oled.drawString(0, 45, "RELEASE OPEN");
    oled.drawString(1, 45, "RELEASE OPEN");
  }


  oled.setFont(ArialMT_Plain_10);                                       // Reset font back to small
  // Is release open?
  //if ( release_is_open == 1 ) oled.drawString(20, 54, "RELEASE OPEN");
  //else oled.drawString(15, 54, "RELEASE CLOSED");

  oled.display();

}


void rgb_led(uint8_t red, uint8_t green, uint8_t blue) {
  if ( (red > 0) || (green > 0) || (blue > 0) ) {
    //VextON();
    rgbpixel.begin(); // INITIALIZE RGB strip object (REQUIRED)
    rgbpixel.clear(); // Set all pixel colors to 'off'
    rgbpixel.setPixelColor(0, rgbpixel.Color(red, green, blue));
    rgbpixel.show();   // Send the updated pixel colors to the hardware.
  }
  else {
    rgbpixel.begin(); // INITIALIZE RGB strip object (REQUIRED)
    rgbpixel.setPixelColor(0, rgbpixel.Color(red, green, blue));
    //rgbpixel.clear(); // Set all pixel colors to 'off'
    rgbpixel.show();   // Send the updated pixel colors to the hardware.
  }

}


void led_flasher() {

  rgb_led(255, 0, 0);
  delay(20);
  rgb_led(0, 0, 0);
  delay(100);
  rgb_led(0, 255, 0);
  delay(20);
  rgb_led(0, 0, 0);
  is_led_activated = 1;

  //if (gps_lock == 1){
  //  rgb_led(0, 0, 32);
  //}

}


// Bootup company logo
void logo() {
  VextON();// oled power on;
  delay(10);

  oled.init();
  oled.clear();
  oled.display();

  oled.clear();
  oled.setFont(ArialMT_Plain_16);
  oled.drawString(10, 15, "NOVA");
  oled.drawString(11, 15, "NOVA");
  oled.drawString(12, 15, "NOVA");
  oled.drawString(35, 30, "ROBOTICS");
  oled.display();
  //oled.setFont(ArialMT_Plain_10);
}



// Waiting screen
void waiting_screen() {
  VextON();// oled power on;
  delay(10);

  oled.init();
  oled.clear();
  oled.display();

  oled.clear();
  oled.setFont(ArialMT_Plain_16);
  oled.drawString(25, 5, "WAITING...");
  //oled.setFont(ArialMT_Plain_10);
  oled.drawString(10, 35, "TAP MAGNET");
  oled.display();
  //oled.setFont(ArialMT_Plain_10);
}
