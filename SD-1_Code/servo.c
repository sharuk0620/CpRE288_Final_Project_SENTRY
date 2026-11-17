#include <inc/tm4c123gh6pm.h>
#include "Timer.h"
#include "servo.h"

//243250 RIGHT(0)   //1204000 LEFT(180) //cyBOT17
//322000 RIGHT(0)   //1319500 LEFT(0)   //cyBOT26
//259000 RIGHT(0)   //1198750 LEFT(180) //cyBOT12
//295750 RIGHT(0)   //1256500 LEFT(180) //cyBOT05
//248500 RIGHT(0)   //1277500 LEFT(180) //cyBOT22
//259000 RIGHT(0)   //1251250 LEFT(180) //cyBOT20
//259000 RIGHT(0)   //1251250 LEFT(180) //cyBOT25
//238000 RIGHT(0)   //1225000 LEFT(180) //cyBOT02
//227500 RIGHT(0)   //1225000 LEFT(180) //cyBOT01
//280000 RIGHT(0)   //1230250 LEFT(180) //cyBOT04
//217000 RIGHT(0)   //1198750 LEFT(180) //cyBOT28
#define COUNT_AT_0    40000
#define COUNT_AT_180  11666

// Last angle commanded, used to decide how long to wait after a move
static float last_degrees = 90;

// Initialize Timer 1B to generate the servo PWM signal on PB5
void servo_init(void)
{
    SYSCTL_RCGCGPIO_R |= 0x02;      // enable clock for Port B                      Ch.5 Reg 60
    SYSCTL_RCGCTIMER_R |= 0x02;     // enable clock for Timer 1                     Ch.5 Reg 59

    TIMER1_CTL_R &= ~0x100;                                         // disable Timer 1B (TBEN = 0) while configuring                        Ch.11 Reg 4
    TIMER1_CTL_R |= 0x4000;                                         // TBPWML = 1: invert output so match = HIGH pulse width                Ch.11 Reg 4
    TIMER1_CFG_R = 0x4;                                             // split Timer 1 into two 16-bit timers                                 Ch.11 Reg 1
    TIMER1_TBMR_R = 0xA;                                            // PWM mode (TBAMS = 1), edge-count (TBCMR = 0), periodic (TBMR = 2)    Ch.11 Reg 3
    TIMER1_TBPR_R = 0x04;                                           // upper 8 bits of the start value (prescaler)                          Ch.11 Reg 15
    TIMER1_TBILR_R = 0xE200;                                        // lower 16 bits of start value: 0x4E200 = 320,000 ticks = 20 ms        Ch.11 Reg 11
    TIMER1_TBPMR_R = 0;                                             // upper 8 bits of the match value                                      Ch.11 Reg 17
    TIMER1_TBMATCHR_R = (COUNT_AT_0 + COUNT_AT_180) / 2;            // lower 16 bits of match, starts at 90 degrees                         Ch.11 Reg 13
    TIMER1_CTL_R |= 0x100;          // enable Timer 1B (TBEN = 1), PWM starts       Ch.11 Reg 4

    GPIO_PORTB_DEN_R |= 0x20;           // enable digital function for PB5                                                      Ch.10 Reg 18
    GPIO_PORTB_PCTL_R &= ~0x00F00000;   // clear PB5 function field                                                             Ch.10 Reg 22
    GPIO_PORTB_PCTL_R |= 0x00700000;    // select function 7 = T1CCP1 (Timer 1B)                                                Ch.10 Reg 22
    GPIO_PORTB_AFSEL_R |= 0x20;         // give PB5 to the timer, done last so the pin never sees a half-configured signal      Ch.10 Reg 10
}

// Move the servo to an angle (0-180), wait for it to arrive, return the match value used
int servo_move(float degrees)
{
    int match;          // match value (high pulse width in ticks)
    float distance;     // degrees the servo has to travel

    if (degrees < 0)
        degrees = 0;
    if (degrees > 180)
        degrees = 180;

    // straight line between the 0 and 180 degree counts: y = m*x + b
    match = ((COUNT_AT_180 - COUNT_AT_0) * degrees / 180.0) + COUNT_AT_0;

    TIMER1_TBMATCHR_R = match; // write new match, servo starts moving     Ch.11 Reg 13

    distance = degrees - last_degrees; // signed distance from the last position
    if (distance < 0)
        distance = -distance;     // make it positive
    last_degrees = degrees;             // remember for the next move

    // servo has no feedback, so wait 20 ms (one period) plus 4 ms per degree
    timer_waitMillis(20 + distance * 4);

    return match;
}
