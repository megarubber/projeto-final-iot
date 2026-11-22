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
#define BUFFER_SIZE                 30

char txPacket[BUFFER_SIZE];
bool loraIdle = true;
static RadioEvents_t radioEvents;

#define MOTOR_CONVEYOR_A  5
#define MOTOR_CONVEYOR_B  6
#define MOISTURE_PIN      7   // YL-83 rain/moisture sensor
#define SERVO_PIN         10
#define PROXIMITY_SENSOR  9

const int moistureThreshold = 500;
const int openBinAngle      = 180;
const int closedBinAngle    = 0;

Servo binServo;
bool alreadySent = false;

void stopConveyor() {
  digitalWrite(MOTOR_CONVEYOR_A, LOW);
  digitalWrite(MOTOR_CONVEYOR_B, LOW);
}

void conveyorForward() {
  digitalWrite(MOTOR_CONVEYOR_A, HIGH);
  digitalWrite(MOTOR_CONVEYOR_B, LOW);
}

void conveyorBackward() {
  digitalWrite(MOTOR_CONVEYOR_A, LOW);
  digitalWrite(MOTOR_CONVEYOR_B, HIGH);
}

void onTxDone() {
  Serial.println("TX done");
  loraIdle = true;
}

void onTxTimeout() {
  Radio.Sleep();
  Serial.println("TX timeout");
  loraIdle = true;
}

void sendLora(const char* msg) {
  if (!loraIdle) return;
  snprintf(txPacket, BUFFER_SIZE, "%s", msg);
  Serial.printf("Sending: %s\r\n", txPacket);
  Radio.Send((uint8_t *)txPacket, strlen(txPacket));
  loraIdle = false;
}

void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_CONVEYOR_A, OUTPUT);
  pinMode(MOTOR_CONVEYOR_B, OUTPUT);
  pinMode(MOISTURE_PIN, INPUT);
  pinMode(PROXIMITY_SENSOR, INPUT);

  binServo.attach(SERVO_PIN);

  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  radioEvents.TxDone    = onTxDone;
  radioEvents.TxTimeout = onTxTimeout;
  Radio.Init(&radioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
}

void loop() {
  Radio.IrqProcess();
  delay(100);

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