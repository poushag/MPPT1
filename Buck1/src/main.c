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

_FOSCSEL(FNOSC_FRC)
_FOSC(FCKSM_CSECMD & OSCIOFNC_ON)
_FWDT(FWDTEN_OFF)
_FPOR(FPWRT_PWR128)// & BOREN_OFF)
_FICD(ICS_PGD2 & JTAGEN_OFF)

#define INPUTUNDERVOLTAGE 391					/* Input voltage <7V --> 2.2k/(10k+2.2k)*7V = 1.2623V
												Now calculate the ADC expected value = 1.2623/3.3*1023 = 391 */
#define INPUTOVERVOLTAGE 839						/* Input voltage >15V --> 2.2k/ (10k+2.2k)*15 = 2.70492V
												Now calculate the ADC expected value  = 2.70492/3.3*1023 = 839 */

int main(void)
{

	int InputVoltage;
	int InputCurrent;

	/* Configure Oscillator to operate the device at 40Mhz
	   Fosc= Fin*M/(N1*N2), Fcy=Fosc/2
 	   Fosc= 7.37*(43)/(2*2)=80Mhz for Fosc, Fcy = 40Mhz */

	/* Configure PLL prescaler, PLL postscaler, PLL divisor */
	PLLFBD = 41; 							/* M = PLLFBD + 2 */
	CLKDIVbits.PLLPOST = 0;   				/* N1 = 2 */
	CLKDIVbits.PLLPRE = 0;    				/* N2 = 2 */

    __builtin_write_OSCCONH(0x01);			/* New Oscillator selection FRC w/ PLL */
    __builtin_write_OSCCONL(0x01);  		/* Enable Switch */
      
	while(OSCCONbits.COSC != 0b001);		/* Wait for Oscillator to switch to FRC w/ PLL */  
    while(OSCCONbits.LOCK != 1);			/* Wait for Pll to Lock */

	/* Now setup the ADC and PWM clock for 120MHz (& after hardcoded 16x multiplier applied, 1.04ns res is achieved)	;agp
	   ((FRC * 16) / APSTSCLR ) = (7.37MHz * 16) / 1 = 117.9MHz*/
	
	ACLKCONbits.FRCSEL = 1;					/* FRC provides input for Auxiliary PLL (x16) */
	ACLKCONbits.SELACLK = 1;				/* Auxiliary Ocillator provides clock source for PWM & ADC */
	ACLKCONbits.APSTSCLR = 7;				/* Divide Auxiliary clock by 1 */
	ACLKCONbits.ENAPLL = 1;					/* Enable Auxiliary PLL */
	
	while(ACLKCONbits.APLLCK != 1);			/* Wait for Auxiliary PLL to Lock */

    Buck1Drive();							/* PWM Setup for 5V Buck1 */			
    CurrentandVoltageMeasurements();		/* ADC Setup for Buck1 */
    Buck1VoltageLoop();    					/* Initialize Buck1 PID */

    ADCONbits.ADON = 1;						/* Enable the ADC */
    PTCONbits.PTEN = 1;						/* Enable the PWM */

    Buck1SoftStartRoutine();    			/* Initiate Buck 1 soft start to 5V */

    /* Setup RB12 to enable Buck 1 Output load */
    TRISBbits.TRISB12 = 0;                  /* Set RB12 as digital output; TRISB = EFFFh */
    LATBbits.LATB12 = 0;                    /* If set, this connects on-board load to Buck1 & LATB = 1000h agp */

    while(1)

	
			{
					if (ADSTATbits.P2RDY ==1)
			
					{
						InputVoltage = ADCBUF4;						/* Read input Voltage */
						ADSTATbits.P2RDY = 0;						/* Clear the ADC pair ready bit */
						
					}
	
	
					if (PTCONbits.SESTAT ==1)	// ;agp
			
					{
						InputCurrent = ADCBUF0;						// Read output Current
						InputCurrent *= PDC1;						// mult output Current by on-time
						InputCurrent /= PTPER;						// div prev result by period
//						LATBbits.LATB12 = LATBbits.LATB12;						// cant Clear the status bit
						
					}
	
	
					if ((InputVoltage <= INPUTUNDERVOLTAGE) || (InputVoltage >= INPUTOVERVOLTAGE)) /* if input voltage is less than 
																				    				underVoltage or greater than over 
																									voltage limit shuts down all PWM output */

					{
						IOCON1bits.OVRENH = 1;							/* Over ride the PWM1H to inactive state */		
						IOCON1bits.OVRENL = 1;							/* Over ride the PWM1L to inactive state */ 
					}

			}

}



