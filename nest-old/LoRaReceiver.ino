/* Heltec Automation Receive communication test example
 *
 * Function:
 * 1. Receive the same frequency band lora signal program
 * 
 * 
 * this project also realess in GitHub:
 * https://github.com/HelTecAutomation/ASR650x-Arduino
 * */

#include "LoRaWan_APP.h"
#include "Arduino.h"
#include "packets.h"
#include "GPS_Air530Z.h"
#include <EEPROM.h>

/*
 * set LoraWan_RGB to 1,the RGB active in loraWan
 * RGB red means sending;
 * RGB green means received done;
 */
#ifndef LoraWan_RGB
#define LoraWan_RGB 0
#endif

// declare GPS class
Air530ZClass GPS; // HTCC-AB0S2 uses Air530Z

#define RF_FREQUENCY                                915000000 // Hz

#define TX_OUTPUT_POWER                             14        // dBm

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

#define PAIR_TIMEOUT_VALUE                          4000
#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 1024 // Define the payload size here

#define EEPROM_SIZE sizeof(dbcc_flash)
#define DBID_EEPROM_ADDR 0
#define BID_EEPROM_ADDR 4

// #define GPS_AGE_MAX 300000 // (5 minutes) time in ms until GPS location is considered outdated
#define GPS_AGE_MAX 10000 // 10 seconds for testing



char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;

// int16_t rssi,rxSize;

volatile bool lora_idle = true;

dbcc_flash* fmem = NULL;

typedef enum {
  RX_ACK,
  RX_ERR,
  RX_ANNOUNCE,
  RX_GPS
} rx_type;

typedef enum {
  TX_REQ,
  TX_ACK,
  TX_PAIR
} tx_type;

static uint8_t transmit_attempt = 0;
static tx_type current_transmit;
static struct packet_gps gps_info;
static int started_pair = -1;
static recv_id_t deckbox_id;
static int last_gps_send = 0;


//void sendPairingPacket(int32_t pairing_code);

void accessEEPROM(){
  EEPROM.begin(EEPROM_SIZE);
  fmem = (dbcc_flash*)EEPROM.getDataPtr();
}

void flushEEPROM(){
  EEPROM.commit();
  EEPROM.end();
}

void setup() {
//  while(!Serial);
  Serial.begin(115200);
  Serial.setTimeout(5);
  accessEEPROM();
  deckbox_id = fmem->deckbox_id;
  flushEEPROM();
//  accessEEPROM();
//  int32_t dbid = fmem->deckbox_id;
//  int32_t bid = fmem->next_buoy_id;
//  // Serial.printf("DEBUG >> dbid: %d, bid: %d\n", dbid, bid);
//  flushEEPROM();
//  
//  accessEEPROM();
//  fmem->deckbox_id = 5678; 
//  fmem->next_buoy_id = 1234; 
//  // Serial.printf("DEBUG >> dbid: %d, bid: %d\n", dbid, bid);
//  flushEEPROM();
  
  GPS.begin();
  setupLoRa();
  afterTxAck();
//  sendPairingPacket(123456);
//  // Serial.printf("DEBUG >> Sent Pairing\n");
}


void loop() {
  if(Serial.available()){
    String pstring = Serial.readString();
    if(pstring.length() && !(pstring.substring(0, 5).compareTo("PAIR:"))){
      uint32_t pcode = pstring.substring(5).toInt();
      if(!pcode || pcode > MAX_24_BIT) {
        // Serial.printf("DEBUG >> invalid pairing code\n");
        return;
      }
      else if (started_pair == -1){
        while(!lora_idle);
        sendPairingPacket(pcode);
        return;
      }
      // Serial.printf("DEBUG >> pairing in progress\n");
    }
  }

  getGPS();
  if(lora_idle){
  	turnOffRGB();
    lora_idle = false;
    //// Serial.printf("DEBUG >> into RX mode\n");
    Radio.Rx(RX_TIMEOUT_VALUE);
  }
}

void getGPS() {
  if (millis() - last_gps_send > GPS_AGE_MAX) {
    // Serial.printf("DEBUG >> (Deckbox) GPS info outdated. Updating location...\n");
  
    uint32_t starttime = millis();
    while( (millis()-starttime) < 1000 ) {
      while (GPS.available() > 0) {
        GPS.encode(GPS.read());
      }
    }  
    if (GPS.location.isValid() && GPS.hdop.hdop() < 20) {
      // send GPS to single board computer (latitude, longitude, age in ms)
      Serial.printf("DECKBOX: latitude = %f, longitude = %f, age = %d\n", GPS.location.lat(), GPS.location.lng(), GPS.location.age());
      last_gps_send = millis();
    }
  }
}

void printPacketHeader(uint8_t *payload) {
  // read header (they all have the same initial fields)
  struct packet_hdr header;
  memcpy(&header, payload, sizeof(header));

  // Serial.printf("DEBUG >> HEADER: Type = %d, Deckbox ID = %d, Buoy ID = %d\n", header.type, header.deckbox_id, header.buoy_id);
  //// Serial.printf("DEBUG >> RAW: %lx\n", *((long int*)payload));
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

void receivedAnnouncement() {
  // send location request (implicitly sets to tx mode)
  struct packet_announce recv_pkt;
  memcpy(&recv_pkt, &rxpacket, sizeof(struct packet_announce));
  if (lora_idle) {

    struct packet_req sending;

    if (recv_pkt.capabilities & CAPABILITIES_LOST) {
      sending.type = PKT_LOST_REQ;
    } else {
      sending.type = PKT_REQ;
    }

    sending.parent_deckbox_id = recv_pkt.deckbox_id;
    sending.req_deckbox_id = deckbox_id;
    sending.buoy_id = recv_pkt.buoy_id;
    sending.crc = crc8((uint8_t *)&sending, sizeof(sending));
    memcpy(&txpacket, &sending, sizeof(sending));
    // Serial.printf("DEBUG >> Sending location request | type = %d, deckbox_id = %d, buoy_id = %d (length = %d)\r\n", sending.type, sending.parent_deckbox_id, sending.buoy_id, sizeof(sending));
    current_transmit = TX_REQ;
    Radio.Send( (uint8_t *)txpacket, sizeof(sending) ); //send the request
    lora_idle = false;
  }
}

void receivedGPS() {
  // save rxpacket
  memcpy(&gps_info, &rxpacket, sizeof(struct packet_gps));

  // check if it's for us
  if (gps_info.recv_deckbox_id != deckbox_id) return;

  // Serial.printf("DEBUG >> Received GPS | buoy id = %d, deckbox id = %d\n", gps_info.buoy_id, gps_info.deckbox_id);

  // send ACK (implicitly sets to tx mode)
  if (lora_idle) {
    struct packet_ack sending = {PKT_ACK, 0, gps_info.deckbox_id, gps_info.buoy_id};
    sending.crc = crc8((uint8_t *)&sending, sizeof(sending));
    memcpy(&txpacket, &sending, sizeof(sending));
    // Serial.printf("DEBUG >> Sending ack | type = %d, deckbox_id = %d, buoy_id = %d (length = %d)\r\n", sending.type, sending.deckbox_id, sending.buoy_id, sizeof(sending));
    current_transmit = TX_ACK;
    Radio.Send( (uint8_t *)txpacket, sizeof(sending) ); //send the ack
    lora_idle = false;
  }
}

void receivedACK(){
  //assumes  only time ack is recieved by db is during pair
  packet_ack* inp = (packet_ack*)rxpacket;
  accessEEPROM();
  if(inp->deckbox_id == fmem->deckbox_id){
    started_pair = -1;
    fmem->next_buoy_id++;
    Serial.printf("STATUS:PAIR_SUCCESS\n");
    flushEEPROM();
  }
}

void receivedERR(){
  packet_err* inp = (packet_err*)rxpacket;
  switch(inp->err_id){
    //do something with error packets based on type
      
    default:
      break;
  }
}


void afterTxReq() {
  // return to receiving
}

void afterTxAck() {
  // send to single board computer
  printGPSInfo();
  // return to receiving
}

void onRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr ) {
  //rssi=rssi; ignoring rssi and snr for now
  //rxSize=size;
  if(started_pair != -1){
    int to_left = PAIR_TIMEOUT_VALUE - (millis() - started_pair);
    //Serial.println(to_left);
    if(to_left <= 0){
      // Serial.printf("DEBUG >> STATUS:PAIR_TO\n");
      started_pair = -1;
    }
  }

  
  memcpy(rxpacket, payload, size );
  rxpacket[size]='\0';
  turnOnRGB(COLOR_RECEIVED,0);
  Radio.Sleep();
  // Serial.printf("DEBUG >> received packet with length %d\r\n", size);
  lora_idle = true;

  // crc
  if (!crc_check(payload, size)) {
    return;
  }

  // check what was received
  pkt_type_t type = getPacketType(payload);
  printPacketHeader(payload);
  switch(type){
    case PKT_ANNOUNCE:
      receivedAnnouncement();
      break;

    case PKT_GPS:
    case PKT_LOST:
      receivedGPS();
      break;

    case PKT_ACK:
      receivedACK();
      break;

    case PKT_ERR:
      receivedERR();
      break;

    default:
      break;
  }
}

void onRxTimeout( void ) {
  // // Serial.printf("DEBUG >> RX_TO\n");
  if(started_pair != -1){
    int to_left = PAIR_TIMEOUT_VALUE - (millis() - started_pair);
    if(to_left <= 0){
      // Serial.printf("DEBUG >> STATUS:PAIR_TO\n");
      started_pair = -1;
    }
  }
  lora_idle = true;
}

void OnTxDone( void ) {
  turnOffRGB();
  // Serial.printf("DEBUG >> TX done...\n");
  lora_idle = true;
  if (current_transmit == TX_REQ) {
    afterTxReq();
  }
  else if (current_transmit == TX_ACK) {
    afterTxAck();
  }
}

void OnTxTimeout( void ) {
  turnOffRGB();
  Radio.Sleep();
  // Serial.printf("DEBUG >> TX Timeout...\n");
  lora_idle = true;
}



void printGPSInfo() {
  struct packet_gps gps;
  memcpy(&gps, rxpacket, sizeof(struct packet_gps));
  // // Serial.printf("DEBUG >>  Type: %d\n", (int)(gps.type));
  // // Serial.printf("DEBUG >>  Deckbox: %d\n", (int)(gps.deckbox_id));
  // // Serial.printf("DEBUG >>  Buoy: %d\n", (int)(gps.buoy_id));
  // // Serial.printf("DEBUG >>  Latitude: %f\n", (float)(gps.latitude));
  // // Serial.printf("DEBUG >>  Longitude: %f\n", (float)(gps.longitude));
  // // Serial.printf("DEBUG >>  HDOP: %f\n", (float)(gps.hdop));
  // // Serial.printf("DEBUG >>  Timestamp: %d\n", (long)(gps.timestamp));
  // Serial.println();
  Serial.printf("Type: %d, Deckbox: %d, Buoy: %d, Latitude: %f, Longitude: %f, Age: %d, Charge: %d\n", (int)(gps.type), (int)(gps.deckbox_id), (int)(gps.buoy_id), latDouble(gps.latitude), lngDouble(gps.longitude), (int)(gps.age), (int)(gps.charge));
}



void sendPairingPacket(int32_t pairing_code){
  //need to ask deckbox for next buoy and its own id (we could do this only once but shouldnt because power loss???)
  accessEEPROM();
  recv_id_t dbid = fmem->deckbox_id;
  buoy_id_t bid = fmem->next_buoy_id;
  //access eeprom
  //read ids from eepointer
  struct packet_pair sending = {PKT_PAIR, 0, bid, dbid, pairing_code};
  sending.crc = crc8((uint8_t *)&sending, sizeof(sending));
  memcpy(&txpacket, &sending, sizeof(sending));
  // Serial.printf("DEBUG >> \r\nSending pairing packet | type = %d, buoy_id = %d, deckbox_id = %d, pairing_code = %d (length = %d)\r\n", sending.type, sending.buoy_id, sending.deckbox_id, sending.pairing_code);
  current_transmit = TX_PAIR;
  lora_idle = false;
  started_pair = millis();
  Radio.Send((uint8_t*)txpacket, sizeof(sending));
}
