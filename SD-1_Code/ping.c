#include "ping.h"
#include "Timer.h"
#include "lcd.h"

// If no echo edge pair arrives within this long, give up instead of freezing
#define PING_TIMEOUT_US     30000
// Value returned on timeout (16 MHz ticks, ~18.5 ms = "no object" for the PING)
#define PING_TIMEOUT_TICKS  296000
// Quiet time after each reading so the sensor can reset before the next trigger
#define PING_SETTLE_MS      5

int overflows = 0;
int pulse;

void send_pulse();

// Initialize the Ping))) Sensor
void ping_init()
{
    // initialize registers
    SYSCTL_RCGCGPIO_R |= 0x02;     //Enables the clock for port B run mode Ch.5 Reg 60
    SYSCTL_RCGCTIMER_R |= 0x08;    //Enables clock to 16/32 bit general-purpose timer module 3 to run mode Ch.5 Reg 59

    GPIO_PORTB_DEN_R |= 0x08;      // enables PB3 digital function  Ch.10 Reg.18
    GPIO_PORTB_AFSEL_R |= 0x08;    // sets PB3 to peripheral      Ch.10 Reg 10
    GPIO_PORTB_PCTL_R &= ~0x0000F000;  //flips the bits allowing it to clear PB3's config.  ch.10 Reg 22
    GPIO_PORTB_PCTL_R |= 0x00007000;   //Changes the alternate function for PB3 to detect edges of the echo pulse from Timer 3B | 0111 selects T3CCP1(Timer 3B capture input) ch.10 Reg 22

    TIMER3_CTL_R &= ~0x100;          //Timer 3B disable while configuring it       ch.11 Reg 4
    TIMER3_CFG_R = 0x4;             //Configures Timer 3 as 16 bits               ch.11 Reg 1
    TIMER3_TBPR_R = 0xFF;           //Configures Timer 3 additional 8 bits        ch.11 Reg 15
    TIMER3_TBILR_R = 0xFFFF;        //Sets the lower 16 bits of the timer's starting value to the 16 bits max                ch.11 Reg 11
    TIMER3_TBMR_R |= 0x0007;         //Configs Timer 3B to capture the value when an input edge occurs | TBCMR                ch.11 Reg 3
    TIMER3_CTL_R |= 0xC00;          //Detects both rising and falling edged        ch.11 Reg 4
    TIMER3_ICR_R = 0x400;          //Clears the capture-event interrupt flag      ch.11 Reg 9
    TIMER3_IMR_R |= 0x400;          //Enables the timer's capture event interrupt  ch.11 Reg 6
    TIMER3_CTL_R |= 0x100;          //Enables Timer 3B                             ch.11 Reg 4

    NVIC_EN1_R |= 0x10;                 //we use timer3B which uses interrupt 36, therefore we enable interrupt 36, which is bit 4 for EN1      ch.3 Reg 38

    IntRegister(INT_TIMER3B, TIMER3B_Handler);  //connects TIMER3B_handler as the ISR for TIMER 3B
    IntMasterEnable(); //globally enables all interrupts called from driverlib/interrupt.h
}

// Triggers the sensor and returns the echo pulse width in 16 MHz clock ticks.
// Never blocks forever: on a missed echo it returns PING_TIMEOUT_TICKS.
unsigned int ping_read()
{
    unsigned int start;

    // send signal to sensor, wait for isr to collect rising
    // and falling edges and return the pulse width
    state = LOW;
    send_pulse();

    start = timer_getMicros();
    while (state != DONE)
    {
        if ((timer_getMicros() - start) > PING_TIMEOUT_US)
        {
            TIMER3_IMR_R &= ~0x400;     // stop capture interrupts until the next trigger
            timer_waitMillis(PING_SETTLE_MS);
            return PING_TIMEOUT_TICKS;
        }
    }

    // Timer counts DOWN, so the rising edge value is larger than the falling
    // edge value. Masking to 24 bits also handles a wrap-around.
    pulse = (rising_time - falling_time) & 0xFFFFFF;

    timer_waitMillis(PING_SETTLE_MS);
    return pulse;
}

// ISR: Capture rising edge and falling edge time of PING sensor
void TIMER3B_Handler()
{
    // store timer values when interrupts are triggered
    if (TIMER3_MIS_R & 0x400)       //Checks if interrupt was caused by Timer 3B
    {
        // The pin level tells us which edge this really was, so a stray edge
        // caused by re-enabling the timer after the trigger pulse is ignored.
        if (state == LOW && (GPIO_PORTB_DATA_R & 0x08))
        {
            rising_time = TIMER3_TBR_R; //stores the rising edge value (GPTMTBR)
            state = HIGH;
        }
        else if (state == HIGH && !(GPIO_PORTB_DATA_R & 0x08))
        {
            falling_time = TIMER3_TBR_R; //stores the falling edge value
            state = DONE;
        }

        TIMER3_ICR_R = 0x400;      //Clears event flag when it handles it
    }
}

// Start a Ping))) read
void send_pulse()
{
    TIMER3_CTL_R &= ~0x100;     //Disables Timer 3B while PB3 is used as GPIO output    ch.11 Reg 4
    TIMER3_IMR_R &= ~0x400;     //Masks capture event interrupts during trigger pulse   ch.11 Reg 6

    GPIO_PORTB_AFSEL_R &= ~0x08;    //Turns off PB3's alternate function so it works as a normal GPIO pin   ch.10 Reg 10
    GPIO_PORTB_DIR_R |= 0x08;       //Sets PB3 data direction to output so the pulse can be send            ch.10 Reg 2

    GPIO_PORTB_DATA_R &= ~0x08;     //Drive PB3 low (start of low-high-low pulse)       ch.10 Reg 1
    GPIO_PORTB_DATA_R |= 0x08;      //Drive PB3 high (the trigger pulse)                ch.10 Reg 1

    timer_waitMicros(5);            //Holds the pulse high for 5 Microseconds

    GPIO_PORTB_DATA_R &= ~0x08;     //Drives PB3 low again (end of trigger pulse)       ch.10 Reg 1

    GPIO_PORTB_DIR_R &= ~0x08;      //Release the line: PB3 must be an INPUT so the sensor can drive the echo   ch.10 Reg 2
    GPIO_PORTB_AFSEL_R |= 0x08;     //Gives PB3 back to timer to detect echo edges      ch.10 Reg 10

    TIMER3_ICR_R = 0x400;           //Clears any stale capture event interrupt flag     ch.11 Reg 9
    TIMER3_CTL_R |= 0x100;          //Re-enables Timer 3B                       ch.11 Reg 4
    TIMER3_ICR_R = 0x400;           //Clears any edge flagged by re-enabling the timer
    TIMER3_IMR_R |= 0x400;          //Re-enabled the capture event interrupt    ch.11 Reg 6
}
