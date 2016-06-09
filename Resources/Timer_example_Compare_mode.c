//Using two timers to blink two LEDs.

#include <msp430.h>
#include <stdint.h>
//#include "intrinsics.h"

#define Red_led 		BIT0
#define Green_led		BIT7
#define one_forth       8192
#define one_half		16384

int main(void) {

    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer

    // Indicator LED
    P1DIR |= BIT0;
    P4DIR |= BIT7;

    TA2CCR0 = one_forth;           	//Channel 0 of Timer A2. No need to clear CCIFG, it is cleared Automatically.
    TA2CCTL0= CCIE;					//Interrupt Enable
    TA2CTL = TASSEL_1|MC_1|ID_0|TACLR; //Upmode, ACLK, /1, TA2R is cleared



    TBCCR0 = one_forth;           	//Channel 0 of Timer B. No need to clear CCIFG, it is cleared Automatically.
    TBCCTL0= CCIE;					//Interrupt Enable
    TBCTL = TBSSEL_1|MC_1|ID_1|TBCLR; //Upmode, ACLK, /2, TB0R is cleared

    __enable_interrupt();

    while(1)
    {
    	__low_power_mode_0();
    }
	return 0;
}

#pragma vector = TIMER2_A0_VECTOR
__interrupt void TIMERA0_ISR (void)
{
  P1OUT= P1OUT ^ Red_led;            //Toggle Red Led
}




#pragma vector = TIMERB0_VECTOR
__interrupt void TIMERB0_ISR (void)
{
  P4OUT= P4OUT ^ Green_led;            //Toggle Green Led
}
