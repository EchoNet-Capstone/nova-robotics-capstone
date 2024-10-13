#include "Arduino.h"
#include "EEPROM.h"
#include "GPS_Air530Z.h"
#include <Wire.h>  
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"
#include "packets.h"



// declare GPS class
Air530ZClass GPS; // HTCC-AB0S2 uses Air530Z

// declare display class
SSD1306Wire  int_display(0x3c, 500000, SDA, SCL, GEOMETRY_128_64, GPIO10 ); // addr , freq , i2c SDA, i2c SCL, resolution , rst

/*
 * set LoraWan_RGB to 1,the RGB active in loraWan
 * RGB red means sending;
 * RGB green means received done;
 */
#ifndef LoraWan_RGB
#define LoraWan_RGB 0
#endif

#define RF_FREQUENCY                                915000000 // Hz

#define TX_OUTPUT_POWER                             22        // dBm

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false

#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 1024 // Define the payload size here in bytes

#define BUOY_CAPABILITY CAPABILITIES_GPS
#define GPS_AGE_MAX 1200000 // (20 minutes) time in ms until GPS location is considered outdated
// #define GPS_AGE_MAX 5000 // 5 seconds for testing
// #define STANDBY_TIME 600000 // (10 minutes) time in ms until handshake is attempted again
#define STANDBY_TIME 10000 // 10 seconds for testing
#define REQ_TIMEOUT 10000 // (10 seconds) time in ms until handshake is aborted
#define ACK_TIMEOUT 5000 // (5 seconds) time in ms until GPS packet is resent
#define GPS_MAX_TRANSMIT 10
#define TX_COOLDOWN 20000 // 20 seconds minimum due to regulations

#define EEPROM_SIZE sizeof(bycc_flash)
#define DBID_EEPROM_ADDR 0
#define BID_EEPROM_ADDR 4

#define LOST_MODE_TIME 20 // time required to activate lost mode in hours
#define BATT_MAX 4050
#define BATT_MIN 3300


char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;

static TimerEvent_t standby;

static buoy_id_t buoy_id;
static recv_id_t parent_deckbox_id;

static uint32_t pairing_code;

bool lora_idle=true;

bycc_flash* fmem = NULL;

typedef enum {
  RX_REQ,
  RX_ACK,
  RX_PAIR
} rx_type;

typedef enum {
  TX_ERR = 0,
  TX_ANNOUNCE = 1,
  TX_ACK = 2,
  TX_GPS = 5,
  TX_LOST = 7
} tx_type;


static bool low_power;
static bool update_gps;
static bool lost_mode;
static uint32_t last_commit_time = 0;
static struct packet_gps last_payload;
static uint8_t transmit_attempt = 0;
static rx_type awaiting_response;
static tx_type current_transmit;
static int started_rx;
static int last_contact = 0; // stored in hours
static int last_transmission = TX_COOLDOWN;

void accessEEPROM(){
  EEPROM.begin(EEPROM_SIZE);
  fmem = (bycc_flash*)EEPROM.getDataPtr();
}

void flushEEPROM(){
  EEPROM.commit();
  EEPROM.end();
}

void setup() {
  Serial.begin(115200);

  // delay(5000);
  // Serial.printf("DEBUG >> Setting up...\n");
  // initialize standby timer event (periodically send announcements)
  TimerInit(&standby, onWakeUp);
  // configure LoRa
  setupLoRa();

  //get buoy and deckbox IDs
  accessEEPROM();
  if (fmem->buoy_id == 0) {
    enterPairing();
  } else {
    buoy_id = fmem->buoy_id;
    parent_deckbox_id = fmem->deckbox_id;
  }

  // Serial.printf("DEBUG >> Deckbox ID: %d\n", parent_deckbox_id);
  // Serial.printf("DEBUG >> Buoy ID: %d\n", buoy_id);

  // start with getting gps data
  // Serial.printf("DEBUG >> Getting GPS...\n");
  low_power = false;
  update_gps = true;
}

void loop() {
  if (low_power) {
    lowPowerHandler();
  }
  else if (update_gps) {
    update_gps = false;
    getGPSInfo(&last_payload);
    onWakeUp();
  }
}



void setupLoRa() {
  RadioEvents.RxDone = onRxDone;
  RadioEvents.RxTimeout = onRxTimeout;
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  Radio.Init( &RadioEvents );
  Radio.SetChannel( RF_FREQUENCY );
  Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                                   LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                                   LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   0, true, 0, 0, LORA_IQ_INVERSION_ON, true );
  Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                  LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                  LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                  true, 0, 0, LORA_IQ_INVERSION_ON, 3000 ); 
}


void enterLPM() {
  // Serial.printf("DEBUG >> Going to sleep\n");
  // start standby timer
  TimerSetValue(&standby, STANDBY_TIME);
  TimerStart(&standby);

  // put to sleep
  low_power = true;
  Radio.Sleep();
}

void onWakeUp() {
  // wake up chip
  low_power = false;
  // Serial.printf("DEBUG >> Waking Up...\n");

  // transmit announcement (implicitly sets to tx mode)
  if(lora_idle){
    struct packet_announce sending = {PKT_ANNOUNCE, 0, parent_deckbox_id, buoy_id, BUOY_CAPABILITY};
    if (millis() / 1000 / 60 / 60 - last_contact > LOST_MODE_TIME) {
      lost_mode = true;
    }
    if (lost_mode) {
      sending.capabilities |= CAPABILITIES_LOST;
    }

    transmitPacket(&sending, sizeof(sending));
  }
}


void afterTxAnnounce() {
  // wait for response (interrupts via recievedResp and respTimeout)
  // Serial.printf("DEBUG >> Waiting for response\n");
  awaiting_response = RX_REQ;
  started_rx = millis();
  Radio.Rx(REQ_TIMEOUT);
}

void afterTxErr() {
  if (awaiting_response = RX_PAIR) {
    Radio.Rx(0);
    return;
  }
  // Serial.printf("DEBUG >> Transmitted error packet, not sure what to do next\n");
}

void respTimeout() {
  // no response received from announcement
  enterLPM();
}

void receivedReq(uint8_t *payload_raw, uint16_t size) {
  struct packet_req* payload = (struct packet_req*)payload_raw;

  // if we are contacted by the owner, reset the timer
  if (payload->req_deckbox_id == parent_deckbox_id) {
    last_contact = millis() / 1000 / 60 / 60;
    lost_mode = false;
  }

  // Serial.printf("DEBUG >> Received request | type = %d, deckbox id = %d, buoy id = %d\n", payload->type, payload->parent_deckbox_id, payload->buoy_id);
  // send GPS Info
  sendGPSInfo(payload->req_deckbox_id, payload->type + 1);
}

void receivedPair(uint8_t* payload, uint16_t size) {
  struct packet_pair* pair_payload = (struct packet_pair*)payload;

  if (buoy_id == 0) {
    // we are in pairing mode

    if (pairing_code == pair_payload->pairing_code) {
      // correct code, set the deckbox ID and buoy ID
      accessEEPROM();
      fmem->buoy_id = pair_payload->buoy_id;
      fmem->deckbox_id = pair_payload->deckbox_id;
      parent_deckbox_id = pair_payload->deckbox_id;
      buoy_id = pair_payload->buoy_id;
      flushEEPROM();

      // turn off display
      char disp_string[30];
      sprintf(disp_string, "Pairing Complete");
      display_show(disp_string);
      delay(5000);
      int_display.displayOff();

      // send an ACK
      struct packet_ack ack = {PKT_ACK, 0, parent_deckbox_id, buoy_id};
      transmitPacket(&ack, sizeof(ack));
    } else {
      // incorrect error code, display error on the screen
      // Serial.printf("DEBUG >> Receieved incorrect pairing code: %d\n", pair_payload->pairing_code);

      // display error message on internal display
      char disp_string[60];
      sprintf(disp_string, "Incorrect Code:\n %d", pair_payload->pairing_code);
      display_show(disp_string);

      // go back to displaying the pairing code
      delay(5000);
      sprintf(disp_string, "Pairing Code:\n\r %d\r\n", pairing_code);
      display_show(disp_string);
    }
  }

}

void sendGPSInfo(recv_id_t req_deckbox_id, pkt_type_t type) {
  // Serial.printf("DEBUG >> Sending GPS info | type = %d, req_deckbox_id = %d\n", type, req_deckbox_id);

  // send info (potentially multiple times)
  transmit_attempt = 0;

  // not updated by the GPS module
  last_payload.recv_deckbox_id = req_deckbox_id;
  last_payload.type = type;
  last_payload.deckbox_id = parent_deckbox_id;
  last_payload.buoy_id = buoy_id;
  last_payload.age = (millis() - last_commit_time) / 60000; // age in minutes

  transmitPacket(&last_payload, sizeof(last_payload));
}

void transmitPacket(void* packet, size_t len) {
  if (lora_idle) {
    ((struct packet_hdr *)packet)->crc = crc8((uint8_t *)packet, len);
    memcpy(txpacket, packet, len);
    pkt_type_t type = getPacketType((uint8_t *)packet);
    turnOnRGB(COLOR_SEND,0); //change rgb color
    current_transmit = (tx_type)type;
    awaitTransmission();
    last_transmission = millis();
    // Serial.printf("DEBUG >> Sending packet (length = %d) (type = %d)\n", len, type);
    lora_idle = false;
    Radio.Send( (uint8_t *)txpacket, len ); //send the package out
  }
}

void awaitTransmission() {
  if (millis() - last_transmission >= TX_COOLDOWN) return;
  // Serial.printf("DEBUG >> Waiting for transmit cooldown (%d seconds)\n", (TX_COOLDOWN - (millis() - last_transmission)) / 1000);
  while (millis() - last_transmission < TX_COOLDOWN);
}

void afterTxGPS() {
  // wait for ack (interrupts via receivedAck and ackTimeout)
  // Serial.printf("DEBUG >> Waiting for ack\n");
  awaiting_response = RX_ACK;
  started_rx = millis();
  Radio.Rx(ACK_TIMEOUT);
}

void ackTimeout() {
  transmit_attempt++;
  // Serial.printf("DEBUG >> No ack received, resending (attempt %d)\n", transmit_attempt);
  if (transmit_attempt > GPS_MAX_TRANSMIT) {
    // max attempts reached
    // update info if necessary
    if (millis() - last_commit_time > GPS_AGE_MAX) {
      // Serial.printf("DEBUG >> Updating GPS info...\n");
      update_gps = true;
    }
    else {
      enterLPM();
    }
  }
  else {
    // try again (after min time plus a random number up to 2^transmit_attempt)
    awaitTransmission();
    delay(random(2^transmit_attempt-1));
    transmitPacket(&last_payload, sizeof(last_payload));
  }
}

void receivedAck(uint8_t *payload_u8, uint16_t size) {
  struct packet_ack* payload = (struct packet_ack*)payload_u8;

  // GPS info successfully sent
  // Serial.printf("DEBUG >> Ack received\n");
  // update info if necessary
  if (millis() - last_commit_time > GPS_AGE_MAX) {
    // Serial.printf("DEBUG >> Updating GPS info...\n");
    update_gps = true;
    return;
  }
  enterLPM();
}

void getGPSInfo(struct packet_gps* payload) {

  GPS.begin();

  uint32_t starttime = millis();
  
  do {
    if (GPS.location.age() > GPS_AGE_MAX) {
      // Serial.printf("DEBUG >> (Buoy) GPS info outdated. Updating location...\n");
    
      starttime = millis();
      while( (millis()-starttime) < 1000 ) {
        while (GPS.available() > 0) {
          GPS.encode(GPS.read());
        }
      }  
    }

    if (millis() > 5000 && GPS.charsProcessed() < 10)
    {
      Serial.printf("No GPS detected: check wiring.\n");
      while(true);
    }

    // Serial.printf("DEBUG >> satellites: %ld\n", GPS.satellites.value());
    // Serial.printf("DEBUG >> latitude = %f, longitude = %f, age = %d\n", GPS.location.lat(), GPS.location.lng(), GPS.location.age());

    delay(1000);
  } while (!GPS.location.isValid() || GPS.hdop.hdop() > 20);
  
  payload->type = PKT_GPS;
  payload->crc = 0;
  payload->deckbox_id = 1; // hardcode for now
  payload->buoy_id = 1; // hardcode for now
  payload->latitude = lat24Bit(GPS.location.lat());
  payload->longitude = lng24Bit(GPS.location.lng());
  payload->age = GPS.location.age() / 60000; // age in minutes (ms -> min)

  GPS.end();

  // calculate the battery level
  uint16_t batt_volt = getBatteryVoltage();
  uint8_t batt_level = (batt_volt - BATT_MIN) * 255 / (BATT_MAX - BATT_MIN);
  payload->charge = batt_level;

  // Serial.printf("DEBUG >> type (payload): %d\n", (int)(payload->type));
  // Serial.printf("DEBUG >> LAT (payload): %ld\n", payload->latitude);
  // Serial.printf("DEBUG >> LONG (payload): %ld\n", payload->longitude);
  // Serial.printf("DEBUG >> AGE (payload): %d\n", payload->age);
  // Serial.printf("DEBUG >> CHARGE (payload): %d\n", payload->charge);

  last_commit_time = millis();
}






void onRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr ) {
  // ignoring rssi and snr for now

  // crc
  if (!crc_check(payload, size)) {
    // Serial.printf("DEBUG >> CRC Failure!");
    return;
  }

  // check if the packet was meant for this buoy
  pkt_type_t type = getPacketType(payload);
  // pkt_type_t type = 255; // testing never getting the correct packet
  if (awaiting_response == RX_REQ && type != PKT_REQ && type != PKT_LOST_REQ
      || awaiting_response == RX_ACK && type != PKT_ACK
      || awaiting_response == RX_PAIR && type != PKT_PAIR) {
    int time_left;
    if (awaiting_response == RX_REQ) {
      time_left = REQ_TIMEOUT - millis() - started_rx;
    }
    else if (awaiting_response == RX_ACK) {
      time_left = ACK_TIMEOUT - millis() - started_rx;
      // Serial.printf("DEBUG >> re-waiting for ack (%d)\n", time_left);
    } else if (awaiting_response = RX_PAIR) {
      time_left = 0;
    }
    if (time_left <= 0) {
      onRxTimeout();
      return;
    }
    Radio.Rx(time_left);
    return;
  }

  // TODO: check buoy id

  if (awaiting_response == RX_REQ) {
    receivedReq(payload, size);
  }
  else if (awaiting_response == RX_ACK) {
    // Serial.printf("DEBUG >> Received ACK\n");
    if (((struct packet_ack *)payload)->buoy_id != buoy_id || ((struct packet_ack *)payload)->deckbox_id != parent_deckbox_id) {
      int time_left = ACK_TIMEOUT - millis() - started_rx;
      if (time_left <= 0) {
        onRxTimeout();
      }
      return;
    }
    receivedAck(payload, size);
  }
  else if (awaiting_response == RX_PAIR) {
    receivedPair(payload, size);
  }
}

void onRxTimeout( void ) {
  // Serial.printf("DEBUG >> RX Timeout\n");
  if (awaiting_response == RX_REQ) {
    respTimeout();
    // uint8_t dummy = PKT_REQ; // testing received resp
    // receivedReq(&dummy, 1); // testing received resp
  }
  else if (awaiting_response == RX_ACK) {
    ackTimeout();
    // uint8_t dummy = PKT_ACK; // testing received ack
    // receivedAck(&dummy, 1);  // testing received ack
  } else if (awaiting_response == RX_PAIR) {
    return; // no action required
  }
}

void OnTxDone( void ) {
  turnOffRGB();
  // Serial.printf("DEBUG >> TX done for %d", current_transmit);
  lora_idle = true;
  if (current_transmit == TX_ANNOUNCE) {
    afterTxAnnounce();
  }
  else if (current_transmit == TX_GPS || current_transmit == TX_LOST) {
    afterTxGPS();
  }
  else if (current_transmit == TX_ERR) {
    afterTxErr();
  }
}

void OnTxTimeout( void ) {
  turnOffRGB();
  Radio.Sleep();
  // Serial.printf("DEBUG >> TX Timeout...\n");
  lora_idle = true;
}


void enterPairing( void ) {
  // Serial.printf("DEBUG >> Entering Pairing Mode...\n");
  pairing_code = random(100000, 999999);

  //initialize the display and print the pairing code to the display
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
  delay(100);
  int_display.init();
  int_display.setTextAlignment(TEXT_ALIGN_LEFT);
  int_display.setFont(ArialMT_Plain_16);

  char pairing_string[20];
  sprintf(pairing_string, "Pairing Code:\n %d\n", pairing_code);
  display_show(pairing_string);

  // wait for the pairing packet
  awaiting_response = RX_PAIR;
  Radio.Rx(0);

  while (buoy_id == 0) {
    lowPowerHandler();
  }
}

void resetFlash(void) {
  accessEEPROM();
  memset(fmem, 0, sizeof(*fmem));
  flushEEPROM();
}

void display_show(char* string) {
  int_display.clear();
  int_display.drawString(0, 0, string);
  int_display.display();
}
