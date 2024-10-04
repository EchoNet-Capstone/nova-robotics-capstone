void TimerWakeUp()
{
  if (debug) Serial.printf("Woke up at %ld s.  Countdown timer %ld.\n", InternalClock(), (long)release_timer );
  lowpower = 0;
  feedInnerWdt();           // Pet the watchdog

  // Is release open?
  am_i_waiting_to_be_recovered();

  set_release_timer();                                                                                    // This opens and closes the release based on time

  // Flash LED or turn it off if not needed
  if (led_enable) {
    if ( (waiting_to_be_retrieved == 1) && (display_active == 0) ) led_flasher();
    else rgb_led(0, 0, 0); // Set all pixel colors to 'off'
  }

}



void go_to_sleep()
{
  // Prevent CPU sleep if GPS is active
  if (gps_enabled == 1) sleep_inhibit = 1;
  else sleep_inhibit = 0;

  if (!sleep_inhibit) lowpower = 1;
  if (sleep_inhibit) lowpower = 0;

  display_active = 0;
  motor_sleep();
  oled_sleep();

  if ( (waiting_to_be_retrieved == 0) ) VextOFF();                  // Turn off Vext only if release is not waiting, otherwise supply power to LED

  // If allowed to sleep, then set timer
  if (!sleep_inhibit) {
    Radio.Sleep( );                                                 // Power down radio if sleep is allowed
    TimerSetValue( &wakeUp, timetillwakeup );
    TimerStart( &wakeUp );
  }
  else {
    delay(timetillwakeup);                                          // Just wait around for a while if sleep is inhibited
    TimerWakeUp();
  }

}



// Reed switch interrupt
void gpio_interrupt() {
  lowpower = 0;
  reed_switch1 = !digitalRead(reed_switch_input1);                                                          // Signal is inverted
  reed_switch2 = !digitalRead(reed_switch_input2);                                                          // Signal is inverted

  noInterrupts();                                                                                         // Disable interrupts or motor won't spin
  if (debug) Serial.printf("GPIO Interrupt at %d ms.\n", InternalClock() );
  reed_switch_first_press = InternalClock();                                                              // new press, record first time of key press
  am_i_waiting_to_be_recovered();
  display_timer = InternalClock() + display_timeout;                                                      // Activate display
  //delay(500);                                                                                           // Add small delay to allow switch to settle - THIS BREAKS THINGS
  interrupts();
}
