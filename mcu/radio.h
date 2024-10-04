void OnTxDone( void )
{
  rgb_led(0, 0, 16);                                                        // Flash blue LED to indicate packet has been sent
  Radio.Sleep( );
  if (debug) Serial.print("LoRa TX sent......");
  //state=RX;
}


void OnTxTimeout( void )
{
    rgb_led(16, 0, 0);                                                        // Flash red LED to indicate failed packet
    Radio.Sleep( );
    if (debug) Serial.print("LoRa TX Timeout......");
}


void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
    Rssi=rssi;
    rxSize=size;
    memcpy(lora_rx_packet, payload, size );
    lora_rx_packet[size]='\0';
    Radio.Sleep( );

    if (debug) Serial.printf("\r\nreceived packet \"%s\" with rssi %d , length %d\r\n", lora_rx_packet, Rssi, rxSize);
    if (debug) Serial.println("wait to send next packet");
}
