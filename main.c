// File Description 	: LCD Driver, i2c driver, UI
// Version		: 7.0
// Author Name 		: Kalyan Pingali
// Contact Information	: pnglkalyan@gmail.com

// Revision History :
//      1.0 : Basic LCD functionality achieved
//      1.1 : Added print support to terminal (Serial Comm)
//      2.0 : I2C drivers added
//      3.0 : UI developed to meet requirements of lab 4
//      4.0 : Added functionality for creating custom characters on LCD
//      5.0 : IO Expander added
//      6.0 : Added Real Time Clock on LCD (using interrupts)
//      7.0 : EEPROM and hardware watchdog timer implemented

// Code reuse details :
// Code has been modified and reused for i2c_send_byte and i2c_receive_byte functions from the following sources:
//      1. http://www.robot-electronics.co.uk/i2c-tutorial
//      2. http://www.8051projects.net/wiki/I2C_Implementation_on_8051#Implementing_I2C_in_C

#include <mcs51reg.h>
#include <stdio.h>
#include <stdlib.h>
#include <at89c51ed2.h>				//also includes 8052.h and 8051.h

#define RS P1_5					//RS of LCD
#define RW P1_6					//RW of LCD
#define SCL P1_0				//SCL for I2C
#define SDA P1_1				//SDA for I2C
#define LED P1_3				//LED
#define EEPROM_CONTROL_BITS 0xA0
#define IO_EXPANDER_CONTROL_BITS 0x40
#define RESET_CONTROL_BITS 0xFF
#define IO_EXP_COUNT_LOCATION 15

xdata char *lcddata = 0xEAAA;
int sVal, ssVal, mmVal, timerCount, timerCount1;
int minutes=0, seconds=0, milliseconds=0;
unsigned char *ssValStr, *mmValStr;
unsigned char lcd_current_pointer;
int random_count_value;
int counter_for_io_exp =0;


// Initializes Serial Communication
void initialize_serial_communication()
{
    	TMOD = 0x20; 				// Using timer 1; mode 2
    	TH1 = 0x0FD; 				// Baud rate = 9600
    	SCON = 0x50; 				// Using serial mode 1, 8 bit data, 1 stop bit, 1 start bit
	TR1 = 1; 				// Start timer 1
    	TI = 1; 				// Ready to transmit
    	RI = 1; 				// Ready to receive
    	SBUF = 0;				// Initializing SBUF to 0
}

// Used to overwrite the default startup, by modifying amount of external memory available
void _sdcc_external_startup()
{
	AUXR = AUXR | 0x0C;			// 1 MB of external memory hidden by internal memory
	WDTPRG = 0x07;				// Hardware Watchdog Timer's Timeout time of 2.09 seconds
	//CMOD = CMOD | 0x40;			// Enabling Watchdog timer mode on PCA module 4
}

// Writes a single character over serial instead of Standard Output
void putchar(char c)
{
    	//EA = 1;
    	//ES = 1;
	while (!TI);				// compare asm code generated for these three lines
	//while (TI == 0);
	//while ((SCON & 0x02) == 0);		// wait for TX ready, spin on TI
	SBUF = c;  				// load serial port with transmit value
	TI = 0;  				// clear TI flag
}

// Sends a string of characters over serial
int putstr(char *s)
{
	int i = 0;
	while (*s){				// output characters until NULL found
		putchar(*s++);
		i++;
	}
	return i+1;
}

// Receive character from Serial instead of Standard Input
char getchar()
{
    	//EA = 1;
    	//ES = 1;
	//char cc;
    	while (!RI);				// compare asm code generated for these three lines
	//while ((SCON & 0x01) == 0);		// wait for character to be received, spin on RI
	//while (RI == 0);
	RI = 0;					// clear RI flag
	return SBUF;  				// return character from SBUF
}

// Stall processor for specified number of milli seconds
void delay(unsigned int milli_seconds)  	// Function to provide time delay in msec
{
	int i,j;
	for(i=0;i<milli_seconds;i++)
	{
		for(j=0;j<1275;j++);		// Generates ~1 milli second delay on 8051 using clock of 11.0592 MHz
	}
}

//########################## LCD Specific commands Start here ############################
// Basic execute function to LCD; Commands sent through MMIO (external data)
void lcdcmd(char instruction)
{
	xdata unsigned int *xdata write_address = 0xEAAA;		// Address EAAA => Sets enable within range (0xE000 and 0xEFFF)
	//printf_tiny("DEBUG : Character to write is : %d\n\r\n\r", a);
	RS = 0;								// RS is cleared
	RW = 0;								// Writing mode
	*write_address = instruction;					// Sending data
}

// Initialization sequence for LCD
void lcdinit()
{
    //printf_tiny("DEBUG : lcdinit called\n\r\n\r");				
	delay(25);							// Waiting for more than 15ms
	lcdcmd(0x30);							// Function Set
	delay(10);							// Waiting for more than 4.1ms
	lcdcmd(0x30);							// Function Set
	delay(1);							// Waiting for more than 100us
	lcdcmd(0x30);							// Function Set
    	delay(1);                                              		// Adding delay for additional safety
	lcdcmd(0x38);							// Function Set
	delay(1);                                               	// Adding delay for additional safety
	lcdcmd(0x0F);							// Display Off
	delay(1);                                               	// Adding delay for additional safety
	lcdcmd(0x01);							// Display Clear
	delay(1);                                               	// Adding delay for additional safety
	lcdcmd(0x06);							// Entry Mode Set
	delay(1);                                               	// Adding delay for additional safety
	lcdcmd(0x02);                                           	// Return cursor home
	delay(5);                                               	// Adding delay for additional safety
}

// Stall call to LCD if previous command is still in execution
void lcdbusywait()
{
	//xdata char *lcddata = 0xEAAA;
	RS = 0;
	RW = 1;
	while((*lcddata & 0x80)==1)
	{
        	RS = 0;
        	RW = 1;
    	}
}

// Go to a particular cell of the LCD
void lcdgotoaddr(unsigned char addr)
{
	/*unsigned char current_address;
	RS = 0;
	RW = 1;
	current_address = AC;
	printf_tiny("DEBUG : Address is : %x\n\r\n\r", AC);
	printf_tiny("DEBUG : OR Address is : %x\n\r\n\r", current_address);*/
	lcdbusywait();
	//delay(100);
	//printf_tiny("DEBUG : Data type? CHAR : %c\n\r\n\r",addr);
	//printf_tiny("DEBUG : Data type? X : %x\n\r\n\r",addr);
	//printf_tiny("DEBUG : Data type? INT : %d\n\r\n\r",addr);
    	lcdcmd(128 + addr);
    	//delay(100);
}

// Go to a particular x,y co-ordinate of LCD
void lcdgotoxy(unsigned char row, unsigned char column)
{
	unsigned char address;
    	//printf_tiny("DEBUG : Entered lcdgotoxy\n\r\n\r");
	if(column <= 0x0F)
	{
		//printf_tiny("DEBUG : Reached column less than 0x0F\n\r\n\r");
		if(row == 0x00)
        	{
            		//printf_tiny("DEBUG : Reached 0x00\n\r\n\r");
            		address = column;
        	}
        	else if(row == 0x01)
        	{
        		//printf_tiny("DEBUG : Reached 0x01\n\r\n\r");
            		address = 0x40 + column;
        	}
        	else if(row == 0x02)
        	{
	            	//printf_tiny("DEBUG : Reached 0x02\n\r\n\r");
        	    	address = 0x10 + column;
        	}
		else if(row == 0x03)
        	{
	            	//printf_tiny("DEBUG : Reached 0x03\n\r\n\r");
	            	address = 0x50 + column;
	        }
		else
		{
			//printf_tiny("Warning : Out of bounds co-ordinates specified\n\r");
			return;
		}
	}
	else
	{
		//printf_tiny("Warning : Out of bounds co-ordinates specified\n\r");
		return;
	}
    	lcdbusywait();
	//printf_tiny("DEBUG : Address is : %x\n\r\n\r", address);
	lcdgotoaddr(address);
}

// Write character to LCD at current cursor address
void lcdputch(char cc)
{
	xdata unsigned int *xdata write_address = 0xEAAA;	// Address EAAA => Sets enable within range (0xE000 and 0xEFFF)
	lcdbusywait();
	RS = 1;							// RS set to access registers
	RW = 0;							// Writing mode
	*write_address = cc;
	//printf_tiny("DEBUG : Character to be written is : %c\n\r\n\r", cc);
}

// Write string to LCD starting at current cursor address
void lcdputstr(char *ss)
{

	unsigned char current_address;                          // To calculate the address that the cursor would point to
	unsigned char row;                                      // Extracting row information to feed to lcdgotoxy()
	unsigned char column;                                   // Extracting column information to feed to lcdgotoxy()
	RS = 0;                                                 // Register select disabled
	RW = 1;                                                 // Reading
	current_address = *lcddata & 0x7F;                      // Extracting current address from DDRAM
	if(current_address - 0x50 >= 0x00)                      // If address is greater than 0x50
	{
		row = 0x03;                                     // => Row 3
		column = current_address - 0x50;
	}
	else if(current_address - 0x40 >= 0x00)
	{
		row = 0x01;
		column = current_address - 0x40;
	}
	else if(current_address - 0x10 >= 0x00)
	{
		row = 0x02;
		column = current_address - 0x10;
	}
	else
	{
		row = 0x00;
		column = current_address;
	}
	while(*ss)						// Print to lcd till null found
	{
		lcdgotoxy(row, column);				// Move to cursor position determined by row and column
		lcdbusywait();
		if(column <= 0x0F)
		{
			lcdputch(*ss++);			// Call to write current character at current cursor location
			column++;
		}
		else
		{
			if(row == 0x03)				// If cursor at the end of LCD (3rd row, last column)
			{
			    	row = 0x00;
			    	column = 0x00;
				//printf_tiny("Warning : Overflow caused! Exiting LCD printing\n\r");
				continue;
			}
			row++;
			column = 0x00;
		}
	}
}
//##########################  LCD Specific commands End here  ############################

//########################## I2C EEPROM Specific commands Start here ############################
// EEPROM is external memory! Read : http://ecee.colorado.edu/~mcclurel/Microchip_24LC16B_Datasheet_21703G.pdf

// This function implements the initialization of i2c
void i2cinit(void)
{
    	SCL = 1;
    	SDA = 1;
}


// This function implements the start sequence of i2c
void i2c_start(void)
{
    	SDA = 1;
    	//delay(1);
	SCL = 1;
	//delay(1);
    	SDA = 0;
	//delay(1);
    	SCL = 0;
    	//delay(1);
}


// This function implements the stop sequence of i2c
void i2c_stop(void)
{
    	SDA = 0;
    	//delay(1);
    	SCL = 1;
    	//delay(1);
    	SDA = 1;
	//delay(1);
}


// This function generates the no acknowledgment condition of the master during an i2c read transfer
void i2c_no_ack(void)
{
	SDA = 1;
	SCL = 1;
	SCL = 0;
	SDA = 1;
}


// This function just sends one byte to the initialized address and returns acknowledgment
unsigned char i2c_send_byte(unsigned char databyte)
{
    	unsigned char i, ack_bit;
	for (i = 0; i < 8; i++)
    	{
	        if ((databyte & 0x80) == 0)
			SDA = 0;
		else
			SDA = 1;
		SCL = 1;
	 	SCL = 0;
		databyte = databyte<<1;
    	}
	SDA = 1;
	SCL = 1;
	ack_bit = SDA;
	SCL = 0;
	return ack_bit;
}


// This function just receives one byte to the initialized address and returns data received
unsigned char i2c_receive_byte()
{
    	unsigned char i, rcd_Data=0;
	for (i = 0; i < 8; i++) {				// Loop to read 8 bits
		SCL = 1;                                        // Pull clock high to read next bit of incoming data
		if(SDA)                                         // If incoming bit is a 1, add to sequence, else by 0 by default
			rcd_Data |=1;                           // Adding 1 to data received (0 by default)
		if(i<7)                                         // Keep shifting till you reach the LSB (7 shifts)
			rcd_Data = rcd_Data<<1;                 // Shift operation by 1 bit (to left)
		SCL = 0;                                        // Pull clock low to begin next cycle
	}
	return rcd_Data;                                        // Return received data
}


// This function writes a byte of data to a specified address (0x000-0x7FF) of EEPROM
unsigned char i2c_write_byte(unsigned char pageblock, unsigned char data_address, unsigned char i2cdata)
{
    	unsigned write_ack;
    	unsigned char control_sequence = EEPROM_CONTROL_BITS + ((pageblock-48)<<1);
                                                                // pageblock -48 because, 0 in ascii corresponds to 48 in dec

    	//printf_tiny("Write : Control sequence offset is %x\n\r", (pageblock-48));
    	//printf_tiny("Write : Control sequence should be 0xA0+something and that being sent is %x\n\r", control_sequence);

    	i2c_start();
    	write_ack = i2c_send_byte(control_sequence);
    	if(write_ack==0)
    	{
	        //printf_tiny("Write : Control sequence successfully sent\n\r\n\r");
        	write_ack = i2c_send_byte(data_address);
        	if(write_ack==0)
        	{
            	//printf_tiny("Write : Data address successfully sent\n\r\n\r");
            	write_ack = i2c_send_byte(i2cdata);
            	//printf_tiny("Write : ack is %d\n\r", write_ack);
        	}
    	}
    	i2c_stop();						// Stop sequence to be generated for EEPROM internal write to be triggered
    	delay(1);                                               // ~0.3ms is taken for write op; 5ms max for page write
                                                                // and 16 bytes write buffer; each buffer write takes around (5*16/256)ms
    	return write_ack;
}


// This function reads a byte of data from a specified address (0x000-0x7FF) of EEPROM
unsigned char i2c_read_byte(unsigned char pageblock, unsigned char data_address)
{
    	unsigned char read_return_value;                        // Is the data or the acknowledgment that is returned
    	unsigned char control_sequence = EEPROM_CONTROL_BITS + ((pageblock-48)<<1);
                                                                // pageblock -48 because, 0 in ascii corresponds to 48 in dec
    	//printf_tiny("Read : Control sequence offset is %x\n\r", (pageblock-48));
    	//printf_tiny("Read : Control sequence should be 0xA0+something and that being sent is %x\n\r", control_sequence);
	
    	i2c_start();
    	read_return_value = i2c_send_byte(control_sequence);
    	if(read_return_value==0)
    	{
	        read_return_value = i2c_send_byte(data_address);
	        if(read_return_value==0)
        	{
            		i2c_start();
            		read_return_value = i2c_send_byte(control_sequence+1);
            		if(read_return_value==0)
            		{
        	        	read_return_value = i2c_receive_byte();
            		}
        	}	
    	}
    	i2c_no_ack();
    	i2c_stop();
    	//printf_tiny("\n\rDEBUG : Value of read is (in read) : %x\n\r", read_return_value);
    	return read_return_value;
}

// This function resets the i2c EEPROM
void i2c_EEPROM_reset(void)
{
    	i2c_start();
    	i2c_send_byte(RESET_CONTROL_BITS);
    	i2c_no_ack();
	i2c_start();
    	i2c_stop();
}

//##########################  I2C EEPROM Specific commands End here  ############################

//######################  I2C IO Expander Specific commands Start here  #########################
void i2c_IO_Expander_Configure_IO(unsigned char inp_or_out)
{
    	int ack;
    	i2c_start();
    	ack = i2c_send_byte(IO_EXPANDER_CONTROL_BITS);
    	//printf_tiny("\r\nInfo : Acknowledgment for 0x40 is %d", ack);
    	if(ack == 0)
    	{
	        ack = i2c_send_byte(inp_or_out);
	        //printf_tiny("\r\nInfo : Acknowledgment for %x is %d", inp_or_out, ack);
	        if(ack != 0)
	        {
        		//printf_tiny("\n\rWarning : Not Acknowledged by IO Expander\n\r");
        	}
    	}
    	i2c_stop();
}


unsigned char i2c_IO_Expander_Get_Current_State()
{
    	int ack;
    	unsigned char inputdata=0;
    	i2c_start();
    	ack = i2c_send_byte(0x41);
    	//printf_tiny("\n\rInfo : Acknowledgment for 0x41 is %d", ack);
    	if(ack == 0)
    	{
	        inputdata = i2c_receive_byte();
    	}
    	i2c_no_ack();
    	i2c_stop();
    	//printf_tiny("\n\rInfo : Input data (in int) is %d", inputdata);
    	//printf_tiny("\n\rInfo : Input data (in hex) is %x", inputdata);
    	return inputdata;
}

//#######################  I2C IO Expander Specific commands End here  ##########################


// This function stops the timer 0 for software RTC
void stopTimer0()
{
    	TR0 = 0;
    	ET0 = 0;	
    	EA = 0;                                                 // disables all interrupts
}
	

// This function resumes software RTC
void resumeTimer0()
{
    	ET0 = 1;
    	EA = 1;                                                 // enables all interrupts
    	TR0 = 1;
}


// This function resets and stops timer 0 for software RTC
void resetTimer0()
{
    	lcdcmd(0xD9);
    	lcdputstr("00:00:0");
    	TR0 = 0;
    	ET0 = 0;
    	EA = 0;                                                 // disables all interrupts
    	TH0 = 0x00;
    	TL0 = 0x00;
    	timerCount1 = -1;
    	timerCount = 0;
    	random_count_value =0;
    	seconds = milliseconds = minutes = 0;
}


// This function initializes timer 0 for software RTC
void initTimer0()
{
    	resetTimer0();
    	resumeTimer0();
}


// This function initializes timer 0 for software RTC
void restartTimer0()
{
    	lcdcmd(0xD9);
    	lcdputstr("00:00:0");
    	initTimer0();
    	seconds = milliseconds = minutes = 0;
}


// This function converts integer to integer string
unsigned char* intToIntStr(int intVal)
{
    	unsigned char i, quotient, c1[3], remainder[3];
    	i = 0;
    	quotient = intVal;
    	while(quotient != 0)
	{
        	remainder[i] = quotient%10;
        	quotient = quotient/10;
        	if(quotient == 0)
		{
        		c1[0] = '0';
        	}
		c1[1-i] = remainder[i] + 48;
		i++;
    	}
    	c1[2] = '\0';
    	return c1;
}

// Convert integer to string
unsigned char * convert_str(int number)
{
    	unsigned char * output=0;
    	if(number <=9)
    	{
	        output[0] = '0' + number;
    	}
    	else
    	{
        	output[0] = 'A' + number - 10;
    	}
    	output[1] = '\0';
    	return output;
}

//#######################  Interrupt Service Routines being here  ##########################

// Interrupt zero handling
void timer_isr (void) __critical __interrupt (1)
{
    	TR0 = 0;
    	TH0 = 0x00;
    	TL0 = 0x00;
    	TR0 = 1;
    	RS = 0;
    	RW = 1;
    	lcd_current_pointer = *lcddata;

	random_count_value++;
    	if((random_count_value%11)==0)
    	{
	        milliseconds++;
        	if(milliseconds==10)
        	{
            		milliseconds=0;
            		seconds++;
            		if(seconds==60)
            		{
                		seconds=0;
                		minutes++;
                		if(minutes==60)
                		{
                    			minutes=0;
                		}
                		lcdgotoxy(3,12);
                		lcdputstr("00");
                		lcdgotoxy(3,9);
                		lcdputstr(intToIntStr(minutes));
                		lcdgotoxy(0x03,0x0F);
                		lcdputstr(convert_str(milliseconds));
		
            		}
            		else
            		{
                		lcdgotoxy(3,12);
                		lcdputstr(intToIntStr(seconds));
                		lcdgotoxy(3,12);
                		lcdputstr(intToIntStr(seconds));
                		lcdgotoxy(0x03,0x0F);
                		lcdputstr(convert_str(milliseconds));
		
			}
		}
		else
        	{
            		lcdgotoxy(3,12);
            		lcdputstr(intToIntStr(seconds));
            		lcdgotoxy(3,12);
            		lcdputstr(intToIntStr(seconds));
            		lcdgotoxy(0x03,0x0F);
            		lcdputstr(convert_str(milliseconds));
        	}
    	}
    	*lcddata = lcd_current_pointer;
}

// Interrupt 0 handling
void int0_isr() __critical __interrupt (0)
{
    	unsigned char ioExpState;
    	//EX0 = 0;
    	counter_for_io_exp++;
    	if(counter_for_io_exp==16)
	{
	        counter_for_io_exp=0;
    	}
    	ioExpState = i2c_IO_Expander_Get_Current_State();
    	ioExpState &= 0xF0;
    	ioExpState |= counter_for_io_exp;
    	i2c_IO_Expander_Configure_IO(ioExpState);
    	lcdgotoxy(0,IO_EXP_COUNT_LOCATION);
    	lcdputstr(convert_str(counter_for_io_exp));
    	//printf_tiny("\r\nDEBUG : IOExpInput State value is %x\r\n",ioExpState);
    	//EX0 = 1;
    	//IT0 = 1;
}

//#######################  Interrupt Service Routines end here  ##########################

// Read LCD RAM data
void readRAMData(void)
{
    	unsigned char read_data;
    	RS = 1;
    	RW = 1;
    	read_data = *lcddata;
    	lcdbusywait();
    	printf_tiny(" %x", read_data);
}

// Convert hex array to hex characters
unsigned char convert_hex(char input[], int limit)
{
    	int iterate_over_string_variable;
    	int hex_value=0, intermediate_number_iterater=-1;

    	for(iterate_over_string_variable=0;iterate_over_string_variable<limit;iterate_over_string_variable++)
    	{
	        if(input[iterate_over_string_variable]>= '0' && input[iterate_over_string_variable] <= '9')
        	{
            		intermediate_number_iterater = input[iterate_over_string_variable]-'0';
        	}	
        	else if(input[iterate_over_string_variable]>= 'a' && input[iterate_over_string_variable] <= 'f')
        	{
            		intermediate_number_iterater = input[iterate_over_string_variable]-'a'+10;
        	}
        	else if(input[iterate_over_string_variable]>= 'A' && input[iterate_over_string_variable] <= 'F')
        	{
            		intermediate_number_iterater = input[iterate_over_string_variable]-'A'+10;
        	}
        	hex_value = hex_value*16 + intermediate_number_iterater;
    	}
    	//printf_tiny("DEBUG : Converted value in hex is : %x\n\r",hex_value);
    	return (unsigned char)hex_value;
}

// Convert integer to string
unsigned char* int_to_str(int integer_input)
{
    	unsigned char* output_str=0;
    	unsigned char temp;
    	temp = (integer_input/16);
    	if(temp<=9)
	        output_str[0] = '0' + temp;
    	else
	        output_str[0] = 'A' + temp - 10;
    	temp = (integer_input%16);
    	if(temp<=9)
	        output_str[1] = '0' + temp;
    	else
	        output_str[1] = 'A' + temp - 10;
    	output_str[2] = '\0';
    	return output_str;
}

// Create custom LCD character
void lcd_create_char(unsigned char cgram_char_code, unsigned char rows[])
{
    	unsigned char iterate_variable;
    	unsigned char cgram_address = 0x40 + (cgram_char_code << 3);    // 0x40 to set CGRAM address; left shifting to adjust to point to address
    	//printf_tiny("\n\rDEBUG : CGRAM address is %x\n\r",cgram_address);
    	lcdcmd(cgram_address);
    	for(iterate_variable=0;iterate_variable<8;iterate_variable++)
    	{
	        lcdputch((cgram_char_code<<5)+rows[iterate_variable]);
	        //printf_tiny("\n\DEBUG :  Row iterate value is %x\n\r",rows[iterate_variable]);
	        //printf_tiny("\n\rDEBUG :  Putchar input is %x\n\r",(cgram_char_code<<5)+rows[iterate_variable]);
    	}
}

// Hardware Watchdog usage
void enable_Hardware_WatchDog_Timer(void)
{
    	WDTRST = 0x1E;                                                 	//Enabling HW Watchdog Timer
    	WDTRST = 0xE1;                                                 	//Enabling HW Watchdog Timer
    	while(1);
}

void help(void)
{
    	//using tiny
    	printf_tiny("################################ HELP MENU ################################\n\r");
    	printf_tiny("Info : Enter h for help\n\r");
    	printf_tiny("Info : Enter w to write byte to EEPROM\n\r");
    	printf_tiny("Info : Enter r to read byte from EEPROM\n\r");
    	printf_tiny("Info : Enter d to display contents of EEPROM location on LCD\n\r");
    	printf_tiny("Info : Enter c to clear contents of LCD display\n\r");
    	printf_tiny("Info : Enter q to display HEX dump of EEPROM in an address range on terminal\n\r");
    	printf_tiny("Info : Enter t to display HEX dump of DDRAM of LCD on terminal\n\r");
    	printf_tiny("Info : Enter e to display HEX dump of CGRAM of LCD on terminal\n\r");
    	printf_tiny("Info : Enter 0 to print a long string on the LCD!\n\r");
    	printf_tiny("Info : Enter 1 to move cursor on LCD!\n\r");
    	printf_tiny("Info : Enter 2 to (re)initialize LCD!\n\r");
    	printf_tiny("Info : Enter n to create custom LCD character\n\r");
    	printf_tiny("Info : Enter i to see your custom character on the LCD!\n\r");
    	printf_tiny("Info : Enter u to print out custom CU logo on LCD\n\r");
    	printf_tiny("Info : Enter z to reset EEPROM\n\r");
    	printf_tiny("Info : Enter y to check out Watchdog timer functionality\n\r");
    	printf_tiny("Info : Enter j to configure IO Expander pins as Input or Output\n\r");
    	printf_tiny("Info : Enter k to get current state of IO Expander port\n\r");
    	printf_tiny("Info : Enter 5 to display timer\n\r");
    	printf_tiny("Info : Enter 6 to resume timer\n\r");
    	printf_tiny("Info : Enter 7 to reset timer\n\r");
    	printf_tiny("Info : Enter 8 to restart timer\n\r");
    	printf_tiny("Info : Enter 9 to stop timer\n\r");
    	printf_tiny("Info : Enter x to reset io expander count\n\r");
    	printf_tiny("\n\rInfo : Enter a character to get started!\n\r");
}
	

void main(void)
{
    	//Definition and declarations of variables
    	char user_input = '~';
    	unsigned char i2c_read_value;
    	unsigned char page_number=-1;
    	unsigned char start_page_number=-1;
    	unsigned char end_page_number=-1;
    	unsigned char rw_address_start[2];
    	unsigned char rw_address_end[2];
    	unsigned char rw_address[2];
    	unsigned char rw_data[2];
    	int input_check_flag=0;
    	unsigned char lcd_row_number = '~';
    	unsigned char * i2c_lcd_str;
    	unsigned char j=0, k=0, l=0, pin_input_or_output;
    	//unsigned char user_input_string[100];
    	int i=0;
    	unsigned char lcd_custom_char_code;
    	unsigned char custom_char_input[2];
    	unsigned char row_custom_char_hex;
    	unsigned char lcdRowVals[8], lcdrowvals[8];
    	unsigned char pin_number_IO_Exp = 0;
    	unsigned char io_exp_current_state = 0;
	unsigned char io_exp_mask, io_exp_output;
    	initialize_serial_communication();
    	IT0 = 1;                                                // IT0 is set for falling edge trigger
    	EX0 = 1;                                                // Enabling INT0 of 8051
	//lcdgotoxy(0x02, 0x00);
	//cdputstr("Lots of random text is going to make the LCD overflow, right? Hello World!");

	while(1)
    	{
	        lcdinit();
        	i2cinit();
        	help();
        	getchar();
        	delay(10);
        	initTimer0();
        	delay(10);
        	while(user_input != '@')
        	{
            		delay(10);
            		printf_tiny("\n\r\n\rEnter a character : ");
            		user_input=getchar();
            		putchar(user_input);
            		printf_tiny("\n\r");
		        switch(user_input)
            		{
                		case 'w':			// Write to EEPROM address
                    		{
                        		//printf_tiny("Entered case 1\n\r");
                        		printf_tiny("Enter the page (0 to 7) that you would like to write to : ");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			page_number = getchar();
                            			if(page_number<'8' && page_number>='0')
                            			{
                                			putchar(page_number);
                                			input_check_flag = 1;
                            			}
                            			else
                                		{
                                    			putchar(page_number);
                                    			printf_tiny("\n\rError : Value entered is invalid\n\r");
                                    			printf_tiny("Enter the page (0 to 7) that you would like to write to : ");
                                		}
                        		}
                        		printf_tiny("\n\rEnter a valid hex address that you would like to write to : 0x");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			rw_address[0] = getchar();
                            			if(rw_address[0]<='9' && rw_address[0]>='0' || rw_address[0]<='f' && rw_address[0]>='a' || rw_address[0] <='F' && rw_address[0]>='A')
                            			{	
		                        	        putchar(rw_address[0]);
                			                //input_check_flag = 1;
                        	        		rw_address[1] = getchar();
                	                		if(rw_address[1]<='9' && rw_address[1]>='0' || rw_address[1]<='f' && rw_address[1]>='a' || rw_address[1] <='F' && rw_address[1]>='A')
        	                        		{
	                                    			putchar(rw_address[1]);
                                    				input_check_flag = 1;
                                			}
                                			else
                                			{	
                                    				putchar(rw_address[1]);
                                    				printf_tiny("\n\rError : Value entered is invalid\n\r");
                                    				printf_tiny("\n\rEnter a valid hex address that you would like to write to : 0x");
                                			}
                        	    		}
                	            		else
        	                    		{
	                               			putchar(rw_address[0]);
                                			printf_tiny("\n\rError : Value entered is invalid\n\r");
                        	        		printf_tiny("\n\rEnter a valid hex address that you would like to write to : 0x");
                	            		}
	                            		//else
        	                        	//printf_tiny("Please enter valid inputs!\n\r");
        	                	}
	
                        		printf_tiny("\n\rEnter the data that you would like to write : 0x");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			rw_data[0] = getchar();
                            			if(rw_data[0]<='9' && rw_data[0]>='0' || rw_data[0]<='f' && rw_data[0]>='a' || rw_data[0] <='F' && rw_data[0]>='A')
                            			{	
                                			putchar(rw_data[0]);
                                			//input_check_flag = 1;
                        	        		rw_data[1] = getchar();
                	                		if(rw_data[1]<='9' && rw_data[1]>='0' || rw_data[1]<='f' && rw_data[1]>='a' || rw_data[1] <='F' && rw_data[1]>='A')
        	                        		{
	                                    			putchar(rw_data[1]);
	                                    			input_check_flag = 1;
                                			}
                                			else
                                			{
                                    				putchar(rw_data[1]);
                                    				printf_tiny("\n\rError : Value entered is invalid\n\r");
                                    				printf_tiny("\n\rEnter the data that you would like to write : 0x");
                                			}
                            			}
                            			else
                            			{
                                			putchar(rw_data[0]);
                                			printf_tiny("\n\rError : Value entered is invalid\n\r");
                                			printf_tiny("\n\rEnter the data that you would like to write : 0x");
                        	    		}
                	            		//else
        	                        	//printf_tiny("Please enter valid inputs!\n\r");
	                        	}

                        		i2c_read_value = i2c_write_byte(page_number, convert_hex(rw_address, 2), convert_hex(rw_data, 2));
	                        	rw_address[0] = '\0';
        	                	rw_data[0] = '\0';
                		}break;

               			case 'r':			// Read EEPROM content
                    		{
                        		printf_tiny("Enter the page (0 to 7) that you would like to read from : ");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			page_number = getchar();
                            			if(page_number<'8' && page_number>='0')
                            			{
                                			putchar(page_number);
                                			input_check_flag = 1;
                            			}
                            			else
                                		{
                                    			putchar(page_number);
                                    			printf_tiny("\n\rError : Value entered is invalid\n\r");
                                    			printf_tiny("Enter the page (0 to 7) that you would like to read from : ");
                                		}
                        		}
                        		printf_tiny("\n\rEnter a valid hex address that you would like to read from : 0x");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			rw_address[0] = getchar();
                            			if(rw_address[0]<='9' && rw_address[0]>='0' || rw_address[0]<='f' && rw_address[0]>='a' || rw_address[0] <='F' && rw_address[0]>='A')
                            			{
                                			putchar(rw_address[0]);
                                			//input_check_flag = 1;
                                			rw_address[1] = getchar();
                                			if(rw_address[1]<='9' && rw_address[1]>='0' || rw_address[1]<='f' && rw_address[1]>='a' || rw_address[1] <='F' && rw_address[1]>='A')
                                			{
                                    				putchar(rw_address[1]);
                                    				input_check_flag = 1;
                                			}
                                			else	
                                			{
                                    				putchar(rw_address[1]);
                                    				printf_tiny("\n\rError : Value entered is invalid\n\r");
                                    				printf_tiny("\n\rEnter a valid hex address that you would like to read from : 0x");
                                			}
                            			}
                            			else
                            			{
                                			putchar(rw_address[0]);
                                			printf_tiny("\n\rError : Value entered is invalid\n\r");
                                			printf_tiny("\n\rEnter a valid hex address that you would like to read from : 0x");
                            			}
                            			//else
                                		//printf_tiny("Please enter valid inputs!\n\r");
                        		}
                        		i2c_read_value = i2c_read_byte(page_number, convert_hex(rw_address, 2));
                        		//printf_tiny("\n\rVALUE OF READ IS : %x\n\r", i2c_read_value);
	
		                        printf_tiny("\n\r\n\r%x", page_number-48);
                		        printf_tiny("%s: %x\n\r", rw_address, i2c_read_value);
                        		rw_address[0] = '\0';
                        		rw_data[0] = '\0';
                    		}break;

                		case 'd':			// To display EEPROM data at specified row on LCD
                    		{
                        		printf_tiny("\n\rEnter a valid EEPROM address (0x000 to 0x7FF) : 0x");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			page_number = getchar();
                            			if(page_number<'8' && page_number>='0')
                            			{
                                			putchar(page_number);
                                			//input_check_flag = 1;
                                			rw_address[0] = getchar();
                                			if(rw_address[0]<='9' && rw_address[0]>='0' || rw_address[0]<='f' && rw_address[0]>='a' || rw_address[0] <='F' && rw_address[0]>='A')
                                			{
                                    				putchar(rw_address[0]);
                                    				//input_check_flag = 1;
                                    				rw_address[1] = getchar();
                                    				if(rw_address[1]<='9' && rw_address[1]>='0' || rw_address[1]<='f' && rw_address[1]>='a' || rw_address[1] <='F' && rw_address[1]>='A')
                                    				{
                                        				putchar(rw_address[1]);
                                        				input_check_flag = 1;
                                    				}
                                    				else
                                    				{
                                        				putchar(rw_address[1]);
                                        				printf_tiny("\n\rError : Value entered is invalid\n\r");
                                        				printf_tiny("\n\rEnter a valid EEPROM address (0x000 to 0x7FF) : 0x");
                                    				}
                                			}
	                               			else
        			                        {
			                                 	putchar(rw_address[0]);
                                    				printf_tiny("\n\rError : Value entered is invalid\n\r");
                                    				printf_tiny("\n\rEnter a valid EEPROM address (0x000 to 0x7FF) : 0x");
			                                }
						}
                            			else
                                		{
                                    			putchar(page_number);
                                    			printf_tiny("\n\rError : Value entered is invalid\n\r");
                                    			printf_tiny("\n\rEnter a valid EEPROM address (0x000 to 0x7FF) : 0x");
                                		}
		                        }
		                        i2c_read_value = i2c_read_byte(page_number, convert_hex(rw_address, 2));
                		        printf_tiny("\n\rEnter a row number (0 to 3) to display data on LCD : ");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			lcd_row_number = getchar();
                            			if(lcd_row_number<'4' && lcd_row_number>='0')
                            			{
	                                		putchar(lcd_row_number);
        			                        input_check_flag = 1;
                            			}
                            			else
                            			{
                                			putchar(lcd_row_number);
                                			printf_tiny("\n\rError : Value entered is invalid\n\r");
                                			printf_tiny("\n\rEnter a row number (0 to 3) to display data on LCD : ");
                            			}
                        		}
                        		lcdgotoxy(lcd_row_number-48,0);
                        		i2c_lcd_str = int_to_str(i2c_read_value);
                        		lcdputch(page_number);
                        		delay(1);
                        		lcdputstr(rw_address);
                        		lcdputstr(": ");
                        		lcdputstr(i2c_lcd_str);
                    		}break;

                		case 'c':			// To clear LCD Display
                    		{			
                        		printf_tiny("\n\rClearing LCD Display!\n\r");	
                        		lcdinit();
                    		}break;
	
        		        case 'e':			// CGRAM Dump
                    		{
                        		printf_tiny("\n\r##################################CGRAM Dump##################################\n\r");
                        		lcdcmd(0x40);
                        		j = 0;
                        		printf_tiny("\r\n0x00: ");
                        		for(i = 0x00; i<0x40; i++)
					{
                            			if(j == 8)
						{		
	                                		printf("\r\n0x%02x: ", i);
	                                		j=0;
	                            		}	
	                            		readRAMData();
	                            		j++;
                        		}
                        		printf_tiny("\n\r##################################CGRAM Dump##################################\n\r");
				}break;

                		case 't':			// DDRAM Dump
                    		{
                        		printf_tiny("\n\r##################################DDRAM Dump##################################\n\r");
                        		lcdcmd(0x80);
                        		printf_tiny("\n\rLCD Line 1: 0x00: ");
                        		for(i = 0x80; i<= 0x8F; i++)
					{
                        	    		readRAMData();
                	        	}
			
        		                lcdcmd(0xC0);
	        	                printf_tiny("\n\rLCD Line 2: 0x40: ");
                        		for(i = 0xC0; i<= 0xCF; i++)
					{
                            			readRAMData();
                        		}

                        		lcdcmd(0x90);
                        		printf_tiny("\n\rLCD Line 3: 0x10: ");
                        		for(i = 0x90; i<= 0x9F; i++)
                        		{
                            			readRAMData();
                        		}
		
                		        lcdcmd(0xD0);
                        		printf_tiny("\n\rLCD Line 4: 0x50: ");
                        		for(i = 0xD0; i<= 0xDF; i++)
					{
                            			readRAMData();
                        		}
                        		printf_tiny("\n\r##################################DDRAM Dump##################################\n\r");
                    		}break;

                		case 'q':			// EEPROM Dump for specified address range
                    		{
                        		printf_tiny("\n\rEnter a valid EEPROM start address (0x000 to 0x7FF) : 0x");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			start_page_number = getchar();
                            			if(start_page_number<'8' && start_page_number>='0')
                            			{
                                			putchar(start_page_number);
                                			//input_check_flag = 1;
                                			rw_address_start[0] = getchar();
                                			if(rw_address_start[0]<='9' && rw_address_start[0]>='0' || rw_address_start[0]<='f' && rw_address_start[0]>='a' || rw_address_start[0] <='F' && rw_address_start[0]>='A')
                                			{
                                    				putchar(rw_address_start[0]);
                                    				//input_check_flag = 1;
                                    				rw_address_start[1] = getchar();
                                    				if(rw_address_start[1]<='9' && rw_address_start[1]>='0' || rw_address_start[1]<='f' && rw_address_start[1]>='a' || rw_address_start[1] <='F' && rw_address_start[1]>='A')
                                    				{
                                        				putchar(rw_address_start[1]);
                                        				input_check_flag = 1;
                                    				}
                                    				else
                                    				{
                                        				putchar(rw_address_start[1]);
                                        				printf_tiny("\n\rError : Value entered is invalid\n\r");
                                        				printf_tiny("\n\rEnter a valid EEPROM start address (0x000 to 0x7FF) : 0x");
                                    				}
                                			}
                                			else
                                			{
                                    				putchar(rw_address_start[0]);
                                    				printf_tiny("\n\rError : Value entered is invalid\n\r");
				                                printf_tiny("\n\rEnter a valid EEPROM start address (0x000 to 0x7FF) : 0x");
			                                }
                            			}
                            			else
                                		{
                                    			putchar(start_page_number);
                                    			printf_tiny("\n\rError : Value entered is invalid\n\r");
                                    			printf_tiny("\n\rEnter a valid EEPROM start address (0x000 to 0x7FF) : 0x");
                                		}
                        		}

                        		printf_tiny("\n\rEnter a valid EEPROM end address (0x000 to 0x7FF) : 0x");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			end_page_number = getchar();
                            			if(end_page_number<'8' && end_page_number>='0')
                            			{
                                			putchar(end_page_number);
                                			//input_check_flag = 1;
                                			rw_address_end[0] = getchar();
                                			if(rw_address_end[0]<='9' && rw_address_end[0]>='0' || rw_address_end[0]<='f' && rw_address_end[0]>='a' || rw_address_end[0] <='F' && rw_address_end[0]>='A')
                                			{
                                    				putchar(rw_address_end[0]);
                                    				//input_check_flag = 1;
                                    				rw_address_end[1] = getchar();
                                    				if(rw_address_end[1]<='9' && rw_address_end[1]>='0' || rw_address_end[1]<='f' && rw_address_end[1]>='a' || rw_address_end[1] <='F' && rw_address_end[1]>='A')
                                    				{
                                        				putchar(rw_address_end[1]);
                                        				if(start_page_number<end_page_number)
                                        				{
                                            					input_check_flag = 1;
                                        				}
                                        				else if(start_page_number==end_page_number)
                                        				{
                                            					if(rw_address_start[0]<rw_address_end[0])
                                            					{
                                                					input_check_flag = 1;
                                            					}
                                            					else if(rw_address_start[0]==rw_address_end[0])
                                            					{
                                                					if(rw_address_start[1]<=rw_address_end[1])
                                                					{
                                                    						input_check_flag = 1;
                                                					}
                                                					else
                                                					{
                                                    						printf_tiny("\n\rWarning : Please enter an end address that is greater than or equal to the start address!\n\r");
                                                    						printf_tiny("\n\rEnter a valid EEPROM end address (0x000 to 0x7FF) : 0x");
                                                					}
                                            					}
                                            					else
                                            					{
                                                					printf_tiny("\n\rWarning : Please enter an end address that is greater than or equal to the start address!\n\r");
                                                					printf_tiny("\n\rEnter a valid EEPROM end address (0x000 to 0x7FF) : 0x");
                                            					}
                                        				}
                                        				else
                                        				{
					                                        printf_tiny("\n\rWarning : Please enter an end address that is greater than or equal to the start address!\n\r");
				                                         	printf_tiny("\n\rEnter a valid EEPROM end address (0x000 to 0x7FF) : 0x");
                                        				}
                                    				}
                                    				else
                                    				{
                                        				putchar(rw_address_end[1]);
                                        				printf_tiny("\n\rError : Value entered is invalid\n\r");
				                                        printf_tiny("\n\rEnter a valid EEPROM end address (0x000 to 0x7FF) : 0x");
                                    				}
                                			}
                                			else
                                			{
                                    				putchar(rw_address_end[0]);
                                    				printf_tiny("\n\rError : Value entered is invalid\n\r");
                                    				printf_tiny("\n\rEnter a valid EEPROM end address (0x000 to 0x7FF) : 0x");
                                			}
                            			}
                            			else
                                		{
                                    			putchar(end_page_number);
                                    			printf_tiny("\n\rError : Value entered is invalid\n\r");
                                    			printf_tiny("\n\rEnter a valid EEPROM end address (0x000 to 0x7FF) : 0x");
                                		}
		                        }
                        		i = convert_hex(rw_address_start, 2);
                        		j = convert_hex(rw_address_end, 2);
                        		page_number = start_page_number;
                        		k = 0;
                        		input_check_flag = 0;
                        		while(1)              // Actually not input check flag in this case; used as a generic flag!
                        		{
                            			printf_tiny("\n\r##################################EEPROM Dump##################################\n\r");
                            			for(page_number;page_number<=end_page_number;)
                            			{
                                			//printf_tiny("\n\r\n\rValue of page number is %x\n\r\n\r", page_number-48);
                                			for(i;i<=255;i++)
                                			{
								i2c_read_value = i2c_read_byte(page_number, i);
                                    				if(k%16==0)
                                    				{
                                        				printf("\n\r%x%02x :", page_number-48, i);
                                    				}
                                    				printf(" %02x", i2c_read_value);
                                    				if((page_number==end_page_number) && (i==j))
                                    				{
                                        				//printf_tiny("\n\rEntered exit condition\n\r");
                                        				break;
                                    				}
                                    				k++;
                                			}
					                //printf_tiny("\n\r\n\rValue of page number is %x\n\r\n\r", page_number-48);
                                			page_number++;
                                			//printf_tiny("\n\r\n\rValue of page number is %x\n\r\n\r", page_number-48);
                                			i=0;
                            			}
                            			//printf_tiny("Break condition triggered\n\r");
                            			printf_tiny("\n\r##################################EEPROM Dump##################################\n\r");
                            			break;
                            			//input_check_flag = 0;
                            			//for(start_page_number<=end_page_number;start_page_number++)
                        		}
                    		}break;

                		case 'h':			// Display help menu
                    		{
                        		help();
                    		}break;

                		/*case '8':			// To initialize CGRAM to some values for testing!
                    		{
                        		lcdcmd(0x40);
                        		for(i=0;i<64;i++)
                        		{
                            			//lcdcmd(0x40+i);
                            			RS = 1;
                            			RW = 0;
                            			delay(1);
                            			*lcddata = '3'+i;
                            			delay(1);
                        		}
                    		}break;*/

                		case '0':
                    		{
                        		lcdputstr("A really long meaningless sentence to show wrapping of data on LCD display!");
                    		}break;
		
	                	case '1':
        	        	{
                        		printf_tiny("\r\n(x,y) location map of the LCD:\r\n");
                        		printf_tiny("\r\n y  x  0     1     2     3     4     5     6     7     8     9     10    11    12    13    14    15");
                        		printf_tiny("\r\n 0   (0,0) (1,0) (2,0) (3,0) (4,0) (5,0) (6,0) (7,0) (8,0) (9,0) (A,0) (B,0) (C,0) (D,0) (E,0) (F,0)");
                        		printf_tiny("\r\n 1   (0,1) (1,1) (2,1) (3,1) (4,1) (5,1) (6,1) (7,1) (8,1) (9,1) (A,1) (B,1) (C,1) (D,1) (E,1) (F,1)");
                        		printf_tiny("\r\n 2   (0,2) (1,2) (2,2) (3,2) (4,2) (5,2) (6,2) (7,2) (8,2) (9,2) (A,2) (B,2) (C,2) (D,2) (E,2) (F,2)");
                        		printf_tiny("\r\n 3   (0,3) (1,3) (2,3) (3,3) (4,3) (5,3) (6,3) (7,3) (8,3) (9,3) (A,3) (B,3) (C,3) (D,3) (E,3) (F,3)");
                        		printf_tiny("\r\nGive the specific (x,y) location you want to move cursor position to: x(column)=");
                        		//printf_tiny("\n\rEnter a row number (0 to 3) to display data on LCD : ");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			j = getchar();
                            			if(j<='9' && j>='0' || j<='f' && j>='a' || j <='F' && j>='A')
                            			{
                                			putchar(j);
                                			input_check_flag = 1;
                            			}
                            			else
                            			{
                                			putchar(j);
                                			printf_tiny("\n\rError : Value entered is invalid\n\r");
                                			printf_tiny("\n\rEnter a column number (0 to F) to move cursor LCD : x(column)=");
                            			}
                        		}
                        		printf_tiny(", y(row)=");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			k = getchar();
                            			if(k<'4' && k>='0')
                            			{
                                			putchar(k);
                                			input_check_flag = 1;
                            			}
                            			else
                            			{
                                			putchar(k);
                                			printf_tiny("\n\rError : Value entered is invalid\n\r");
                                			printf_tiny("\n\ry(row)=");
                            			}
                        		}
                        		if(j>='0'&&j<='9')
                        		{
                        	    		j = j - '0';
                        		}
                        		else if(j>='a'&&j<='f')
                        		{
                        	    		j = j-'a'+10;
                        		}
                        		else
                        		{
                            			j = j-'A'+10;
                        		}
                        		lcdgotoxy(k-'0',j);
                        		//lcdgotoxy(convert_hex(&j,1), convert_hex(&k,1));
                    		}break;

                		case '2':       		// Clear LCD display!
                    		{
                       			lcdinit();
                    		}break;
	
                		case 'n':       		// To create custom LCD character		
                    		{
                        		printf_tiny("\n\rEnter a custom character code(0 to 7):");
                        		lcd_custom_char_code = getchar();
                        		putchar(lcd_custom_char_code);
                        		while(lcd_custom_char_code >= '8' || lcd_custom_char_code <'0')
                        		{
		                        	printf_tiny("\n\rPlease enter a valid input\n\r");
                            			printf_tiny("\n\rEnter a custom character code(0 to 7):");
                            			lcd_custom_char_code = getchar();
                            			putchar(lcd_custom_char_code);
                       			}

                        		for(i=0;i<8;i++)
                        		{
                            			printf_tiny("\n\rEnter custom character values by row in hex(0x00 to 0x1F): 0x");
                            			input_check_flag = 0;
                            			while(input_check_flag == 0)
                            			{
                                			custom_char_input[0]=getchar();
                                			if(custom_char_input[0]=='0' || custom_char_input[0] == '1')
                                			{
	                             				putchar(custom_char_input[0]);
                                    				custom_char_input[1] = getchar();
                                    				if(custom_char_input[1]<='9' && custom_char_input[1]>='0' || custom_char_input[1]<='f' && custom_char_input[1]>='a' || custom_char_input[1] <='F' && custom_char_input[1]>='A')
                                    				{
                                        				putchar(custom_char_input[1]);
                                        				input_check_flag = 1;
                                    				}	
                                    				else
                                    				{
                                        				printf_tiny("\n\rPlease enter valid values!\n\r");
                                        				printf_tiny("\n\rEnter custom character values by row in hex(0x00 to 0x1F): 0x");
                                    				}
                                			}
                                			else
                                			{
                                    				printf_tiny("\n\rPlease enter valid values!");
                                    				printf_tiny("\n\rEnter custom character values by row in hex(0x00 to 0x1F): 0x");
                                			}
                            			}
                            			row_custom_char_hex = convert_hex(custom_char_input, 2);
                            			printf_tiny("\n\rDebug : row_custom_char_hex value is %x",row_custom_char_hex);
                            			lcdRowVals[i] = row_custom_char_hex;
                            			printf_tiny("\n\rDebug : Loop number is %d of 8", i+1);
                            			printf_tiny("\n\rYour custom character would look like : ");
                            			for(k=0; k<=i; k++)
                            			{
                                			printf_tiny("\n\r");
                                			//printf_tiny("\n\rDebug : Inside for looppp 1!\n\r");
                                			for(j=0; j<8; j++)
                                			{
                                    				if(j>2)
                                    				{
                                        				l = !!((lcdRowVals[k] << j) & 0x80);
                                        				if(l == 1)
									{
                                            					printf_tiny("*");
                                        				}
                                        				else if(l == 0)
                                        				{
	                                            				printf_tiny(" ");
                                        				}
                                    				}
                                			}			
                            			}
                        		}
                        		lcd_create_char(lcd_custom_char_code - '0', lcdRowVals);
                    		}break;		

                		case 'u':           		// CU Logo!
                    		{
                        		//delay(20);

                        		lcdrowvals[0]=00;lcdrowvals[1]=15;lcdrowvals[2]=16;lcdrowvals[3]=16;
                        		lcdrowvals[4]=16;lcdrowvals[5]=16;lcdrowvals[6]=16;lcdrowvals[7]=16;
                        		lcd_create_char(6,lcdrowvals);
                        		lcdgotoxy(0,3);
                        		lcdputch(6);
                        		//delay(20);

                        		lcdrowvals[0]=00;lcdrowvals[1]=24;lcdrowvals[2]=04;lcdrowvals[3]=04;
                        		lcdrowvals[4]=04;lcdrowvals[5]=04;lcdrowvals[6]=00;lcdrowvals[7]=00;
                        		lcd_create_char(7,lcdrowvals);
                        		lcdgotoxy(0,4);
                        		lcdputch(7);
                        		//delay(20);
				
                        		lcdrowvals[0]=00;lcdrowvals[1]=00;lcdrowvals[2]=00;lcdrowvals[3]=04;
                        		lcdrowvals[4]=04;lcdrowvals[5]=04;lcdrowvals[6]=04;lcdrowvals[7]=24;
                        		lcd_create_char(1,lcdrowvals);
                        		lcdgotoxy(1,4);
                        		lcdputch(1);
                        		//delay(20);
	
		                        lcdrowvals[0]=16;lcdrowvals[1]=19;lcdrowvals[2]=17;lcdrowvals[3]=17;
                        		lcdrowvals[4]=17;lcdrowvals[5]=17;lcdrowvals[6]=17;lcdrowvals[7]=15;
                        		lcd_create_char(0,lcdrowvals);
                        		lcdgotoxy(1,3);
                        		lcdputch(0);
                        		//delay(20);

                        		lcdrowvals[0]=01;lcdrowvals[1]=01;lcdrowvals[2]=01;lcdrowvals[3]=01;
                        		lcdrowvals[4]=01;lcdrowvals[5]=01;lcdrowvals[6]=01;lcdrowvals[7]=00;
                        		lcd_create_char(2,lcdrowvals);
                        		lcdgotoxy(2,3);
                        		lcdputch(2);
                        		//delay(20);
		
                        		lcdrowvals[0]=00;lcdrowvals[1]=00;lcdrowvals[2]=00;lcdrowvals[3]=00;
                        		lcdrowvals[4]=00;lcdrowvals[5]=00;lcdrowvals[6]=00;lcdrowvals[7]=31;
                        		lcd_create_char(3,lcdrowvals);
                        		lcdgotoxy(2,4);
                        		lcdputch(3);
	                        	//delay(20);

                        		lcdrowvals[0]= 8;lcdrowvals[1]= 8;lcdrowvals[2]= 8;lcdrowvals[3]= 8;
                        		lcdrowvals[4]= 8;lcdrowvals[5]= 8;lcdrowvals[6]= 8;lcdrowvals[7]=16;
                        		lcd_create_char(5,lcdrowvals);
                        		lcdgotoxy(2,5);
                        		lcdputch(5);
                        		//delay(20);
		
                 		       lcdrowvals[0]=00;lcdrowvals[1]=12;lcdrowvals[2]= 8;lcdrowvals[3]= 8;
                 		       lcdrowvals[4]= 8;lcdrowvals[5]= 8;lcdrowvals[6]= 8;lcdrowvals[7]= 8;
                 		       lcd_create_char(4,lcdrowvals);
                 		       lcdgotoxy(1,5);
                 		       lcdputch(4);
                 		       //delay(20);

                   		}break;

                		case 'z':           		// Reset EEPROM!
                    		{
                        		i2c_EEPROM_reset();
                    		}break;
			
                		case 'y':           		// Watchdog timer functionality
                    		{
                        		printf_tiny("\n\rYou've upset the software :(\n\r");
                        		printf_tiny("\n\rNeeds to restart!!!\n\r");
                        		printf_tiny("1\n\r");
                        		printf_tiny("2\n\r");
                        		printf_tiny("3\n\r");
                        		printf_tiny("4\n\r");
                        		printf_tiny("5\n\r");
                        		printf_tiny("$@(*&!)%7^!*#!&(%!#*)&$!(#$^@^*TE!^@$8`@(*$9127$*!3270\n\r");
                        		printf_tiny("\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r");
                        		enable_Hardware_WatchDog_Timer();
                    		}break;

                		case 'i':
                    		{
                        		printf_tiny("\n\rEnter a custom character code(0 to 7):");
                        		lcd_custom_char_code = getchar();
                        		putchar(lcd_custom_char_code);
                        		while(lcd_custom_char_code >= '8' || lcd_custom_char_code <'0')
                        		{
                            			printf_tiny("\n\rPlease enter a valid input\n\r");
                            			printf_tiny("\n\rEnter a custom character code(0 to 7):");
                            			lcd_custom_char_code = getchar();
                            			putchar(lcd_custom_char_code);
                        		}
                        		printf_tiny("\r\n(x,y) location map of the LCD:\r\n");
                        		printf_tiny("\r\n y  x  0     1     2     3     4     5     6     7     8     9     10    11    12    13    14    15");
                        		printf_tiny("\r\n 0   (0,0) (1,0) (2,0) (3,0) (4,0) (5,0) (6,0) (7,0) (8,0) (9,0) (A,0) (B,0) (C,0) (D,0) (E,0) (F,0)");
                        		printf_tiny("\r\n 1   (0,1) (1,1) (2,1) (3,1) (4,1) (5,1) (6,1) (7,1) (8,1) (9,1) (A,1) (B,1) (C,1) (D,1) (E,1) (F,1)");
                        		printf_tiny("\r\n 2   (0,2) (1,2) (2,2) (3,2) (4,2) (5,2) (6,2) (7,2) (8,2) (9,2) (A,2) (B,2) (C,2) (D,2) (E,2) (F,2)");
                        		printf_tiny("\r\n 3   (0,3) (1,3) (2,3) (3,3) (4,3) (5,3) (6,3) (7,3) (8,3) (9,3) (A,3) (B,3) (C,3) (D,3) (E,3) (F,3)");
                        		printf_tiny("\r\nGive the specific (x,y) location you want to move cursor position to: x(column)=");
                        		//printf_tiny("\n\rEnter a row number (0 to 3) to display data on LCD : ");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                            			j = getchar();
                            			if(j<='9' && j>='0' || j<='f' && j>='a' || j <='F' && j>='A')
                            			{
							putchar(j);
							input_check_flag = 1;
                            			}
                            			else
                            			{
                            			    	putchar(j);
                            			    	printf_tiny("\n\rError : Value entered is invalid\n\r");
                            			    	printf_tiny("\n\rEnter a column number (0 to F) to move cursor LCD : x(column)=");
                            			}
                        		}
                        		printf_tiny(", y(row)=");
                        		input_check_flag = 0;
                        		while(input_check_flag==0)
                        		{
                        		    	k = getchar();
                        		    	if(k<'4' && k>='0')
                        		    	{
                        		    	    	putchar(k);
                        		    	    	input_check_flag = 1;
                        		    	}
                        		    	else
                        		    	{
                        		    	    	putchar(k);
                        		    	    	printf_tiny("\n\rError : Value entered is invalid\n\r");
                        		    	    	printf_tiny("\n\ry(row)=");
                        		    	}
                        		}
                        		if(j>='0'&&j<='9')
                        		{
                            			j = j - '0';
                        		}
                        		else if(j>='a'&&j<='f')
                        		{
                            			j = j-'a'+10;
                        		}
                        		else
                        		{
                            			j = j-'A'+10;
                        		}
                        		lcdgotoxy(k-'0',j);
                        		lcdputch(lcd_custom_char_code - '0');
                        		printf_tiny("\n\rInfo : Custom character displayed on LCD!\n\r");	
                    		}break;

                		case 'j':           		// To configure IO Exp Pins
                    		{
                        		printf_tiny("\n\rEnter the Pin (P0 to P7) that you want to configure as I/O : P");
                        		pin_number_IO_Exp = getchar();
                        		putchar(pin_number_IO_Exp);
                        		while(pin_number_IO_Exp >= '8' || pin_number_IO_Exp <'0')
                        		{
                            			printf_tiny("\n\rPlease enter a valid input\n\r");
                            			printf_tiny("\n\rEnter the Pin (P0 to P7) that you want to configure as I/O : P");
                            			pin_number_IO_Exp = getchar();
                            			putchar(pin_number_IO_Exp);
                        		}

                        		printf_tiny("\n\rEnter 0 to configure as input or 1 to configure as output : ");
                        		pin_input_or_output = getchar();
                        		putchar(pin_input_or_output);
                        		while(pin_input_or_output < '0' || pin_input_or_output > '1')
                        		{
                        		    	printf_tiny("\n\rPlease enter a valid input\n\r");
                        		    	printf_tiny("\n\rEnter 0 to configure as input or 1 to configure as output : ");
                        		    	pin_input_or_output = getchar();
                        		    	putchar(pin_input_or_output);
                        		}
                        		io_exp_current_state = i2c_IO_Expander_Get_Current_State();
                        		if(pin_input_or_output == '0')
                        		{
                        		    	io_exp_mask = 1;
                        		    	io_exp_mask = io_exp_mask << (pin_number_IO_Exp - '0');
                        		    	io_exp_current_state |= io_exp_mask;
                        		}
		
                        		else if(pin_input_or_output == '1')
                        		{
                            			printf_tiny("\n\rEnter a 0 to drive low, 1 to drive high at output : ");
                            			io_exp_output = getchar();
                            			putchar(io_exp_output);
                            			while(!(io_exp_output == '0' || io_exp_output == '1'))
                            			{
	                            		    	printf_tiny("\n\rPlease enter a valid input\n\r");
	                            		    	printf_tiny("\n\rEnter a 0 to drive low. 1 to drive high at output : ");
        	                    		    	io_exp_output = getchar();
                            		    		putchar(io_exp_output);
		              			}
                            			if(io_exp_output == '1')
                            			{
	                            		    	//printf_tiny("\n\rDebug : Output to be driven to 1");
        	                        		io_exp_mask = 1;
	                                		io_exp_mask = io_exp_mask << (pin_number_IO_Exp - '0');
	                                		io_exp_current_state |= io_exp_mask;
	                                		//printf_tiny("\n\rDebug1 : Value off bit mask is %x", io_exp_mask);
	                                		//printf_tiny("\n\rDebug1 : current state would be %x", io_exp_current_state);
	                            		}
	                            		if(io_exp_output == '0')
	                            		{
	                                		io_exp_mask = 1;
	                                		//printf_tiny("\n\rDebug : Output to be driven to 0");
	                                		io_exp_mask = ~(io_exp_mask << (pin_number_IO_Exp - '0'));
	                                		io_exp_current_state &= io_exp_mask;
	                                		//printf_tiny("\n\rDebug0 : Value of bit mask is %x", io_exp_mask);
	                                		//printf_tiny("\n\rDebug0 : current state would be %x", io_exp_current_state);
	                            		}
	                        	}
	                        	i2c_IO_Expander_Configure_IO(io_exp_current_state);
	
	                    	}break;


                		case 'k':           		// To get current state of IO Exp port
                    		{
                        		io_exp_current_state = i2c_IO_Expander_Get_Current_State();
                        		printf_tiny("\n\rInfo : Input data is %x", io_exp_current_state);
		
				}break;


                		case '5':           		// To display timer
                    		{
                        		lcdgotoxy(3,9);
                        		lcdputstr("00:00:0");
                        		initTimer0();
                    		}break;

					
                		case '6':           		// To resume timer
                    		{
                        		resumeTimer0();
                    		}break;


                		case '7':           		// To reset timer
                    		{
                        		resetTimer0();
                    		}break;
		

                		case '8':           		// To restart timer
                    		{
                        		restartTimer0();
                    		}break;
		

		                case '9':          		// To stop timer
				{
                        		stopTimer0();
                    		}break;
	
                		case 'x':			// To reset IO Expander count to 0
                    		{
                        		printf_tiny("\n\rInfo : Resetting IO Expander count!\n\r");
                        		counter_for_io_exp = 0;
                        		io_exp_current_state = i2c_IO_Expander_Get_Current_State();
                        		io_exp_current_state &= 0xF0;
                        		io_exp_current_state |= counter_for_io_exp;
                        		i2c_IO_Expander_Configure_IO(io_exp_current_state);
                        		lcdgotoxy(0,IO_EXP_COUNT_LOCATION);
                        		lcdputstr(convert_str(counter_for_io_exp));
                    		}break;
		
                		default:			// When an unitialized character is entered by the user
                    		{	
	                        	printf_tiny("\n\rCommand not initialized!\n\r");
                    		}
            		}
		}
	}
}
