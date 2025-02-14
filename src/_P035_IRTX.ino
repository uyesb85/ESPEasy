#ifdef USES_P035
//#######################################################################################################
//#################################### Plugin 035: Output IR ############################################
//#######################################################################################################
//
// Usage: Connect an IR led to ESP8266 GPIO14 (D5) preferably. (various schematics can be found online)
// On the device tab add a new device and select "Communication - IR Transmit"
// Enable the device and select the GPIO led pin
// Power on the ESP and connect to it
// Commands can be send to this plug in and it will translate them to IR signals.
// Possible commands are IRSEND and IRSENDAC
//---IRSEND: That commands format is: IRSEND,<protocol>,<data>,<bits>,<repeat>
// bits and repeat default to 0 if not used and they are optional
// For protocols RAW and RAW2 there is no bits and repeat part, they are supposed to be replayed as they are calculated by a Google docs sheet or by plugin P016
//---IRSENDAC: That commands format is: IRSENDAC,{"Protocol":"COOLIX","Power":true,"Opmode":"dry","Fanspeed":"auto","Degrees":22,"Swingv":"max","Swingh":"off"}
// The possible values
// Protocols: Argo Coolix Daikin Fujitsu Haier Hitachi Kelvinator Midea Mitsubishi MitsubishiHeavy Panasonic Samsung Sharp Tcl Teco Toshiba Trotec Vestel Whirlpool
//---Opmodes:      ---Fanspeed:   --Swingv:       --Swingh:
// - "off"          - "auto"       - "off"         - "off"
// - "auto"         - "min"        - "auto"        - "auto"
// - "cool"         - "low"        - "highest"     - "leftmax"
// - "heat"         - "medium"     - "high"        - "left"
// - "dry"          - "high"       - "middle"      - "middle"
// - "fan_only"     - "max"        - "low"         - "right"
//                                 - "lowest"      - "rightmax"
// TRUE - FALSE parameters are:
// - "Power" - "Celsius" - "Quiet" - "Turbo" - "Econo" - "Light" - "Filter" - "Clean" - "Light" - "Beep"
// If celcius is set to FALSE then farenheit will be used
// - "Sleep" Nr. of mins of sleep mode, or use sleep mode. (<= 0 means off.)
// - "Clock" Nr. of mins past midnight to set the clock to. (< 0 means off.)
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRutils.h>
#include <IRsend.h>

#ifdef P016_P035_Extended_AC
#include <IRac.h>
IRac *Plugin_035_commonAc = nullptr;
#endif

IRsend *Plugin_035_irSender = nullptr;

#define PLUGIN_035
#define PLUGIN_ID_035 35
#define PLUGIN_NAME_035 "Communication - IR Transmit"
#define STATE_SIZE_MAX 53U
#define PRONTO_MIN_LENGTH 6U

#define from_32hex(c) ((((c) | ('A' ^ 'a')) - '0') % 39)

#define P35_Ntimings 250 //Defines the ammount of timings that can be stored. Used in RAW and RAW2 encodings

boolean Plugin_035(byte function, struct EventStruct *event, String &string)
{
  boolean success = false;

  switch (function)
  {
  case PLUGIN_DEVICE_ADD:
  {
    Device[++deviceCount].Number = PLUGIN_ID_035;
    Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
    Device[deviceCount].SendDataOption = false;
    break;
  }

  case PLUGIN_GET_DEVICENAME:
  {
    string = F(PLUGIN_NAME_035);
    break;
  }

  case PLUGIN_GET_DEVICEVALUENAMES:
  {
    break;
  }

  case PLUGIN_GET_DEVICEGPIONAMES:
  {
    event->String1 = formatGpioName_output("LED");
    break;
  }
  case PLUGIN_WEBFORM_LOAD:
  {
    addRowLabel(F("Command"));
    addHtml(F("IRSEND,[PROTOCOL],[DATA],[BITS optional],[REPEATS optional]<BR>BITS and REPEATS are optional and default to 0"));
    addHtml(F("IRSENDAC,{JSON formated AC command}"));

    success = true;
    break;
  }

  case PLUGIN_INIT:
  {
    int irPin = CONFIG_PIN1;
    if (Plugin_035_irSender == 0 && irPin != -1)
    {
      addLog(LOG_LEVEL_INFO, F("INIT: IR TX"));
      Plugin_035_irSender = new IRsend(irPin);
      Plugin_035_irSender->begin(); // Start the sender
    }
    if (Plugin_035_irSender != 0 && irPin == -1)
    {
      addLog(LOG_LEVEL_INFO, F("INIT: IR TX Removed"));
      delete Plugin_035_irSender;
      Plugin_035_irSender = 0;
    }

#ifdef P016_P035_Extended_AC
    if (Plugin_035_commonAc == 0 && irPin != -1)
    {
      addLog(LOG_LEVEL_INFO, F("INIT AC: IR TX"));
      Plugin_035_commonAc = new IRac(irPin);
    }
    if (Plugin_035_commonAc != 0 && irPin == -1)
    {
      addLog(LOG_LEVEL_INFO, F("INIT AC: IR TX Removed"));
      delete Plugin_035_commonAc;
      Plugin_035_commonAc = 0;
    }
#endif

    success = true;
    break;
  }

  case PLUGIN_WRITE:
  {
    uint64_t IrCode = 0;
    uint16_t IrBits = 0;

    String cmdCode = string;
    int argIndex = cmdCode.indexOf(',');
    if (argIndex)
      cmdCode = cmdCode.substring(0, argIndex);

#ifdef P016_P035_Extended_AC
    if ((cmdCode.equalsIgnoreCase(F("IRSEND")) || cmdCode.equalsIgnoreCase(F("IRSENDAC"))) && (Plugin_035_irSender != 0 || Plugin_035_commonAc != 0))
#else
    if (cmdCode.equalsIgnoreCase(F("IRSEND")) && Plugin_035_irSender != 0)
#endif
    {
      success = true;
#ifdef PLUGIN_016
      if (irReceiver != 0)
        irReceiver->disableIRIn(); // Stop the receiver
#endif

      String IrType ="";
      String IrType_orig= "";
      String TmpStr1 ="";
      if (GetArgv(string.c_str(), TmpStr1, 2))
      {
        IrType = TmpStr1;
        IrType_orig = TmpStr1;
        IrType.toLowerCase();
      }

      if (IrType.equals(F("raw")) || IrType.equals(F("raw2")))
      {
        String IrRaw;
        uint16_t IrHz = 0;
        unsigned int IrPLen = 0;
        unsigned int IrBLen = 0;

        if (GetArgv(string.c_str(), TmpStr1, 3))
          IrRaw = TmpStr1; //Get the "Base32" encoded/compressed Ir signal
        if (GetArgv(string.c_str(), TmpStr1, 4))
          IrHz = str2int(TmpStr1.c_str()); //Get the base freguency of the signal (allways 38)
        if (GetArgv(string.c_str(), TmpStr1, 5))
          IrPLen = str2int(TmpStr1.c_str()); //Get the Pulse Length in ms
        if (GetArgv(string.c_str(), TmpStr1, 6))
          IrBLen = str2int(TmpStr1.c_str()); //Get the Blank Pulse Length in ms

        printWebString += IrType_orig + String(F(": Base32Hex RAW Code: ")) + IrRaw + String(F("<BR>kHz: ")) + IrHz + String(F("<BR>Pulse Len: ")) + IrPLen + String(F("<BR>Blank Len: ")) + IrBLen + String(F("<BR>"));

        uint16_t idx = 0; //If this goes above the buf.size then the esp will throw a 28 EXCCAUSE
        uint16_t *buf;
        buf = new uint16_t[P35_Ntimings]; //The Raw Timings that we can buffer.
        if (buf == nullptr)
        { // error assigning memory.
          success = false;
          return success;
        }

        if (IrType.equals(F("raw")))
        {
          unsigned int c0 = 0; //count consecutives 0s
          unsigned int c1 = 0; //count consecutives 1s

          //printWebString += F("Interpreted RAW Code: ");  //print the number of 1s and 0s just for debugging/info purposes
          //Loop throught every char in RAW string
          for (unsigned int i = 0; i < IrRaw.length(); i++)
          {
            //Get the decimal value from base32 table
            //See: https://en.wikipedia.org/wiki/Base32#base32hex
            char c = from_32hex(IrRaw[i]);

            //Loop through 5 LSB (bits 16, 8, 4, 2, 1)
            for (unsigned int shft = 1; shft < 6; shft++)
            {
              //if bit is 1 (5th position - 00010000 = 16)
              if ((c & 16) != 0)
              {
                //add 1 to counter c1
                c1++;
                //if we already have any 0s in counting (the previous
                //bit was 0)
                if (c0 > 0)
                {
                  //add the total ms into the buffer (number of 0s multiplied
                  //by defined blank length ms)
                  buf[idx++] = c0 * IrBLen;
                  //print the number of 0s just for debuging/info purpouses
                  //for (uint t = 0; t < c0; t++)
                  //printWebString += '0';
                }
                //So, as we receive a "1", and processed the counted 0s
                //sending them as a ms timing into the buffer, we clear
                //the 0s counter
                c0 = 0;
              }
              else
              {
                //So, bit is 0

                //On first call, ignore 0s (suppress left-most 0s)
                if (c0 + c1 != 0)
                {
                  //add 1 to counter c0
                  c0++;
                  //if we already have any 1s in counting (the previous
                  //bit was 1)
                  if (c1 > 0)
                  {
                    //add the total ms into the buffer (number of 1s
                    //multiplied by defined pulse length ms)
                    buf[idx++] = c1 * IrPLen;
                    //print the number of 1s just for debugging/info purposes
                    //                          for (uint t = 0; t < c1; t++)
                    //                            printWebString += '1';
                  }
                  //So, as we receive a "0", and processed the counted 1s
                  //sending them as a ms timing into the buffer, we clear
                  //the 1s counter
                  c1 = 0;
                }
              }
              //shift to left the "c" variable to process the next bit that is
              //in 5th position (00010000 = 16)
              c <<= 1;
            }
          }

          //Finally, we need to process the last counted bit that we were
          //processing

          //If we have pendings 0s
          if (c0 > 0)
          {
            buf[idx++] = c0 * IrBLen;
            //for (uint t = 0; t < c0; t++)
            //printWebString += '0';
          }
          //If we have pendings 1s
          if (c1 > 0)
          {
            buf[idx++] = c1 * IrPLen;
            //for (uint t = 0; t < c1; t++)
            //printWebString += '1';
          }

          //printWebString += F("<BR>");
        }
        else
        { // RAW2
          for (unsigned int i = 0, total = IrRaw.length(), gotRep = 0, rep = 0; i < total;)
          {
            char c = IrRaw[i++];
            if (c == '*')
            {
              if (i + 2 >= total || idx + (rep = from_32hex(IrRaw[i++])) * 2 > sizeof(buf[0]) * P35_Ntimings)
              {
                delete[] buf;
                buf = nullptr;
                return addErrorTrue();
              }
              gotRep = 2;
            }
            else
            {
              if ((c == '^' && i + 1 >= total) || idx >= sizeof(buf[0]) * P35_Ntimings)
              {
                delete[] buf;
                buf = nullptr;
                return addErrorTrue();
              }

              uint16_t irLen = (idx & 1) ? IrBLen : IrPLen;
              if (c == '^')
              {
                buf[idx++] = (from_32hex(IrRaw[i]) * 32 + from_32hex(IrRaw[i + 1])) * irLen;
                i += 2;
              }
              else
                buf[idx++] = from_32hex(c) * irLen;

              if (--gotRep == 0)
              {
                while (--rep)
                {
                  buf[idx] = buf[idx - 2];
                  buf[idx + 1] = buf[idx - 1];
                  idx += 2;
                }
              }
            }
          }
        } //End RAW2

        Plugin_035_irSender->sendRaw(buf, idx, IrHz);
        delete[] buf;
        buf = nullptr;
        //String line = "";
        //for (int i = 0; i < idx; i++)
        //    line += uint64ToString(buf[i], 10) + ",";
        //serialPrintln(line);

        //sprintf_P(log, PSTR("IR Params1: Hz:%u - PLen: %u - BLen: %u"), IrHz, IrPLen, IrBLen);
        //addLog(LOG_LEVEL_INFO, log);
        //sprintf_P(log, PSTR("IR Params2: RAW Code:%s"), IrRaw.c_str());
        //addLog(LOG_LEVEL_INFO, log);
      }
      else if (cmdCode.equalsIgnoreCase(F("IRSEND")))
      {
        uint16_t IrRepeat = 0;
        //  unsigned long IrSecondCode=0UL;
        String ircodestr ="";
        if (GetArgv(string.c_str(), TmpStr1, 2))
        {
          IrType = TmpStr1;
          IrType_orig = TmpStr1;
          IrType.toUpperCase(); //strToDecodeType assumes all capital letters
        }
        if (GetArgv(string.c_str(), ircodestr, 3))
        {
          IrCode = strtoull(ircodestr.c_str(), NULL, 16);
        }
        IrBits = 0; //Leave it to 0 for default protocol bits
        if (GetArgv(string.c_str(), TmpStr1, 4))
          IrBits = str2int(TmpStr1.c_str()); // Number of bits to be sent. USE 0 for default protocol bits
        if (GetArgv(string.c_str(), TmpStr1, 5))
          IrRepeat = str2int(TmpStr1.c_str()); // Nr. of times the message is to be repeated
        sendIRCode(strToDecodeType(IrType.c_str()), IrCode, ircodestr.c_str(), IrBits, IrRepeat);
      }
#ifdef P016_P035_Extended_AC
      if (cmdCode.equalsIgnoreCase(F("IRSENDAC")))
      { //Preliminary-test code for the standardized AC commands.

        StaticJsonDocument<300> doc;
        int argIndex = string.indexOf(',') + 1;
        if (argIndex)
          TmpStr1 = string.substring(argIndex, string.length());
        addLog(LOG_LEVEL_INFO, String(F("IRTX: JSON received: ")) + TmpStr1);

        // Deserialize the JSON document
        DeserializationError error = deserializeJson(doc, TmpStr1);
        // Test if parsing succeeds.
        if (error)
        {
          addLog(LOG_LEVEL_INFO, String(F("IRTX: Deserialize Json failed: ")) + error.c_str());
          ReEnableIRIn();
          return true; //do not continue with sending the signal.
        }
        String sprotocol = doc[F("protocol")];
        decode_type_t protocol = strToDecodeType(sprotocol.c_str());
        if (!IRac::isProtocolSupported(protocol)) //Check if we support the protocol
        {
          addLog(LOG_LEVEL_INFO, String(F("IRTX: Protocol not supported:")) + sprotocol);
          ReEnableIRIn();
          return true; //do not continue with sending of the signal.
        }
        //Check for Values that absolutly need to exist in the JSON
        // if (doc.containsKey(F("power")) && doc.containsKey(F("temp")) && doc.containsKey(F("mode")))
        // {
        //   addLog(LOG_LEVEL_ERROR, String(F("IRTX: JSON keys missing")));
        //   ReEnableIRIn();
        //   return true; //do not continue with sending of the signal.
        // }

        String tempstr = "";
        tempstr = doc[F("model")].as<String>();
        uint16_t model = IRac::strToModel(tempstr.c_str()); //The specific model of A/C if applicable. //strToModel();. Defaults to -1 (unknown) if missing from JSON
        tempstr = doc[F("power")].as<String>();
        bool power = IRac::strToBool(tempstr.c_str(), false); //POWER ON or OFF. Defaults to false if missing from JSON
        float degrees = doc[F("temp")] | 22.0;                //What temperature should the unit be set to?. Defaults to 22c if missing from JSON
        tempstr = doc[F("use_celsius")].as<String>();
        bool celsius = IRac::strToBool(tempstr.c_str(), true); //Use degreees Celsius, otherwise Fahrenheit. Defaults to true if missing from JSON
        tempstr = doc[F("mode")].as<String>();
        stdAc::opmode_t opmode = IRac::strToOpmode(tempstr.c_str(), stdAc::opmode_t::kAuto); //What operating mode should the unit perform? e.g. Cool. Defaults to auto if missing from JSON
        tempstr = doc[F("fanspeed")].as<String>();
        stdAc::fanspeed_t fanspeed = IRac::strToFanspeed(tempstr.c_str(), stdAc::fanspeed_t::kAuto); //Fan Speed setting. Defaults to auto if missing from JSON
        tempstr = doc[F("swingv")].as<String>();
        stdAc::swingv_t swingv = IRac::strToSwingV(tempstr.c_str(), stdAc::swingv_t::kAuto); //Vertical swing setting. Defaults to auto if missing from JSON
        tempstr = doc[F("swingh")].as<String>();
        stdAc::swingh_t swingh = IRac::strToSwingH(tempstr.c_str(), stdAc::swingh_t::kAuto); //Horizontal Swing setting. Defaults to auto if missing from JSON
        tempstr = doc[F("quiet")].as<String>();
        bool quiet = IRac::strToBool(tempstr.c_str(), false); //Quiet setting ON or OFF. Defaults to false if missing from JSON
        tempstr = doc[F("turbo")].as<String>();
        bool turbo = IRac::strToBool(tempstr.c_str(), false); //Turbo setting ON or OFF. Defaults to false if missing from JSON
        tempstr = doc[F("econo")].as<String>();
        bool econo = IRac::strToBool(tempstr.c_str(), false); //Economy setting ON or OFF. Defaults to false if missing from JSON
        tempstr = doc[F("light")].as<String>();
        bool light = IRac::strToBool(tempstr.c_str(), false); //Light setting ON or OFF. Defaults to false if missing from JSON
        tempstr = doc[F("filter")].as<String>();
        bool filter = IRac::strToBool(tempstr.c_str(), false); //Filter setting ON or OFF. Defaults to false if missing from JSON
        tempstr = doc[F("clean")].as<String>();
        bool clean = IRac::strToBool(tempstr.c_str(), false); //Clean setting ON or OFF. Defaults to false if missing from JSON
        tempstr = doc[F("beep")].as<String>();
        bool beep = IRac::strToBool(tempstr.c_str(), false); //Beep setting ON or OFF. Defaults to false if missing from JSON

        int16_t sleep = doc[F("sleep")] | -1; //Nr. of mins of sleep mode, or use sleep mode. (<= 0 means off.). Defaults to -1 if missing from JSON
        int16_t clock = doc[F("clock")] | -1; //Nr. of mins past midnight to set the clock to. (< 0 means off.). Defaults to -1 if missing from JSON

        //Send the IR command
        Plugin_035_commonAc->sendAc(protocol, model, power, opmode, degrees, celsius, fanspeed, swingv, swingh, quiet, turbo, econo, light, filter, clean, beep, sleep, clock);
        ReEnableIRIn();
        //addLog(LOG_LEVEL_INFO, String(F("IRTX: IR Code Sent: ")) + Plugin_035_commonAc->toString().c_str());
      }
#endif // P016_P035_Extended_AC

      addLog(LOG_LEVEL_INFO, String(F("IRTX: IR Code Sent: ")) + IrType_orig);
      if (printToWeb)
      {
        printWebString += String(F("IRTX: IR Code Sent: ")) + IrType_orig + String(F("<BR>"));
      }

      ReEnableIRIn();
    }
    break;
  } //PLUGIN_WRITE END
  } // SWITCH END
  return success;
} // Plugin_035 END

boolean addErrorTrue()
{
  addLog(LOG_LEVEL_ERROR, F("RAW2: Invalid encoding!"));
  ReEnableIRIn();
  return true;
}

void ReEnableIRIn()
{
#ifdef PLUGIN_016
  if (irReceiver != 0)
    irReceiver->enableIRIn(); // Start the receiver
#endif
}

// A lot of the following code has been taken directly (with permission) from the IRMQTTServer.ino example code
// of the IRremoteESP8266 library. (https://github.com/markszabo/IRremoteESP8266)

// Transmit the given IR message.
//
// Args:
//   irsend:   A pointer to a IRsend object to transmit via.
//   irtype:  enum of the protocol to be sent.
//   code:     Numeric payload of the IR message. Most protocols use this.
//   code_str: The unparsed code to be sent. Used by complex protocol encodings.
//   bits:     Nr. of bits in the protocol. 0 means use the protocol's default.
//   repeat:   Nr. of times the message is to be repeated. (Not all protcols.)
// Returns:
//   bool: Successfully sent or not.
bool sendIRCode(int const irtype,
                uint64_t const code, char const *code_str, uint16_t bits,
                uint16_t repeat)
{
  decode_type_t irType = (decode_type_t)irtype;
  bool success = true; // Assume success.
  repeat = std::max(IRsend::minRepeats(irType), repeat);
    if (bits == 0) bits = IRsend::defaultBits(irType);
  // send the IR message.

      if (hasACState(irType))  // protocols with > 64 bits
        success = parseStringAndSendAirCon(irType, code_str);
      else  // protocols with <= 64 bits
        success = Plugin_035_irSender->send(irType, code, bits, repeat);
        
  return success;
}

// Parse an Air Conditioner A/C Hex String/code and send it.
// Args:
//   irtype: Nr. of the protocol we need to send.
//   str: A hexadecimal string containing the state to be sent.
bool parseStringAndSendAirCon(const int irtype, const String str)
{
  decode_type_t irType = (decode_type_t)irtype;
  uint8_t strOffset = 0;
  uint8_t state[kStateSizeMax] = {0};  // All array elements are set to 0.
  uint16_t stateSize = 0;

  if (str.startsWith("0x") || str.startsWith("0X"))
    strOffset = 2;
  // Calculate how many hexadecimal characters there are.
  uint16_t inputLength = str.length() - strOffset;
  if (inputLength == 0) {
   // debug("Zero length AirCon code encountered. Ignored.");
    return false;  // No input. Abort.
  }

  switch (irType) {  // Get the correct state size for the protocol.
    case DAIKIN:
      // Daikin has 2 different possible size states.
      // (The correct size, and a legacy shorter size.)
      // Guess which one we are being presented with based on the number of
      // hexadecimal digits provided. i.e. Zero-pad if you need to to get
      // the correct length/byte size.
      // This should provide backward compatiblity with legacy messages.
      stateSize = inputLength / 2;  // Every two hex chars is a byte.
      // Use at least the minimum size.
      stateSize = std::max(stateSize, kDaikinStateLengthShort);
      // If we think it isn't a "short" message.
      if (stateSize > kDaikinStateLengthShort)
        // Then it has to be at least the version of the "normal" size.
        stateSize = std::max(stateSize, kDaikinStateLength);
      // Lastly, it should never exceed the "normal" size.
      stateSize = std::min(stateSize, kDaikinStateLength);
      break;
    case FUJITSU_AC:
      // Fujitsu has four distinct & different size states, so make a best guess
      // which one we are being presented with based on the number of
      // hexadecimal digits provided. i.e. Zero-pad if you need to to get
      // the correct length/byte size.
      stateSize = inputLength / 2;  // Every two hex chars is a byte.
      // Use at least the minimum size.
      stateSize = std::max(stateSize,
                           (uint16_t) (kFujitsuAcStateLengthShort - 1));
      // If we think it isn't a "short" message.
      if (stateSize > kFujitsuAcStateLengthShort)
        // Then it has to be at least the smaller version of the "normal" size.
        stateSize = std::max(stateSize, (uint16_t) (kFujitsuAcStateLength - 1));
      // Lastly, it should never exceed the maximum "normal" size.
      stateSize = std::min(stateSize, kFujitsuAcStateLength);
      break;
    case MWM:
      // MWM has variable size states, so make a best guess
      // which one we are being presented with based on the number of
      // hexadecimal digits provided. i.e. Zero-pad if you need to to get
      // the correct length/byte size.
      stateSize = inputLength / 2;  // Every two hex chars is a byte.
      // Use at least the minimum size.
      stateSize = std::max(stateSize, (uint16_t) 3);
      // Cap the maximum size.
      stateSize = std::min(stateSize, kStateSizeMax);
      break;
    case SAMSUNG_AC:
      // Samsung has two distinct & different size states, so make a best guess
      // which one we are being presented with based on the number of
      // hexadecimal digits provided. i.e. Zero-pad if you need to to get
      // the correct length/byte size.
      stateSize = inputLength / 2;  // Every two hex chars is a byte.
      // Use at least the minimum size.
      stateSize = std::max(stateSize, (uint16_t) (kSamsungAcStateLength));
      // If we think it isn't a "normal" message.
      if (stateSize > kSamsungAcStateLength)
        // Then it probably the extended size.
        stateSize = std::max(stateSize,
                             (uint16_t) (kSamsungAcExtendedStateLength));
      // Lastly, it should never exceed the maximum "extended" size.
      stateSize = std::min(stateSize, kSamsungAcExtendedStateLength);
      break;
    default:  // Everything else.
      stateSize = IRsend::defaultBits(irType) / 8;
      if (!stateSize || !hasACState(irType)) {
        // Not a protocol we expected. Abort.
       // debug("Unexpected AirCon protocol detected. Ignoring.");
        return false;
      }
  }
  if (inputLength > stateSize * 2) {
   // debug("AirCon code to large for the given protocol.");
    return false;
  }

  // Ptr to the least significant byte of the resulting state for this protocol.
  uint8_t *statePtr = &state[stateSize - 1];

  // Convert the string into a state array of the correct length.
  for (uint16_t i = 0; i < inputLength; i++) {
    // Grab the next least sigificant hexadecimal digit from the string.
    uint8_t c = tolower(str[inputLength + strOffset - i - 1]);
    if (isxdigit(c)) {
      if (isdigit(c))
        c -= '0';
      else
        c = c - 'a' + 10;
    } else {
     // debug("Aborting! Non-hexadecimal char found in AirCon state:");
     // debug(str.c_str());
      return false;
    }
    if (i % 2 == 1) {  // Odd: Upper half of the byte.
      *statePtr += (c << 4);
      statePtr--;  // Advance up to the next least significant byte of state.
    } else {  // Even: Lower half of the byte.
      *statePtr = c;
    }
  }
  if (!Plugin_035_irSender->send(irType, state, stateSize)) {
    //debug("Unexpected AirCon type in send request. Not sent.");
    return false;
  }
  return true;  // We were successful as far as we can tell.
}

// Count how many values are in the String.
// Args:
//   str:  String containing the values.
//   sep:  Character that separates the values.
// Returns:
//   The number of values found in the String.
uint16_t countValuesInStr(const String &str, char sep)
{
  int16_t index = -1;
  uint16_t count = 1;
  do
  {
    index = str.indexOf(sep, index + 1);
    count++;
  } while (index != -1);
  return count;
}

#endif // USES_P035
