             Readme File for Buck Boost PICtail Code Example using the 16-Bit 28-Pin Starter Board:
                        The 16-Bit 28-Pin Starter Board Utilizing One Buck stages
                               ---------------------------------------------

In this code example, the first buck stage is being controlled using voltage mode control. 
The first buck stage is controlled at 5V. The PWM frequency is 400kHz and the ADC routine 
is triggered every PWM cycles 2.5us. The ADC has two SARs which in return can produces a max 
speed of 4MSPS. Each power stage has a soft start routine of 50ms that helps minimize 
overshoot and the inrush current.

The following files are included in the project:
	main.c    	- Sets up all necessary clocks
	init.c    	- Sets up the PIDs, PWMs, and ADC
	isr_asm.s 	- ADC ISRs that measure feedback signals and call PID
	pid.s	  	- PID Control 
	isr.c	  	- Includes Timer ISR used for the delay routine
	dspcommon.inc	- Used for PID Structure
	dsp.h
	Function.h	- define all the fucntions used in the software.

	

Also included are the device header and linker files.  

