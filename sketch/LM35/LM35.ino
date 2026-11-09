const float LSB = 0.00488;
const float conv = 0.488;
void setup(){
Serial.begin(9600);
}
void loop(){
int adc = analogRead(0);
float V = adc*LSB;
float T = 100*V - 50;
//float T = conv*adc;
Serial.print("Valor lido: ");
Serial.println(adc);
Serial.print("Tensão: ");
Serial.println(V);
Serial.print("Temperatura: ");
Serial.println(T);
delay(100);
}
