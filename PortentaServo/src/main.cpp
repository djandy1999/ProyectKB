#include <Arduino.h>
#include "RPC.h"
#include "CommandHandler.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <QueueArray.h>

// Buffer size
#define BUFFER_SIZE 50

// Data array size
#define DATA_ARRAY_SIZE 8

// Declare the buffer as a queue of int arrays
QueueArray<float*> buffer;
float* sensor_data = new float[DATA_ARRAY_SIZE]; 
bool rBool = true;

const int dataSize = 8;
float mean[dataSize] = {1991.033102, 979.100292, 1384.605909, 690.083826, 202.240331, 50218.089443, 303.198568, 152.770388};
float stdDev[dataSize] = {1158.654284, 1460.097603, 806.427103, 1100.119750, 167.640898,268559.217730,386.393986, 135.929749};
unsigned long last_time = 0;
bool hb = false;
int i = 0;
static char RPC_in;
float g;
int glob_a;
String valuesString = "";

const int g6[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,50};
const int g7[16] = {180,180,0,180,180,0,180,180,0,180,180,0,180,180,0,0};

CommandHandler<10, 90, 15> SerialCommandHandler;


const int SERVOMIN = 125;
const int SERVOMAX = 600;
const int SERVONUM = 16;
//static int servoAngles[SERVONUM];
static Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

const int m_windowsize = 5; // define the window size
int inputValues[m_windowsize]; // create an array to store the values in the window
int bufferIndex = 0; // set the current index to 0
int bufferCount = 0;

int angleToPulse(int ang){
  int pulse = map(ang, 0, 190, SERVOMIN, SERVOMAX);
  return pulse;
}
int angleToPulseCMC(int ang){
  int pulse = map(ang, 70, 50, 250, 500);
  return pulse;
}

int angleToPulseinv(int ang){
  int pulse = map(ang, 190, 0, SERVOMIN, SERVOMAX);
  return pulse;
}

int getMajorityVote(int values[], int size) {
  int count = 0;
  int candidate = -1;

  for (int i = 0; i < size; i++) {
    if (count == 0) {
      candidate = values[i];
      count = 1;
    } else {
      count += (values[i] == candidate) ? 1 : -1;
    }
  }

  count = 0;
  for (int i = 0; i < size; i++) {
    if (values[i] == candidate) {
      count++;
    }
  }

  if (count > size / 2) {
    return candidate;
  } else {
    return -1;
  }
}

void gesturalUpdate(const int gestureArray[]){

  pwm.setPWM(0,0,angleToPulseinv(gestureArray[0]));
  for(int x = 1; x < 14; x++){
    pwm.setPWM(x,0,angleToPulse(gestureArray[x]));
  }
  pwm.setPWM(15,0,angleToPulseinv(gestureArray[15]));
}

 void addSample(int sample) {
   inputValues[bufferIndex] = sample;
   bufferIndex = (bufferIndex + 1) % m_windowsize;
   if (bufferCount < m_windowsize) {
     bufferCount++;
   }
 }

 int majorityVote() {
   int count[8] = {0}; // Initialize count array, with indices ranging from 0 to 7 (we will not use index 0)
   for (int i = 0; i < bufferCount; i++) {
     count[inputValues[i]]++;
   }

   int majority = 1;
   for (int i = 2; i <= 7; i++) {
     if (count[i] > count[majority]) {
       majority = i;
     }
   }
   return majority;
 }


int setVar(int a) {
  // glob_a = (int)a;
   
  addSample(a);
  int majority = majorityVote();
  Serial.print(majority);
   if(a == 6){
     gesturalUpdate(g6);
   }  
   if(a == 7){
     gesturalUpdate(g7);
  }


  return a;
}



void modelPredict(CommandParameter &parameters){  

        if (!buffer.isEmpty()) {
           float* dequeuedArray = buffer.dequeue(); 
          //  char buffer_s1[10];
          //   snprintf(buffer_s1, sizeof(buffer_s1), "%.0f;", (dequeuedArray[1]*stdDev[2]) + mean[2]); 
          //  Serial.print(buffer_s1);        
        for (int i = 0; i < 8; i++) {
          char buffer_s[10];
          snprintf(buffer_s, sizeof(buffer_s), "%.5f", dequeuedArray[i]); // convert float to string with 2 decimal places
          valuesString += buffer_s;
          if (i < 7) {
            valuesString += ","; // add comma between values (except for the last one)
          }
          }
        //Serial.print("e"+ valuesString);
        RPC.print("e"+ valuesString );
        valuesString = "";
        delete[] dequeuedArray; // free the memory
        }     
        float e1 = parameters.NextParameterAsDouble();
      sensor_data[0] = (e1 - mean[0]) / stdDev[0];
        float e2 = parameters.NextParameterAsDouble();
      sensor_data[4] = (e2 - mean[1]) / stdDev[1];
        float e3 = parameters.NextParameterAsDouble();
      sensor_data[1] = (e3 - mean[2]) / stdDev[2];
        float e4 = parameters.NextParameterAsDouble();
      sensor_data[5] = (e4 - mean[3]) / stdDev[3];
        float e5 = parameters.NextParameterAsDouble();
      sensor_data[2] = (e5 - mean[4]) / stdDev[4];
        float e6 = parameters.NextParameterAsDouble();
      sensor_data[6] = (e6 - mean[5]) / stdDev[5];
        float e7 = parameters.NextParameterAsDouble();
      sensor_data[3] = (e7 - mean[6]) / stdDev[6];
        float e8 = parameters.NextParameterAsDouble();
      sensor_data[7] = (e8 - mean[7]) / stdDev[7];
      //Serial.print((e1 - mean[0]) / stdDev[0]);



    // Check if the buffer is full
  if (buffer.count() >= BUFFER_SIZE) {
    float* oldestData = buffer.dequeue(); // Dequeue the oldest data
    delete[] oldestData;               // Free the memory
  }
  
  //float sensor_data[8] = {1335.0, 399.0, 1377.0, 133.0, 65.0, 2532943.0, 253.0, 149.0};

  buffer.enqueue(sensor_data);
}



void Cmd_Unknown()
{
  Serial.println(F("Servo_Board(UnKown) \r"));
}

void conn(CommandParameter &parameters){
  Serial.println(F("M7 Succesfully Conected")); 
  RPC.print("c");  
}

void heartbeat(CommandParameter &parameters){

  i = 0;
  hb = true;
  
  }

void Disconn(CommandParameter &parameters){
  Serial.println(F("Succesfully Disconected")); 
  Serial.end();
}
  
  
void BBHIdentity(CommandParameter &parameters){
  Serial.println(F("Servo_Board_Portenta \r")); 
  }

void UpdateDeg(CommandParameter &parameters){

    int ang0 = parameters.NextParameterAsInteger();
    pwm.setPWM(0,0,angleToPulseinv(ang0));
    int ang1 = parameters.NextParameterAsInteger();
    pwm.setPWM(1,0,angleToPulse(ang1));
    int ang2 = parameters.NextParameterAsInteger();
    pwm.setPWM(2,0,angleToPulse(ang2));
    int ang3 = parameters.NextParameterAsInteger();
    pwm.setPWM(3,0,angleToPulse(ang3));
    int ang4 = parameters.NextParameterAsInteger();
    pwm.setPWM(4,0,angleToPulse(ang4));
    int ang5 = parameters.NextParameterAsInteger();
    pwm.setPWM(5,0,angleToPulse(ang5));
    int ang6 = parameters.NextParameterAsInteger();
    pwm.setPWM(6,0,angleToPulse(ang6));
    int ang7 = parameters.NextParameterAsInteger();
    pwm.setPWM(7,0,angleToPulse(ang7));
    int ang8 = parameters.NextParameterAsInteger();
    pwm.setPWM(8,0,angleToPulse(ang8));
    int ang9 = parameters.NextParameterAsInteger();
    pwm.setPWM(9,0,angleToPulse(ang9));
    int ang10 = parameters.NextParameterAsInteger();
    pwm.setPWM(10,0,angleToPulse(ang10));
    int ang11 = parameters.NextParameterAsInteger();
    pwm.setPWM(11,0,angleToPulse(ang11));
    int ang12 = parameters.NextParameterAsInteger();
    pwm.setPWM(12,0,angleToPulse(ang12));
    int ang13 = parameters.NextParameterAsInteger();
    pwm.setPWM(13,0,angleToPulse(ang13));
    int ang14 = parameters.NextParameterAsInteger();
    pwm.setPWM(14,0,angleToPulse(ang14));
    int ang15 = parameters.NextParameterAsInteger();
    pwm.setPWM(15,0,angleToPulseCMC(ang15));
    //Serial.println("g"+String(ang0));
    
}

void setup() {
  Serial.begin(250000);
  bootM4();
  RPC.begin();
  
  
  while (!Serial) {}

  for (int i = 0; i < m_windowsize; i++) {
    inputValues[i] = 0;
  }


  pwm.begin();
  pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWM(0, 0, SERVOMIN);
   
  SerialCommandHandler.AddCommand(F("UD"), UpdateDeg);
  SerialCommandHandler.AddCommand(F("identity"), BBHIdentity);
  SerialCommandHandler.AddCommand(F("connected"), conn);
  SerialCommandHandler.AddCommand(F("DC"), Disconn);
  SerialCommandHandler.AddCommand(F("heartbeat"), heartbeat);
  SerialCommandHandler.AddCommand(F("UG"), modelPredict);
  
  SerialCommandHandler.SetDefaultHandler(Cmd_Unknown);
  
}

// TO DO: SIMULATE OUTPUT ON UNITY, SEND DATA TO ARDUINO WITH e prefix, store the data in a 5 windowed buffer and send it to the M4 and only deque when an answer is received
void loop()
{  
  SerialCommandHandler.Process();
  while(RPC.available()){
    RPC_in = RPC.read();
    if(isDigit(RPC_in)){
      int receivedNumber = RPC_in - '0';
      //Serial.write(RPC_in);
      setVar(receivedNumber);
    }else{
      Serial.write(RPC_in);
    }
    
  }

  
 
  if (millis() > last_time + 2000 && hb == true && i < 10)
      {
          Serial.println("Arduino is alive!!");
          i++;
          last_time = millis();
          
      }


}
