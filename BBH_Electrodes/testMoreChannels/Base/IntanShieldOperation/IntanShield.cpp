#include "IntanShield.h"

/* Global variables */

int speakerPin = 3; //Output pin which drives speaker with PWM - pin 3
unsigned char const *sounddata_data = 0; //Pointer to the raw data of the sound
int sounddata_length = 0; //Keep track of the length of the sound sample sequence
volatile uint16_t sample = 0; //Global counter variable to cycle through the sound, sample by sample
byte lastSample; //Last sample to be played - treated specially because the sound needs to decrease gradually afterwards to avoid an ugly pop as soon as the sample is finished

enum Notchselect { Notch60, Notch50, NoNotch };
Notchselect notch_setting; //notch_setting has 3 possible values. It is set by Analog Pins 2 and 3 (aka Digital Pins 16 and 17) and updated every 0.5 ms

volatile bool audio_enable; //audio_enable is either true or false. It is set by Digital Pin 6 and updated every 0.5 ms

volatile bool low_gain_mode; //low_gain_mode is either true or false. It is set by Digital Pin 7 and updated every 0.5 ms
                             //When enabled, scale channel data down by a factor of 4. By default, channel data begins to clip when input voltages are 1.1 mV peak-to-peak. When low_gain_mode is enabled, this increases to 4.4 mV peak-to-peak
                             //Should be enabled when signals appear too strong and are clipping (more common with EKG than EMG). Should be disabled when signals appear too weak and are hard to see
                    
volatile bool sampleflag = false; //Keep track of whether or not a sample is being played through the speaker

volatile long accumulator[2] = {0L, 0L}; //Add up the amplitudes of the signal over a short period to determine how strong the signal is
volatile long amplitude[2] = {0, 0}; //Determine the amplitude of a signal, sample by sample, to determine how strong the signal is
volatile long ampmax[2] = {0, 0}; //Keep track of the maximum amplitude of every accumulation period - initialized to the lowest possible 8-bit value so the first read data will become the max
volatile long ampmin[2] = {255, 255}; //Keep track of the minimum amplitude of every accumulation period - initialized to the highest possible 8-bit value so the first read data will becom the min
volatile long finalaccumulator[2] = {0L, 0L}; //Retain the final values of the accumulator, allowing them to be output even while a new accumulator period is beginning

volatile long high_threshold; //Set the high threshold of each channel - how strong must a signal be to trigger a sound - set by potentiometer

volatile bool reset[2] = {true, true}; //Keep track of recent history of signal strength.
                                       //If a signal was recently strong enough to trigger a sound or digital out pulse, reset = false
                                       //Strong signals thereafter will not trigger new sounds or pulses (otherwise one muscle contraction could trigger dozens or hundreds of sounds),
                                       //until the signal becomes weak enough to fall below the low threshold, signifying the end of a muscle contraction. At this point, reset = true

volatile int i = 0; //Keep track of which channel's turn it is to be sampled. The ISR runs every 0.5 ms, and we sample 1 channel per ISR
                    //Since we sample 2 channels total, each channel is sampled every 1 ms, giving us our sample rate of 1 kHz

volatile int16_t channel_data[2] = {0, 0}; //Store the 16-bit channel data from two channels of the RHD
volatile int16_t final_channel_data[2] = {0, 0}; //Store the final 8-bit version of the channel data
volatile float in[2][3] = {{0}}; //Keep track of past values of the original signal, which helps the notch filter remove 50 Hz or 60 Hz noise
volatile float out[2][3] = {{0}}; //Keep track of the past values of the processed signal, which helps the notch filter remove 50 Hz or 60 Hz noise
volatile bool firstchannelpower; //Adapt FirstChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool secondchannelpower; //Adapt SecondChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR


/* ISR is the Interrupt Service Routine, a function triggered by a hardware interrupt every 1/2000th of a second (2 kHz).
 *  This 500 microsecond value is controlled by the register OCR1A, which is set in the setup() function and can be altered to give different frequencies
 *  
 *  The ISR has two main purposes:
 *    a) Update the PWM output that's being sent to the speaker to reflect the sound sample for that 1/2000th of a second
 *    b) Update the channel_data array with a new sample for that 1/2000th of a second. Since we have 2 channels and can only update 1 per iteration, each individual channel is only updated once every 1/1000th of a second, giving an effective sampling rate of 1 kHz
 *    
 *                          Warning: It is not recommended to set the ISR to run faster than 2 kHz
 *  It takes less than 100 microseconds to complete one iteration with no serial data being sent.
 *  However, it takes ~240 microseconds to complete one iteration with an 8-bit number being sent to the computer through the serial port.
 *  Much of this program's code assumes the default sampling rate of 1 kHz - while it is possible to accommodate a different sampling rate,
 *  everything from audio data length to DAC I2C speed to inclusion of Serial communications should be adjusted.
 *  
 *  Above all, if the ISR takes longer than the interrupt period (by default 500 microseconds), then the timer is attempting to trigger an interrupt before the previous ISR is finished.
 *  This causes unreliable execution of the ISR as well as unreliable timer period, compromising the integrity of the read channel data.
 *  If you change the interrupt period, you must ensure the ISR has time to fully execute before the next ISR begins.
 *  It is recommended to monitor the duty cycle and frequency on Digital Pin 2 for this purpose (Header pin B3 on the shield, illustrated in Figure 3 of the User Manual) - this pin is HIGH while an ISR is executing and LOW otherwise
 * 
 *  If a faster channel sampling rate is needed, you can:
 *    a) Remove a channel from the ISR. For example, you can collect data from channel 0 and ignore channel 16.
 *        Change the maximum value of the variable i to 1 instead of 2, and change the SendConvertCommand to only read from channel 0.
 *        The ISR will still execute every 500 microseconds, but only has 1 channel to cycle through, so the channel will be updated every 500 microseconds instead of 1 milliseconds, giving an effective sampling rate of 2 kHz.
 *        You should also change the loop() function to send to one DAC twice per loop, rather than each of the two DACs once per loop since the number of DACs should reflect the number of channels and twice as much data is coming in.
 *        
 *    b) Delete or comment out all Serial.println statements. These are helpful for monitoring the data from a single channel on the computer, but are slow and can be removed if not needed.
 *        Even at the max Baud rate of 250 kHz, these statements take up the majority of the ISR's time when they are active.
 *        Removing these would allow you to manually shorten the timer before the ISR repeats.
 *        This period is controlled by the register OCR1A, which is set in the setup() function.
 *        
 *    b) Remove the processing-time intensive DSP notch filter by disabling the notchfilter through the DIP switch.
 *        50 or 60 Hz noise from power lines will no longer be filtered.
 *        This reduces the amount of time the ISR needs to complete, allowing you to manually shorten the timer before the ISR repeats.
 *        This period is controlled by the register OCR1A, which is set in the setup() function.
 *        
 *   NOTE:
 *        Make sure not to add any more processor-intensive code to the ISR or the ISR may take more time to execute than it is allotted (500 ms by default).   
 */
 
ISR(TIMER1_COMPA_vect) {

    //Every time Timer1 matches the value set in OCR1A (every 500 microseconds, or at a rate of 2 kHz)

    
	digitalWrite(2, HIGH); //Optional but helpful - monitor how much time each interrupt cycle takes compared to the amount it is allotted by monitoring the duty cycle from pin 2
  
	//Update notch_setting in case the user changed the state of the DIP switch
	if (digitalRead(16) == LOW) {
		notch_setting = NoNotch;
	}
	else {
		if (digitalRead(17) == HIGH) {
			notch_setting = Notch60; 
		}
		else {
			notch_setting = Notch50;
		}
	}

	//Update audio_enable in case the user changed the state of the DIP switch
	if (digitalRead(6) == HIGH)
		audio_enable = true;
	else
		audio_enable = false;

	//Update low_gain_mode in case the user changed the state of the DIP switch
	if (digitalRead(7) == HIGH)
		low_gain_mode = true;
	else
		low_gain_mode = false;
  
	static int loopcounter = 0; //Variable to keep track of how many times the ISR has been run since beginning of the accumulation period

	/* Handling audio playback */
	//If audio sample is about to stop playing
	if (sample >= sounddata_length && sampleflag == true) {
		if (sample == sounddata_length + lastSample) {
			StopSound();
			sampleflag = false;
		}
		else {
		//Ramp down to zero to reduce the click at the end of playback.
		OCR2B = sounddata_length + lastSample - sample;
		}
	}
	
	//If audio sample is not about to stop playing
	else {
	  //If audio sample is in the middle of playing
		if (sampleflag == true) {
			OCR2B = pgm_read_byte(&sounddata_data[sample]);
		}
	}

    /* Reading channel data from the RHD, and filtering/formatting the data */
	static int testcount = 0;
	
	//Read the channel data from whichever sample in the pipeline corresponds to this 1/2000th of a second
    if (i == 0) {
		channel_data[0] = SendConvertCommand(FIRSTCHANNEL);
		if (!firstchannelpower)
			channel_data[0] = 0;
	}

	if (i == 1) {
		channel_data[1] = SendConvertCommand(SECONDCHANNEL);
		if (!secondchannelpower)
			channel_data[1] = 0;
	}
	

    //Notch filter calculation - if no notch filter is desired, NotchFilterNone should still be called to scale the channel data to values suitable for an 8-bit DAC
    //Scaling is performed to give a total gain from electrode signal to output voltage of 5,000 for the Arduino UNO, and 3,300 for the Arduino DUE
    //To decrease this gain by a factor of 4 (useful for larger signals), enable low_gain_mode. If this is not done, larger signals will be clipped at the maximum value of the DAC (5 V for UNO, 3.3 V for DUE) 
    //To further customize this gain value, see the comments within NotchFilter60(), NotchFilter50(), or NotchFilterNone() regarding the selection of DAC_SCALE_COEFFICIENT
    switch(notch_setting) {
		case Notch60:
			NotchFilter60();
			break;
		case Notch50:
			NotchFilter50();
			break;
		case NoNotch:
			NotchFilterNone();
			break;
    }
	
	final_channel_data[i] = channel_data[i]; //Save channel data into a variable that is not directly manipulated by the NotchFilter functions, so that it can be output at any time
   
	/* Every two iterations (since there are 2 channels being sampled) is a cycle of period 1 ms in which each channel has been sampled once.
	* To avoid reading a long muscle contraction as many "strong" signals, it is divided into 20 ms accumulation periods, where each channel's data is added up 20 times per period.
	* Each accumulation "bin" of 20 ms is examined, looking at the amplitude of the data to determine how strong the signal is. A sound or digital output pulse is only triggered when there is a weak signal between strong signals, so one muscle contraction only triggers one sound
	*/

	if (loopcounter < 20) {

		//Find the maximum absolute measurement from the data for this cycle's channel
		ampmax[i] = max(channel_data[i], ampmax[i]);

		//Find the minimum absolute measurement from the data for this cycle's channel
		ampmin[i] = min(channel_data[i], ampmin[i]);

		//Find the "amplitude" of the data as the largest possible difference between max and min
		//For a weak signal (no muscle contraction), the amplitude will be small
		//For a strong signal (muscle contraction), the amplitude will be large
		amplitude[i] = max(ampmax[i] - ampmin[i], amplitude[i]);

		//To read lower-frequency signals, also consider large offsets that approach DC
		amplitude[i] = max(amplitude[i], ampmax[i]-128);
		amplitude[i] = max(amplitude[i], 128-ampmin[i]);

		//Add the amplitude to the accumulator bin
		accumulator[i] += amplitude[i];

		if (i == 1) {
			//When both channels have been updated, increase loopcounter by one. When loopcounter reaches 20, we have reached the end of the accumulator period
			loopcounter++;
		}
	}

	
	 else if (i == 1) {
		//If this is the end of the accumulator period, determine if sample should be played or if digital output should be changed
      
		/* Read potentiometer value from ADC to determine what the threshold value should be
		* Multiply and offset ADC reading to set high_threshold as always being positive, and of a suitable range (0 to ~ 300uV averaged over 20 ms)
		*/
		high_threshold = 30 * analogRead(A0) + 200;
		if (low_gain_mode)
			high_threshold = high_threshold * 4;
		//high_threshold = 3.226 * analogRead(A0) + 200;
		//Serial.println(high_threshold);
		for (int x = 0; x < 2; x++) {
			//For each channel

			if (accumulator[x] > high_threshold && reset[x] == true) {
				//If the signal is strong enough to surpass the high threshold, and was preceded by at least one weak accumulation period
				digitalWrite(x + 4, HIGH); //Write HIGH to the Digital Pin correponding to the channel - Channel 1 -> Digital 4. Channel 6 -> Digital 5.
            
				//Trigger the A sound if audio is enabled and the high was from Channel 1
				if (x == 0 && audio_enable == true) {
					PlayNoteA();
					//Flag that a sound is being played 
					sampleflag = true;
				}

				//Trigger the E sound if audio is enabled and the high was from Channel 6
				if (x == 1 && audio_enable == true) {
					PlayNoteE();
					sampleflag = true;
				}

			//Flag that this channel's last accumulation was strong so another sound won't be played, or digital output triggered, until the signal stays weak for at least 20 ms.
			reset[x] = false;
			}
        
			else if (accumulator[x] < 0.5 * high_threshold) {
				//If the signal is lower than 30% of the high threshold, it is considered low enough enough to reset the channel
				//Once the channel is reset, signifying that the muscle contraction is over, the digital output is set to LOW again
				//Once the channel is reset for at least one accumulator period, the next accumulator period to surpass the threshold will trigger a new sound and digital output pulse
				digitalWrite(x + 4, LOW);
				reset[x] = true;
			}
			//Keep data in final output accumulator to be read to the DAC even while the next accumulator is being filled
			finalaccumulator[x] = accumulator[x];
	  
			//Clear the accumulator bin to 0 to prepare for the next accumulation period
			accumulator[x] = 0L; 
		} 

		//Set the loop counter to 0 to prepare for the next accumulation period  
		loopcounter = 0;
		for (int x = 0; x < 2; x++) {
			//Reinitialize the accumulator calculation variables for all channels to prepare for the next accumulation period
			amplitude[x] = 0;
			accumulator[x] = 0;
			//Initial max value is the lowest possible 8-bit value, 0, so that the first channel data read will become the max
			ampmax[x] = 0;
			//Initial min value is the highest possible 8-bit value, 255, so that the first channel data read will become the min
			ampmin[x] = 255;
		}
	}

	if (i == 1)
		//If we just read the data from the SECONDCHANNEL, read FIRSTCHANNEL on the next iteration
		i = 0;
	else
		//If we just read the data from the FIRSTCHANNEL, read SECONDCHANNEL on the next iteration
		i++;
      
	if (sampleflag == true)
    //If we just played a sound sample, increase the sound sample counter variable so the next ISR will play the next sample
	++sample;
   
	//Optional but helpful - monitor how much time each interrupt cycle takes compared to the amount it is allotted by monitoring the duty cycle from pin 2
	digitalWrite(2, LOW);
}

void StartSound(unsigned char const *data, int length)
{
	//Begins the playing of a sound and sets up interrupts to continue playing the sound every cycle thereafter until it finishes
	sounddata_data = data;
	sounddata_length = length;

	//Set up speaker pin to be output
	pinMode(speakerPin, OUTPUT);
  
	//Set up Timer 2 to do pulse width modulation on the speaker pin
  
	//Set fast PWM mode
	TCCR2A |= _BV(WGM21) | _BV(WGM20);
	TCCR2B &= ~_BV(WGM22);
  
	//Do non-inverting PWM on pin OC2B. On the Arduino this is pin 3
	TCCR2A = (TCCR2A | _BV(COM2B1)) & ~_BV(COM2B0);
	TCCR2A &= ~(_BV(COM2A1) | _BV(COM2A0));
  
	//No prescaler
	TCCR2B = (TCCR2B & ~(_BV(CS22) | _BV(CS21))) | _BV(CS20);
  
	//Set initial pulse width to the first sample
	OCR2B = pgm_read_byte(&sounddata_data[0]);
  
	//Set lastSample to help with tapering off the sound when it stops
	lastSample = pgm_read_byte(&sounddata_data[sounddata_length-1]);
	sample = 0;
}

void StopSound()
{ 
	//Disable the PWM timer.
	TCCR2B &= ~_BV(CS20);
  
	//Power off the speaker
	digitalWrite(speakerPin, LOW);
}

void PlayNoteE() {
	//Play the E sample
	StartSound(e, sizeof(e));
}

void PlayNoteA() {
	//Play the A sample
	StartSound(a, sizeof(a));
}

uint16_t SendReadCommand(uint8_t regnum) {
	//Sends a READ command to the RHD chip through the SPI interface
	//The regnum is the register number that is desired to be read. Registers 0-17 are writeable RAM registers, Registers 40-44 & 60-63 are read-only ROM registers
	uint16_t mask = regnum << 8;
	mask = 0b1100000000000000 | mask;
	digitalWrite(chipSelectPin, LOW);
	uint16_t out = SPI.transfer16(mask);
	digitalWrite(chipSelectPin, HIGH);
	return out;
}

uint16_t SendConvertCommand(uint8_t channelnum) {
	//Sends a CONVERT command to the RHD chip through the SPI interface
	//The channelnum is the channel number that is desired to be converted. When a channel is specified, the amplifier and on-chip ADC send the digital data back through the SPI interface
	uint16_t mask = channelnum << 8;
	mask = 0b0000000000000000 | mask;
	digitalWrite(chipSelectPin, LOW);
	uint16_t out = SPI.transfer16(mask);
	digitalWrite(chipSelectPin, HIGH);
	return out;
}

uint16_t SendConvertCommandH(uint8_t channelnum) {
	//Sends a CONVERT command to the RHD chip through the SPI interface, with the LSB set to 1
	//If DSP offset removel is enabled, then the output of the digital high-pass filter associated with the given channel number is reset to zero
	//Useful for initializing amplifiers before begin to record, or to rapidly recover from a large transient and settle to baseline
	uint16_t mask = channelnum << 8;
	mask = 0b0000000000000001 | mask;
	digitalWrite(chipSelectPin, LOW);
	uint16_t out = SPI.transfer16(mask);
	digitalWrite(chipSelectPin, HIGH);
	return out;
}

uint16_t SendWriteCommand(uint8_t regnum, uint8_t data) {
	//Sends a WRITE command to the RHD chip through the SPI interface
	//The regnum is the register number that is desired to be written to
	//The data are the 8 bits of information that are written into the chosen register
	//NOTE: For different registers, 'data' can signify many different things, from bandwidth settings to absolute value mode. Consult the datasheet for what the data in each register represents
	uint16_t mask = regnum << 8;
	mask = 0b1000000000000000 | mask | data;
	digitalWrite(chipSelectPin, LOW);
	uint16_t out = SPI.transfer16(mask);
	digitalWrite(chipSelectPin, HIGH);
	return out;
}

void Calibrate() {
	//Sends a CALIBRATE command to the RHD chip through the SPI interface
	//Initiates an ADC self-calibration routine that should be performed after chip power-up and register configuration
	//Takes several clock cycles to execute, and requires 9 clock cycles of dummy commands that will be ignored until the calibration is complete
	digitalWrite(chipSelectPin, LOW);
	SPI.transfer16(0b0101010100000000);
	digitalWrite(chipSelectPin, HIGH);
	int i = 0;
	for (i = 0; i < 9; i++) {
		//Use the READ command as a dummy command that acts only to set the 9 clock cycles the calibration needs to complete
		SendReadCommand(40);
	}
}

uint8_t ScaleForDAC(int rawdata) {
	//Converts raw 16-bit data from the chip to 8-bit data suitable for sending over the DAC
	//Since average_energy_mode is not active, data will be centered around 0: Add 128 bits to make 0 the center of the DAC's output
	
	//Save rawdata into a temporary variable, to avoid the value from being updated during the scaling
	int temp = rawdata;
	
	//Comment the following line if you want the least significant 8 bits of data (not recommended, as all but the smallest of signals will result in saturation of the DAC):
	temp = rawdata >> 8;
	
	//Multiply the data by a scaling coefficient
	temp = temp * DAC_SCALE_COEFFICIENT;
	
	//If low_gain_mode is enabled, divide output by 4
	if (low_gain_mode == true)
		temp = temp / 4;
	
	//If the output is below the DAC's output range, set it to the lowest value (induce clipping)
	if (temp < -128)
		temp = -128;
	
	//If the output is above the DAC's output range, set it to the highest value (induce clipping)
	else if (temp > 127)
		temp = 127;
	
	//Return the scaled output, with an offset to make it a range of unsigned integers from 0 to 255, suitable for the DAC
	return (uint8_t) (temp + 128);
}

uint8_t ScaleForDAC_ACC(int rawdata) {
	//Converts raw 16-bit data from the chip to 8-bit data suitable for sending over the DAC
	//Since average_energy_mode is active, data will be postive starting from 0: Leave 0 to be the lowest edge of the DAC's output
	
	//Save rawdata into a temporary variable, to avoid the value from being updated during the scaling
	int temp = rawdata;
	
	//Comment the following line if you want the least significant 8 bits of data (not recommended, as all but the smallest of signals will result in saturation of the DAC):
	temp = rawdata >> 8;
	
	//Multiply the data by a scaling coefficient
	temp = temp * DAC_SCALE_COEFFICIENT;
	
	//If low_gain_mode is enabled, divide output by 4
	if (low_gain_mode == true)
		temp = temp / 4;
	
	//If the output is below the DAC's output range, set it to the lowest value (induce clipping)
	if (temp < 0)
		temp = 0;
	
	//If the output is above the DAC's output range, set it to the highest value (induce clipping)
	else if (temp > 255)
		temp = 255;
	
	//Return the scaled output to make it a range of unsigned integers from 0 to 255, suitable for the DAC
	return (uint8_t) (temp);
}


int ReadChannelData(uint8_t channelnum) {
	//Function called by the main loop of the sketch to access the global sample variable native to this file
	//Return the signal value of the current sample
	if (channelnum == FIRSTCHANNEL)
		return (int) final_channel_data[0];
	else if (channelnum == SECONDCHANNEL)
		return (int) final_channel_data[1];
}


long ReadAccumulatorData(uint8_t channelnum) {
	//Function called by the main loop of the sketch to access the global accumulator variable native to this file
	//Return the signal value of the previous accumulator
	if (channelnum == FIRSTCHANNEL)
		return (long) finalaccumulator[0];
	else if (channelnum == SECONDCHANNEL)
		return (long) finalaccumulator[1];
}

/* The following two functions use an IIR notch filter algorithm to filter out either 60 Hz or 50 Hz signals (i.e. noise from power lines).
 * The algorithm is: out(2) = a*b2*in(0) + a*b1*in(1) + a*b0*in(2) - a2*out(0) - a1*out(1) where out and in have memory of the previous two values, and update for each new value.
 * Coefficients a, a1, a2, b0, b1, and b2 are given by calculating IIR filter parameters, and by default correspond to either 60 Hz or 50 Hz notch filters.
 * To create a customized notch filter, change the following parameters
 * 
 * fSample = 1000 (sampling frequency, we sample each channel at 1 kHz)
 * fNotch = 60, 50 (frequency around which the notch filter should be centered. We use either 60 or 50 Hz to filter out power line noise
 * Bandwidth = 10 (bandwidth around the center frequency where the filter is active. Recommended to NOT decrease this number before 10, as this tends to cause the filter to respond very slowly)
 * 
 * Once these are changed, calculate the following parameters, and substitute in a, a1, a2, b0, b1, and b2 for their default values
 * 
 * tstep = 1/fSample;
 * Fc = fNotch*tsep;
 * d = exp(-2*pi*(Bandwidth/2)*tstep);
 * b = (1+d*d)*cos(2*pi*Fc);
 * a0 = 1;
 * a1 = -b;
 * a2 = d*d;
 * a = (1 * d*d)/2;
 * b0 = 1;
 * b1 = -2(cos(2*pi*Fc);
 * b2 = 1;
 * 
 * When these coefficients have been calculated, choose either NotchFilter60() or NotchFilter50() to act instead as your custom notch filter, and replace that filter's coefficients with yours.
 * To enable your notch filter, make sure the DIP switch's position for notch_enable is ON, and that notch_select is set to whichever filter whose coefficients you replaced with your own (ON = 60 Hz, OFF = 50 Hz)
 */

void NotchFilter60() {
    //Function that uses an IIR notch filter to remove 60 Hz noise, and scale the data to be easily read by an 8-bit DAC
  
	//Updating the previous values for the input arrays as a new sample comes in
    in[i][0] = in[i][1];
    in[i][1] = in[i][2];
    in[i][2] = channel_data[i];

    //Performing the IIR notch filter algorithm
    //out[i][2] = a*b2*in[i][0] + a*b1*in[i][1] + a*b0*in[i][2] - a2*out[i][0] - a1*out[i][1];
    out[i][2] = 0.9696 * in[i][0] - 1.803 * in[i][1] + 0.9696 * in[i][2] - 0.9391 * out[i][0] + 1.8029 * out[i][1];

    //Update the previous values for the output arrays
    out[i][0] = out[i][1];
    out[i][1] = out[i][2];

	//Save the output of the IIR notch filter algorithm to the global variable channel_data
    channel_data[i] = out[i][2];
}

void NotchFilter50() {
    //Function that uses an IIR notch filter to remove 50 Hz noise, and scale the data to be easily read by an 8-bit DAC
    
    //Updating the previous values for the input arrays as a new sample comes in
    in[i][0] = in[i][1];
    in[i][1] = in[i][2];
    in[i][2] = channel_data[i];

    //Performing the IIR notch filter algorithm
    //out[i][2] = a*b2*in[i][0] + a*b1*in[i][1] + a*b0*in[i][2] - a2*out[i][0] - a1*out[i][1];
    out[i][2] = 0.9696 * in[i][0] - 1.8443 * in[i][1] + 0.9696 * in[i][2] - 0.9391 * out[i][0] + 1.8442 * out[i][1];

    //Update the previous values for the output arrays
    out[i][0] = out[i][1];
    out[i][1] = out[i][2];

	//Save the output of the IIR notch filter algorithm to the global variable channel_data
    channel_data[i] = out[i][2];
}

void NotchFilterNone() {
	//Does nothing, only used as a placeholder in case the user wishes to add additional processing when switch 5 is off
}

void SetAmpPwr(bool Ch1, bool Ch2) {
	//Function that takes amplifier channel power information from the .ino file (which is easier for the user to change), and sets global variables equal to those values so they can be used in the ISR
	//FirstChannelPwr and SecondChannelPwr are used to determine which channels of the RHD2216 should be powered on during initial setup
	//Their values are also read into global variables "firstchannelpower" and "secondchannelpower" so that if a channel is not powered on, all data read from that channel is set to 0.
	//This is important because turning off a channel's power doesn't guarantee its output to be 0, and if the user chooses not to turn on an amplifier its readings shouldn't affect the sketch in any way
	
	//This function also sends the READ command over the SPI interface to the chip, performs bit operations to the result, and sends the WRITE command to power on individual amplifiers on the chip
	
	//2 8-bit variables to hold the values read from Registers 14 and 15
	uint8_t previousreg14;
	uint8_t previousreg15;
	
	//Use READ commands to assign values from the chip to the 8-bit variables
	SendReadCommand(14);
	SendReadCommand(14);
	previousreg14 = SendReadCommand(14);
	SendReadCommand(15);
	SendReadCommand(15);
	previousreg15 = SendReadCommand(15);
	
	//If FirstChannelPwr has been set true in the main .ino file, then set global variable firstchannelpower as true and send a WRITE command to the corresponding register setting the corresponding bit
	if (Ch1) {
		firstchannelpower = true;
		if (FIRSTCHANNEL < 8) {
			//If FIRSTCHANNEL is an amplifier channel between 0 and 7, send the WRITE command to Register 14 (See RHD2216 datasheet for details)
			SendWriteCommand(14, (1<<FIRSTCHANNEL | previousreg14));
			previousreg14 = 1 << FIRSTCHANNEL | previousreg14;
		}
		else if (FIRSTCHANNEL >= 8){
			//If FIRSTCHANNEL is an amplifier channel between 8 and 15, send the WRITE command to Register 15 (See RHD2216 datasheet for details)
			SendWriteCommand(15, (1<<abs(FIRSTCHANNEL-8) | previousreg15)); //abs() is not necessary, as the conditional "else if()" ensures FIRSTCHANNEL-8 is positive. However, the compiler gives a warning unless the FIRSTCHANNEL-8 is positive. Hence abs()
			previousreg15 = 1 << abs(FIRSTCHANNEL-8) | previousreg15;
	  }
	}
	else {
		//If FirstChannelPwr has been set false in the main .ino file, then set global variable firstchannelpower as false
		firstchannelpower = false;
	}
	
	//If SecondChannelPwr has been set true in the main .ino file, then set global variable secondchannelpower as true and send a WRITE command to the corresponding register setting the corresponding bit
	if (Ch2) {
		secondchannelpower = true;
		if (SECONDCHANNEL < 8)
			//If SECONDCHANNEL is an amplifier channel between 0 and 7, send the WRITE command to Register 14 (See RHD2216 datasheet for details)
			SendWriteCommand(14, (1<<SECONDCHANNEL | previousreg14));
		else if (SECONDCHANNEL >= 8)
			//If SECONDCHANNEL is an amplifier channel between 8 and 15, send the WRITE command to Register 15 (See RHD2216 datasheet for details)
			SendWriteCommand(15, (1<<abs(SECONDCHANNEL-8) | previousreg15)); //abs() is not necessary, as the conditional "else if()" ensures SECONDCHANNEL-8 is positive. However, the compiler gives a warning unless the SECONDCHANNEL-8 is positive. Hence abs()
	}
	else {
		//If SecondChannelPwr has been set false in the main .ino file, then set global variable secondchannelpower as false
		secondchannelpower = false;
	}
}

bool LowGainMode() {
	//Simple function that grabs global variable low_gain_mode from this .cpp file and returns it back to the main .ino file
	return low_gain_mode;
}

long ReadThreshold() {
	//Simple function that grabs global variable high_threshold from this .cpp file and returns it, as a string, back to the main .ino file
	return high_threshold;
}
