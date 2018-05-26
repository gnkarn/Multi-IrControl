// & gustavo karnincic

#include "IRrecv.h"
#include "IRsend.h"
#include "IRutils.h"


#define HDR_MARK 3780U
#define BIT_MARK 630U
#define HDR_SPACE 1438U
#define ONE_SPACE 1148U
#define ZERO_SPACE 476U
#define BLUESKY_BITS 112U
#define HVAC_BLUESKY_DATALEN 14U

//#if SEND_BLUESKY
// Alternative >64 bit Function
void IRsend::send_bluesky(uint8_t data[], uint16_t nbytes) {
  // nbytes should typically be XYZ_STATE_LENGTH
  // data should typically be:
  //    data = {0x74, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x07, 0x07, 0x24, 0x00, 0x01, 0x26, 0xCB, 0x23};
     //    uint8_t data[HVAC_BLUESKY_DATALEN] = {0xC4, 0xD3, 0x64, 0x80, 0x00, 0x24, 0x80, 0xA0, 0x14, 0x00, 0x00, 0x00, 0x00, 0xE6};
  // data[] is assumed to be in MSB order for this code.
  uint8_t repeat =0;                                                           //  fixed repeat for testing
  for (uint16_t r = 0; r <= repeat; r++) {
    sendGeneric(HDR_MARK, HDR_SPACE,
                BIT_MARK, ONE_SPACE,
                BIT_MARK, ZERO_SPACE,
                BIT_MARK,
                100000, // 100% made-up guess at the message gap.
                data, nbytes,                                                   // 112 bits codded in 14 bytes
                38000, // Complete guess of the modulation frequency.
                true, 0, 50);
              }
}
//#endif
