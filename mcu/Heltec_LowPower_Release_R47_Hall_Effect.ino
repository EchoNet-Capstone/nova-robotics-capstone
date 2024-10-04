#include "Arduino.h"
#include "innerWdt.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "CubeCell_NeoPixel.h"
#include "LoRaWan_APP.h"
//#include "LoRa_APP.h"
#include "GPS_Air530.h"
#include "GPS_Air530Z.h"
#include "globals.h"
#include "motor.h"
#include "subroutines.h"
#include "display.h"
#include "radio.h"
#include "sleep.h"
#include "gps.h"



void setup() {
  // put your setup code here, to run once:
  if (debug) Serial.begin(115200);
  TimerReset(0);
  boardInitMcu();                                                                                         // Hopefully reset onboard timers

  battery_volts = sampleBatteryVoltage();                                                                 // Initial battery state measurement

  delay(100);

  if (debug) Serial.printf("Booting up...\n");

  // Disable interrupts for bootup
  noInterrupts();

  // Userkey interrupt
  //pinMode(INT_GPIO, INPUT);
  if (hall_effect) {
    pinMode(reed_switch_input1, INPUT);
    pinMode(reed_switch_input2, INPUT);
  }
  else {
    pinMode(reed_switch_input1, INPUT_PULLUP);                                                               // Internal pull-up is 4.7k --maybe
    pinMode(reed_switch_input2, INPUT_PULLUP);
  }

  attachInterrupt(reed_switch_input1, gpio_interrupt, FALLING);
  attachInterrupt(reed_switch_input2, gpio_interrupt, FALLING);

  // Setup motor quadrature and interrupt
  pinMode(motor_quad_a, INPUT);
  pinMode(motor_quad_b, INPUT);
  attachInterrupt(motor_quad_a, motor_quadrature_interrupt, RISING);

  // Setup motor driver - Uses 1-2 mA
  pinMode(motor_driver_power, OUTPUT);
  pinMode(motor_driver_a, OUTPUT);
  pinMode(motor_driver_b, OUTPUT);
  digitalWrite(motor_driver_power, LOW);
  digitalWrite(motor_driver_a, LOW);
  digitalWrite(motor_driver_b, LOW);

  // Setup GPS Power Control
  //pinMode(gps_power, OUTPUT);                               // This is controlled by Air530 library
  //gps_wake();
  //Air530.begin();
  //Air530.reset();
  //gps_sleep();                                              // Disable GPS module on boot
  //Serial.printf("GPS Initialized\n");

  // Enable interrupts
  interrupts();

  logo();
  //rgb_led(0, 0, 8);
  //rgbpixel.clear();

  // ROTATE SHAFT ON BOOT!!!
  motor_run_to_position(closed_position);
  delay(5000);
  motor_run_to_position(open_position);

  //Enable the WDT.
  //autoFeed = false: do not auto feed wdt.
  //autoFeed = true : it auto feed the wdt in every watchdog interrupt.
  //bool autoFeed = false;
  //innerWdtEnable(autoFeed);

  // Star LoRa radio
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;

  Radio.Init( &RadioEvents );
  Radio.SetChannel( RF_FREQUENCY );
  Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                     LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                     LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                     true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

  Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                     LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                     LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                     0, true, 0, 0, LORA_IQ_INVERSION_ON, true );

  Radio.Sleep( );

  VextOFF();

  TimerInit( &wakeUp, TimerWakeUp );
  go_to_sleep();
}


void loop() {
  if (debug) Serial.printf("Loop Started\n");

  long release_delta = release_timer - InternalClock();         // Calculate difference from clock to set release time

  feedInnerWdt();                                               // Pet the watchdog - interrupt generated every 1.4 seconds.  Two consecutive interrupts causes a reboot (2.8s)

  if (lowpower) {
    //if (debug) Serial.printf("lowPowerHandler\n");
    lowPowerHandler();                                          //note that lowPowerHandler() runs six times before the mcu goes into lowpower mode;
  }

  // Release is opened and closed in TimerWakeUp interrupt routine

  // Time until release
  time_until_release = release_timer - InternalClock();
  if (time_until_release < 0) time_until_release = 0;

  // Update battery samples
  if (battery_timer < InternalClock() ) {
    //Serial.printf("Sampling battery voltage.\n");
    battery_volts = sampleBatteryVoltage();
    battery_timer = InternalClock() + battery_interval;
  }

  // Wiggle motor to break barnacles if release is closed
  if ( (wiggle_timer < InternalClock() ) && ( release_is_open == 0 ) ) {

    // If we're more than wiggle_deadband seconds from opening the release, then allow a wiggle
    if ( wiggle_timer < (release_timer - wiggle_deadband) ) {
      wiggle_motor();
    }

    wiggle_timer = InternalClock() + wiggle_interval;
  }

  // Enable GPS if release is waiting to be retrieved
  if ( gps_enable && waiting_to_be_retrieved && ( ( InternalClock() - last_gps_fix ) > gps_interval ) ) {
    //Serial.printf("Waking GPS.\n");
    gps_wake();                                               // This will only wake if GPS is asleep
    if (Air530.available() > 0) {
      if (debug) Serial.printf("GPS Available.\n");
      update_gps();
    }
  }
  else gps_sleep();


  // Power down encoder and motor driver if it's been a while since motor has been energized
  if (encoder_timer < InternalClock() ) {
    motor_sleep();
  }


  // Send a LoRa packet
  if ( lora_enable && waiting_to_be_retrieved && ( lora_timer < InternalClock() ) ) {
    if (debug) Serial.printf("Sending LoRa packet.\n");
    oled_wake();                                                                                     // Need to power up Vext to supply power to LoRa radio
    //sprintf(txpacket, "%s %f %s %f", "Lat:", gps_latitude, "Long:", gps_longitude);                  // Save packet in txpacket
    //Radio.Send( (uint8_t *)lora_tx_packet, strlen(lora_tx_packet) );                                           // send the package out
    //printf("%s", txpacket);
    lora_timer = InternalClock() + lora_interval;
    //rgb_led(0, 0, 16);                                                                             // Flash blue LED to indicate packet has been sent
    //delay(20);
    packet_number++;                                                                                 // Use this for testing only
  }
  else {
    Radio.Sleep();                                                                                   // Put LoRa to sleep if we're not sending a packet
    //lora_timer = InternalClock() + lora_interval;                                                  // Increment this even if not tranmitting.  That way the LoRa radio won't transmit immediately upon activation (let float to surface) THIS BREAKS THINGS
  }


  // Draw OLED display
  int loop_counter = 0;
  timer_tap_multiplier1 = 0;                                                                          // Reset timer tap counter
  timer_tap_multiplier2 = 0;                                                                          // Reset timer tap counter

  while ( (display_timer > InternalClock() ) || (abs(release_delta) < 10) ) {
    release_delta = release_timer - InternalClock();                                                 // Re-calculate difference from clock to set release time or it'll get stuck in display loop

    //VextON();                                                                                      // Power up Vext
    TimerWakeUp();                                                                                   // Call timer routine because it stops working when display loop is active
    display_active = 1;

    if ( loop_counter > 5 ) reed_switch_debounce();                                                  // Check reed switch input

    update_display();
    delay(500);                                                                                      // This delay controls how fast time is added
    loop_counter++;
  }

  // Display notification screen if Waiting to be retrieved
  if ( (waiting_to_be_retrieved == 1) && (display_active == 0) ) {
    waiting_screen();
    delay(500);
  }


  // Flash LED or turn it off if not needed
  //if ( (waiting_to_be_retrieved == 1) && (display_active == 0) ) led_flasher();
  //else rgbpixel.clear(); // Set all pixel colors to 'off'

  //Make sure Vext is off
  if ( (waiting_to_be_retrieved == 0) && (display_active == 0) ) VextOFF();

  if (debug) debug_subroutine();
  Radio.IrqProcess( );                                                                               // I have no idea what this does - something about being able to call an interrupt on completion of LoRa TX
  if (debug) Serial.printf("Loop Ended, going to sleep.\n");
  go_to_sleep();

}
