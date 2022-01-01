  unsigned char xmodem;
  bool xmodem_done=false;
  unsigned long xmodem_address=0;
  bool xmodem_received=false;
  unsigned long t0;
  
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
	  char b0=UART_Receive(0);
	  char b1=UART_Receive(0);

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
  