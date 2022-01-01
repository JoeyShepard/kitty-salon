#include <msp430.h>
#include <stdbool.h>

//TODO: Add trail off at end so PWN isn't stuck on
//TODO: Low power mode
//TODO: Test storing cycle and width (ie 3:4 instead of 192 to see if sound cleaner)

//Hardware:
//Resistor on PWN pin calculation:
//
//  0.25W speaker
//  =============
//  First, need to determine how much current through speaker 
//  V = IR and E = IV
//  E = IV = 0.25W = IV = I*(IR) = I*(I*8) = 8*I^2
//  0.25W = 8*I^2
//  I = 177mA
//
//  Check work:
//  V = IR = 0.177A * 8ohms = 1.414V
//  E = IV = 0.177A * 1.414V = 0.25W
//  
//  Base resistor:
//  PN2222A gain is around 50
//  Base current = 177mA / 50 = 3.54mA
//
//  New battery:
//  Base voltage = 3.6v
//  3.6v-0.7v = 2.9v
//  Resistor = 2.9v/0.00354A = 819ohm
//
//  Nearly dead battery
//  Base voltage = 2.7v
//  2.7v-0.7v = 2.0v
//  Resistor = 2.0v/0.00354A = 565ohm
//
//  Conclusion: 1k ohm is closest on hand

#define UART_RXD            BIT1  //P1.1
#define UART_TXD            BIT2  //P1.2
#define EEPROM_CS           BIT3  //P1.3
#define IO_CLOCK            BIT5  //P1.5 
#define IO_MISO             BIT6  //P1.6
#define IO_MOSI             BIT7  //P1.7

#define LED                 BIT1  //P2.1
#define PWM_OUT             BIT6  //P2.6
#define BUTTON              BIT0  //P2.0

#define WAV_BPS 8000
#define WAV_COUNT 5
#define LED_STEPS 10
#define BLINK_PER_SEC 2
#define LED_STEP_TIME ((WAV_BPS/BLINK_PER_SEC)/LED_STEPS)
#define SAMPLE_CYCLES 1500 //12MHz/WAV_BPS
#define PWM_ROLLOVER 256

//Function prototypes
unsigned char SPI_Send(unsigned char data);
void UART_Hex(unsigned char data);
unsigned char UART_Receive();
void UART_Send(unsigned char data);
void UART_Text(const char *data);

unsigned int EEPROM_WaitBusy(unsigned int timeout,bool ms);
int SetPWM(unsigned char freq);

int main(void)
{
    WDTCTL=WDTPW | WDTHOLD;
    
    BCSCTL1=CALBC1_12MHZ;
    DCOCTL=CALDCO_12MHZ;
    
    P1OUT=EEPROM_CS;
    P1DIR=EEPROM_CS;

    P1SEL= IO_CLOCK|IO_MISO|IO_MOSI;
    P1SEL2=IO_CLOCK|IO_MISO|IO_MOSI;

    P2OUT=BUTTON;
    P2DIR=PWM_OUT|LED;
    P2SEL=PWM_OUT;
    P2REN|=BUTTON;  //Pull up
    P2IES|=BUTTON;  //Edge detection
    P2IFG&=~BUTTON; //Clear IFG
    P2IE|=BUTTON;   //Enable interrupt

    //SPI
    UCB0CTL1=UCSWRST;
    UCB0CTL0=UCCKPH|UCMST|UCSYNC|UCMSB;//mode 0
    UCB0CTL1|=UCSSEL_2;
    UCB0BR0=2;
    UCB0BR1=0;

    //PWM
    TA0CCR0=0;              //PWM roll over - off for now
    TA0CCTL1=OUTMOD_7;      //PWM mode

    //Sample timer
    TA1CCR0=0;
    
    //Enable peripherals
    UCB0CTL1&=~UCSWRST;                 //Enable SPI
    TA0CTL=TASSEL_2|MC_1;               //Start PWM timer - main clock, up mode
    TA1CTL=TASSEL_2|MC_1|TACLR;    //Start sample timer - main clock, up mode

    unsigned char sample;
    unsigned long sample_count;
    unsigned int LED_step, LED_dir;
    unsigned long wav_sample_count[]={  WAV_BPS*8.5,    //Track 1: 0-8.5 =      8.5 seconds
                                        WAV_BPS*4,      //Track 2: 9.1-13.1 =   4 seconds
                                        WAV_BPS*3,      //Track 3: 13.8-16.8 =  3 seconds
                                        WAV_BPS*9.4,    //Track 4: 17.4-26.8 =  9.4 seconds
                                        WAV_BPS*7.2     //Track 5: 27.8-35 =   7.2 seconds
                                        };
    unsigned long wav_sample_start[]={  WAV_BPS*0,      //Track 1: 0 seconds
                                        WAV_BPS*9.1,    //Track 2: 9.1 seconds
                                        WAV_BPS*13.8,   //Track 3: 13.8 seconds
                                        WAV_BPS*17.4,   //Track 4: 17.4 seconds
                                        WAV_BPS*27.8,   //Track 5: 27.8 seconds
                                        };
    unsigned int wav_index=0;

    //Eyes on
    P2OUT|=LED;

    //Wait one second at startup
    __delay_cycles(12000000);

    while(1)
    {   
        //LPM and wait for button interrupt
        _enable_interrupts();
        _low_power_mode_4();
        _disable_interrupts();
       
        //Woken from LPM
        sample_count=wav_sample_count[wav_index];
        LED_step=0;
        LED_dir=0;
        P1OUT&=~EEPROM_CS;
        SPI_Send(0x03);
        SPI_Send((wav_sample_start[wav_index]>>16)&0xFF);
        SPI_Send((wav_sample_start[wav_index]>>8)&0xFF);
        SPI_Send((wav_sample_start[wav_index])&0xFF);
        TA0CCR0=PWM_ROLLOVER;   //Start PWM timer
        TA1CCR0=SAMPLE_CYCLES;
        
        while(1)
        {   
            //Next sample
            sample=SPI_Send(0);
            SetPWM(sample);

            //LED effect
            if (sample_count%LED_STEP_TIME==0)
            {
                if (!LED_dir)
                {
                    LED_step++;
                    if (LED_step==LED_STEPS) LED_dir=1;
                }
                else
                {
                    LED_step--;
                    if (LED_step==0) LED_dir=0;
                }
            }
            if ((sample_count%LED_STEPS)<LED_step) P2OUT|=LED;
            else P2OUT&=~LED;

            //Wait on sample time to finish
            while((TA1CTL&TAIFG)==0);
            TA1CTL&=~TAIFG;

            //Next sample
            sample_count--;
            if (sample_count==0) break;
        }
        TA0CCR0=0;  //Stop PWM timer
        TA1CCR0=0;  //Stop sample timer

        P1OUT|=EEPROM_CS;
        P2OUT|=LED;

        wav_index++;
        if (wav_index==WAV_COUNT) wav_index=0;

        __delay_cycles(12000000);
    }
}

void __interrupt_vec(PORT2_VECTOR) Port2_ISR(void)
{
    P2IFG&=~BUTTON;
    _low_power_mode_off_on_exit();
    return;
}

unsigned int EEPROM_WaitBusy(unsigned int timeout,bool ms)
{
  char retval;
  unsigned int timeout_count=0;
  P1OUT&=~EEPROM_CS;
  SPI_Send(0x05);
  do
  {
    if (ms) __delay_cycles(12000);
    retval=SPI_Send(0x00)&1;
    if (++timeout_count==timeout)
    {
      UART_Text("\r\n\r\nEEPROM write timeout exceeded!\r\nStatus register:");
      UART_Hex(SPI_Send(0x05));
      while(1);
    }
  } while(retval);
  P1OUT|=EEPROM_CS;
  return timeout_count;
}
 
int SetPWM(unsigned char freq)
{
    //TA0CTL=MC_0;            //Stop timer
    TA0CCR1=freq;           //New PWM value
    //TA0CTL=TASSEL_2+MC_1;   //Restart timer - main clock, up mode
    return 0;
}

unsigned char SPI_Send(unsigned char data)
{
    unsigned char buff;
    while(!(UC0IFG&UCB0TXIFG));
    UCB0TXBUF=data;
    while (UCB0STAT & UCBUSY);
    
    buff=UCB0RXBUF;
    return buff;
}

void UART_Hex(unsigned char data)
{
    unsigned char buff;
    buff=data/16;
    if (buff>9) buff+=55;
    else buff+='0';
    UART_Send(buff);
    buff=data%16;
    if (buff>9) buff+=55;
    else buff+='0';
    UART_Send(buff);
}

unsigned char UART_Receive()
{
    while (!(UC0IFG&UCA0RXIFG));
    return UCA0RXBUF;
}

void UART_Send(unsigned char data)
{
    while(!(UC0IFG&UCA0TXIFG));
    UCA0TXBUF=data;
    
    while (UCA0STAT & UCBUSY);
}

void UART_Text(const char *data)
{
    int i=0;
    while (data[i]) UART_Send(data[i++]);
}


