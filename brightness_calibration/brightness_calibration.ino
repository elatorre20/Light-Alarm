//light
#define LAMP_PIN 5 //lamp FET pin
uint8_t brightness= 0; //lamp brightness
uint8_t alarm_brightness = 0;

void setup() {
  // put your setup code here, to run once:
  pinMode(LAMP_PIN, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  while(!Serial.available()){};
  brightness = Serial.readString().toInt(); //get brightness from serial
  analogWrite(LAMP_PIN, brightness); //apply lamp brightness
}
