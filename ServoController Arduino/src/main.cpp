#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

int x;
unsigned long last_time = 0;
int InBytes;
static char serial_in;
static int servo_angles[16];
int i = 0;
static int k[1];



const int SERVOMIN = 125;
const int SERVOMAX = 575;
const int SERVONUM = 16;

static Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

void setup() {
  Serial.begin(250000);
 
  pwm.begin();
  pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWM(0, 0, SERVOMIN);
}
int angleToPulse(int ang){
  int pulse = map(ang, 0, 180, SERVOMIN, SERVOMAX);
  return pulse;
}
int angleToPulseinv(int ang){
  int pulse = map(ang, 180, 0, SERVOMIN, SERVOMAX);
  return pulse;
}

void setAllServosAngle(){
{
  char buffer[32];
  
  for (int i = 0; i < 3; i ++)
  {
    pwm.setPWM(i+k,0,angleToPulse(servo_angles[i]));
  }
  if (k == 0){
    pwm.setPWM(3,0,angleToPulseinv(servo_angles[3]));
  }else{
    pwm.setPWM(3+k,0,angleToPulse(servo_angles[3+k]));
  }
}
    
}

void loop() {
  if(Serial.available()>0){
    serial_in =  Serial.read();
    switch (serial_in){
      case 'c':
            Serial.println("Arduino is CONNECTED!!");
            break;
      case 'k':
            //Serial.print("K found: ");
            k = Serial.parseInt();
            Serial.print(k);
      case 'a':
        for(int i = 0; i < 5;i++){
          servo_angles[i] = Serial.parseInt();
        }
    }
    setAllServosAngle();
}
}


