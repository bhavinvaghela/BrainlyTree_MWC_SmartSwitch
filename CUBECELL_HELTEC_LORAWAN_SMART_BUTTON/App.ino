#include <Arduino.h>
#include <string.h>
#include <stdint.h>
#include "stdint.h"
#include "LoRa_APP.h"
#include <EEPROM.h>


#define FLOW_INPUT_GPIO GPIO4
#define RED_LED GPIO3
unsigned long long previousMillisRxTimeout = 0;
unsigned long long prevMotionMillis = 0;
unsigned long long currMotionMillis = 0;
uint16_t updateDelay;
String HubId = "";
String SensorID = "";
String deviceTypes = "flow switch";
bool PairCmdReceived = false;
bool CanDeviceSleep = false;

typedef enum {
  PAIR_STOP,
  PAIR_INIT,
  PAIR_WAIT,
  PAIR_DONE
} pairState_t;

typedef enum {
  DEV_INVALID = 0,
  DEV_VALVE,
  DEV_REMOTE,
  DEV_FLOW,
  DEV_MOTION
} deviceType_t;

typedef struct {
  deviceType_t type;
  pairState_t state;
  bool activated;
  int8_t maxRetry;
  uint32_t updateDelay;  // time in msec
  uint32_t lastTime;     // time in msec
  uint16_t hubId;
  uint16_t dataLen;
} __attribute__((packed)) pairDevice_t;
//--------------------------------- LORA Start ---------------------------------//
uint64_t getEfuseMacId;
String eMacFuseIdStr = "";
bool loraDataRcvd = false;
bool motionDetected = false;
bool rejoinRequest = false;
#define PIR_MOTION_SENSOR 2

String sendDataStart = "{\"pi\":\"cmd\",\"command\":\"open\",\"sensor_id\":\"";
String sendPairResponse = "{\"pi\":\"rsp\",\"command\":\"open\",\"sensor_id\":\"";
String sendRejoin = "{\"pi\":\"rjn\",\"command\":\"open\",\"sensor_id\":\"";
String sensorEmacFuseId = "123456789";
String sendDeviceType = "\",\"device_type\":\"";
String sendDataEnd = "\"}";

void array_to_string(char array[], unsigned int len, char buffer[]);

void array_to_string(char array[], unsigned int len, char buffer[]) {
  for (unsigned int i = 0; i < len; i++) {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i * 2 + 0] = nib1 < 0xA ? '0' + nib1 : 'A' + nib1 - 0xA;
    buffer[i * 2 + 1] = nib2 < 0xA ? '0' + nib2 : 'A' + nib2 - 0xA;
  }
  buffer[len * 2] = '\0';
}
void handle_lora_rx_msg(String cbLoraRxString) {
  Serial.println("LORA Incoming String - " + cbLoraRxString);

  int i = 0;
  int indexOfPi = cbLoraRxString.indexOf("\"pi\"");
  int indexOfAck = cbLoraRxString.indexOf("\"ack\"");
  int indexOfHubId = cbLoraRxString.indexOf("\"hub_id\"");
  int indexOfSensorId = cbLoraRxString.indexOf("\"sensor_id\"");
  int indexOfCommand = cbLoraRxString.indexOf("\"command\"");
  int indexOfCommandOpen = cbLoraRxString.indexOf("\"open\"");
  int indexOfDeviceType = cbLoraRxString.indexOf("\"device_type\"");
  int indexOfDataEnd = cbLoraRxString.indexOf("\"}");

  Serial.println("indexOfPi : " + String(indexOfPi));
  Serial.println("indexOfAck : " + String(indexOfAck));
  Serial.println("indexOfHubId : " + String(indexOfHubId));
  Serial.println("indexOfSensorId : " + String(indexOfSensorId));
  Serial.println("indexOfCommand : " + String(indexOfCommand));
  Serial.println("indexOfCommandOpen : " + String(indexOfCommandOpen));
  Serial.println("indexOfDeviceType : " + String(indexOfDeviceType));
  Serial.println("send data End : " + String(indexOfDataEnd));

  String pType = "";
  String ReceivedID = "";
  String deviceType = "";

  pType = cbLoraRxString.substring(indexOfPi + 6, indexOfHubId - 2);
  Serial.print("pType: ");
  Serial.println(pType);
  if (pType == "lnk") {
    HubId = cbLoraRxString.substring(indexOfHubId + 10, indexOfSensorId - 2);
    Serial.print("HubID: ");
    Serial.println(HubId);

    ReceivedID = cbLoraRxString.substring(indexOfSensorId + 13, indexOfCommand - 2);
    Serial.print("Received SensorID for join request: ");
    Serial.println(ReceivedID);
    if (eMacFuseIdStr == ReceivedID) {
      Serial.println("pair command received for this device");
      PairCmdReceived = true;
    }
    deviceType = cbLoraRxString.substring(indexOfDeviceType + 15, indexOfDataEnd);
    Serial.println("Device Type : " + String(deviceType));
  }

  if (pType == "rjn") {
    SensorID = cbLoraRxString.substring(indexOfSensorId + 13, indexOfCommand - 2);
    Serial.print("device rejoined for SensorID: ");
    Serial.println(SensorID);
    //   PairCmdReceived = true;
    if (SensorID == eMacFuseIdStr) {
      CanDeviceSleep = true;
    } else {
      Serial.println("Rejoin request is not for this device");
    }
  }
  if ((indexOfPi != 0) && (indexOfAck != 0) && (indexOfHubId != 0) && (indexOfSensorId != 0) && (indexOfCommand != 0) && (indexOfCommandOpen != 0)) {
    loraDataRcvd = true;
  }
}
//--------------------------------- LORA End ---------------------------------//

//--------------------------------- PIR Trigger Start ---------------------------------//
uint8_t pirLowpower = 0;
static TimerEvent_t flowSleep;
#define pirTimetillsleep 30000  // 10Sec(10000) | 5 minutes(300000)
int counterDeepSleep = 0;
bool state = false;

void onFlowSleep() {
  // No need

  /*Serial.printf("Wakeup after 30 sec.........p\r\n \r\n");
  attachInterrupt(FLOW_INPUT_GPIO, onFlowWakeUp, RISING);
  // TimerStop(&flowSleep);
  delay(5);
  */
}
void onFlowWakeUp() {
  Serial.println("Flow detected....");
  digitalWrite(RED_LED, HIGH);
  delay(300);
  digitalWrite(RED_LED, LOW);
  state = true;
  if (CanDeviceSleep == true) {
    pirLowpower = 0;
    Serial.println("send data to Hub");
    lora_send_packet(sendDataStart + eMacFuseIdStr + sendDeviceType + deviceTypes + sendDataEnd);
    lora_app_loop();
    lora_app_loop();
    lora_app_loop();
    lora_app_loop();
    lora_app_loop();

    counterDeepSleep++;
  }
  // detachInterrupt(FLOW_INPUT_GPIO);
}
//--------------------------------- PIR Trigger End ---------------------------------//

//--------------------------------- HeartBeat Trigger Start ---------------------------------//
#define timetillsleep 100
#define heartBeatTimetillwakeup 20000

static TimerEvent_t heartBeatSleep;
static TimerEvent_t heartBeatWakeUp;
uint8_t heartBeatLowpower = 1;

void onHeartBeatSleep() {
  Serial.printf("Going into HeartBeat Lowpower mode, %d ms later wake up.\r\n \r\n", heartBeatTimetillwakeup);
  heartBeatLowpower = 1;
  //heartBeatTimetillwakeup ms later wake up;
  TimerSetValue(&heartBeatWakeUp, heartBeatTimetillwakeup);
  TimerStart(&heartBeatWakeUp);
}
void onHeartBeatWakeUp() {
  Serial.printf("Woke up, %d ms later into HeartBeat Lowpower mode.\r\n \r\n", timetillsleep);
  heartBeatLowpower = 0;
  //timetillsleep ms later into lowpower mode;
  TimerSetValue(&heartBeatSleep, timetillsleep);
  TimerStart(&heartBeatSleep);
}
//--------------------------------- HeartBeat Trigger End ---------------------------------//

// void task1_max_loop(void) {
//   motionDetected = digitalRead(PIR_MOTION_SENSOR);
//   if(motionDetected == true)
//   {
//     lora_send_packet(sendDataStart + eMacFuseIdStr + sendDataEnd);
//   }
// }

void app_init() {

  Serial.begin(115200);
  getEfuseMacId = getID();

  String eMacFuseIdStr1 = String((uint32_t)(getEfuseMacId >> 32));
  // Serial.println("eMacFuseIdStr1 : " + eMacFuseIdStr1);
  String eMacFuseIdStr2 = String((uint32_t)(getEfuseMacId));
  // Serial.println("eMacFuseIdStr2 : " + eMacFuseIdStr2);
  eMacFuseIdStr = eMacFuseIdStr1 + eMacFuseIdStr2;
  Serial.println("eMacFuseIdStr : " + eMacFuseIdStr);

  delay(100);

  lora_app_init();
  Radio.Sleep();

  pinMode(FLOW_INPUT_GPIO, INPUT_PULLDOWN);
  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);
#if 1  //Disable for testing by Ashish
  attachInterrupt(FLOW_INPUT_GPIO, onFlowWakeUp, RISING);
  TimerInit(&flowSleep, onFlowSleep);
#endif

  prevMotionMillis = millis();
  delay(1000);
}

void app_loop() {
  lora_app_loop();
  unsigned long long currentMillis = millis();
  if (CanDeviceSleep == true) {
    delay(50);  //disabled by ashish
    if (heartBeatLowpower && pirLowpower) {
      Radio.Sleep();
      delay(50);
      Serial.println("Entering in sleep mode");
      Serial.flush();
      attachInterrupt(FLOW_INPUT_GPIO, onFlowWakeUp, RISING);
      delay(10);
      lowPowerHandler();
    }
  }
  if ((loraDataRcvd == true) || (currentMillis - previousMillisRxTimeout >= 5000)) {
    //timetillsleep ms later into lowpower mode;
    if (CanDeviceSleep == true) {
      // TimerSetValue(&flowSleep, pirTimetillsleep);  //disabled by ashish
      // TimerStart(&flowSleep);                       //disabled by ashish
      // pirLowpower = 1;
    }
    delay(50);
    loraDataRcvd = false;
  }
  if (PairCmdReceived == true) {
    lora_send_packet(sendPairResponse + eMacFuseIdStr + sendDeviceType + deviceTypes + sendDataEnd);
    PairCmdReceived = false;
    delay(50);
    CanDeviceSleep = true;
  }
  if (rejoinRequest == false) {
    rejoinRequest = true;
    lora_send_packet(sendRejoin + eMacFuseIdStr + sendDeviceType + deviceTypes + sendDataEnd);
    lora_app_loop();

    delay(1000);
  }
}
