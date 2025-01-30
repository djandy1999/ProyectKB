#include "IntanShield.h"

/* Global variables */

int speakerPin = 3; //Output pin which drives speaker with PWM - pin 3
unsigned char const *sounddata_data = 0; //Pointer to the raw data of the sound
int sounddata_length = 0; //Keep track of the length of the sound sample sequence
volatile uint16_t sample = 0; //Global counter variable to cycle through the sound, sample by sample
byte lastSample; //Last sample to be played - treated specially because the sound needs to decrease gradually afterwards to avoid an ugly pop as soon as the sample is finished
int channel_num = 0; //Keep track of which channel is being sampled

enum Notchselect { Notch60, Notch50, NoNotch };
Notchselect notch_setting; //notch_setting has 3 possible values. It is set by Analog Pins 2 and 3 (aka Digital Pins 16 and 17) and updated every 0.5 ms

volatile bool audio_enable; //audio_enable is either true or false. It is set by Digital Pin 6 and updated every 0.5 ms

volatile bool low_gain_mode; //low_gain_mode is either true or false. It is set by Digital Pin 7 and updated every 0.5 ms
                             //When enabled, scale channel data down by a factor of 4. By default, channel data begins to clip when input voltages are 1.1 mV peak-to-peak. When low_gain_mode is enabled, this increases to 4.4 mV peak-to-peak
                             //Should be enabled when signals appear too strong and are clipping (more common with EKG than EMG). Should be disabled when signals appear too weak and are hard to see
                    
volatile bool sampleflag = false; //Keep track of whether or not a sample is being played through the speaker
volatile long high_threshold; //Set the high threshold of each channel - how strong must a signal be to trigger a sound - set by potentiometer

volatile bool reset[2] = {true, true}; //Keep track of recent history of signal strength.
                                       //If a signal was recently strong enough to trigger a sound or digital out pulse, reset = false
                                       //Strong signals thereafter will not trigger new sounds or pulses (otherwise one muscle contraction could trigger dozens or hundreds of sounds),
                                       //until the signal becomes weak enough to fall below the low threshold, signifying the end of a muscle contraction. At this point, reset = true

volatile int i = 0; //Keep track of which channel's turn it is to be sampled. The ISR runs every 0.5 ms, and we sample 1 channel per ISR
                    //Since we sample 2 channels total, each channel is sampled every 1 ms, giving us our sample rate of 1 kHz

volatile int16_t channel_data[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0}; //Store the 16-bit channel data from two channels of the RHD
volatile int16_t final_channel_data[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0};  //Store the final 8-bit version of the channel data
volatile float in[13][3] = {{0}}; //Keep track of past values of the original signal, which helps the notch filter remove 50 Hz or 60 Hz noise
volatile float out[13][3] = {{0}}; //Keep track of the past values of the processed signal, which helps the notch filter remove 50 Hz or 60 Hz noise

volatile bool firstchannelpower; //Adapt FirstChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool secondchannelpower; //Adapt SecondChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool thirdchannelpower; //Adapt ThirdChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool fourthchannelpower; //Adapt FourthChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR	
volatile bool fifthchannelpower; //Adapt FifthChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool sixthchannelpower; //Adapt SixthChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool seventhchannelpower; //Adapt SeventhChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool eighthchannelpower; //Adapt EighthChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool ninthchannelpower; //Adapt NinthChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool tenthchannelpower; //Adapt TenthChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool eleventhchannelpower; //Adapt EleventhChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR
volatile bool twelfthchannelpower; //Adapt TwelfthChannelPwr from the .ino file (which is easier for the user to change), into a global variable that can be used in the ISR

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


	//Read the channel data from whichever sample in the pipeline corresponds to this 1/2000th of a second

	channel_data[i] = SendConvertCommand(i);
	
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
	
	final_channel_data[i] = channel_data[i];

	/* Every two iterations (since there are 2 channels being sampled) is a cycle of period 1 ms in which each channel has been sampled once.
	* To avoid reading a long muscle contraction as many "strong" signals, it is divided into 20 ms accumulation periods, where each channel's data is added up 20 times per period.
	* Each accumulation "bin" of 20 ms is examined, looking at the amplitude of the data to determine how strong the signal is. A sound or digital output pulse is only triggered when there is a weak signal between strong signals, so one muscle contraction only triggers one sound
	*/
	
	if (i >= NUM_CHANNELS)
 	{
    	i = 0;  // reset
	}else{
		i++;																
	}



	//Optional but helpful - monitor how much time each interrupt cycle takes compared to the amount it is allotted by monitoring the duty cycle from pin 2
	digitalWrite(2, LOW);
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

int ReadChannelData(uint8_t channelnum) {
	channel_num = channelnum;
	//Function called by the main loop of the sketch to access the global sample variable native to this file
	//Return the signal value of the current sample
	if (channelnum == FIRSTCHANNEL)
		return (int) final_channel_data[0];
	else if (channelnum == SECONDCHANNEL)
		return (int) final_channel_data[1];
	else if (channelnum == THIRDCHANNEL)
		return (int) final_channel_data[2];
	else if (channelnum == FOURTHCHANNEL)
		return (int) final_channel_data[3];
	else if (channelnum == FIFTHCHANNEL)
		return (int) final_channel_data[4];
	else if (channelnum == SIXTHCHANNEL)
		return (int) final_channel_data[5];
	else if (channelnum == SEVENTHCHANNEL)
		return (int) final_channel_data[6];
	else if (channelnum == EIGHTHCHANNEL)
		return (int) final_channel_data[7];
	else if (channelnum == NINTHCHANNEL)
		return (int) final_channel_data[8];
	else if (channelnum == TENTHCHANNEL)
		return (int) final_channel_data[9];
	else if (channelnum == ELEVENTHCHANNEL)
		return (int) final_channel_data[10];
	else if (channelnum == TWELFTHCHANNEL)
		return (int) final_channel_data[11];
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

void SetAmpPwr(bool Ch1, bool Ch2, bool Ch3, bool Ch4, bool Ch5, bool Ch6, bool Ch7, bool Ch8, bool Ch9, bool Ch10, bool Ch11, bool Ch12) {
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
	
	// First Channel
	if (Ch1) {
		firstchannelpower = true;
		if (FIRSTCHANNEL < 8) {
			SendWriteCommand(14, (1<<FIRSTCHANNEL | previousreg14));
			previousreg14 = 1 << FIRSTCHANNEL | previousreg14;
		} else if (FIRSTCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(FIRSTCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(FIRSTCHANNEL-8) | previousreg15;
		}
	} else {
		firstchannelpower = false;
	}

	// Second Channel
	if (Ch2) {
		secondchannelpower = true;
		if (SECONDCHANNEL < 8) {
			SendWriteCommand(14, (1<<SECONDCHANNEL | previousreg14));
			previousreg14 = 1 << SECONDCHANNEL | previousreg14;
		} else if (SECONDCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(SECONDCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(SECONDCHANNEL-8) | previousreg15;
		}
	} else {
		secondchannelpower = false;
	}

	// Third Channel
	if (Ch3) {
		thirdchannelpower = true;
		if (THIRDCHANNEL < 8) {
			SendWriteCommand(14, (1<<THIRDCHANNEL | previousreg14));
			previousreg14 = 1 << THIRDCHANNEL | previousreg14;
		} else if (THIRDCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(THIRDCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(THIRDCHANNEL-8) | previousreg15;
		}
	} else {
		thirdchannelpower = false;
	}

	// Fourth Channel
	if (Ch4) {
		fourthchannelpower = true;
		if (FOURTHCHANNEL < 8) {
			SendWriteCommand(14, (1<<FOURTHCHANNEL | previousreg14));
			previousreg14 = 1 << FOURTHCHANNEL | previousreg14;
		} else if (FOURTHCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(FOURTHCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(FOURTHCHANNEL-8) | previousreg15;
		}
	} else {
		fourthchannelpower = false;
	}

	// Fifth Channel
	if (Ch5) {
		fifthchannelpower = true;
		if (FIFTHCHANNEL < 8) {
			SendWriteCommand(14, (1<<FIFTHCHANNEL | previousreg14));
			previousreg14 = 1 << FIFTHCHANNEL | previousreg14;
		} else if (FIFTHCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(FIFTHCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(FIFTHCHANNEL-8) | previousreg15;
		}
	} else {
		fifthchannelpower = false;
	}

	// Sixth Channel
	if (Ch6) {
		sixthchannelpower = true;
		if (SIXTHCHANNEL < 8) {
			SendWriteCommand(14, (1<<SIXTHCHANNEL | previousreg14));
			previousreg14 = 1 << SIXTHCHANNEL | previousreg14;
		} else if (SIXTHCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(SIXTHCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(SIXTHCHANNEL-8) | previousreg15;
		}
	} else {
		sixthchannelpower = false;
	}

	// Seventh Channel
	if (Ch7) {
		seventhchannelpower = true;
		if (SEVENTHCHANNEL < 8) {
			SendWriteCommand(14, (1<<SEVENTHCHANNEL | previousreg14));
			previousreg14 = 1 << SEVENTHCHANNEL | previousreg14;
		} else if (SEVENTHCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(SEVENTHCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(SEVENTHCHANNEL-8) | previousreg15;
		}
	} else {
		seventhchannelpower = false;
	}

	// Eighth Channel
	if (Ch8) {
		eighthchannelpower = true;
		if (EIGHTHCHANNEL < 8) {
			SendWriteCommand(14, (1<<EIGHTHCHANNEL | previousreg14));
			previousreg14 = 1 << EIGHTHCHANNEL | previousreg14;
		} else if (EIGHTHCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(EIGHTHCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(EIGHTHCHANNEL-8) | previousreg15;
		}
	} else {
		eighthchannelpower = false;
	}

	// Ninth Channel
	if (Ch9) {
		ninthchannelpower = true;
		if (NINTHCHANNEL < 8) {
			SendWriteCommand(14, (1<<NINTHCHANNEL | previousreg14));
			previousreg14 = 1 << NINTHCHANNEL | previousreg14;
		} else if (NINTHCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(NINTHCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(NINTHCHANNEL-8) | previousreg15;
		}
	} else {
		ninthchannelpower = false;
	}

	// Tenth Channel
	if (Ch10) {
		tenthchannelpower = true;
		if (TENTHCHANNEL < 8) {
			SendWriteCommand(14, (1<<TENTHCHANNEL | previousreg14));
			previousreg14 = 1 << TENTHCHANNEL | previousreg14;
		} else if (TENTHCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(TENTHCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(TENTHCHANNEL-8) | previousreg15;
		}
	} else {
		tenthchannelpower = false;
	}

	// Eleventh Channel
	if (Ch11) {
		eleventhchannelpower = true;
		if (ELEVENTHCHANNEL < 8) {
			SendWriteCommand(14, (1<<ELEVENTHCHANNEL | previousreg14));
			previousreg14 = 1 << ELEVENTHCHANNEL | previousreg14;
		} else if (ELEVENTHCHANNEL >= 8) {
			SendWriteCommand(15, (1<<abs(ELEVENTHCHANNEL-8) | previousreg15));
			previousreg15 = 1 << abs(ELEVENTHCHANNEL-8) | previousreg15;
		}
	} else {
		eleventhchannelpower = false;
	}
	
	//If SecondChannelPwr has been set true in the main .ino file, then set global variable secondchannelpower as true and send a WRITE command to the corresponding register setting the corresponding bit
	if (Ch12) {
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
