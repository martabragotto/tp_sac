# tp_sac
# MARTA BRAGOTTO & GIUSEPPE MANZO 



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

Output test --> look at the image "screenOscilloscopioFrequencyCheck" below for frequency and duty cycle check.

screenOscilloscopioFrequencyCheck:

![screenOscilloscopioFrequencyCheck](https://user-images.githubusercontent.com/73655064/211313367-2397494a-979b-47e1-9afe-b66d3b97218e.png)


Duty cycle setting by shell commands:
In order to do that we "dutycycle" string is detected as argv[0] and the new percentage value is detected as argv[1].
Then the percentage value has been divided by 100 to have it convertd into the factor by which multiply counter period value and get new pulse value for channel1 PWM
The second pulse value has been calculated as before (pulse channel 2 = counter period - pulse channel 1)

Output tests with 0.15, 0.60, 0.80 ad dutycycle values: look at the images "screenOscilloscopioPeriodValue", "screenOscilloscopioAlpha0.15", "screenOscilloscopioAlpha0.60", "screenOscilloscopioAlpha0.80" below for check.


screenOscilloscopioPeriodValue:

![screenOscilloscopioPeriodValue](https://user-images.githubusercontent.com/73655064/211312404-436823cc-92ef-4d11-abac-c8a478f99e61.png)

screenOscilloscopioAlpha0.15 :

![screenOscilloscopioAlpha0 15](https://user-images.githubusercontent.com/73655064/211312472-44317d62-266e-492d-871c-f5b0c61cca7f.png)

screenOscilloscopioAlpha0.60 :

![screenOscilloscopioAlpha0 60](https://user-images.githubusercontent.com/73655064/211312477-838bf9b9-dadd-4115-88ef-dd7b62ffb20e.png)

screenOscilloscopioAlpha0.80 :

![screenOscilloscopioAlpha0 80](https://user-images.githubusercontent.com/73655064/211312479-1a788364-bc73-403b-b7ea-6983249a837f.png)


Notice that the up time values are at least 2microseconds shorter due to dead time setting.

Dead time is set as follow:

Regarding at the datasheet the dead time generator setup has been found:

Following the shown formulas the first 3 lines of calculations have been computed as follows: 

1-> DTG=DT/t_DTS= 2us/(1/17MHz)=340 > 2^7= 128      											                        																	//NOT RIGHT ONE
	notice that 2^7 has been used because 7 is the number of empty bits of the DTG 
			0XXXXXXX
2-> DTG=(DT/t_DTS)-64 = (2us/ (2*1/f_clock))-64=(2us/ (2*1/170MHz))-64=170-64=106>2^6=64						//NOT RIGHT ONE
	notice that 2^6 has been used because 6 is the number of empty bits of the DTG 
			10XXXXXX
3-> DTG=(DT/t_DTS)-32 = (2us/ (8*1/f_clock))-32=(2us/ (8*1/170MHz))-32=42,5-32= 10,5 -> 11 < 2^5=32 					//RIGHT ONE
	notice that 2^5 has been used because 6 is the number of empty bits of the DTG 
			110XXXXX 
	11 in binary is 01011 so the final minimum DTG in binary will be 11001011 that is 203 in decimal 

Actually 203 has been chosen as DTG in the code.
This is the minimum value, this value can be increased if needed (i.e. at 205)
	
ISO RESET CODE 
In order to let inverter receive an up signal of at least 2 us, the following calculation has been done: 
since the clock works at a frequency of 170MHz, the number of clockturns needed to arrive to 2 us is:
	1/170 MHz : ClockTurnsNumber = 2us --> at least ClockTurnsNumber= 340 are needed to reach 2us //350 has been chosen in the beginning as ISO_RESET_TIME (number of clockturns)
	
Then by the oscilloscope (see picture screenOscilloscopioPeriod1 below) the up period time of the pin has been measured, and it could be noticed that it was 18,76 us, almost 10 times the minimum value. This is because the while cycle actually did not last only one clock period. So the ClockTurnsNumber have been changed to 70 (see picture screenOscilloscopioPeriod2 below).
	
screenOscilloscopioPeriod2:

![screenOscilloscopioPeriod1](https://user-images.githubusercontent.com/73655064/211313674-b7dcab79-2535-4594-9d06-65a85d6fc3f5.png)

	
screenOscilloscopioPeriod2:

![screenOscilloscopioPeriod2](https://user-images.githubusercontent.com/73655064/211313663-76b20660-0c11-4052-be43-d1382e8421c9.png)

	
DMA Settings and CURRENT SENSING
The DMA has been associated to a new timer TIM8. The voltage output frequency of the full bridge is two times the switching frequency which is 16KHz, and since the required number of samplings per period is 10 the Timer has been set in order to work at 320KHz. 
Callback function has been used only as a flag while the conversion has been performed in the main. 
The conversion has been performed by averaging the 10 raw data and then dividing it by 4095 and multiplying it by 3,3.
Have a look at the images "currentSensedValuesOnTerminal" "CurrentSensedWithOscilloscope" to see the measured current.

currentSensedValuesOnTerminal:

![CurrentSensedValuesOnTerminal](https://user-images.githubusercontent.com/73655064/211314058-48021472-fce9-40b1-8f2b-854a97929819.jpeg)

CurrentSensedWithOscilloscope:

![CurrentSensedWithOscilloscope](https://user-images.githubusercontent.com/73655064/211314079-e1d13715-a702-4531-aa73-76ab3e6fd57f.png)



Notice that in the following test, we had a hardware problem. Current sensor in the power module had higher values than expected in output. So the gain is set to 3 instead of 12 as shown in the datasheet. With this changed the sensor output and oscilloscope output are coherent. Have a look at the images "CurrentSpeedSensed" "NewCurrentSensedWithOscilloscope" to see the measured current.

CurrentSpeedSensed:

![CurrentSpeedSensed](https://user-images.githubusercontent.com/73655064/211314136-92f26c73-2b67-4ad0-84fd-9ef46d4939d3.JPG)

NewCurrentSensedWithOscilloscope

![NewCurrentSensedWithOscilloscope](https://user-images.githubusercontent.com/73655064/211314178-c18fb638-6b82-4327-b16d-f90fab26b754.png)


SPEED SENSING

In order to sense the current, two timers are used. The first one is set to work in "encoder mode" to sens the input values from the motor's encoder. In particular the counter is set to the middle value. The encoder mode adds one to the counter when motor turns in clockwise direction and subtracts one when in conterclockwise direction, so we start from the middle value of the counter to take into account both clockwise and conterclockwise rotations.
Then a second timer is set to evaluate the value of the rotor position at constant rate to calculate the speed of rotation. Indeed
	RotationSpeed=deltaAngle x TimerFrequency
The timer frequency is set at 100Hz
The encoder mode timer is set to tick at both rising and folling edge of the encoder signal and it processes the signal of two. Then the total number of trigger events that can increase or decrease the timer counter is 1024 x 4=4096 per round. A changing in the encoder timer counter means a chainging if 2pi/4096 radiant in rotor's position.

The following lines converts the data from encoder mode timer into rotational speed in radiant per second
	speed = 2 * 3.14 * (((float)counter-HALF_COUNTER_ENCODER)/RISES_PER_TURN) * SAMPLING_FREQ;
where
	HALF_COUNTER_ENCODER=32767 (Middle value of the encoder mode timer's counter)
	RISES_PER_TURN=4096
	SAMPLING_FREQ=100

CURRENT CONTROL 

According to the control scheme realised through simulink the following coefficients for the current PID has been found: 

	KpC = 0,0342717381517845
	KiC = 25,206814338745
	
Then the continuous time control scheme has been translated into C, discret time code as , the following: 
AntiWindup solution: in order to not realise the control for saturised alfa then if alfa is >1 is then set to 1 and if alfa is <0 is then set to 0.

The following "currentControl" image shows the functioning of the current control at the set current of 400 mA.

currentControl: 

![currentControl](https://user-images.githubusercontent.com/73655064/211350204-776997bf-cf80-477c-8b3b-81a354d241cd.png)

SPEED CONTROL 

According to the control scheme realised through simulink the following coefficients for the current PID has been found: 

	KpC = 2,25311945059239
	KiC = 7,88672319650441


The following "speedControl" image shows the functioning of the speed control at the set speed of 

speedControl: 
