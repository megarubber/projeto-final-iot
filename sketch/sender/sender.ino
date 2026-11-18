#include <ESP32Servo.h>
#include <LoRaWan_APP.h>

#define RF_FREQUENCY 915000000
#define TX_OUTPUT_POWER 5
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

double txNumber;

bool lora_idle=true;

static RadioEvents_t RadioEvents;

#define MOTOR_CONVEYOR_A 5
#define MOTOR_CONVEYOR_B 6
#define YL83_PIN 7 
#define SERVO_PIN 10
#define PROXIMITY_SENSOR 9

const int thresholdValue = 500;
const int openTrashAngle = 180;

Servo mg996r;

void stopMotor() {
  digitalWrite(MOTOR_CONVEYOR_A, LOW);
  digitalWrite(MOTOR_CONVEYOR_B, LOW);
}

void clockwise() {
  digitalWrite(MOTOR_CONVEYOR_A, HIGH);
  digitalWrite(MOTOR_CONVEYOR_B, LOW);
}

void counterclockwise() {
  digitalWrite(MOTOR_CONVEYOR_A, HIGH);
  digitalWrite(MOTOR_CONVEYOR_B, LOW);
}

void OnTxDone(void) {
	Serial.println("TX done......");
	lora_idle = true;
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.println("TX Timeout......");
  lora_idle = true;
}

void setup() {
  Serial.begin(9600);
  pinMode(MOTOR_CONVEYOR_A, OUTPUT);
  pinMode(MOTOR_CONVEYOR_B, OUTPUT);
  pinMode(YL83_PIN, INPUT);
  pinMode(PROXIMITY_SENSOR, INPUT); // INPUT_PULLUP for NPN sensors
  mg996r.attach(SERVO_PIN);

  Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);

  txNumber = 0;

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(
    MODEM_LORA, 
    TX_OUTPUT_POWER,
    0,
    LORA_BANDWIDTH,
    LORA_SPREADING_FACTOR, 
    LORA_CODINGRATE,
    LORA_PREAMBLE_LENGTH, 
    LORA_FIX_LENGTH_PAYLOAD_ON,
    true, 
    0, 
    0, 
    LORA_IQ_INVERSION_ON, 3000
  );
}

void loop() {
  delay(1000);
  int endOfLine = digitalRead(PROXIMITY_SENSOR);
  if (endOfLine == HIGH) {
    clockwise();
    return;
  }

  stopMotor();

  int yl83value = analogRead(YL83_PIN);
  if (yl83value < thresholdValue) {
    Serial.println("It's Wet");
    mg996r.write(openTrashAngle);
  } else {
    Serial.println("It's Dry");
    mg996r.write(-openTrashAngle);
  }
}
