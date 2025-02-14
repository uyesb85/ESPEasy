#ifdef USES_P016
//#######################################################################################################
//#################################### Plugin 016: Input IR #############################################
//#######################################################################################################

// Usage: Connect a TSOP module, preferably a 38Khz one, preferably to GPIO14 (D5)
// On the device tab add a new device and select "Communication - TSOP4838"
// Enable the device and select the GPIO pin
// Power on the ESP and connect to it
// By monitoring the serial or the web log (Tools -> Log) and then pressing a button on a remote
// you will get as output the replay solutions that where found.
// Typicaly solutions are given by the IRremoteESP8266 library (example IRSEND,NEC,ASDFZCV,32)
// but if no replay solutions are found by this library then a RAW2 solution in computed (example IRSEND,RAW2,ASDFASDFASDFASDF,38,50,40)
// If RAW2 also fails, then in the serial monitor there are dumped the raw IR data timings
// that can be used to calculate a RAW solution with use of GDocs for this purpose.
//
// IF the IR code is an Air Condition protocol that the  IR library can decode, then there will be a human-readable description of that IR message.
// If the IR library can encode those kind of messages then a JSON formated command will be given, that can be replayed by P035 as well.
// That commands format is: IRSENDAC,{"Protocol":"COOLIX","Power":true,"Opmode":"dry","Fanspeed":"auto","Degrees":22,"swingv":"max","swingh":"off"}
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include <IRrecv.h>

#ifdef P016_P035_Extended_AC
#include <IRac.h>
#endif

#define PLUGIN_016
#define PLUGIN_ID_016 16
#define PLUGIN_NAME_016 "Communication - TSOP4838"
#define PLUGIN_VALUENAME1_016 "IR"

// A lot of the following code has been taken directly (with permission) from the IRrecvDumpV2.ino example code
// of the IRremoteESP8266 library. (https://github.com/markszabo/IRremoteESP8266)

// ==================== start of TUNEABLE PARAMETERS ====================
// As this program is a special purpose capture/decoder, let us use a larger
// than normal buffer so we can handle Air Conditioner remote codes.
const uint16_t kCaptureBufferSize = 1024;

// kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a
// message ended.
// This parameter is an interesting trade-off. The longer the timeout, the more
// complex a message it can capture. e.g. Some device protocols will send
// multiple message packets in quick succession, like Air Conditioner remotes.
// Air Coniditioner protocols often have a considerable gap (20-40+ms) between
// packets.
// The downside of a large timeout value is a lot of less complex protocols
// send multiple messages when the remote's button is held down. The gap between
// them is often also around 20+ms. This can result in the raw data be 2-3+
// times larger than needed as it has captured 2-3+ messages in a single
// capture. Setting a low timeout value can resolve this.
// So, choosing the best kTimeout value for your use particular case is
// quite nuanced. Good luck and happy hunting.
// NOTE: Don't exceed kMaxTimeoutMs. Typically 130ms.
//#if DECODE_AC
// Some A/C units have gaps in their protocols of ~40ms. e.g. Kelvinator
// A value this large may swallow repeats of some protocols
const uint8_t P016_TIMEOUT = 50;
//#else   // DECODE_AC
// Suits most messages, while not swallowing many repeats.
//const uint8_t P016_TIMEOUT = 15;
//#endif  // DECODE_AC
// Alternatives:
// const uint8_t kTimeout = 90;
// Suits messages with big gaps like XMP-1 & some aircon units, but can
// accidentally swallow repeated messages in the rawData[] output.
//
// const uint8_t kTimeout = kMaxTimeoutMs;
// This will set it to our currently allowed maximum.
// Values this high are problematic because it is roughly the typical boundary
// where most messages repeat.
// e.g. It will stop decoding a message and start sending it to serial at
//      precisely the time when the next message is likely to be transmitted,
//      and may miss it.

// Set the smallest sized "UNKNOWN" message packets we actually care about.
// This value helps reduce the false-positive detection rate of IR background
// noise as real messages. The chances of background IR noise getting detected
// as a message increases with the length of the kTimeout value. (See above)
// The downside of setting this message too large is you can miss some valid
// short messages for protocols that this library doesn't yet decode.
//
// Set higher if you get lots of random short UNKNOWN messages when nothing
// should be sending a message.
// Set lower if you are sure your setup is working, but it doesn't see messages
// from your device. (e.g. Other IR remotes work.)
// NOTE: Set this value very high to effectively turn off UNKNOWN detection.
const uint16_t kMinUnknownSize = 12;
// ==================== end of TUNEABLE PARAMETERS ====================

IRrecv *irReceiver = NULL;
decode_results results;

boolean Plugin_016(byte function, struct EventStruct *event, String &string)
{
  boolean success = false;

  switch (function)
  {
  case PLUGIN_DEVICE_ADD:
  {
    Device[++deviceCount].Number = PLUGIN_ID_016;
    Device[deviceCount].Type = DEVICE_TYPE_SINGLE;
    Device[deviceCount].VType = SENSOR_TYPE_LONG;
    Device[deviceCount].Ports = 0;
    Device[deviceCount].PullUpOption = true;
    Device[deviceCount].InverseLogicOption = true;
    Device[deviceCount].FormulaOption = false;
    Device[deviceCount].ValueCount = 1;
    Device[deviceCount].SendDataOption = true;
    Device[deviceCount].TimerOption = false;
    Device[deviceCount].GlobalSyncOption = true;
    break;
  }

  case PLUGIN_GET_DEVICENAME:
  {
    string = F(PLUGIN_NAME_016);
    break;
  }

  case PLUGIN_GET_DEVICEVALUENAMES:
  {
    strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_016));
    break;
  }

  case PLUGIN_GET_DEVICEGPIONAMES:
  {
    event->String1 = formatGpioName_input(F("IR"));
    break;
  }

  case PLUGIN_INIT:
  {
    int irPin = CONFIG_PIN1;
    if (irReceiver == 0 && irPin != -1)
    {
      serialPrintln(F("INIT: IR RX"));
      irReceiver = new IRrecv(irPin, kCaptureBufferSize, P016_TIMEOUT, true);
      irReceiver->setUnknownThreshold(kMinUnknownSize); // Ignore messages with less than minimum on or off pulses.
      irReceiver->enableIRIn();                         // Start the receiver
    }
    if (irReceiver != 0 && irPin == -1)
    {
      irReceiver->disableIRIn();
      delete irReceiver;
      irReceiver = 0;
    }
    success = true;
    break;
  }
  case PLUGIN_EXIT:
  {
    {
      if (irReceiver != 0)
      {
        irReceiver->disableIRIn(); // Stop the receiver
        delete irReceiver;
        irReceiver = 0;
      }
    }
    success = true;
    break;
  }
  case PLUGIN_WEBFORM_LOAD:
  {
    addRowLabel(F("Info"));
    addHtml(F("Check serial or web log for replay solutions via Communication - IR Transmit plugin"));

    success = true;
    break;
  }

  case PLUGIN_TEN_PER_SECOND:
  {
    if (irReceiver->decode(&results))
    {
      yield(); // Feed the WDT after a time expensive decoding procedure
      if (results.overflow)
      {
        addLog(LOG_LEVEL_INFO, F("IR: WARNING, IR code is too big for buffer. Try pressing the transmiter button only momenteraly"));
        success = false;
        break; //Do not continue and risk hanging the ESP
      }

      // Display the basic output of what we found.
      if (results.decode_type != decode_type_t::UNKNOWN)
      {
        addLog(LOG_LEVEL_INFO, String(F("IRSEND,")) + typeToString(results.decode_type, results.repeat) + ',' + resultToHexidecimal(&results) + ',' + uint64ToString(results.bits)); //Show the appropriate command to the user, so he can replay the message via P035
      }
      //Check if a solution for RAW2 is found and if not give the user the option to access the timings info.
      if (results.decode_type == decode_type_t::UNKNOWN && !displayRawToReadableB32Hex())
      {
        addLog(LOG_LEVEL_INFO, F("IR: No replay solutions found! Press button again or try RAW encoding (timmings are in the serial output)"));
        serialPrint(String(F("IR: RAW TIMINGS: ")) + resultToSourceCode(&results));
        yield(); // Feed the WDT as it can take a while to print.
                 //addLog(LOG_LEVEL_DEBUG,(String(F("IR: RAW TIMINGS: ")) + resultToSourceCode(&results))); // Output the results as RAW source code //not showing up nicely in the web log
      }

#ifdef P016_P035_Extended_AC
      // Display any extra A/C info if we have it.
      // Display the human readable state of an A/C message if we can.
      stdAc::state_t state;
      //Initialize state settings
      state.protocol = decode_type_t::UNKNOWN;
      state.model = -1; // Unknown.
      state.power = false;
      state.mode = stdAc::opmode_t::kAuto;
      state.celsius = true;
      state.degrees = 22;
      state.fanspeed = stdAc::fanspeed_t::kAuto;
      state.swingv = stdAc::swingv_t::kAuto;
      state.swingh = stdAc::swingh_t::kAuto;
      state.quiet = false;
      state.turbo = false;
      state.econo = false;
      state.light = false;
      state.filter = false;
      state.clean = false;
      state.beep = false;
      state.sleep = -1;
      state.clock = -1;
      
      String description = IRAcUtils::resultAcToString(&results);
      if (description != "")
        addLog(LOG_LEVEL_INFO, String(F("AC State: ")) + description); // If we got a human-readable description of the message, display it.
      if (IRac::isProtocolSupported(results.decode_type))                   //Check If there is a replayable AC state and show the JSON command that can be send
      {
        IRAcUtils::decodeToState(&results, &state);
        StaticJsonDocument<300> doc;
        //Checks if a particular state is something else than the default and only then it adds it to the JSON document
        doc[F("protocol")] = typeToString(state.protocol);
        if (state.model >= 0)
          doc[F("model")] = IRac::strToModel(String(state.model).c_str()); //The specific model of A/C if applicable.
        doc[F("power")] = IRac::boolToString(state.power);                 //POWER ON or OFF
        doc[F("mode")] = IRac::opmodeToString(state.mode);                 //What operating mode should the unit perform? e.g. Cool = doc[""]; Heat etc.
        doc[F("temp")] = state.degrees;                                    //What temperature should the unit be set to?
        if (!state.celsius)
          doc[F("use_celsius")] = state.celsius; //Use degreees Celsius, otherwise Fahrenheit.
        if (state.fanspeed != stdAc::fanspeed_t::kAuto)
          doc[F("fanspeed")] = IRac::fanspeedToString(state.fanspeed); //Fan Speed setting
        if (state.swingv != stdAc::swingv_t::kAuto)
          doc[F("swingv")] = IRac::swingvToString(state.swingv); //Vertical swing setting
        if (state.swingh != stdAc::swingh_t::kAuto)
          doc[F("swingh")] = IRac::swinghToString(state.swingh); //Horizontal swing setting
        if (state.quiet)
          doc[F("quiet")] = IRac::boolToString(state.quiet); //Quiet setting ON or OFF
        if (state.turbo)
          doc[F("turbo")] = IRac::boolToString(state.turbo); //Turbo setting ON or OFF
        if (state.econo)
          doc[F("econo")] = IRac::boolToString(state.econo); //Economy setting ON or OFF
        if (state.light)
          doc[F("light")] = IRac::boolToString(state.light); //Light setting ON or OFF
        if (state.filter)
          doc[F("filter")] = IRac::boolToString(state.filter); //Filter setting ON or OFF
        if (state.clean)
          doc[F("clean")] = IRac::boolToString(state.clean); //Clean setting ON or OFF
        if (state.beep)
          doc[F("beep")] = IRac::boolToString(state.beep); //Beep setting ON or OFF
        if (state.sleep > 0)
          doc[F("sleep")] = state.sleep; //Nr. of mins of sleep mode, or use sleep mode. (<= 0 means off.)
        if (state.clock > 0)
          doc[F("clock")] = state.clock; //Nr. of mins past midnight to set the clock to. (< 0 means off.)
        String output = F("IRSENDAC,");
        serializeJson(doc, output);
        addLog(LOG_LEVEL_INFO, output); //Show the command that the user can put to replay the AC state with P035
      }
#endif // P016_P035_Extended_AC

      unsigned long IRcode = results.value;
      UserVar[event->BaseVarIndex] = (IRcode & 0xFFFF);
      UserVar[event->BaseVarIndex + 1] = ((IRcode >> 16) & 0xFFFF);
      sendData(event);
    }
    success = true;
    break;
  }
  }
  return success;
}

#define PCT_TOLERANCE 8u                                //Percent tolerance
#define pct_tolerance(v) ((v) / (100u / PCT_TOLERANCE)) //Tolerance % is calculated as the delta between any original timing, and the result after encoding and decoding
//#define MIN_TOLERANCE       10u
//#define get_tolerance(v)    (pct_tolerance(v) > MIN_TOLERANCE? pct_tolerance(v) : MIN_TOLERANCE)
#define get_tolerance(v) (pct_tolerance(v))
#define MIN_VIABLE_DIV 40u // Minimum viable timing denominator
#define to_32hex(c) ((c) < 10 ? (c) + '0' : (c) + 'A' - 10)

// This function attempts to convert the raw IR timings buffer to a short string that can be sent over as
// an IRSEND HTTP/MQTT command. It analyzes the timings, and searches for a common denominator which can be
// used to compress the values. If found, it then produces a string consisting of B32 Hex digit for each
// timing value, appended by the denominators for Pulse and Blank. This string can then be used in an
// IRSEND command. An important advantage of this string over the current IRSEND RAW B32 format implemented
// by GusPS is that it allows easy inspections and modifications after the code is constructed.
//
// Author: Gilad Raz (jazzgil)  23sep2018

boolean displayRawToReadableB32Hex()
{
  String line;
  uint16_t div[2];

  // print the values: either pulses or blanks
  for (uint16_t i = 1; i < results.rawlen; i++)
    line += uint64ToString(results.rawbuf[i] * RAWTICK, 10) + ",";
  addLog(LOG_LEVEL_DEBUG, line); //Display the RAW timings

  // Find a common denominator divisor for odd indexes (pulses) and then even indexes (blanks).
  for (uint16_t p = 0; p < 2; p++)
  {
    uint16_t cd = 0xFFFFU; // current divisor
    // find the lowest value to start the divisor with.
    for (uint16_t i = 1 + p; i < results.rawlen; i += 2)
    {
      uint16_t val = results.rawbuf[i] * RAWTICK;
      if (cd > val)
        cd = val;
    }

    uint16_t bstDiv = -1, bstAvg = 0xFFFFU;
    float bstMul = 5000;
    cd += get_tolerance(cd) + 1;
    //serialPrintln(String("p="+ uint64ToString(p, 10) + " start cd=" + uint64ToString(cd, 10)).c_str());
    // find the best divisor based on lowest avg err, within allowed tolerance.
    while (--cd >= MIN_VIABLE_DIV)
    {
      uint32_t avg = 0;
      uint16_t totTms = 0;
      // calculate average error for current divisor, and verify it's within tolerance for all timings.
      for (uint16_t i = 1 + p; i < results.rawlen; i += 2)
      {
        uint16_t val = results.rawbuf[i] * RAWTICK;
        uint16_t rmdr = val >= cd ? val % cd : cd - val;
        if (rmdr > get_tolerance(val))
        {
          avg = 0xFFFFU;
          break;
        }
        avg += rmdr;
        totTms += val / cd + (cd > val ? 1 : 0);
      }
      if (avg == 0xFFFFU)
        continue;
      avg /= results.rawlen / 2;
      float avgTms = (float)totTms / (results.rawlen / 2);
      if (avgTms <= bstMul && avg < bstAvg)
      {
        bstMul = avgTms;
        bstAvg = avg;
        bstDiv = cd;
        //serialPrintln(String("p="+ uint64ToString(p, 10) + " cd=" + uint64ToString(cd, 10) +"  avgErr=" + uint64ToString(avg, 10) + " totTms="+ uint64ToString(totTms, 10) + " avgTms="+ uint64ToString((uint16_t)(avgTms*10), 10) ).c_str());
      }
    }
    if (bstDiv == 0xFFFFU)
    {
      //addLog(LOG_LEVEL_INFO, F("IR2: No proper divisor found. Try again..."));
      return false;
    }
    div[p] = bstDiv;

    line = String(p ? String(F("Blank: ")) : String(F("Pulse: "))) + String(F(" divisor=")) + uint64ToString(bstDiv, 10) + String(F("  avgErr=")) + uint64ToString(bstAvg, 10) + String(F(" avgMul=")) + uint64ToString((uint16_t)bstMul, 10) + '.' + ((char)((bstMul - (uint16_t)bstMul) * 10) + '0');
    addLog(LOG_LEVEL_DEBUG, line);
  }

  // Generate the B32 Hex string, per the divisors found.
  uint16_t total = results.rawlen - 1, tmOut[total];
  //line = "Timing muls ("+ uint64ToString(total, 10) + "): ";

  for (unsigned int i = 0; i < total; i++)
  {
    uint16_t val = results.rawbuf[i + 1] * RAWTICK;
    unsigned int dv = div[(i)&1];
    unsigned int tm = val / dv + (val % dv > dv / 2 ? 1 : 0);
    tmOut[i] = tm;
    //line += uint64ToString(tm, 10) + ",";
  }
  //serialPrintln(line);

  char out[total];
  unsigned int iOut = 0, s = 2, d = 0;
  for (; s + 1 < total; d = s, s += 2)
  {
    unsigned int vals = 2;
    while (s + 1 < total && tmOut[s] == tmOut[d] && tmOut[s + 1] == tmOut[d + 1])
    {
      vals += 2;
      s += 2;
    }
    if (iOut + 5 > sizeof(out) || tmOut[d] >= 32 * 32 || tmOut[d + 1] >= 32 * 32 || vals >= 64)
    {
      //addLog(LOG_LEVEL_INFO, F("IR2: Raw code too long. Try again..."));
      return false;
    }

    if (vals > 4 || (vals == 4 && (tmOut[d] >= 32 || tmOut[d + 1] >= 32)))
    {
      out[iOut++] = '*';
      out[iOut++] = to_32hex(vals / 2);
      vals = 2;
    }
    while (vals--)
      iOut = storeB32Hex(out, iOut, tmOut[d++]);
  }
  while (d < total)
    iOut = storeB32Hex(out, iOut, tmOut[d++]);

  out[iOut] = 0;
  line = String(F("IRSEND,RAW2,")) + String(out) + String(F(",38,")) + uint64ToString(div[0], 10) + ',' + uint64ToString(div[1], 10);
  addLog(LOG_LEVEL_INFO, line);
  return true;
}

unsigned int storeB32Hex(char out[], unsigned int iOut, unsigned int val)
{
  if (val >= 32)
  {
    out[iOut++] = '^';
    out[iOut++] = to_32hex(val / 32);
    val %= 32;
  }
  out[iOut++] = to_32hex(val);
  return iOut;
}

#endif // USES_P016