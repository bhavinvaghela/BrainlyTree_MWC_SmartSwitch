#include "LoRaWan_APP.h"

#define RF_FREQUENCY 868000000   // Hz
#define TX_OUTPUT_POWER 20       // dBm
#define LORA_BANDWIDTH 0         // [0: 125 kHz, \
                                 //  1: 250 kHz, \
                                 //  2: 500 kHz, \
                                 //  3: Reserved]
#define LORA_SPREADING_FACTOR 7  // [SF7..SF12]
#define LORA_CODINGRATE 1        // [1: 4/5, \
                                 //  2: 4/6, \
                                 //  3: 4/7, \
                                 //  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8   // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0    // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE 1000
#define BUFFER_SIZE 100  // Define the payload size here

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

int16_t txNumber;
int16_t rxNumber;

int16_t Rssi, rxSize;

bool receiveflag = false;  // software flag for LoRa receiver, received data makes it true.

String loraRxString;
String cbLoraRxString;

bool loraRxDataPending = false;
typedef void (*lora_comm_rx_cb_t)(String loraRxString);
void lora_comm_rx_msg_register_cb(lora_comm_rx_cb_t cb_lora);
static lora_comm_rx_cb_t _cb_lora = NULL;
pairDevice_t HubData;

void lora_comm_rx_msg_register_cb(lora_comm_rx_cb_t cb_lora) {
  _cb_lora = cb_lora;
}

void OnTxDone(void) {
  Serial.println("TX done......");
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.print("TX Timeout......");
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  rxNumber++;
  Rssi = rssi;
  rxSize = size;
  char decryptedText[size];

  decryptMessage(payload, size, decryptedText, sizeof(decryptedText));

  Serial.print("Decrypted message: ");
  Serial.println(decryptedText);
  strcpy(rxpacket, decryptedText);

  // memcpy(rxpacket, payload, size);
  // rxpacket[size] = '\0';
  Radio.Sleep();

  Serial.println("\r\nreceived packet" + String(rxpacket) + String(" with Rssi ") + String(Rssi) + String(" , length : ") + String(rxSize) + String("\r\n"));
  // Serial.println("wait to send next packet");
  Serial.println("Lora Rx Size - " + String(rxSize));

  loraRxString = "";
  // char localBuff[100];
  // memset(localBuff, 0, 100);
  // memcpy(localBuff, payload, rxSize);
  loraRxString = String(decryptedText);
  loraRxDataPending = true;
}

void lora_init(void) {
  txNumber = 0;
  Rssi = 0;
  rxNumber = 0;

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
}

void lora_send_packet(String packetString) {
  Serial.print("packetString : ");
  Serial.println(packetString);
  delay(200);
  const char *myCString = packetString.c_str();
  encryptAndTransmit(myCString);

  // packetString.toCharArray(txpacket, (packetString.length() + 1));

  // Serial.println(txpacket);
  // Radio.Send((uint8_t *)txpacket, strlen(txpacket));
  // delay(200);
  // Radio.IrqProcess();
}

void lora_app_init() {
  delay(100);
  lora_init();
  delay(100);
  lora_comm_rx_msg_register_cb(&handle_lora_rx_msg);
}

void lora_app_loop() {
  // Serial.println("in callback loop");
  if (loraRxDataPending == true) {
    Serial.println("loraRxDataPending == true");
    loraRxDataPending = false;
    if (_cb_lora) {
      //   Serial.println("_cb_lora");
      cbLoraRxString = loraRxString;
      _cb_lora(cbLoraRxString);
    }
  }
  Radio.Rx(0);
  Radio.IrqProcess();
}
