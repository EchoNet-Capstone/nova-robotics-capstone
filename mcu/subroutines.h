
void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}


void VextOFF(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);
}


void set_release_timer() {
  if (release_timer > InternalClock() ) {
    motor_run_to_position(closed_position);
    waiting_to_be_retrieved = 0;                                                        // If there is time on the clock, not waiting to be recovered
  }
  else {
    motor_run_to_position(open_position);
  }
}


// Measure battery voltage
uint16_t sampleBatteryVoltage(void) {
  noInterrupts();
  VextON();

  // Other ADC Channels
  //uint16_t level1;
  //uint16_t level2;
  //uint16_t level3;
  //return adc level 0-4095;
  //level1=analogRead(ADC1);
  //level2=analogRead(ADC2);
  //level3=analogRead(ADC3);


  //pinMode(VBAT_ADC_CTL, OUTPUT);
  //digitalWrite(VBAT_ADC_CTL, LOW);
  uint16_t volts = getBatteryVoltage();

  //analogRead(ADC) * 2;
  //pinMode(VBAT_ADC_CTL, INPUT);
  interrupts();                                                                         // Reenable interrupts after sample

  uint16_t battery_usable_volts = battery_full - battery_empty;

  battery_percent = (int)( (100 * (volts - battery_empty) ) / battery_usable_volts);
  if (battery_percent > 100) battery_percent = 100;
  if (battery_percent < 0) battery_percent = 0;

  if (battery_percent < low_battery) release_timer = InternalClock();             // Release trap if battery gets low

  return volts;
}


void reed_switch_debounce() {
  long time_delta;

  reed_switch1 = !digitalRead(reed_switch_input1);                                                                        // Signal is inverted
  reed_switch2 = !digitalRead(reed_switch_input2);                                                                        // Signal is inverted
  time_delta = InternalClock() - reed_switch_first_press;

  // Time until release
  //time_until_release = release_timer - InternalClock();
  //if (time_until_release < 0) time_until_release = 0;

  if ( (reed_switch1 || reed_switch2) && (release_timer < InternalClock() ) ) release_timer = InternalClock();            // Don't let release_timer have old time


  // If magnet is present
  if ( reed_switch1 || reed_switch2 ) {

    waiting_to_be_retrieved = 0;                                                                                          // User has interacted - not waiting to be retrieved

    if ( time_delta > reed_switch_calibrate ) {
      while (!digitalRead(reed_switch_input1) ) {
        time_delta = InternalClock() - reed_switch_first_press;
        motor_forward();
        noInterrupts();
        if ( time_delta < reed_switch_super_long_press ) feedInnerWdt();                                                  // Pet the watchdog while stuck in calibrate loop, stop if super long press to allow reboot
        delay(80);
        motor_position = 0;                                                                                               // If calibrate is active, reset motor position to zero
        motor_off();
        interrupts();
        delay(500);
        waiting_to_be_retrieved = 1;
      }
    }
    else if ( time_delta > reed_switch_long_press ) {
      release_timer = InternalClock();                                                                                    // Reset timer if long press
      //timer_tap_multiplier1 = 0;                                                                                          // Reset timer tap counter
      //timer_tap_multiplier2 = 0;
      //release_timer1 = 0;
      //release_timer2 = 0;
    }
    else if ( time_delta > reed_switch_short_press ) {

      input_slowdown_toggle = !input_slowdown_toggle;                                                                     // Slow down adding time

      if (reed_switch1 && input_slowdown_toggle) {
        if ( release_timer < ( 60 + InternalClock() ) ) release_timer = InternalClock() + release_timer_first_press_1;
        else release_timer += release_timer_add_1;
      }

      else if (reed_switch2 && input_slowdown_toggle) {
        if ( release_timer < ( 60 + InternalClock() ) ) release_timer = InternalClock() + release_timer_first_press_2;
        else release_timer += release_timer_add_2;
      }

      //timer_tap_multiplier1++;                                                                                            // Add time to the release countdown timer
      //release_timer1 = InternalClock() + release_timer_first_press_1 + ( ( (timer_tap_multiplier1 - 1) / 2) * release_timer_add_1) ;              // Divide by 2 to slow down timer increase rate

      //release_timer = release_timer1 + release_timer2;

      wiggle_timer = InternalClock() + wiggle_interval;                                                                   // Reset the wiggle timer
    }
  }

}


void am_i_waiting_to_be_recovered() {
  if ( abs(motor_position) < (open_position + 1000) ) release_is_open = 1;
  else if ( abs(motor_position) > (closed_position - 1000) ) release_is_open = 0;

  //if ( (motor_target == open_position) && (motor_target_last == closed_position) ) waiting_to_be_retrieved = 1;                            // If it was closed but is now open, release is waiting to be picked up

  if ( reed_switch1 || reed_switch2 ) waiting_to_be_retrieved = 0;
  else if ( (release_is_open == 1) && (release_last_position == 0) ) waiting_to_be_retrieved = 1;

  release_last_position = release_is_open;
  last_reed_switch_state = ( reed_switch1 || reed_switch2 );                                                                                   // Save state for next time
}




void debug_subroutine(void) {
  Serial.printf("Main Clock: %ld Time Until Release: %ld Encoder Time: %ld Encoder Power: %d Vext: %d\n", InternalClock(), time_until_release, encoder_timer, digitalRead(motor_driver_power), digitalRead(Vext));
}
