/*
  xdrv_02_irremote.ino - infra red support for Sonoff-Tasmota

  Copyright (C) 2018  Heiko Krupp, Lazar Obradovic and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_IR_REMOTE
/*********************************************************************************************\
 * IR Remote send and receive using IRremoteESP8266 library
\*********************************************************************************************/
#include "IRrecv.h"
#include "IRsend.h"
#include "IRutils.h"

#include <IRremoteESP8266.h>

// Based on IRremoteESP8266.h enum decode_type_t
const char kIrRemoteProtocols[] PROGMEM =
  "UNUSED|RC5|RC6|NEC|SONY|PANASONIC|JVC|SAMSUNG|WHYNTER|AIWA_RC_T501|LG|SANYO|MITSUBISHI|DISH|SHARP|COOLIX|DAIKIN|DENON|KELVINATOR|SHERWOOD|MITSUBISHI_AC|RCMM|SANYO_LC7461|RC5X|GREE|PRONTO|NEC_LIKE|ARGO|TROTEC|NIKAI|RAW|GLOBALCACHE|TOSHIBA_AC|FUJITSU_AC|MIDEA|MAGIQUEST|LASERTAG|CARRIER_AC|HAIER_AC|MITSUBISHI2|HITACHI_AC|GICABLE|BLUESKY";

#ifdef USE_IR_HVAC

#include <ir_Mitsubishi.h>

// HVAC TOSHIBA_
#define HVAC_TOSHIBA_HDR_MARK 4400
#define HVAC_TOSHIBA_HDR_SPACE 4300
#define HVAC_TOSHIBA_BIT_MARK 543
#define HVAC_TOSHIBA_ONE_SPACE 1623
#define HVAC_MISTUBISHI_ZERO_SPACE 472
#define HVAC_TOSHIBA_RPT_MARK 440
#define HVAC_TOSHIBA_RPT_SPACE 7048 // Above original iremote limit
#define HVAC_TOSHIBA_DATALEN 9

IRMitsubishiAC *mitsubir = NULL;

const char kFanSpeedOptions[] = "A123456S";
const char kHvacModeOptions[] = "HDCAXYV"; // added VENT mode for Bluesky
#endif

//  bluesky definitions , should go on a separate file in the end
#define HDR_MARK 3780U
#define BIT_MARK 630U
#define HDR_SPACE 1438U
#define ONE_SPACE 1148U
#define ZERO_SPACE 476U
#define BLUESKY_BITS 112U
#define HVAC_BLUESKY_DATALEN 14U

// Alternative >64 bit Function
void IRsend::send_bluesky(uint8_t data[], uint16_t nbytes) {
  // nbytes should typically be XYZ_STATE_LENGTH
  // data should typically be:
  //    data = {0x74, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x07, 0x07, 0x24, 0x00, 0x01, 0x26, 0xCB, 0x23};
     //    uint8_t data[HVAC_BLUESKY_DATALEN] = {0xC4, 0xD3, 0x64, 0x80, 0x00, 0x24, 0x80, 0xA0, 0x14, 0x00, 0x00, 0x00, 0x00, 0xE6};
  // data[] is assumed to be in MSB order for this code.
  //sendGeneric(headermark, headerspace, onemark, onespace, zeromark, zerospace,
    //          footermark, gap, 0U, data, nbits, frequency, MSBfirst, repeat,
    //          dutycycle);

  uint8_t repeat =0;                                                           //  fixed repeat for testing
  for (uint16_t r = 0; r <= repeat; r++) {
    sendGeneric(HDR_MARK, HDR_SPACE,
                BIT_MARK, ONE_SPACE,
                BIT_MARK, ZERO_SPACE,
                BIT_MARK,
                100000, // 100% made-up guess at the message gap.
                data, nbytes,                                                   // 112 bits codded in 14 bytes
                40923, // with 38 , it is 2 us short , so i use 38 x 28/26
                false, 0, 50);
              }
}

/*********************************************************************************************\
 * IR Send
\*********************************************************************************************/

#include <IRsend.h>

IRsend *irsend = NULL;

void IrSendInit(void)
{
  irsend = new IRsend(pin[GPIO_IRSEND]); // an IR led is at GPIO_IRSEND
  irsend->begin();

#ifdef USE_IR_HVAC
  mitsubir = new IRMitsubishiAC(pin[GPIO_IRSEND]);
#endif //USE_IR_HVAC
}

#ifdef USE_IR_RECEIVE
/*********************************************************************************************\
 * IR Receive
\*********************************************************************************************/

#include <IRrecv.h>

#define IR_TIME_AVOID_DUPLICATE 500 // Milliseconds

IRrecv *irrecv = NULL;
unsigned long ir_lasttime = 0;

void IrReceiveInit(void)
{
  irrecv = new IRrecv(pin[GPIO_IRRECV]); // an IR led is at GPIO_IRRECV
  irrecv->enableIRIn();                  // Start the receiver

  //  AddLog_P(LOG_LEVEL_DEBUG, PSTR("IrReceive initialized"));
}

void IrReceiveCheck()
{
  char sirtype[14];  // Max is AIWA_RC_T501
  int8_t iridx = 0;

  decode_results results;

  if (irrecv->decode(&results)) {

    snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_IRR "RawLen %d, Bits %d, Value %08X, Decode %d"),
               results.rawlen, results.bits, results.value, results.decode_type);
    AddLog(LOG_LEVEL_DEBUG);

    unsigned long now = millis();
    if ((now - ir_lasttime > IR_TIME_AVOID_DUPLICATE) && (UNKNOWN != results.decode_type) && (results.bits > 0)) {
      ir_lasttime = now;

      iridx = results.decode_type;
      if ((iridx < 0) || (iridx > 14)) {
        iridx = 0;
      }
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_JSON_IRRECEIVED "\":{\"" D_JSON_IR_PROTOCOL "\":\"%s\",\"" D_JSON_IR_BITS "\":%d,\"" D_JSON_IR_DATA "\":\"%lX\"}}"),
        GetTextIndexed(sirtype, sizeof(sirtype), iridx, kIrRemoteProtocols), results.bits, (uint32_t)results.value);
      MqttPublishPrefixTopic_P(RESULT_OR_TELE, PSTR(D_JSON_IRRECEIVED));
#ifdef USE_DOMOTICZ
      unsigned long value = results.value | (iridx << 28);  // [Protocol:4, Data:28]
      DomoticzSensor(DZ_COUNT, value);                      // Send data as Domoticz Counter value
#endif                                                      // USE_DOMOTICZ
    }

    irrecv->resume();
  }
}
#endif // USE_IR_RECEIVE

#ifdef USE_IR_HVAC
/*********************************************************************************************\
 * IR Heating, Ventilation and Air Conditioning using IRMitsubishiAC library
\*********************************************************************************************/

boolean IrHvacToshiba(const char *HVAC_Mode, const char *HVAC_FanMode, boolean HVAC_Power, int HVAC_Temp)
{
  uint16_t rawdata[2 + 2 * 8 * HVAC_TOSHIBA_DATALEN + 2];
  byte data[HVAC_TOSHIBA_DATALEN] = {0xF2, 0x0D, 0x03, 0xFC, 0x01, 0x00, 0x00, 0x00, 0x00};

  char *p;
  uint8_t mode;

  if (HVAC_Mode == NULL) {
    p = (char *)kHvacModeOptions; // default HVAC_HOT
  }
  else {
    p = strchr(kHvacModeOptions, toupper(HVAC_Mode[0]));
  }
  if (!p) {
    return true;
  }
  data[6] = (p - kHvacModeOptions) ^ 0x03; // HOT = 0x03, DRY = 0x02, COOL = 0x01, AUTO = 0x00

  if (!HVAC_Power) {
    data[6] = (byte)0x07; // Turn OFF HVAC
  }

  if (HVAC_FanMode == NULL) {
    p = (char *)kFanSpeedOptions; // default FAN_SPEED_AUTO
  }
  else {
    p = strchr(kFanSpeedOptions, toupper(HVAC_FanMode[0]));
  }
  if (!p) {
    return true;
  }
  mode = p - kFanSpeedOptions + 1;
  if ((1 == mode) || (7 == mode)) {
    mode = 0;
  }
  mode = mode << 5; // AUTO = 0x00, SPEED = 0x40, 0x60, 0x80, 0xA0, 0xC0, SILENT = 0x00
  data[6] = data[6] | mode;

  byte Temp;
  if (HVAC_Temp > 30) {
    Temp = 30;
  }
  else if (HVAC_Temp < 17) {
    Temp = 17;
  }
  else {
    Temp = HVAC_Temp;
  }
  data[5] = (byte)(Temp - 17) << 4;

  data[HVAC_TOSHIBA_DATALEN - 1] = 0;
  for (int x = 0; x < HVAC_TOSHIBA_DATALEN - 1; x++) {
    data[HVAC_TOSHIBA_DATALEN - 1] = (byte)data[x] ^ data[HVAC_TOSHIBA_DATALEN - 1]; // CRC is a simple bits addition
  }

  int i = 0;
  byte mask = 1;

  //header
  rawdata[i++] = HVAC_TOSHIBA_HDR_MARK;
  rawdata[i++] = HVAC_TOSHIBA_HDR_SPACE;

  //data
  for (int b = 0; b < HVAC_TOSHIBA_DATALEN; b++) {
    for (mask = B10000000; mask > 0; mask >>= 1) { //iterate through bit mask
      if (data[b] & mask) { // Bit ONE
        rawdata[i++] = HVAC_TOSHIBA_BIT_MARK;
        rawdata[i++] = HVAC_TOSHIBA_ONE_SPACE;
      }
      else { // Bit ZERO
        rawdata[i++] = HVAC_TOSHIBA_BIT_MARK;
        rawdata[i++] = HVAC_MISTUBISHI_ZERO_SPACE;
      }
    }
  }

  //trailer
  rawdata[i++] = HVAC_TOSHIBA_RPT_MARK;
  rawdata[i++] = HVAC_TOSHIBA_RPT_SPACE;

  noInterrupts();
  irsend->sendRaw(rawdata, i, 38);
  irsend->sendRaw(rawdata, i, 38);
  interrupts();

  return false;
}

boolean IrHvacMitsubishi(const char *HVAC_Mode, const char *HVAC_FanMode, boolean HVAC_Power, int HVAC_Temp)
{
  char *p;
  uint8_t mode;

  mitsubir->stateReset();

  if (HVAC_Mode == NULL) {
    p = (char *)kHvacModeOptions; // default HVAC_HOT
  }
  else {
    p = strchr(kHvacModeOptions, toupper(HVAC_Mode[0]));
  }
  if (!p) {
    return true;
  }
  mode = (p - kHvacModeOptions + 1) << 3; // HOT = 0x08, DRY = 0x10, COOL = 0x18, AUTO = 0x20
  mitsubir->setMode(mode);

  mitsubir->setPower(HVAC_Power);

  if (HVAC_FanMode == NULL) {
    p = (char *)kFanSpeedOptions; // default FAN_SPEED_AUTO
  }
  else {
    p = strchr(kFanSpeedOptions, toupper(HVAC_FanMode[0]));
  }
  if (!p) {
    return true;
  }
  mode = p - kFanSpeedOptions; // AUTO = 0, SPEED = 1 .. 5, SILENT = 6
  mitsubir->setFan(mode);

  mitsubir->setTemp(HVAC_Temp);
  mitsubir->setVane(MITSUBISHI_AC_VANE_AUTO);
  mitsubir->send();

  //  snprintf_P(log_data, sizeof(log_data), PSTR("IRHVAC: Mitsubishi Power %d, Mode %d, FanSpeed %d, Temp %d, VaneMode %d"),
  //    mitsubir->getPower(), mitsubir->getMode(), mitsubir->getFan(), mitsubir->getTemp(), mitsubir->getVane());
  //  AddLog(LOG_LEVEL_DEBUG);

  return false;
}

// **************************
// *       BLUESKY          *
// **************************

// IrHvacBluesky(HVAC_Mode(HDCAV), HVAC_FanMode(A12345S), HVAC_Power(), HVAC_Temp( 16-31));
// mqtt command format:  cmnd/Multi-IR/irhvac  {"VENDOR": "BLUESKY","POWER":1,"MODE":"V","FANSPEED":2,"TEMP":25}

boolean IrHvacBluesky(const char *HVAC_Mode, const char *HVAC_FanMode, boolean HVAC_Power, int HVAC_Temp)
{
  uint16_t rawdata[2 + 2 * 8 * HVAC_BLUESKY_DATALEN + 2];
  // byte data[HVAC_BLUESKY_DATALEN] =  {0xC4, 0xD3, 0x64, 0x80, 0x00, 0x24, 0xE0, 0xE0, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x2E}; // MSB first
  byte data[HVAC_BLUESKY_DATALEN] =  { 0x23, 0xCB, 0x26, 0x01, 0x00, 0x24, 0x07, 0x07, 0x2D, 0x00, 0x00, 0x00, 0x00,0x74};
//  byte data[HVAC_BLUESKY_DATALEN] =  {  0x74, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x07, 0x07, 0x24, 0x00, 0x01, 0x26, 0xCB, 0x23};
  //  LSB FIRST : 0x74, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x07, 0x07, 0x24, 0x00, 0x01, 0x26, 0xCB, 0x23
  char *p;
  uint8_t mode;

  if (HVAC_Mode == NULL) {
    p = (char *)kHvacModeOptions; // default HVAC_VENT
  }
  else {
    p = strchr(kHvacModeOptions, toupper(HVAC_Mode[0]));
  }
  if (!p) {
    return true;
  }
  // **************************
  // *     SET MODE BYTE 6     *
  // **************************
   // HeatT = 0x07, DRY = 0x??, COOL = 0x03, AUTO = 0x08, VENT:0x07
   mode = (p - kHvacModeOptions + 1);
   if ((5 == mode) || (6 == mode)) {
     mode = 7;  // if nonexistent mode then VENT
   }
   if (mode == 4 ){mode = (byte)0x08 ;} // for bluesky the auto mode code is 8 not 4
   data[6] = mode;
   // **************************
   // *     SET power BYTE 5    *
   // **************************
   // timer on is 0x2C ; not implemented
  if (!HVAC_Power) {
    data[5] = (byte)0x20; // Turn OFF HVAC
  }
    else {
      data[5]= (byte)0x24; // Turn ON HVAC
    }

    //const char kFanSpeedOptions[] = "A1234XS";// 5 NOT USED( add 1 to decode)
    //const char kHvacModeOptions[] = "HDCAXYV"; // added VENT mode for Bluesky
    // fanmode = swing << 3 + fan = 0x38 (swing auto) + fan (0 = auto, 2,3,5)
    // code: 00sssvvv
  // **************************
  // * SET FAN-SWING BYTE 8   *
  // **************************
  if (HVAC_FanMode == NULL) {
    p = (char *)kFanSpeedOptions; // default FAN_SPEED_AUTO
  }
  else {
    p = strchr(kFanSpeedOptions, toupper(HVAC_FanMode[0]));
  }
  if (!p) {
    return true;
  }
  mode = (p - kFanSpeedOptions );
  data[8] = (byte)0x38 + mode;
  byte Temp;
  // **************************
  // *     SET TEMP  BYTE  7  *
  // **************************
  if (HVAC_Temp > 31) {
    Temp = 31;
  }
  else if (HVAC_Temp < 16) {
    Temp = 16;
  }
  else {
    Temp = HVAC_Temp;
  }
  data[7] = (byte)(31 - Temp);

  // ****************************************
  // * checksum8 modulo 256  calc BYTE 13   *
  // ****************************************
  int sum = 0;
  for(int i = 0; i < (HVAC_BLUESKY_DATALEN-1); ++i) {
     sum += data[i];
  }
  //modulo 256 sum
  sum %= 256;
  char ch = sum;

  //twos complement
  unsigned char twoscompl = ~ch + 1;
  //data[13] = twoscompl;
  data[13] = ch;

  DPRINTLN(" data[] ");
  for(int i = 0; i < (HVAC_BLUESKY_DATALEN); ++i) {
     DPRINT(String(data[i], HEX));DPRINT(", ");  // debug
  }
  DPRINTLN("");

  //data
  uint8_t repeat =0;                        //  fixed repeat for testing
  // invierte el vector para probar
  // for(int i = 0; i < (HVAC_BLUESKY_DATALEN/2); ++i) {
  //   uint8_t aux =  data[i];
  //   data[i]= data[HVAC_BLUESKY_DATALEN-i-1];
  //   data[HVAC_BLUESKY_DATALEN-i-1] = aux;
  // }
  //
  // DPRINTLN(" data[] ");
  // for(int i = 0; i < (HVAC_BLUESKY_DATALEN); ++i) {
  //    DPRINT(String(data[i], HEX));DPRINT(", ");  // debug
  // }

  irsend->send_bluesky(data, HVAC_BLUESKY_DATALEN);


  //trailer
  return false;
}
#endif // USE_IR_HVAC


/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

/*
 * ArduinoJSON entry used to calculate jsonBuf: JSON_OBJECT_SIZE(3) + 40 = 96
 IRsend:
 { "protocol": "SAMSUNG", "bits": 32, "data": 551502015 }

 IRhvac:
 { "Vendor": "<Toshiba|Mitsubishi>", "Power": <0|1>, "Mode": "<Hot|Cold|Dry|Auto>", "FanSpeed": "<1|2|3|4|5|Auto|Silence>", "Temp": <17..30> }
*/

//boolean IrSendCommand(char *type, uint16_t index, char *dataBuf, uint16_t data_len, int16_t payload)
boolean IrSendCommand()
{
  boolean serviced = true;
  boolean error = false;
  char dataBufUc[XdrvMailbox.data_len];
  char protocol_text[20];
  const char *protocol;
  uint32_t bits = 0;
  uint32_t data = 0;
  uint8_t *array_data[14];                                                       // for bluesky 112 bits , 14 bytes
  for (uint16_t i = 0; i <= sizeof(dataBufUc); i++) {
    dataBufUc[i] = toupper(XdrvMailbox.data[i]);
  }
  if (!strcasecmp_P(XdrvMailbox.topic, PSTR(D_CMND_IRSEND))) {
    if (XdrvMailbox.data_len) {
      StaticJsonBuffer<128> jsonBuf;
      JsonObject &ir_json = jsonBuf.parseObject(dataBufUc);
      if (!ir_json.success()) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_IRSEND "\":\"" D_JSON_INVALID_JSON "\"}")); // JSON decode failed
      }
      else {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_IRSEND "\":\"" D_JSON_DONE "\"}"));
        protocol = ir_json[D_JSON_IR_PROTOCOL];
        bits = ir_json[D_JSON_IR_BITS];
        data = ir_json[D_JSON_IR_DATA];
        if (protocol && bits && data) {
          int protocol_code = GetCommandCode(protocol_text, sizeof(protocol_text), protocol, kIrRemoteProtocols);

          snprintf_P(log_data, sizeof(log_data), PSTR("IRS: protocol_text %s, protocol %s, bits %d, data %d, protocol_code %d"),
            protocol_text, protocol, bits, data, protocol_code);
          AddLog(LOG_LEVEL_DEBUG);

          switch (protocol_code) {
            case NEC:
              irsend->sendNEC(data, (bits > NEC_BITS) ? NEC_BITS : bits); break;
            case SONY:
              irsend->sendSony(data, (bits > SONY_20_BITS) ? SONY_20_BITS : bits, 2); break;
            case RC5:
              irsend->sendRC5(data, bits); break;
            case RC6:
              irsend->sendRC6(data, bits); break;
            case DISH:
              irsend->sendDISH(data, (bits > DISH_BITS) ? DISH_BITS : bits); break;
            case JVC:
              irsend->sendJVC(data, (bits > JVC_BITS) ? JVC_BITS : bits, 1); break;
            case SAMSUNG:
              irsend->sendSAMSUNG(data, (bits > SAMSUNG_BITS) ? SAMSUNG_BITS : bits ); break;
            case PANASONIC:
              irsend->sendPanasonic(bits, data); break;
            case GICABLE:
                irsend->sendGICable(data, bits); break;
            // case BLUESKY:
            //         irsend->send_bluesky(  *array_data, bits); break;
            default:
              snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_IRSEND "\":\"" D_JSON_PROTOCOL_NOT_SUPPORTED "\"}"));
          }
        }
        else {
          error = true;
        }
      }
    }
    else {
      error = true;
    }
    if (error) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_IRSEND "\":\"" D_JSON_NO " " D_JSON_IR_PROTOCOL ", " D_JSON_IR_BITS " " D_JSON_OR " " D_JSON_IR_DATA "\"}"));
    }
  }
#ifdef USE_IR_HVAC
  else if (!strcasecmp_P(XdrvMailbox.topic, PSTR(D_CMND_IRHVAC))) {
    const char *HVAC_Mode;
    const char *HVAC_FanMode;
    const char *HVAC_Vendor;
    int HVAC_Temp = 21;
    boolean HVAC_Power = true;

    if (XdrvMailbox.data_len) {
      StaticJsonBuffer<164> jsonBufer;
      JsonObject &root = jsonBufer.parseObject(dataBufUc);
      if (!root.success()) {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_IRHVAC "\":\"" D_JSON_INVALID_JSON "\"}")); // JSON decode failed
      }
      else {
        snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_IRHVAC "\":\"" D_JSON_DONE "\"}"));
        HVAC_Vendor = root[D_JSON_IRHVAC_VENDOR];
        HVAC_Power = root[D_JSON_IRHVAC_POWER];
        HVAC_Mode = root[D_JSON_IRHVAC_MODE];
        HVAC_FanMode = root[D_JSON_IRHVAC_FANSPEED];
        HVAC_Temp = root[D_JSON_IRHVAC_TEMP];

        //        snprintf_P(log_data, sizeof(log_data), PSTR("IRHVAC: Received Vendor %s, Power %d, Mode %s, FanSpeed %s, Temp %d"),
        //          HVAC_Vendor, HVAC_Power, HVAC_Mode, HVAC_FanMode, HVAC_Temp);
        //        AddLog(LOG_LEVEL_DEBUG);

        if (HVAC_Vendor == NULL || !strcasecmp_P(HVAC_Vendor, PSTR("TOSHIBA"))) {
          error = IrHvacToshiba(HVAC_Mode, HVAC_FanMode, HVAC_Power, HVAC_Temp);
        }
        else if (!strcasecmp_P(HVAC_Vendor, PSTR("MITSUBISHI"))) {
          error = IrHvacMitsubishi(HVAC_Mode, HVAC_FanMode, HVAC_Power, HVAC_Temp);
        }
        else if (!strcasecmp_P(HVAC_Vendor, PSTR("BLUESKY"))) {
          error = IrHvacBluesky(HVAC_Mode, HVAC_FanMode, HVAC_Power, HVAC_Temp);
        }
        else {
          error = true;
        }
      }
    }
    else {
      error = true;
    }
    if (error) {
      snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"" D_CMND_IRHVAC "\":\"" D_JSON_WRONG " " D_JSON_IRHVAC_VENDOR ", " D_JSON_IRHVAC_MODE " " D_JSON_OR " " D_JSON_IRHVAC_FANSPEED "\"}"));
    }
  }
#endif // USE_IR_HVAC
  else serviced = false; // Unknown command
  return serviced;
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

#define XDRV_02

boolean Xdrv02(byte function)
{
  boolean result = false;

  if ((pin[GPIO_IRSEND] < 99) || (pin[GPIO_IRRECV] < 99)) {
    switch (function) {
      case FUNC_INIT:
        if (pin[GPIO_IRSEND] < 99) {
          IrSendInit();
        }
#ifdef USE_IR_RECEIVE
        if (pin[GPIO_IRRECV] < 99) {
          IrReceiveInit();
        }
#endif  // USE_IR_RECEIVE
        break;
      case FUNC_EVERY_50_MSECOND:
#ifdef USE_IR_RECEIVE
        if (pin[GPIO_IRRECV] < 99) {
          IrReceiveCheck();  // check if there's anything on IR side
        }
#endif  // USE_IR_RECEIVE
        break;
      case FUNC_COMMAND:
        if (pin[GPIO_IRSEND] < 99) {
          result = IrSendCommand();
        }
        break;
    }
  }
  return result;
}

#endif // USE_IR_REMOTE
