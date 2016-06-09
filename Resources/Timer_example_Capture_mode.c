//***********************************************************************
//  MSP-430FF5529 Demo - Timer_A0, Capture of S2 switch using SMCLK
//
//  Description: Input capture of S2 on P1.1(TA0)
//  Run to breakpoint at the _NOP() instruction to see 16 capture
//  values and the differences.
//  SMCLK = default ~1000kHz
//
//
//                MSP430F5529
//             -----------------
//         /|\|              XIN|-
//          | |                 | 32kHz
//          --|RST          XOUT|-
//            |                 |
//            |       			|
//            |                 |
//            |         P1.1/TA0|
//            |                 |
//            |             	|

//Bilal

//******************************************************************************

#include <msp430.h>
#include <stdint.h>

uint16_t new_cap=0;
uint16_t old_cap=0;
uint16_t cap_diff=0;

uint16_t diff_array[16];                // RAM array for differences
uint16_t capture_array[16];             // RAM array for captures
uint8_t index=0;
uint8_t count = 0;

int main(void)
{
  volatile unsigned int i;
  WDTCTL = WDTPW + WDTHOLD;                 // Stop watchdog timer
  for (i=0; i<20000; i++)                   // Delay for crystal stabilization
  {
  }


  	P4DIR |= BIT7;							//Green Led


    P1SEL = BIT1;                             // Set P1.1 to TA0, Now P1.1 will cause a timer interrupt


  TA0CCTL0 = CM_3 | SCS | CCIS_0 | CAP | CCIE; // both edges, synchronized, Continuous up mode, Capture mode

  TA0CTL = TASSEL__SMCLK | MC_2;                  // SMCLK + Continuous Up Mode


  __enable_interrupt();

  while (1)
  {
	  __low_power_mode_0();
  }
}


#pragma vector=TIMER0_A0_VECTOR
__interrupt void TimerA0(void)
{
   new_cap = TA0CCR0;
   cap_diff = new_cap - old_cap;

   diff_array[index] = cap_diff;            // record difference to RAM array
   capture_array[index++] = new_cap;
   if (index == 16)
   {
     index = 0;
     P4OUT ^= BIT7;                         // Toggle P4.7 using exclusive-OR
   }
   old_cap = new_cap;                       // store this capture value
   count ++;
   if (count == 32)
   {
     count = 0;
     __no_operation();                      // SET BREAKPOINT HERE
   }

}
