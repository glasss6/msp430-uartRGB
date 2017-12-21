/**
 * main.c
 Receive a string of hexadecimal values via UART.
 Control a RGB LED using hardware PWM.
 MSP430F5529
 Stephen Glass
 */

#include <msp430.h>


/* Macros and Prototypes */

#define LED_RED      BIT2
#define LED_GREEN    BIT0
#define LED_BLUE     BIT4
#define LED_STATUS   BIT7

unsigned int curByte = 0;

void initUart(void);
void initLeds(void);
void initPWM(void);

/* Initialize UART */

void initUart(void)
{
    P3SEL    |=  BIT3;      // UART TX
    P3SEL    |=  BIT4;      // UART RX
    UCA0CTL1 |=  UCSWRST;   // Resets state machine
    UCA0CTL1 |=  UCSSEL_2;  // SMCLK
    UCA0BR0   =  6;         // 9600 Baud Rate
    UCA0BR1   =  0;         // 9600 Baud Rate
    UCA0MCTL |=  UCBRS_0;   // Modulation
    UCA0MCTL |=  UCBRF_13;  // Modulation
    UCA0MCTL |=  UCOS16;    // Modulation
    UCA0CTL1 &= ~UCSWRST;   // Initializes the state machine
    UCA0IE   |=  UCRXIE;    // Enables USCI_A0 RX Interrupt
}



/* Initialize LEDs */

void initLeds(void)
{
    P4DIR |= (LED_STATUS);    // Status LED
    P4OUT &= ~(LED_STATUS);   // Status LED default off
    P1DIR |= (LED_RED);       // P1.2 output
    P1SEL |= (LED_RED);       // P1.2 to TA0.1
    P2DIR |= (LED_GREEN);     // P2.0 output
    P2SEL |= (LED_GREEN);     // P2.0 to TA1.1
    P1DIR |= (LED_BLUE);      // P1.4 output
    P1SEL |= (LED_BLUE);      // P1.4 to TA0.3

    P1SEL &= ~(BIT1 + BIT5); // Disable unused GPIO
    P1DIR &= ~(BIT1 + BIT5);
}



/* Initialize PWM */

void initPWM(void)
{
    TA0CTL = (MC__UP + TASSEL__SMCLK + ID_2); // Configure TA0: Upmode using 1MHz clock / 4 = 250k
    TA0CCR0 = 255; // 250k / 255 = ~1kHz, set compare to 255
    TA0CCTL1 = OUTMOD_7; // Set to the output pin (Hardware PWM)
    TA0CCR1  = 255; // Set duty cycle to 0% as defau;t
    TA0CCTL3 = OUTMOD_7;
    TA0CCR3  = 255;

    TA1CTL = (MC__UP + TASSEL__SMCLK + ID_2); // Configure TA1
    TA1CCR0 = 255;
    TA1CCTL1 = OUTMOD_7;
    TA1CCR1 = 255;
}

/* Main */

void main(void)
{
    WDTCTL = WDTPW + WDTHOLD;

    initUart();
    initLeds();
    initPWM();

    __bis_SR_register(LPM0_bits + GIE);
}

/* UART */

#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
    P4OUT |= LED_STATUS;
    switch(__even_in_range(UCA0IV,USCI_UCTXIFG))
    {
        case USCI_NONE: break;
        case USCI_UCRXIFG:
        {
            switch(curByte) // Switch between which byte is being processed
            {
                case 0: // Number of bytes (N) including this byte
                {
                    while(!(UCA0IFG & UCTXIFG)); // Wait until buffer is ready
                    UCA0TXBUF = (int)UCA0RXBUF - 3; // Number of bytes after processing will be current - 3
                    __no_operation();
                    break;
                }
                case 1:
                {
                    TA0CCR1 = 255 - (int)UCA0RXBUF; // Set the correct duty cycle to the RED LED
                    break;
                }
                case 2:
                {
                    TA1CCR1 = 255 - (int)UCA0RXBUF; // Set the correct duty cycle to the GREEN LED
                    break;
                }
                case 3:
                {
                    TA0CCR3 = 255 - (int)UCA0RXBUF; // Set the correct duty cycle to the BLUE LED
                    break;
                }
                default:
                {
                    while(!(UCA0IFG & UCTXIFG)); // Wait until the buffer is ready
                    UCA0TXBUF = UCA0RXBUF; // Everything after RGB is extra that we need to send to next node
                    break;
                }
            }
            if(UCA0RXBUF != 0x0D) curByte++; // Don't increment and do stuff if there's nothing to parse
            else // If we reached the end of the string
            {
                curByte = 0; // Reset and start receiving again
                P4OUT &= ~(LED_STATUS); // Reset the status LED
            }
            break;
        }
        case USCI_UCTXIFG : break;
        default: break;
    }
}
