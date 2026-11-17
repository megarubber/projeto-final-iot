#include <Servo.h>

#define MOTOR_CONVEYOR_A 5
#define MOTOR_CONVEYOR_B 6
#define YL83_PIN 7 
#define SERVO_PIN 10
#define PROXIMITY_SENSORS 9

const int thresholdValue = 500;
const openTrashAngle = 180;

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

void setup() {
  Serial.begin(9600);
  pinMode(MOTOR_CONVEYOR_A, OUTPUT);
  pinMode(MOTOR_CONVEYOR_B, OUTPUT);
  pinMode(YL83_PIN, INPUT);
  pinMode(PROXIMITY_SENSORS, INPUT); // INPUT_PULLUP for NPN sensors
  mg996r.attach(SERVO_PIN);
}

void loop() {
  delay(1000);
  int endOfLine = digitalRead();
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
