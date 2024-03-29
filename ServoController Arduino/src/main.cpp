#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "CommandHandler.h"

int x;
unsigned long last_time = 0;
int InBytes;
static char serial_in;
static int servo_angles[16];
int i = 0;
static int k[1];

CommandHandler<10, 90, 15> SerialCommandHandler;


const int SERVOMIN = 125;
const int SERVOMAX = 575;
const int SERVONUM = 16;

static Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

void updateAllServosAngle(CommandParameter &parameters){

    int ang0 = parameters.NextParameterAsInteger();
    servo_angles[0] = ang0;
    int ang1 = parameters.NextParameterAsInteger();
    servo_angles[1] = ang1;
    int ang2 = parameters.NextParameterAsInteger();
    servo_angles[2] = ang2;
    int ang3 = parameters.NextParameterAsInteger();
    servo_angles[3] = ang3;
    int ang4 = parameters.NextParameterAsInteger();
    servo_angles[4] = ang4;
    int ang5 = parameters.NextParameterAsInteger();
    servo_angles[5] = ang5;
    int ang6 = parameters.NextParameterAsInteger();
    servo_angles[6] = ang6;
    int ang7 = parameters.NextParameterAsInteger();
    servo_angles[7] = ang7;
    int ang8 = parameters.NextParameterAsInteger();
    servo_angles[8] = ang8;
    int ang9 = parameters.NextParameterAsInteger();
    servo_angles[9] = ang9;
    int ang10 = parameters.NextParameterAsInteger();
    servo_angles[10] = ang10;
    int ang11 = parameters.NextParameterAsInteger();
    servo_angles[11] = ang11;
    int ang12 = parameters.NextParameterAsInteger();
    servo_angles[12] = ang12;
    int ang13 = parameters.NextParameterAsInteger();
    servo_angles[13] = ang13;
    int ang14 = parameters.NextParameterAsInteger();
    servo_angles[14] = ang14;
    int ang15 = parameters.NextParameterAsInteger();
    servo_angles[15] = ang15;
    
}

void updateMode (CommandParameter &parameters){

  int k_val = parameters.NextParameterAsInteger();
  Serial.print(k_val);
  k[0] = k_val;

}


void conn(CommandParameter &parameters){

    Serial.print(F("Connection confirmed and Motors Started!")); // When Connected Return to Python initiating the Motors
   
    
}

void setup() {
  Serial.begin(250000);
 
  pwm.begin();
  pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWM(0, 0, SERVOMIN);


  SerialCommandHandler.AddCommand(F("connect"), conn);
  SerialCommandHandler.AddCommand(F("UA"), updateAllServosAngle);
  SerialCommandHandler.AddCommand(F("UK"), updateMode);


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

  for (int i = 0; i < 3; i ++)
  {
    pwm.setPWM(i+k[0],0,angleToPulse(servo_angles[i]));
  }
  if (k == 0){
    pwm.setPWM(3,0,angleToPulseinv(servo_angles[3]));
  }else{
    pwm.setPWM(3+k[0],0,angleToPulse(servo_angles[3+k[0]]));
  }
}
    
}

void loop() {
  SerialCommandHandler.Process();
  setAllServosAngle();
}



