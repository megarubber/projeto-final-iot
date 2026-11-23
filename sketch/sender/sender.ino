#include <DHT.h>
#include <ESP32Servo.h>
#include <LoRaWan_APP.h>

// LoRa protocol setup
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

char txPacket[BUFFER_SIZE];
char rxPacket[BUFFER_SIZE];
bool loraIdle = true;
static RadioEvents_t radioEvents;

#define MOTOR_CONVEYOR_A  5
#define MOTOR_CONVEYOR_B  6
#define ENCODER_C1 14
#define ENCODER_C2 16
#define MOISTURE_PIN      7   // YL-83 rain/moisture sensor
#define SERVO_PIN         10
#define PROXIMITY_SENSOR  9
#define TEMP_SENSOR_PIN   4   // Pino do DHT11/DHT22 (ajuste conforme seu circuito)
#define DHTTYPE           DHT11

const int moistureThreshold = 500;
const int openBinAngle      = 180;
const int closedBinAngle    = 0;
const int MAX_MOTOR_RPM     = 120; // RPM máximo estimado do seu motor a 100% PWM

DHT dht(TEMP_SENSOR_PIN, DHTTYPE);
Servo binServo;

byte Encoder_C1Last;
int duracao = 0;
bool Direcao = false;
bool alreadySent        = false;
int conveyorSpeed       = 200; // Velocidade inicial PWM (0 a 255)
bool isRunning          = false;
unsigned long lastTick  = 0;
unsigned long activeMs  = 0;
unsigned long lastStat  = 0;

void calculapulso() {
  int Lstate = digitalRead(ENCODER_C1);
  if ((Encoder_C1Last == LOW) && Lstate == HIGH)
  {
    int val = digitalRead(ENCODER_C2);
    if (val == LOW && Direcao)
    {
      Direcao = false; //Reverse
    }
    else if (val == HIGH && !Direcao)
    {
      Direcao = true;  //Forward
    }
  }
  Encoder_C1Last = Lstate;

  if (!Direcao)  duracao++;
  else  duracao--;
}

void stopConveyor() {
  analogWrite(MOTOR_CONVEYOR_A, 0);
  digitalWrite(MOTOR_CONVEYOR_B, LOW);
  isRunning = false;
}

void conveyorForward() {
  analogWrite(MOTOR_CONVEYOR_A, conveyorSpeed);
  digitalWrite(MOTOR_CONVEYOR_B, LOW);
  isRunning = (conveyorSpeed > 0);
}

void conveyorBackward() {
  digitalWrite(MOTOR_CONVEYOR_A, LOW);
  analogWrite(MOTOR_CONVEYOR_B, conveyorSpeed);
  isRunning = (conveyorSpeed > 0);
}

void onTxDone() {
  Serial.println("TX done");
  loraIdle = true;
  Radio.Rx(300); // Abre janela de 300ms para receber comando de velocidade do Receiver
}

void onTxTimeout() {
  Radio.Sleep();
  Serial.println("TX timeout");
  loraIdle = true;
}

void onRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  if (size >= BUFFER_SIZE) size = BUFFER_SIZE - 1;
  memcpy(rxPacket, payload, size);
  rxPacket[size] = '\0';

  if (strncmp(rxPacket, "SPD:", 4) == 0) {
    int newSpeed = atoi(rxPacket + 4);
    conveyorSpeed = constrain(newSpeed, 0, 255);
    Serial.printf("Nova velocidade PWM: %d\r\n", conveyorSpeed);
  }

  Radio.Sleep();
  loraIdle = true;
}

void sendLora(const char* msg) {
  if (!loraIdle) return;
  snprintf(txPacket, BUFFER_SIZE, "%s", msg);
  Serial.printf("Sending: %s\r\n", txPacket);
  Radio.Send((uint8_t *)txPacket, strlen(txPacket));
  loraIdle = false;
}

void sendTelemetry() {
  float temp = dht.readTemperature();
  if (isnan(temp)) temp = 0.0;

  int rawMoisture = analogRead(MOISTURE_PIN);
  // Converte leitura analógica (4095 seco -> 0 molhado) para 0-100%
  int humidityPct = constrain(map(rawMoisture, 4095, 1000, 0, 100), 0, 100);

  int estRpm = isRunning ? map(conveyorSpeed, 0, 255, 0, MAX_MOTOR_RPM) : 0;
  unsigned long uptimeSec = activeMs / 1000;

  char statMsg[BUFFER_SIZE];
  snprintf(statMsg, sizeof(statMsg), "STAT:%.1f,%d,%d,%lu,%d",
           temp, humidityPct, estRpm, uptimeSec, conveyorSpeed);
  sendLora(statMsg);
}

void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_CONVEYOR_A, OUTPUT);
  pinMode(MOTOR_CONVEYOR_B, OUTPUT);
  pinMode(MOISTURE_PIN, INPUT);
  pinMode(PROXIMITY_SENSOR, INPUT);

  pinMode(ENCODER_C2, INPUT);
  attachInterrupt(0, calculapulso, CHANGE);

  dht.begin();
  binServo.attach(SERVO_PIN);

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  radioEvents.TxDone    = onTxDone;
  radioEvents.TxTimeout = onTxTimeout;
  radioEvents.RxDone    = onRxDone;
  Radio.Init(&radioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, false);

  lastTick = millis();
}

void loop() {
  Radio.IrqProcess();

  unsigned long now = millis();
  if (isRunning) {
    activeMs += (now - lastTick);
  }
  lastTick = now;

  // Envia telemetria periódica a cada 2 segundos quando o rádio estiver livre
  if (now - lastStat >= 2000 && loraIdle) {
    lastStat = now;
    sendTelemetry();
  }

  delay(50);

  int endOfLine = digitalRead(PROXIMITY_SENSOR);

  if (endOfLine == HIGH) {
    alreadySent = false;
    conveyorForward();
    return;
  }
  stopConveyor();

  if (alreadySent) return;

  int moistureValue = analogRead(MOISTURE_PIN);

  if (moistureValue < moistureThreshold) {
    Serial.println("Wet waste");
    binServo.write(openBinAngle);
    sendLora("WASTE:WET");
  } else {
    Serial.println("Dry waste");
    binServo.write(closedBinAngle);
    sendLora("WASTE:DRY");
  }

  alreadySent = true;
}