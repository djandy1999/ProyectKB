#include <SPI.h>
#include "mbed.h"
#include "rtos.h"
#include "events/EventQueue.h"  // For Ticker and EventQueue (from the Portenta Arduino core)
#include <Wire.h>
#include "stm32h7xx.h"  // STM32 registers

// Define number of channels to sample (0 through 11)
#define NUM_CHANNELS 10

const int chipSelectPin = PIN_SPI_SS;

// Replace 2-channel arrays with 12-channel arrays
volatile int16_t channel_data[NUM_CHANNELS] = { 0 };
volatile int16_t final_channel_data[NUM_CHANNELS] = { 0 };

// Forward declarations of your SPI commands
uint16_t SendConvertCommand(uint8_t channelnum);

// Create an EventQueue and a Ticker (from Mbed OS)
events::EventQueue queue(32 * EVENTS_EVENT_SIZE);
mbed::Ticker sampleTicker;

//================================================================
// Sampling task: Called from the event thread (not in interrupt context)
//================================================================
void spiSampleTask() {
    // Loop through each channel and sample it.
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
        channel_data[ch] = SendConvertCommand(ch);
    }
    // Optionally, you might add filtering here
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
        final_channel_data[ch] = channel_data[ch];
    }
}

//================================================================
// SPI command functions (unchanged)
//================================================================
uint16_t SendReadCommand(uint8_t regnum) {
  uint16_t mask = regnum << 8;
  mask = 0b1100000000000000 | mask;
  digitalWrite(chipSelectPin, LOW);
  SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE0));
  uint16_t out = SPI.transfer16(mask);
  SPI.endTransaction();
  digitalWrite(chipSelectPin, HIGH);
  return out;
}

uint16_t SendConvertCommandH(uint8_t channelnum) {
  uint16_t mask = channelnum << 8;
  mask = 0b0000000000000001 | mask;
  digitalWrite(chipSelectPin, LOW);
  SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE0));
  uint16_t out = SPI.transfer16(mask);
  SPI.endTransaction();
  digitalWrite(chipSelectPin, HIGH);
  return out;
}

uint16_t SendWriteCommand(uint8_t regnum, uint8_t data) {
  uint16_t mask = regnum << 8;
  mask = 0b1000000000000000 | mask | data;
  digitalWrite(chipSelectPin, LOW);
  SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE0));
  uint16_t out = SPI.transfer16(mask);
  SPI.endTransaction();
  digitalWrite(chipSelectPin, HIGH);
  return out;
}

void Calibrate() {
  digitalWrite(chipSelectPin, LOW);
  SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE0));
  SPI.transfer16(0b0101010100000000);
  SPI.endTransaction();
  digitalWrite(chipSelectPin, HIGH);
  for (int i = 0; i < 9; i++) {
    SendReadCommand(40);
  }
}

//================================================================
// Timer callback: posts the sampling task to the event queue.
//================================================================
void timerCallback() {
    queue.call(spiSampleTask);
}

//================================================================
// New function to power all 12 channels
//
// The original SetAmpPwr() function only toggled two channels (via FIRSTCHANNEL and SECONDCHANNEL).
// Here we read registers 14 and 15 (which hold bits for channels 0–7 and 8–15 respectively) and then
// set the appropriate bits for channels 0 to NUM_CHANNELS-1.
//================================================================
void SetAllAmpPwr() {
    uint8_t previousreg14, previousreg15;
    
    // Read the current settings from registers 14 and 15.
    SendReadCommand(14);
    SendReadCommand(14);
    previousreg14 = SendReadCommand(14);
    SendReadCommand(15);
    SendReadCommand(15);
    previousreg15 = SendReadCommand(15);
    
    // For channels 0-7, set the corresponding bit in register 14.
    for (uint8_t ch = 0; ch < 8; ch++) {
        previousreg14 |= (1 << ch);
    }
    // For channels 8-11 (since NUM_CHANNELS==12), set the appropriate bits in register 15.
    for (uint8_t ch = 8; ch < NUM_CHANNELS; ch++) {
        previousreg15 |= (1 << (ch - 8));
    }
    
    SendWriteCommand(14, previousreg14);
    SendWriteCommand(15, previousreg15);
}

//================================================================
// CHIP Timer setup and register initialization
//
// (Most of the register configuration remains unchanged. The key changes are:
//  1. Powering on channels 0–11 using SetAllAmpPwr(),
//  2. Calling SendConvertCommandH() and SendConvertCommand() for all channels.)
//================================================================
void setupCHIP_Timer() {
  // Example initialization of registers 0 to 11 and bandwidth settings.
  SendWriteCommand(0, 0b11011110);
  SendWriteCommand(1, 0b00100000);
  SendWriteCommand(2, 0b00101000);
  SendWriteCommand(3, 0b00000000);
  SendWriteCommand(4, 0b11011000);
  SendWriteCommand(5, 0b00000000);
  SendWriteCommand(6, 0b00000000);
  SendWriteCommand(7, 0b00000000);
  SendWriteCommand(8, 30);
  SendWriteCommand(9, 5);
  SendWriteCommand(10, 43);
  SendWriteCommand(11, 6);
  
  // Bandwidth low-frequency settings (example for LowCutoff10Hz)
  uint8_t RL = 0, RLDAC1 = 5;
  uint8_t ADCaux3en = 0, RLDAC3 = 0, RLDAC2 = 1;
  uint8_t R12 = ((RL << 7) | RLDAC1);
  uint8_t R13 = (ADCaux3en << 7) | (RLDAC3 << 6) | RLDAC2;
  SendWriteCommand(12, R12);
  SendWriteCommand(13, R13);

  // Turn off all amplifier channels first.
  SendWriteCommand(14, 0b00000000);
  SendWriteCommand(15, 0b00000000);

  // Power on channels 0 through 11.
  SetAllAmpPwr();

  // Calibrate the chip.
  Calibrate();

  // Reset the DSP high-pass filter offset for each channel.
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
      SendConvertCommandH(ch);
  }
  // Send an initial convert command for each channel.
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
      SendConvertCommand(ch);
  }

  // Optional: initialize I2C interface for DAC or other peripherals.
  Wire.begin();
  Wire.beginTransmission(56);
  Wire.write(0b11110000);
  Wire.write(0b00001100);
  Wire.endTransmission();
}

//================================================================
// SPI Test: unchanged
//================================================================
void testSPIConnection() {
  Serial.println("Starting SPI connection test...");  
  SPI.begin();
  SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE0));
  delay(250);
  digitalWrite(chipSelectPin, LOW);
  uint8_t testByte = 0xAA;
  uint8_t response = SPI.transfer(testByte);
  digitalWrite(chipSelectPin, HIGH);
  Serial.print("SPI Test: Sent 0x");
  Serial.print(testByte, HEX);
  Serial.print(", Received 0x");
  Serial.println(response, HEX);
}

//================================================================
// I2C Scanner: unchanged
//================================================================
void scanI2C() {
  byte error, address;
  int count = 0;
  Serial.println("Scanning for I2C devices...");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      count++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (count == 0) {
    Serial.println("No I2C devices found.");
  }
  Serial.println("I2C scan complete.");
}

//================================================================
// Setup function: initialize SPI, I2C, timer, etc.
//================================================================
void setup() {
    Serial.begin(250000);
    while (!Serial) {}  // Wait for Serial to initialize
    Serial.println("Starting simplified connection test...");
    testSPIConnection();
    Wire.begin();
    delay(100);
    scanI2C();
    setupCHIP_Timer();

    // Create thread for the event queue
    static rtos::Thread eventThread(osPriorityHigh, 4096); // 4KB stack
    eventThread.start(callback(&queue, &events::EventQueue::dispatch_forever));
    Serial.println("Event thread started");

    // Set the sampling ticker to trigger at 1/12000 second intervals.
    sampleTicker.attach(timerCallback, 1.0/10000.0);
}

//================================================================
// Main loop: print out all channel values.
//================================================================
void loop() {
    for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
        Serial.print(final_channel_data[ch]);
        if (ch < NUM_CHANNELS - 1)
            Serial.print(", ");
    }
    Serial.println();
    delay(1);  // Adjust delay as needed.
}

//================================================================
// SendConvertCommand: unchanged except that it now supports channelnum from 0 to 11.
//================================================================
uint16_t SendConvertCommand(uint8_t channelnum) {
  uint16_t mask = channelnum << 8;
  // No extra bit is set for a basic conversion command.
  digitalWrite(chipSelectPin, LOW);
  SPI.beginTransaction(SPISettings(24000000, MSBFIRST, SPI_MODE0));
  uint16_t out = SPI.transfer16(mask);
  SPI.endTransaction();
  digitalWrite(chipSelectPin, HIGH);
  return out;
}
