#include "IntanShield.h"
#include <SPI.h>
#include <Wire.h>

/**********************************************
 * User Settings
 **********************************************/
#define N_CHANNELS       12   // total amplifier channels
#define CH_PER_ISR       2    // channels read each interrupt
#define DESIRED_CH_RATE  1000 // 1 kHz per channel
#define TOTAL_SPS        (N_CHANNELS * DESIRED_CH_RATE) // 12 kS/s
#define ISR_RATE         (TOTAL_SPS / CH_PER_ISR)       // 6000 Hz

// Timer1 prescaler: we choose 8
// => Timer clock = F_CPU / 8 = 2 MHz
// => OCR1A = TimerClock / ISR_RATE

/**********************************************
 * Global ring buffer
 **********************************************/
#define RING_SIZE 54
volatile int16_t ringBuffer[RING_SIZE][N_CHANNELS];
volatile uint8_t head = 0, tail = 0;

// Temporary frame
volatile int16_t tempFrame[N_CHANNELS];
volatile uint8_t currentChannel = 0;

// For optional CPU usage monitoring
static const uint8_t CPU_USAGE_PIN = 2;

void setup()
{
  // Basic setup
  Serial.begin(250000);
  pinMode(CPU_USAGE_PIN, OUTPUT);
  digitalWrite(CPU_USAGE_PIN, LOW);

  SPI.begin();
  pinMode(chipSelectPin, OUTPUT);
  digitalWrite(chipSelectPin, HIGH);

  // ----- Configure RHD2216 (Write Registers, etc.) -----
  // This is truncated. In a full code, you'd do:
  //   1) SendWriteCommand(...) for R0..R13
  //   2) Power on channels 0..11 in R14..R15
  //   3) Calibrate();

  // Example snippet just powering on 0..11:
  {
    SendReadCommand(14);  SendReadCommand(14);
    uint8_t old14 = SendReadCommand(14);
    SendReadCommand(15);  SendReadCommand(15);
    uint8_t old15 = SendReadCommand(15);

    // channels 0..7 => 0xFF, channels 8..11 => 0x0F
    SendWriteCommand(14, old14 | 0xFF);
    SendWriteCommand(15, old15 | 0x0F);
  }
  // Calibrate();

  // ----- Timer1 Setup with Prescaler of 8 -----
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;

  // CTC Mode
  TCCR1B |= (1 << WGM12);

  // *** Prescaler = 8 ***
  //  Bits CS12:CS11:CS10 => 010 => /8
  //  Reference: Table 17-6 in ATmega328 datasheet
  TCCR1B |= (1 << CS11);   // sets prescaler=8

  // Timer clock = 16 MHz / 8 = 2 MHz
  // OCR1A = TimerClock / ISR_RATE
  uint32_t timerClock = F_CPU / 8;         // = 2,000,000
  uint32_t compareVal = timerClock / ISR_RATE; // e.g. 2e6 / 6000 ~ 333
  OCR1A = (uint16_t)(compareVal);

  // Enable compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  interrupts();
}

void loop()
{
  // Read ring buffer
  while (tail != head)
  {
    int16_t* frame = (int16_t*)ringBuffer[tail];
    for (uint8_t c = 0; c < N_CHANNELS; c++) {
      Serial.print(frame[c]);
      if (c < (N_CHANNELS - 1)) Serial.print(',');
    }
    Serial.println();
    tail = (tail + 1) % RING_SIZE;
  }
}

// ----- ISR: Two channels each pass -----
ISR(TIMER1_COMPA_vect)
{
  digitalWrite(CPU_USAGE_PIN, HIGH);

  // read channel currentChannel
  {
    uint16_t raw = SendConvertCommand(currentChannel);
    tempFrame[currentChannel] = (int16_t)raw;
    currentChannel++;
  }

  // read next channel
  if (currentChannel < N_CHANNELS) {
    uint16_t raw = SendConvertCommand(currentChannel);
    tempFrame[currentChannel] = (int16_t)raw;
    currentChannel++;
  }

  // if we've filled all 12 channels:
  if (currentChannel >= N_CHANNELS) {
    currentChannel = 0;

    // push to ring buffer
    uint8_t nextHead = (head + 1) % RING_SIZE;
    if (nextHead == tail) {
      // buffer is full => discard oldest
      tail = (tail + 1) % RING_SIZE;
    }
    for (uint8_t i = 0; i < N_CHANNELS; i++) {
      ringBuffer[head][i] = tempFrame[i];
    }
    head = nextHead;
  }

  digitalWrite(CPU_USAGE_PIN, LOW);
}
