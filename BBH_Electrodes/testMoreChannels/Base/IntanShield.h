#include "sound.h"
#include <SPI.h>
#include <Wire.h>

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

void StartSound(unsigned char const *data, int length);
void StopSound();
void PlayNoteA();
void PlayNoteE();
uint16_t SendReadCommand(uint8_t regnum);
uint16_t SendConvertCommand(uint8_t channelnum);
uint16_t SendConvertCommandH(uint8_t channelnum);
uint16_t SendWriteCommand(uint8_t regnum, uint8_t data);
uint8_t ScaleForDAC(int rawdata);
uint8_t ScaleForDAC_ACC(int rawdata);
void Calibrate();
int ReadChannelData(uint8_t channelnum);
long ReadAccumulatorData(uint8_t channelnum);
void NotchFilter60();
void NotchFilter50();
void NotchFilterNone();
void SetAmpPwr(bool Ch1, bool Ch2);
bool LowGainMode();
long ReadThreshold();

const int chipSelectPin = 10;
const byte PS_128 = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
const byte PS_16 = (1 << ADPS2);
#define FIRSTCHANNEL 10
#define SECONDCHANNEL 11
#define DAC_SCALE_COEFFICIENT 12
