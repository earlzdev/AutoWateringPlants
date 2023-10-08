#include <Arduino.h>
#include <Wire.h>
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);

 // bluetooth constants
// Пример команды -  <FirstParameter=20000, NextParameter=value, AnotherParameter=anotherValue> 
char turnWateringOnForciblyCommand = '<startAll>';
char turnWateringOffForciblyCommand = '<stopAll>';

// pins
int waterLevelPin = 3;
int k1 = 4;
int k2 = 5;
int k3 = 6;
int k4 = 7;

// properties
#define SOP '<'
#define EOP '>'
String k1WateringStartTime = "";
String k1WateringStopTime = "";
bool k1CurrentlyWatering = false;
String k2WateringStartTime = "";
String k2WateringStopTime = "";
bool k2CurrentlyWatering = false;
String k3WateringStartTime = "";
String k3WateringStopTime = "";
bool k3CurrentlyWatering = false;
String k4WateringStartTime = "";
String k4WateringStopTime = "";
bool k4CurrentlyWatering = false;
bool startWateringAllPlants = false;
bool stopWateringAllPlants = false;
bool commandStarted = false;
bool commandEnded = false;
char commandBody[80];
byte index;
String relayKeyword = "relay";
const char data[] = "what time is it";

// methods
void turnWateringOnForcibly();
void turnWateringOffForcibly();
void observeCommands();
void parseCommand(char command[80]);
void setNewAutoWateringTime(int relay, String time, String stopTime);
void executeCommand(String command);
void parseSetWateringTimeCommand(String readyCommand);
void printDateTime(const RtcDateTime& dt);
void setupRtc();
void checkAutoWateringTime();
void startWateringForRelay(int relay);
void stopWateringForRelay(int relay);

bool wasError(const char* errorTopic = "")
{
    uint8_t error = Rtc.LastError();
    if (error != 0)
    {
        // we have a communications error
        // see https://www.arduino.cc/reference/en/language/functions/communication/wire/endtransmission/
        // for what the number means
        Serial.print("[");
        Serial.print(errorTopic);
        Serial.print("] WIRE communications error (");
        Serial.print(error);
        Serial.print(") : ");

        switch (error)
        {
        case Rtc_Wire_Error_None:
            Serial.println("(none?!)");
            break;
        case Rtc_Wire_Error_TxBufferOverflow:
            Serial.println("transmit buffer overflow");
            break;
        case Rtc_Wire_Error_NoAddressableDevice:
            Serial.println("no device responded");
            break;
        case Rtc_Wire_Error_UnsupportedRequest:
            Serial.println("device doesn't support request");
            break;
        case Rtc_Wire_Error_Unspecific:
            Serial.println("unspecified error");
            break;
        case Rtc_Wire_Error_CommunicationTimeout:
            Serial.println("communications timed out");
            break;
        }
        return true;
    }
    return false;
}

void setup() {
  pinMode(k1, OUTPUT);
  pinMode(k2, OUTPUT);
  pinMode(k3, OUTPUT);
  pinMode(k4, OUTPUT);
  digitalWrite(k1, 1);
  digitalWrite(k2, 1);
  digitalWrite(k3, 1);
  digitalWrite(k4, 1);
  Serial.begin(9600);  
  setupRtc();
}

void loop() {
  if (startWateringAllPlants) {
    Serial.println("startWateringAllPlants");
    turnWateringOnForcibly();
  } else if (stopWateringAllPlants) {
    turnWateringOffForcibly();
    Serial.println("stopWateringAllplants");
  }
  observeCommands();
  checkAutoWateringTime();
}

void setupRtc() {
    Serial.print("compiled: ");
    Serial.print(__DATE__);
    Serial.println(__TIME__);

    //--------RTC SETUP ------------
    // if you are using ESP-01 then uncomment the line below to reset the pins to
    // the available pins for SDA, SCL
    // Wire.begin(0, 2); // due to limited pins, use pin 0 and 2 for SDA, SCL
    
    Rtc.Begin();
#if defined(WIRE_HAS_TIMEOUT)
    Wire.setWireTimeout(3000 /* us */, true /* reset_on_timeout */);
#endif

    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    printDateTime(compiled);
    Serial.println();

    if (!Rtc.IsDateTimeValid()) 
    {
        if (!wasError("setup IsDateTimeValid"))
        {
            // Common Causes:
            //    1) first time you ran and the device wasn't running yet
            //    2) the battery on the device is low or even missing

            Serial.println("RTC lost confidence in the DateTime!");

            // following line sets the RTC to the date & time this sketch was compiled
            // it will also reset the valid flag internally unless the Rtc device is
            // having an issue

            Rtc.SetDateTime(compiled);
        }
    }

    if (!Rtc.GetIsRunning())
    {
        if (!wasError("setup GetIsRunning"))
        {
            Serial.println("RTC was not actively running, starting now");
            Rtc.SetIsRunning(true);
        }
    }

    RtcDateTime now = Rtc.GetDateTime();
    if (!wasError("setup GetDateTime"))
    {
        if (now < compiled)
        {
            Serial.println("RTC is older than compile time, updating DateTime");
            Rtc.SetDateTime(compiled);
        }
        else if (now > compiled)
        {
            Serial.println("RTC is newer than compile time, this is expected");
        }
        else if (now == compiled)
        {
            Serial.println("RTC is the same as compile time, while not expected all is still fine");
        }
    }

    // never assume the Rtc was last configured by you, so
    // just clear them to your needed state
    Rtc.Enable32kHzPin(false);
    wasError("setup Enable32kHzPin");
    Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 
    wasError("setup SetSquareWavePin");
}

void observeCommands() {
  while(Serial.available() > 0)
  {
    char inChar = Serial.read();
    if(inChar == SOP)
    {
       index = 0;
       commandBody[index] = '\0';
       commandStarted = true;
       commandEnded = false;
    }
    else if(inChar == EOP)
    {
       commandEnded = true;
       break;
    }
    else
    {
      if(index < 79)
      {
        commandBody[index] = inChar;
        index++;
        commandBody[index] = '\0';
      }
    }
  }

  if(commandStarted && commandEnded)
  {
    parseCommand(commandBody);
    commandStarted = false;
    commandEnded = false;
    index = 0;
    commandBody[index] = '\0';
  }
}

void parseCommand(char command[80]) {
  Serial.println("COMMAND -> " + String(command));
  if (String(command) == "startAll") {
    Serial.println("STARTALL");
    startWateringAllPlants = true;
    stopWateringAllPlants = false;
  } else if (String(command) == "stopAll") {
    Serial.println("STOPALL");
    startWateringAllPlants = false;
    stopWateringAllPlants = true;
  }
  char* pch = strtok (command,",=");
  String readyCommand = "";
  bool isSetAutoWaterginCommand = false;
  char setWateringTimeCommand[6] = {};
  int currentLoopCycle = 0;
  while (pch != NULL)                       
  {
    if (String(pch) == "relay" || isSetAutoWaterginCommand) {
      Serial.println("RELAY");
      isSetAutoWaterginCommand = true;
      setWateringTimeCommand[currentLoopCycle] = (char)pch;
      currentLoopCycle++;
    }
    //  else if (pch == "startAll") {
    //   Serial.println("STARTALL");
    //   startWateringAllPlants = true;
    //   stopWateringAllPlants = false;
    //   // turnWateringOnForcibly();
    //   pch = NULL;
    // } else if (pch == "stopAll") {
    //   Serial.println("STOPALL");
    //   startWateringAllPlants = false;
    //   stopWateringAllPlants = true;
    //   // turnWateringOffForcibly();
    //   pch = NULL;
    // }
      readyCommand += pch;
      pch = strtok (NULL, ",=");
  }
  if (isSetAutoWaterginCommand) {
    Serial.println(readyCommand);
    parseSetWateringTimeCommand(readyCommand);
  }
}

void parseSetWateringTimeCommand(String readyCommand) {
  Serial.println("gotcommnad:");
  // Serial.println(readyCommand);
  int foundRelayIndex = readyCommand.indexOf("relay");
  int foundStartTimeIndex = readyCommand.indexOf("startWateringTime");
  int foundDurationIndex = readyCommand.indexOf("stopWateringTime");
  Serial.println("foundDurationIndex:");
  Serial.println(foundDurationIndex);
  if (foundDurationIndex != -1 && foundRelayIndex != -1 && foundStartTimeIndex != -1) {
    String relayNumber = readyCommand.substring(foundRelayIndex + 5, foundRelayIndex + 5 + 1);
    String startTime = readyCommand.substring(foundStartTimeIndex + 17, foundStartTimeIndex + 17 + 5);
    String stopTime = readyCommand.substring(foundDurationIndex + 16, foundDurationIndex + 16 + 5);
    Serial.println(relayNumber);
    Serial.println(startTime);
    Serial.println(stopTime);
    setNewAutoWateringTime(relayNumber.toInt(), startTime, stopTime);
  } else {
    Serial.println("PARSE COMMAND ERROR:");
    Serial.println(readyCommand);
    Serial.println("foundRelayIndex");
    Serial.println(foundRelayIndex);
    Serial.println("foundStartTimeIndex");
    Serial.println(foundStartTimeIndex);
    Serial.println("foundStopTimeIndex");
    Serial.println(foundDurationIndex);
  }
}

void setNewAutoWateringTime(int relay, String time, String stopTime) {
  switch (relay)
  {
  case 1:
    k1WateringStartTime = time;
    k1WateringStopTime = stopTime;
    Serial.println("set new auto watering time for relay 1");
    break;
  case 2:
    k2WateringStartTime = time;
    k2WateringStopTime = stopTime;
    Serial.println("set new auto watering time for relay 2");
    break;
  case 3:
    k3WateringStartTime = time;
    k3WateringStopTime = stopTime;
    Serial.println("set new auto watering time for relay 3");
    break;
  case 4:
    k4WateringStartTime = time;
    k4WateringStopTime = stopTime;
    Serial.println("set new auto watering time for relay 4");
    break;
  default:
    break;
  }
}

void turnWateringOnForcibly() {
  // todo подать 1 на все реле для включения принудительного полива
  Serial.println("Turn on all relays");
  startWateringAllPlants = false;
  stopWateringAllPlants = false;
  digitalWrite(k1, 0);
  digitalWrite(k2, 0);
  digitalWrite(k3, 0);
  digitalWrite(k4, 0);
}

void turnWateringOffForcibly() {
  // todo подать 0 на все реле для выключения принудительного полива
  Serial.println("Turn off all relays");
  startWateringAllPlants = false;
  stopWateringAllPlants = false;
  digitalWrite(k1, 1);
  digitalWrite(k2, 1);
  digitalWrite(k3, 1);
  digitalWrite(k4, 1);
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.println(datestring);
}

void checkAutoWateringTime() {
  RtcDateTime currentDateTime = Rtc.GetDateTime();
  uint8_t currentHour = currentDateTime.Hour();
  uint8_t currentMin = currentDateTime.Minute();
  String formattedTime = String(currentHour) + ":" + String(currentMin);
  if (formattedTime != "") {
    if (formattedTime == k1WateringStartTime && k1WateringStartTime != "") {
     startWateringForRelay(1);
   }
    if (formattedTime == k2WateringStartTime && k2WateringStartTime != "") {
      startWateringForRelay(2);
    }
    if (formattedTime == k3WateringStartTime && k3WateringStartTime != "") {
     startWateringForRelay(3);
    }
    if (formattedTime == k4WateringStartTime && k4WateringStartTime != "") {
     startWateringForRelay(4);
    }
    if (formattedTime == k1WateringStopTime && k1WateringStopTime != "") {
     stopWateringForRelay(1);
    }
    if (formattedTime == k2WateringStopTime && k2WateringStopTime != "") {
     stopWateringForRelay(2);
    }
    if (formattedTime == k3WateringStopTime && k3WateringStopTime != "") {
     stopWateringForRelay(3);
    }
    if (formattedTime == k4WateringStopTime && k4WateringStopTime != "") {
     stopWateringForRelay(4);
    } 
  }
}

void startWateringForRelay(int relay) {
  int rel;
  bool curentlyWatering;
  if (relay == 1) {
    if (k1CurrentlyWatering == false) {
    digitalWrite(k1, 0);
    k1CurrentlyWatering = true;
    Serial.println("start watering for relay 1");
    }
    // rel = k1;
    // curentlyWatering = k1CurrentlyWatering;
    // k1CurrentlyWatering = true;
  } else if (relay == 2) {
    if (k1CurrentlyWatering == false) {
    digitalWrite(k2, 0);
    k2CurrentlyWatering = true;
    Serial.println("start watering for relay 2");
    }
    // rel = k2;
    // curentlyWatering = k2CurrentlyWatering;
    // k2CurrentlyWatering = true;
  } else if (relay == 3) {
    if (k3CurrentlyWatering == false) {
    digitalWrite(k3, 0);
    k3CurrentlyWatering = true;
    Serial.println("start watering for relay 3");
    }
    // rel = k3;
    // curentlyWatering = k3CurrentlyWatering;
    // k3CurrentlyWatering = true;
  } else if (relay == 4) {
    if (k4CurrentlyWatering == false) {
    digitalWrite(k4, 0);
    k1CurrentlyWatering = true;
    Serial.println("start watering for relay 4");
    }
    // rel = k4;
    // curentlyWatering = k4CurrentlyWatering;
    // k4CurrentlyWatering = true;
  }
  // if (curentlyWatering == false) {
    // digitalWrite(rel, 0);
    // curentlyWatering = true;
    // Serial.println("start watering for relay " + rel);
  // }
}

void stopWateringForRelay(int relay) {
  int rel;
  bool curentlyWatering;
  if (relay == 1) {
    if (k1CurrentlyWatering == true) {
      digitalWrite(k1, 1);
      k1CurrentlyWatering = false;
      Serial.println("stop watering for relay 1");
    }
    // rel = k1;
    // curentlyWatering = k1CurrentlyWatering;
  } else if (relay == 2) {
    if (k1CurrentlyWatering == true) {
      digitalWrite(k2, 1);
      k2CurrentlyWatering = false;
      Serial.println("stop watering for relay 2");
    }
    // rel = k2;
    // curentlyWatering = k2CurrentlyWatering;
  } else if (relay == 3) {
    if (k1CurrentlyWatering == true) {
      digitalWrite(k3, 1);
      k3CurrentlyWatering = false;
      Serial.println("stop watering for relay 3");
    }
    // rel = k3;
    // curentlyWatering = k3CurrentlyWatering;
  } else if (relay == 4) {
    if (k1CurrentlyWatering == true) {
      digitalWrite(k4, 1);
      k4CurrentlyWatering = false;
      Serial.println("stop watering for relay 4");
    }
    // rel = k4;
    // curentlyWatering = k4CurrentlyWatering;
  }
  // if (curentlyWatering == true) {
  //   digitalWrite(rel, 1);
  //   curentlyWatering = false;
  //   Serial.println("stop watering for relay " + rel);
  // }
}