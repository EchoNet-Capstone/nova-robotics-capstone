#include <stdint.h>
#include <string.h>

#define PKT_ERR 0
#define PKT_ANNOUNCE 1
#define PKT_ACK 2
#define PKT_PAIR 3
#define PKT_REQ 4
#define PKT_GPS 5
#define PKT_LOST_REQ 6
#define PKT_LOST 7

#define ERR_UNKNOWN 0
#define ERR_NO_SUPPORT 1
#define ERR_PARSE 2
#define ERR_PAIR 3

#define MAX_24_BIT 16777216-1

#define CAPABILITIES_GPS (1 << 0)
#define CAPABILITIES_LOST (1 << 1)

typedef uint16_t buoy_id_t;
typedef uint16_t recv_id_t;
typedef uint8_t  pkt_type_t;

struct packet_hdr {
  pkt_type_t type;
  uint8_t crc;
  recv_id_t deckbox_id;
  buoy_id_t buoy_id;
} __attribute__((packed));

struct packet_err {
  pkt_type_t type;
  uint8_t crc;
  recv_id_t deckbox_id;
  buoy_id_t buoy_id;
  uint8_t err_id;
} __attribute__((packed));

struct packet_announce {
  pkt_type_t type;
  uint8_t crc;
  recv_id_t deckbox_id;
  buoy_id_t buoy_id;
  uint32_t capabilities;
} __attribute__((packed));

struct packet_ack {
  pkt_type_t type;
  uint8_t crc;
  recv_id_t deckbox_id;
  buoy_id_t buoy_id;
} __attribute__((packed));

struct packet_req {
  pkt_type_t type;
  uint8_t crc;
  recv_id_t parent_deckbox_id; // parent deckbox to the buoy
  buoy_id_t buoy_id;
  recv_id_t req_deckbox_id; // deckbox that is requesting the buoys location
} __attribute__((packed));

struct packet_gps {
  pkt_type_t type;
  uint8_t crc;
  recv_id_t deckbox_id;
  buoy_id_t buoy_id;
  recv_id_t recv_deckbox_id;
  uint32_t latitude : 24;
  uint32_t longitude : 24;
  uint32_t age : 16;
  uint8_t charge;
} __attribute__((packed));

struct packet_pair{
  pkt_type_t type;
  uint8_t crc;
  buoy_id_t buoy_id;
  recv_id_t deckbox_id;
  int32_t pairing_code : 24;
}__attribute__((packed));

//add members to this struct to store more on deckbox flash, you don't need to touch anything else
//NOTE: max cubecell user flash size of 1k
struct dbcc_flash{
  recv_id_t deckbox_id;
  buoy_id_t next_buoy_id;
  //other stuff
}__attribute__((packed));

struct bycc_flash{
  recv_id_t deckbox_id;
  buoy_id_t buoy_id;
}__attribute__((packed));

pkt_type_t getPacketType(uint8_t *payload) {
  // read header (they all have the same initial fields)
  struct packet_hdr header;
  memcpy(&header, payload, sizeof(header));

  return header.type;
}

recv_id_t getDeckboxID(uint8_t *payload) {
  // read header (they all have the same initial fields)
  struct packet_hdr header;
  memcpy(&header, payload, sizeof(header));

  return header.deckbox_id;
}

buoy_id_t getBuoyID(uint8_t *payload) {
  // read header (they all have the same initial fields)
  struct packet_hdr header;
  memcpy(&header, payload, sizeof(header));

  return header.buoy_id;
}


uint32_t lat24Bit(double latitude) {
  return (uint32_t)((latitude+90) / 180 * (MAX_24_BIT));
}
uint32_t lng24Bit(double longitude) {
  return (uint32_t)((longitude+180) / 360 * (MAX_24_BIT));
}
double latDouble(uint32_t latitude) {
  return (double)latitude / (MAX_24_BIT) * 180 - 90;
}
double lngDouble(uint32_t longitude) {
  return (double)longitude / (MAX_24_BIT) * 360 - 180;
}


/*
 * from https://stackoverflow.com/questions/13491700/8-bit-fletcher-checksum-of-16-byte-data
 *  8-bit CRC with polynomial x^8+x^6+x^3+x^2+1, 0x14D.
 * Chosen based on Koopman, et al. (0xA6 in his notation = 0x14D >> 1):
 * http://www.ece.cmu.edu/~koopman/roses/dsn04/koopman04_crc_poly_embedded.pdf
 */

static uint8_t crc8_table[] = {
  0x00, 0x3e, 0x7c, 0x42, 0xf8, 0xc6, 0x84, 0xba, 0x95, 0xab, 0xe9, 0xd7,
  0x6d, 0x53, 0x11, 0x2f, 0x4f, 0x71, 0x33, 0x0d, 0xb7, 0x89, 0xcb, 0xf5,
  0xda, 0xe4, 0xa6, 0x98, 0x22, 0x1c, 0x5e, 0x60, 0x9e, 0xa0, 0xe2, 0xdc,
  0x66, 0x58, 0x1a, 0x24, 0x0b, 0x35, 0x77, 0x49, 0xf3, 0xcd, 0x8f, 0xb1,
  0xd1, 0xef, 0xad, 0x93, 0x29, 0x17, 0x55, 0x6b, 0x44, 0x7a, 0x38, 0x06,
  0xbc, 0x82, 0xc0, 0xfe, 0x59, 0x67, 0x25, 0x1b, 0xa1, 0x9f, 0xdd, 0xe3,
  0xcc, 0xf2, 0xb0, 0x8e, 0x34, 0x0a, 0x48, 0x76, 0x16, 0x28, 0x6a, 0x54,
  0xee, 0xd0, 0x92, 0xac, 0x83, 0xbd, 0xff, 0xc1, 0x7b, 0x45, 0x07, 0x39,
  0xc7, 0xf9, 0xbb, 0x85, 0x3f, 0x01, 0x43, 0x7d, 0x52, 0x6c, 0x2e, 0x10,
  0xaa, 0x94, 0xd6, 0xe8, 0x88, 0xb6, 0xf4, 0xca, 0x70, 0x4e, 0x0c, 0x32,
  0x1d, 0x23, 0x61, 0x5f, 0xe5, 0xdb, 0x99, 0xa7, 0xb2, 0x8c, 0xce, 0xf0,
  0x4a, 0x74, 0x36, 0x08, 0x27, 0x19, 0x5b, 0x65, 0xdf, 0xe1, 0xa3, 0x9d,
  0xfd, 0xc3, 0x81, 0xbf, 0x05, 0x3b, 0x79, 0x47, 0x68, 0x56, 0x14, 0x2a,
  0x90, 0xae, 0xec, 0xd2, 0x2c, 0x12, 0x50, 0x6e, 0xd4, 0xea, 0xa8, 0x96,
  0xb9, 0x87, 0xc5, 0xfb, 0x41, 0x7f, 0x3d, 0x03, 0x63, 0x5d, 0x1f, 0x21,
  0x9b, 0xa5, 0xe7, 0xd9, 0xf6, 0xc8, 0x8a, 0xb4, 0x0e, 0x30, 0x72, 0x4c,
  0xeb, 0xd5, 0x97, 0xa9, 0x13, 0x2d, 0x6f, 0x51, 0x7e, 0x40, 0x02, 0x3c,
  0x86, 0xb8, 0xfa, 0xc4, 0xa4, 0x9a, 0xd8, 0xe6, 0x5c, 0x62, 0x20, 0x1e,
  0x31, 0x0f, 0x4d, 0x73, 0xc9, 0xf7, 0xb5, 0x8b, 0x75, 0x4b, 0x09, 0x37,
  0x8d, 0xb3, 0xf1, 0xcf, 0xe0, 0xde, 0x9c, 0xa2, 0x18, 0x26, 0x64, 0x5a,
  0x3a, 0x04, 0x46, 0x78, 0xc2, 0xfc, 0xbe, 0x80, 0xaf, 0x91, 0xd3, 0xed,
  0x57, 0x69, 0x2b, 0x15
};

uint8_t crc8(uint8_t *data, size_t len) {
  uint8_t crc = 0xff;
  uint8_t packet[1024];

  if (len == 0) {
    return 0;
  }

  memcpy(&packet, data, len);
  packet[1] = 0;

  for (int i = 0; i < len; i++) {
    crc = crc8_table[crc ^ packet[i]];
  }
  
  return crc ^ 0xff;
}

uint8_t crc_check(uint8_t *packet, size_t len) {
  uint8_t given = packet[1];
  uint8_t expected = crc8(packet, len);

  return (given == expected);
}
