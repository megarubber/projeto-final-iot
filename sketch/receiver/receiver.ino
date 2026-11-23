#include <WiFi.h>
#include <PubSubClient.h>
#include <LoRaWan_APP.h>

#define RF_FREQUENCY                915000000
#define TX_OUTPUT_POWER             5
#define LORA_BANDWIDTH              0
#define LORA_SPREADING_FACTOR       7
#define LORA_CODINGRATE             1
#define LORA_PREAMBLE_LENGTH        8
#define LORA_SYMBOL_TIMEOUT         0
#define LORA_FIX_LENGTH_PAYLOAD_ON  false
#define LORA_IQ_INVERSION_ON        false
#define BUFFER_SIZE                 64

static RadioEvents_t radioEvents;
char rxPacket[BUFFER_SIZE];
char txPacket[BUFFER_SIZE];
volatile bool loraIdle  = true;
volatile bool newPacket = false;
int16_t lastRssi = 0;
int8_t  lastSnr  = 0;

const char* WIFI_SSID     = "your-ssid";
const char* WIFI_PASSWORD = "your-password";

const char*    MQTT_SERVER = "192.168.1.10";   // Coloque o IPv4 do seu PC aqui
const uint16_t MQTT_PORT   = 1883;
const char*    MQTT_USER   = NULL;             // Mosquitto local sem autenticação
const char*    MQTT_PASS   = NULL;
const char*    MQTT_ID     = "waste-receiver-01";

const char* TOPIC_EVENT     = "waste/bin1/event";
const char* TOPIC_TELEMETRY = "waste/bin1/telemetry";
const char* TOPIC_STATUS    = "waste/bin1/receiver/status";
const char* TOPIC_CMD_SPEED = "waste/bin1/cmd/speed";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
unsigned long lastReconnectAttempt = 0;

String pendingSpeedCmd = "";

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  if (strcmp(topic, TOPIC_CMD_SPEED) == 0) {
    pendingSpeedCmd = "SPD:" + msg;
    Serial.printf("Comando agendado para LoRa: %s\r\n", pendingSpeedCmd.c_str());
  }
}

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

void onTxDone() {
  Serial.println("Comando enviado ao Sender");
  loraIdle = true;
}

void onTxTimeout() {
  Radio.Sleep();
  loraIdle = true;
}

void setupLoRa() {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  radioEvents.RxDone    = onRxDone;
  radioEvents.TxDone    = onTxDone;
  radioEvents.TxTimeout = onTxTimeout;
  Radio.Init(&radioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
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

  bool ok = mqttClient.connect(MQTT_ID, MQTT_USER, MQTT_PASS,
                               TOPIC_STATUS, 0, true, "offline");
  if (ok) {
    Serial.println("MQTT connected");
    mqttClient.publish(TOPIC_STATUS, "online", true);
    mqttClient.subscribe(TOPIC_CMD_SPEED);
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
  char json[160];

  if (strncmp(rxPacket, "WASTE:", 6) == 0) {
    const char* wasteType = rxPacket + 6;      // "WET" or "DRY"
    snprintf(json, sizeof(json),
             "{\"type\":\"%s\",\"rssi\":%d,\"snr\":%d,\"uptime_ms\":%lu}",
             wasteType, lastRssi, lastSnr, millis());

    Serial.printf("Event: %s | RSSI %d | SNR %d\r\n", rxPacket, lastRssi, lastSnr);
    if (mqttClient.connected()) mqttClient.publish(TOPIC_EVENT, json);
  }
  else if (strncmp(rxPacket, "STAT:", 5) == 0) {
    float temp = 0;
    int umid = 0, rpm = 0, pwm = 0;
    unsigned long activeSec = 0;

    if (sscanf(rxPacket + 5, "%f,%d,%d,%lu,%d", &temp, &umid, &rpm, &activeSec, &pwm) == 5) {
      snprintf(json, sizeof(json),
               "{\"temp\":%.1f,\"umid\":%d,\"rpm\":%d,\"active_s\":%lu,\"pwm\":%d,\"rssi\":%d}",
               temp, umid, rpm, activeSec, pwm, lastRssi);

      Serial.printf("Telemetry: %s\r\n", json);
      if (mqttClient.connected()) mqttClient.publish(TOPIC_TELEMETRY, json);
    }
  }
  else {
    Serial.printf("Ignoring unknown packet: %s\r\n", rxPacket);
  }
}

void setup() {
  Serial.begin(115200);
  setupWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setSocketTimeout(2);
  mqttClient.setCallback(onMqttMessage);
  setupLoRa();
}

void loop() {
  Radio.IrqProcess();
  maintainMqtt();

  if (newPacket) {
    newPacket = false;
    publishPacket();

    // Aproveita a janela de RX de 300ms do Sender para despachar a nova velocidade
    if (pendingSpeedCmd.length() > 0) {
      delay(40);
      snprintf(txPacket, BUFFER_SIZE, "%s", pendingSpeedCmd.c_str());
      Radio.Send((uint8_t *)txPacket, strlen(txPacket));
      pendingSpeedCmd = "";
      loraIdle = false;
    }
  }

  if (loraIdle) {
    loraIdle = false;
    Radio.Rx(0);
  }
}