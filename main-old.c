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

//Sound data defined in wav-data.c
extern const unsigned int wav_data_size;
extern const unsigned char wav_data[];

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
    
    BCSCTL1=CALBC1_16MHZ;
    DCOCTL=CALDCO_16MHZ;
    
    P1OUT=EEPROM_CS;
    P1DIR=EEPROM_CS;

    P1SEL= IO_CLOCK|IO_MISO|IO_MOSI|UART_RXD|UART_TXD;
    P1SEL2=IO_CLOCK|IO_MISO|IO_MOSI|UART_RXD|UART_TXD;

    P2DIR=PWM_OUT|LED;
    P2SEL=PWM_OUT;

    //UART
    UCA0CTL1=UCSWRST|UCSSEL_2;
    UCA0CTL0 = 0;
    //9.6k
    UCA0MCTL = UCBRS_5+UCBRF_0;
    UCA0BR0 = 0x82;
    UCA0BR1 = 0x06;

    //SPI
    UCB0CTL1=UCSWRST;
    UCB0CTL0=UCCKPH|UCMST|UCSYNC|UCMSB;//mode 0
    UCB0CTL1|=UCSSEL_2;
    UCB0BR0=2;
    UCB0BR1=0;

    //PWM
    TA0CCR0=256;            //PWM roll over
    TA0CCTL1=OUTMOD_7;      //PWM mode

    //Enable peripherals
    UCA0CTL1&=~UCSWRST;     //Enable UART
    UCB0CTL1&=~UCSWRST;     //Enable SPI
    TA0CTL=TASSEL_2+MC_1;   //Start PWM timer - main clock, up mode

    unsigned char xmodem;
    bool xmodem_done=false;
    unsigned long xmodem_address=0;
    bool xmodem_received=false;
    unsigned long t0;

    UART_Text("Contents of EEPROM:\r\n");
	P1OUT&=~EEPROM_CS;
	SPI_Send(0x03);
    SPI_Send(0x0);
    SPI_Send(0x0);
    SPI_Send(0x0);
    for (int i=0;i<1024;i++)
    {
        UART_Hex(SPI_Send(0));
        UART_Send(' ');
    }
    P1OUT|=EEPROM_CS;
    UART_Text("\r\n");


    /*
    unsigned char eeprom_status;
    UART_Text("Erasing EEPROM...\r\n");
    //Write enable
    P1OUT&=~EEPROM_CS;
    SPI_Send(0x06);
    P1OUT|=EEPROM_CS;
	//Erase chip
    P1OUT&=~EEPROM_CS;
    SPI_Send(0xC7);
    P1OUT|=EEPROM_CS;
    //Poll status flag
    P1OUT&=~EEPROM_CS;
    SPI_Send(0x05);
    while(true)
    {
        __delay_cycles(16000000);
        eeprom_status=SPI_Send(0x05);
        if (eeprom_status&1) UART_Send('.');
        else break;
    }
    P1OUT|=EEPROM_CS;
    UART_Text("Erasing done\r\n");
    */

    UART_Text("Awaiting XMODEM...\r\n");
    xmodem_received=false;
    xmodem_address=0;
    while (!xmodem_received)
    {
	    UART_Send(0x15);    //NAK
	    t0=1;
	    while ((!(UC0IFG&UCA0RXIFG))&&(t0!=0))
	    {
	        if (t0++==0x100000) t0=0;
	    }
	    if (t0!=0)
	    {
	        xmodem_received=true;
	    }
    }
    //Upon exit, first character is waiting in UCA0RXBUF
  
    xmodem_done=false;
    while (!xmodem_done)
    {
	    xmodem=UART_Receive(0);
	
	    if (xmodem==0x01)         //SOH
	    {
            //Begin transfer
            UART_Receive(0);
            UART_Receive(0);

	        unsigned char xmodem_data[128];
	        for (int i=0;i<128;i++)
		        xmodem_data[i]=UART_Receive(0);
      
	        UART_Receive(0);        //ignore checksum
	  
	        P1OUT&=~EEPROM_CS;
		    SPI_Send(0x06);
	        P1OUT|=EEPROM_CS;
	        P1OUT&=~EEPROM_CS;
		    SPI_Send(0x02);
		    SPI_Send((xmodem_address>>16)&0xFF);
		    SPI_Send((xmodem_address>>8)&0xFF);
		    SPI_Send((xmodem_address)&0xFF);
		    for (int i=0;i<128;i++) SPI_Send(xmodem_data[i]);		
	        P1OUT|=EEPROM_CS;
	        EEPROM_WaitBusy(1000,true);
	  
	        xmodem_address+=128;

	        UART_Send(0x06);          //ACK
	    }
	    else if (xmodem==0x04)    //EOT
	    {
	        UART_Text("Done\r\n\r\n");
	        xmodem_done=true;
	    }
	    else
	    {
	        UART_Text("Failed: ");
	        UART_Hex(xmodem);
	        UART_Text("\r\n\r\n");
	        xmodem_done=true;
	    }
    }

    //EEPROM model number
    while(1)
    {
        
        P1OUT&=~EEPROM_CS;
        SPI_Send(0x9F);
        UART_Hex(SPI_Send(0));
        UART_Hex(SPI_Send(0));
        UART_Hex(SPI_Send(0));
        UART_Text("\r\n");
        P1OUT|=EEPROM_CS;
        
        __delay_cycles(16000000);
    }

    //Play recording from internal flash
    unsigned int sample_count;
    const unsigned char *sample_ptr;
    while(1)
    {   
        sample_count=wav_data_size;
        sample_ptr=wav_data;

        while(1)
        {   
            SetPWM(*sample_ptr++);
            //8,000 samples/s, 8 bits/sample
            //16,000,000/8,000 = ~2000 cycles/sample
            //Delay 2,000 while PWM drives speaker
            __delay_cycles(2000);
            sample_count--;
            if (sample_count==0)
            {
                //Leave PWN on to prevent click
                //TA0CTL=MC_0;        //Stop PWM timer
                break;
            }
        }

        //Toggle LED to show still alive
        P2OUT^=BIT1; 

        //Send message out of UART
        UART_Text("Done!");

        //Delay one second between plays
        __delay_cycles(16000000);

        //Toggle LED to show still alive
        P2OUT^=BIT1; 

    }
}

unsigned int EEPROM_WaitBusy(unsigned int timeout,bool ms)
{
  char retval;
  unsigned int timeout_count=0;
  P1OUT&=~EEPROM_CS;
  SPI_Send(0x05);
  do
  {
    if (ms) __delay_cycles(16000);
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


