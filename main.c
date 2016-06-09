#include <msp430.h> 
#include <intrinsics.h>
#include <stdint.h>

/*
 * main.c
 */


#define ONE_SEC_DELAY 4096		//ACLK, /8, 1 s
#define TRIGGER_DELAY 590		//ACLK, /1, 18 ms

#define CAPTURE_MODE 0
#define DISP_MODE 1

#define HIGH_THRESH 100

#define LEFT_LED 0
#define RIGHT_LED 1

#define TRIGGER_ON 1
#define TRIGGER_OFF 0


//7-seg
#define SEG_A   BIT0          //  AAAA
#define SEG_B   BIT1          // F    B
#define SEG_C   BIT2          // F    B
#define SEG_D   BIT3          //  GGGG
#define SEG_E   BIT4          // E    C
#define SEG_F   BIT5          // E    C
#define SEG_G   BIT6          //  DDDD
#define SEGS_ALL (SEG_A|SEG_B|SEG_C|SEG_D|SEG_E|SEG_F|SEG_G)

static const uint8_t SEGS_NUM[16] = {
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,        // "0"
  SEG_B | SEG_C,                                        // "1"
  SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                // "2"
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                // "3"
  SEG_B | SEG_C | SEG_F | SEG_G,                        // "4"
  SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                // "5"
  SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,        // "6"
  SEG_A | SEG_B | SEG_C,                                // "7"
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,// "8"
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,        // "9"
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,        // "A"
  SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,                // "b"
  SEG_A | SEG_D | SEG_E | SEG_F,                        // "C"
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,                // "d"
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,                // "E"
  SEG_A | SEG_E | SEG_F | SEG_G                         // "F"
};

//funtion prototypes
void convert_data_to_bin();
void show_hum();
void set(volatile unsigned int LED);
void convert_data_to_bin();
void show_error();
void show_temp();

//global variables
volatile unsigned int prog_state = DISP_MODE;
volatile unsigned int data_cap_state = TRIGGER_ON;
volatile unsigned int chksum_flag = 0;

uint16_t new_cap = 0;
uint16_t old_cap = 0;
uint16_t cap_diff = 0;

uint16_t diff_arr[42];			//40 (bits) + 1 (20 us) + 1 (ACK)
uint16_t cap_arr[42];
uint16_t index = 0;

uint16_t hum_bin = 0;		//response variables
uint16_t temp_bin = 0;
uint16_t chksum_bin = 0;

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
	
    //sensor data pin
    P2DIR |= BIT3;				//set to output
    P2OUT |= BIT3;

    P6DIR |= BIT0|BIT1|BIT2|BIT3|BIT4|BIT5|BIT6;	//A,B,C,D,E,F,G
	P4DIR |= BIT1|BIT2;								//Clock Pulse Left and Right

    __enable_interrupt();

    while(1){

    	if (prog_state == DISP_MODE){

    	    		convert_data_to_bin();					//also sets chksum_flag

    	    		P2SEL &= ~BIT3;				//set sensor output to HIGH during IDLE
    	    		P2DIR |= BIT3;
					P2OUT |= BIT3;

    	    		TA1CCR0 = ONE_SEC_DELAY;				//display timer
    	    		TA1CTL = TASSEL_1 | MC_1 | ID_3 | TACLR;		//ACLK, Upmode, /8, TA0R is cleared
    	    		TA1CCTL0 = CCIE;					//interrupt enable

    	    		(chksum_flag) ? show_hum() : show_error();			//show humidity
    	    		__low_power_mode_0();
    	    		(chksum_flag) ? show_temp() : show_error();			//show temperature
    	    		__low_power_mode_0();

    	    		TA1CCTL0 = ~CCIE;				//disable display timer
    	    		prog_state = CAPTURE_MODE;
    	    		data_cap_state = TRIGGER_ON;
    	}
    	else if (prog_state == CAPTURE_MODE){
    		if (data_cap_state == TRIGGER_ON){

				P2DIR |= BIT3;					//pull down for 18 ms
				P2OUT &= ~BIT3;

				TA0CCR0 = TRIGGER_DELAY;
				TA0CCTL0 = CCIE;					//trigger interrupt enable
				TA0CTL = TASSEL_1 | MC_1 | ID_0 | TACLR;		//ACLK, Upmode, /1, TA0R is cleared

				data_cap_state = TRIGGER_OFF;
    		}
    	}

    }

}

//1s display timer
#pragma vector = TIMER1_A0_VECTOR
__interrupt void timerA1_ISR(void){

	__low_power_mode_off_on_exit();
}

//Sensor trigger timer
#pragma vector = TIMER0_A0_VECTOR
__interrupt void timerA0_ISR (void){

	if(data_cap_state == TRIGGER_OFF){
		TA0CCTL0 = ~CCIE;		//disable trigger interrupt
		P2DIR &= ~BIT3;			//sensor input

		//start response capture timer
		P2SEL |= BIT3;									//sensor pin - triggers interrupt
		TA2CCTL0 = CM_2 | SCS | CCIS_0 | CAP | CCIE; 	// falling edge, synchronized, Continuous up mode, Capture mode, Enable interrutps
		TA2CTL = TASSEL__SMCLK | MC_2 | TACLR;                  // SMCLK + Continuous Up Mode
	}


}

//Sensor response capture timer
#pragma vector = TIMER2_A0_VECTOR
__interrupt void timer2_ISR (void){
		new_cap = TA2CCR0;		//TA2R;
		cap_diff = new_cap - old_cap;

		diff_arr[index] = cap_diff;            // record difference to RAM array
		cap_arr[index++] = new_cap;
		old_cap = new_cap;                       // store this capture value

		if (index == 42)		//end of response
		{
			index = 0;
			old_cap = 0;

			TA2CCTL0 = ~CCIE;				//disable data capture timer

			prog_state = DISP_MODE;
		}

}

void show_hum(){

	int i;
	int hum = hum_bin;

	for(i=0; i<2; i++){
		P6OUT = ~SEGS_NUM[hum%10];			//Common Anode 7-seg
		(i==0)? set(RIGHT_LED) : set(LEFT_LED);		//first set Right and then Left LED
		hum /= 10;			//second digit
	}


}

void show_temp(){

	unsigned int i;
	unsigned int temp = temp_bin;

	for(i=0; i<2; i++){
		P6OUT = ~SEGS_NUM[temp%10];			//Common Anode 7-seg
		(i==0)? set(RIGHT_LED) : set(LEFT_LED);		//first set Right and then Left LED
		temp /= 10;			//second digit
	}


}

void show_error(){

		P6OUT = ~SEG_G;			//Common Anode 7-seg
		set(RIGHT_LED);			//first set Right and then Left LED
		set(LEFT_LED);
}

void convert_data_to_bin(){
		//diff_arr indexes:	0 - Resp. start, 1 - ACK, [2:17] - HUM, [18:33] - Temp, [34:41] - Chk. sum

		int i = 0;
		int chksum_calc = 0;

		hum_bin = 0;		//clear data
		temp_bin = 0;
		chksum_bin = 0;

		chksum_flag = 0;

		//extract data
		for (i=2; i<=17; i++){		//HUMIDITY
			if(diff_arr[i]>HIGH_THRESH){
				hum_bin |= (BIT0 << (17-i));		//shift: 15-i+2
			}
		}

		for (i=18; i<=33; i++){		//Temperature
			if(diff_arr[i]>HIGH_THRESH){
				temp_bin |= (BIT0 << (33-i));		//shift: 15-i+18
			}
		}

		for (i=34; i<=41; i++){		//Chk. sum
			if(diff_arr[i]>HIGH_THRESH){
				chksum_bin |= (BIT0 << (41-i));		//shift: 7-i+34
			}
		}

		//perform check sum
		chksum_calc = (hum_bin >> 8)  +  ((hum_bin << 8) >> 8) + (temp_bin >> 8)  +  ((temp_bin << 8) >> 8);		//logical shifts

		//check sum flag
		chksum_flag = ((chksum_calc & 255) == chksum_bin)? 1 : 0;			//8-bit check sum

		//final values after removing the fractional part
		hum_bin = (int) hum_bin/10;
		temp_bin = (int) temp_bin/10;

}

void set(volatile unsigned int LED){		//0 - LEFT, 1 - RIGHT
	if(LED == LEFT_LED){
		P4OUT &= ~BIT1;			//Spin Clock
		P4OUT |= BIT1;
	}
	else if(LED == RIGHT_LED){
		P4OUT &= ~BIT2;			//Spin Clock
		P4OUT |= BIT2;
	}

}
