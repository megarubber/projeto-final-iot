#include <WiFi.h>
#include <PubSubClient.h>
#include <LoRaWan_APP.h>

#define RF_FREQUENCY                915000000
#define LORA_BANDWIDTH              0
#define LORA_SPREADING_FACTOR       7
#define LORA_CODINGRATE             1
#define LORA_PREAMBLE_LENGTH        8
#define LORA_SYMBOL_TIMEOUT         0
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_IQ_INVERSION_ON        false
#define BUFFER_SIZE                 30

static RadioEvents_t radioEvents;
char rxPacket[BUFFER_SIZE];
volatile bool loraIdle  = true;
volatile bool newPacket = false;
int16_t lastRssi = 0;
int8_t  lastSnr  = 0;

const char* WIFI_SSID     = "your-ssid";
const char* WIFI_PASSWORD = "your-password";

const char*    MQTT_SERVER = "192.168.1.10";   // Mosquitto host
const uint16_t MQTT_PORT   = 1883;
const char*    MQTT_USER   = "esp32";
const char*    MQTT_PASS   = "mqtt-password";
const char*    MQTT_ID     = "waste-receiver-01";

const char* TOPIC_EVENT  = "waste/bin1/event";
const char* TOPIC_STATUS = "waste/bin1/receiver/status";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
unsigned long lastReconnectAttempt = 0;

void onRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  if (size >= BUFFER_SIZE) size = BUFFER_SIZE - 1;
  memcpy(rxPacket, payload, size);
  rxPacket[size] = '\0';
  lastRssi  = rssi;
  lastSnr   = snr;
  newPacket = true;
  Radio.Sleep();
  loraIdle = true;
}

void setupLoRa() {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  radioEvents.RxDone = onRxDone;
  Radio.Init(&radioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConnected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi not connected (will keep retrying in background).");
  }
}

bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) return false;

  // Last Will: broker publishes "offline" if the receiver drops
  bool ok = mqttClient.connect(MQTT_ID, MQTT_USER, MQTT_PASS,
                               TOPIC_STATUS, 0, true, "offline");
  if (ok) {
    Serial.println("MQTT connected");
    mqttClient.publish(TOPIC_STATUS, "online", true);
  } else {
    Serial.printf("MQTT failed, rc=%d\r\n", mqttClient.state());
  }
  return ok;
}

void maintainMqtt() {
  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }
  unsigned long now = millis();
  if (now - lastReconnectAttempt > 5000) {
    lastReconnectAttempt = now;
    connectMqtt();
  }
}

void publishPacket() {
  if (strncmp(rxPacket, "WASTE:", 6) != 0) {
    Serial.printf("Ignoring unknown packet: %s\r\n", rxPacket);
    return;
  }

  const char* wasteType = rxPacket + 6;      // "WET" or "DRY"

  char json[128];
  snprintf(json, sizeof(json),
           "{\"type\":\"%s\",\"rssi\":%d,\"snr\":%d,\"uptime_ms\":%lu}",
           wasteType, lastRssi, lastSnr, millis());

  Serial.printf("Packet: %s | RSSI %d | SNR %d\r\n", rxPacket, lastRssi, lastSnr);

  if (mqttClient.connected()) {
    mqttClient.publish(TOPIC_EVENT, json);
  } else {
    Serial.println("MQTT offline, event lost");
  }
}

void setup() {
  Serial.begin(115200);
  setupWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setSocketTimeout(2);
  setupLoRa();
}

void loop() {
  Radio.IrqProcess();
  maintainMqtt();

  if (newPacket) {
    newPacket = false;
    publishPacket();
  }

  if (loraIdle) {
    loraIdle = false;
    Radio.Rx(0);
  }
}