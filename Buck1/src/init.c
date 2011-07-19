/**********************************************************************
* © 2008 Microchip Technology Inc.
*
* SOFTWARE LICENSE AGREEMENT:
* Microchip Technology Incorporated ("Microchip") retains all ownership and 
* intellectual property rights in the code accompanying this message and in all 
* derivatives hereto.  You may use this code, and any derivatives created by 
* any person or entity by or on your behalf, exclusively with Microchip's
* proprietary products.  Your acceptance and/or use of this code constitutes 
* agreement to the terms and conditions of this notice.
*
* CODE ACCOMPANYING THIS MESSAGE IS SUPPLIED BY MICROCHIP "AS IS".  NO 
* WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED 
* TO, IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A 
* PARTICULAR PURPOSE APPLY TO THIS CODE, ITS INTERACTION WITH MICROCHIP'S 
* PRODUCTS, COMBINATION WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION. 
*
* YOU ACKNOWLEDGE AND AGREE THAT, IN NO EVENT, SHALL MICROCHIP BE LIABLE, WHETHER 
* IN CONTRACT, WARRANTY, TORT (INCLUDING NEGLIGENCE OR BREACH OF STATUTORY DUTY), 
* STRICT LIABILITY, INDEMNITY, CONTRIBUTION, OR OTHERWISE, FOR ANY INDIRECT, SPECIAL, 
* PUNITIVE, EXEMPLARY, INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, FOR COST OR EXPENSE OF 
* ANY KIND WHATSOEVER RELATED TO THE CODE, HOWSOEVER CAUSED, EVEN IF MICROCHIP HAS BEEN 
* ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT 
* ALLOWABLE BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY RELATED TO 
* THIS CODE, SHALL NOT EXCEED THE PRICE YOU PAID DIRECTLY TO MICROCHIP SPECIFICALLY TO 
* HAVE THIS CODE DEVELOPED.
*
* You agree that you are solely responsible for testing the code and 
* determining its suitability.  Microchip has no obligation to modify, test, 
* certify, or support the code.
*
*******************************************************************************/

#include "p33FJ16GS502.h"
#include "Functions.h"
#include "dsp.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* ~~~~~~~~~~~~~~~~~~~~~~  PID Variable Definitions  ~~~~~~~~~~~~~~~~~~~~~~~ */

/* Variable Declaration required for each PID controller in the application. */
tPID Buck1VoltagePID; 

/* These data structures contain a pointer to derived coefficients in X-space and 
   pointer to controler state (history) samples in Y-space. So declare variables 
   for the derived coefficients and the controller history samples */
fractional Buck1VoltageABC[3] __attribute__ ((section (".xbss, bss, xmemory")));
fractional Buck1VoltageHistory[3] __attribute__ ((section (".ybss, bss, ymemory")));

/* Buck1 is the 5V output with Voltage Mode Control implemented */
#define PID_BUCK1_KP 0.23
#define PID_BUCK1_KI 0.0016
#define PID_BUCK1_KD 0

/* These macros are used for faster calculation in the PID routine and are used for limiting the PID Coefficients */
#define PID_BUCK1_A Q15(PID_BUCK1_KP + PID_BUCK1_KI + PID_BUCK1_KD)
#define PID_BUCK1_B Q15(-1 *(PID_BUCK1_KP + 2 * PID_BUCK1_KD))
#define PID_BUCK1_C Q15(PID_BUCK1_KD)

#define PID_BUCK1_VOLTAGE_REFERENCE 0x6120				// Reference voltage 4V = 5D60 (or 6120 to compensate for -4% error)
														//                   5V = 74C0 (or 7960 to compensate for -4% error)
														//                   3V = ? (48E0 to compensate for -4% error)
													    /* Reference voltage is from resistor divider circuit R11 & R12 
													    	Voltage FB1 = (5kOhm / (5kOhm + 3.3kOhm)) * 5V = 3V
														    Now calculate expected ADC value (3V * 1024)/3.3V = 931 
															Then left shift by 5 for Q15 format (931 * 32) = 29792 = 0x7460 */		 
 
#define PID_BUCK1_VOLTAGE_MIN 	  	  2304				 /* Minimum duty cycle is total dead time (72) left shifted by 5 bits*/

/* This is increment rate to give us desired PID_BUCK1VOLTAGE_REFERENCE. The sofstart takes 50ms */
 #define BUCK1_SOFTSTART_INCREMENT	 ((PID_BUCK1_VOLTAGE_REFERENCE - PID_BUCK1_VOLTAGE_MIN) / 50 ) 

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

extern unsigned int TimerInterruptCount;

void Buck1Drive(void)
{
    /* Buck1 converter setup to output 5V */
    
    PTPER = 2404;                         /* PTPER = ((1 / 400kHz) / 1.04ns) = 2404, where 400kHz 
											 is the desired switching frequency and 1.04ns is PWM resolution. */

    IOCON1bits.PENH = 1;                  /* PWM1H is controlled by PWM module */
    IOCON1bits.PENL = 1;                  /* PWM1L is controlled by PWM module */

    IOCON1bits.PMOD = 0;                  /* Complementary Mode */
    
    IOCON1bits.POLH = 0;                  /* Drive signals are active-high */
    IOCON1bits.POLL = 0;                  /* Drive signals are active-high */

	IOCON1bits.OVRENH = 0;				  /* Disable Override feature for shutdown PWM */  
	IOCON1bits.OVRENL = 0;				  /* Disable Override feature for shutdown PWM */
	IOCON1bits.OVRDAT = 0b00;			  /* Shut down PWM with Over ride 0 on PWMH and PWML */	


    PWMCON1bits.DTC = 0;                  /* Positive Deadtime enabled */
    
    DTR1    = 0x18;                  	  /* DTR = (25ns / 1.04ns), where desired dead time is 25ns. 
										     Mask upper two bits since DTR<13:0> */
    ALTDTR1 = 0x30;               		  /* ALTDTR = (50ns / 1.04ns), where desired dead time is 50ns. 
										     Mask upper two bits since ALTDTR<13:0> */
    
    PWMCON1bits.IUE = 0;                  /* Disable Immediate duty cycle updates */
    PWMCON1bits.ITB = 0;                  /* Select Primary Timebase mode */
    
    FCLCON1bits.FLTMOD = 3;               /* Fault Disabled */
        
    TRGCON1bits.TRGDIV = 0;               /* Trigger interrupt generated every PWM cycle */
    TRGCON1bits.TRGSTRT = 0;              /* Trigger generated after waiting 0 PWM cycles */
    
	PDC1 = 72;                            /* Initial pulse-width [ns] = minimum deadtime required (DTR1 + ALDTR1)*/            
 	TRIG1 = 8;                            /* Trigger generated at beginning of PWM active period */


}

void CurrentandVoltageMeasurements(void)
{
    ADCONbits.FORM = 0;                   /* Integer data format */
    ADCONbits.EIE = 0;                    /* Early Interrupt disabled */
    ADCONbits.ORDER = 0;                  /* Convert even channel first */
    ADCONbits.SEQSAMP = 0;                /* Select simultaneous sampling */
    ADCONbits.ADCS = 5;                   /* ADC clock = FADC/6 = 120MHz / 6 = 20MHz, 
											12*Tad = 1.6 MSPS, two SARs = 3.2 MSPS */
	
    IFS6bits.ADCP0IF = 0;		    	  /* Clear ADC Pair 0 interrupt flag */ 
    IPC27bits.ADCP0IP = 5;			      /* Set ADC interrupt priority */ 
    IEC6bits.ADCP0IE = 1;				  /* Enable the ADC Pair 0 interrupt */

    ADPCFGbits.PCFG0 = 0; 				  /* Current Measurement for Buck 1 */ 
    ADPCFGbits.PCFG1 = 0; 				  /* Voltage Measurement for Buck 1 */

	ADPCFGbits.PCFG4 = 0; 				  /* Voltage Measurement for input voltage source */
    
    ADSTATbits.P0RDY = 0; 				  /* Clear Pair 0 data ready bit */
    ADCPC0bits.IRQEN0 = 1; 				  /* Enable ADC Interrupt for Buck 1 control loop */ 
    ADCPC0bits.TRGSRC0 = 4; 			  /* ADC Pair 0 triggered by PWM1 */


    ADSTATbits.P2RDY = 0; 				  /* Clear Pair 2 data ready bit */
    ADCPC1bits.IRQEN2 = 0;                /* Disable ADC Interrupt for input voltage measurment */
    ADCPC1bits.TRGSRC2 = 4; 			  /* ADC Pair 2 triggered by PWM1 */

    
}

void Buck1VoltageLoop(void)
{
    Buck1VoltagePID.abcCoefficients = Buck1VoltageABC;      /* Set up pointer to derived coefficients */
    Buck1VoltagePID.controlHistory = Buck1VoltageHistory;   /* Set up pointer to controller history samples */
    
    PIDInit(&Buck1VoltagePID); 
                             
	if ((PID_BUCK1_A == 0x7FFF || PID_BUCK1_A == 0x8000) || 
		(PID_BUCK1_B == 0x7FFF || PID_BUCK1_B == 0x8000) ||
		(PID_BUCK1_C == 0x7FFF || PID_BUCK1_C == 0x8000))
	{
		while(1);											/* This is a check for PID Coefficients being saturated */ 
	}																	

	Buck1VoltagePID.abcCoefficients[0] = PID_BUCK1_A;		/* Load calculated coefficients */	
	Buck1VoltagePID.abcCoefficients[1] = PID_BUCK1_B;
	Buck1VoltagePID.abcCoefficients[2] = PID_BUCK1_C;
   
	Buck1VoltagePID.controlReference = PID_BUCK1_VOLTAGE_MIN;

	Buck1VoltagePID.measuredOutput = 0;       				/* Measured Output is 0 volts before softstart */       	 

}

void Buck1SoftStartRoutine(void)
{
  /* This routine increments the control reference until the reference reaches 
     the desired output voltage reference. In this case the we have a softstart of 50ms */

	while (Buck1VoltagePID.controlReference <= PID_BUCK1_VOLTAGE_REFERENCE)
	{
		Delay_ms(64);										/* orig val of 1 agp */
		Buck1VoltagePID.controlReference += BUCK1_SOFTSTART_INCREMENT;
	}

	Buck1VoltagePID.controlReference = PID_BUCK1_VOLTAGE_REFERENCE;
}


void Delay_ms (unsigned int delay)
{
	TimerInterruptCount = 0;			 /* Clear Interrupt counter flag */

	PR1 = 0x9C40;						 /* (1ms / 25ns) = 40,000 = 0x9C40 */ 
	IPC0bits.T1IP = 4;					 /* Set Interrupt Priority lower then ADC */
	IEC0bits.T1IE = 1;				   	 /* Enable Timer1 interrupts */

	T1CONbits.TON = 1;					 /* Enable Timer1 */

	while (TimerInterruptCount < delay); /* Wait for Interrupt counts to equal delay */

	T1CONbits.TON = 0;					 /* Disable the Timer */
}
