#include <WiFi.h>
#include <LoRaWan_APP.h>

#define RF_FREQUENCY 915000000
#define TX_OUTPUT_POWER 14
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE 1000
#define BUFFER_SIZE 30

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

static RadioEvents_t RadioEvents;
int16_t txNumber;
int16_t rssi,rxSize;
bool lora_idle = true;

const char* ssid = "";
const char* password = "";

void onRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  rssi = rssi;
  rxSize = size;
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Radio.Sleep();
  Serial.printf("\r\nreceived packet \"%s\" with rssi %d , length %d\r\n", rxpacket, rssi, rxSize);
  lora_idle = true;
}

void setupLoRa() {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  
  txNumber = 0;
  rssi = 0;

  RadioEvents.RxDone = onRxDone;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(
    MODEM_LORA, 
    LORA_BANDWIDTH, 
    LORA_SPREADING_FACTOR, 
    LORA_CODINGRATE, 
    0, 
    LORA_PREAMBLE_LENGTH, 
    LORA_SYMBOL_TIMEOUT, 
    LORA_FIX_LENGTH_PAYLOAD_ON, 
    0, 
    true, 
    0, 
    0, 
    LORA_IQ_INVERSION_ON, 
    true
  );

  if(lora_idle) {
    lora_idle = false;
    Serial.println("into RX mode");
    Radio.Rx(0);
  }
  Radio.IrqProcess();
}

int setupWiFi() {
  WiFi.disconnect();
  WiFi.setAutoReconnect(true); 
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    Serial.print(".");
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected");
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP());
    return 0;
  }

  WiFi.disconnect();
  delay(2000);
  return -1;
}

void setup() {
  Serial.begin(9600);
  int wifiStatus = setupWiFi();
  if (wifiStatus == 0)
    while(1) Serial.println("Wi-Fi not connected.");
  setupLoRa();
}

void loop() {

}