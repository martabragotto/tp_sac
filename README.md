# tp_sac

TP notes of 07/11/2022


After setting PA8, PA9, PA11, PA12 pins to work in PWM mode PWM generator setting has been defined for "Commande complémentaire décalée"

The power module works at 16kHz and the board works at 170MHZ. With these data the counter period can be set.
In particular the "center alligned mode" of counting has been chosen, so the counter period value is defined by the following equation
(clock frequency)/(powermodule frequency)=2*counter period
the counter period value is then set to 5313-1

Now the pulse value needs to be defined. It is the value for which we define the duty cycle value.
For the first test a duty cycle value of 0.25 has been chosen.

For the first channel, that commands PA8 (H1 command) and PA11 (L1 command), pulse=0.25*counter period
pulse channel 1 = 1239-1 has been put.

Channel 2 had to be shifted of half a period, with respect to PA11 output. To do this, pulse value has been set as: pulse channel 2 = counter period - pulse channel 1 
pulse channel 2 = 3987-1

Output test --> look at the image "screenOscilloscopioFrequencyCheck" for frequency and duty cycle check.

Duty cycle setting by shell commands:
In order to do that we "dutycycle" string is detected as argv[0] and the new percentage value is detected as argv[1].
Then the percentage value has been divided by 100 to have it convertd into the factor by which multiply counter period value and get new pulse value for channel1 PWM
The second pulse value has been calculated as before (pulse channel 2 = counter period - pulse channel 1)

Output tests with 0.15, 0.60, 0.80 ad dutycycle values: look at the images "screenOscilloscopioPeriodValue", "screenOscilloscopioAlpha0.15", "screenOscilloscopioAlpha0.60", "screenOscilloscopioAlpha0.80" for check
Notice that the up time values are at least 2microseconds shorter due to dead time setting.

Dead time is set as follow:

Regarding at the datasheet the dead time generator setup has been found:

Following the shown formulas the first 3 lines of calculations have been computed as follows: 

1-> DTG=DT/t_DTS= 2us/(1/17MHz)=340 > 2^7= 128      											                        	//NOT RIGHT ONE
	notice that 2^7 has been used because 7 is the number of empty bits of the DTG 
			0XXXXXXX
2-> DTG=(DT/t_DTS)-64 = (2us/ (2*1/f_clock))-64=(2us/ (2*1/170MHz))-64=170-64=106>2^6=64						//NOT RIGHT ONE
	notice that 2^6 has been used because 6 is the number of empty bits of the DTG 
			10XXXXXX
3-> DTG=(DT/t_DTS)-32 = (2us/ (8*1/f_clock))-32=(2us/ (8*1/170MHz))-32=42,5-32= 10,5 -> 11 < 2^5=32 				//RIGHT ONE
	notice that 2^5 has been used because 6 is the number of empty bits of the DTG 
			110XXXXX 
	11 in binary is 01011 so the final minimum DTG in binary will be 11001011 that is 203 in decimal 

	Actually 203 has been chosen as DTG in the code.
	This is the minimum value, this value can be increased if needed (i.e. at 205)
	
ISO RESET CODE 
In order to let inverter receive an up signal of at least 2 us, the following calculation has been done: 
since the clock works at a frequency of 170MHz, the number of clockturns needed to arrive to 2 us is:
	1/170 MHz : ClockTurnsNumber = 2us --> at least ClockTurnsNumber= 340 are needed to reach 2us //350 has been chosen in the beginning as ISO_RESET_TIME (number of clockturns)
	Then by the oscilloscope (picture screenOscilloscopioPeriod1) the up period time of the pin has been measured, and it could be noticed that it was 18,76 us, almost 10 times the minimum value. This is because the while cycle actually did not last only one clock period. So the ClockTurnsNumber have been changed to 70 (picture screenOscilloscopioPeriod2).
	
DMA Settings 
The DMA has been associated to a new timer TIM8. The voltage output frequency of the full bridge is two times the switching frequency which is 16KHz, and since the required number of samplings per period is 10 the Timer has been set in order to work at 320KHz. 
Callback function has been used only as a flag while the conversion has been performed in the main. 



