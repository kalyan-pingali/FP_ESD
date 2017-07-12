*Author Name* 			: Kalyan Pingali

*File Description* 	: LCD Driver, i2c driver, UI - Lab 4; Embedded System Design - Fall 2016

*Version*				: 7.0

**Revision History**

1.0 : Basic LCD functionality achieved
     
1.1 : Added print support to terminal (Serial Comm)
     
2.0 : I2C drivers added
     
3.0 : UI developed to meet requirements of lab 4
     
4.0 : Added functionality for creating custom characters on LCD
     
5.0 : IO Expander added
     
6.0 : Added Real Time Clock on LCD (using interrupts)
     
7.0 : EEPROM and hardware watchdog timer implemented

**Code reuse details**

Code has been modified and reused for i2c_send_byte and i2c_receive_byte functions from the following sources:
     
1. http://www.robot-electronics.co.uk/i2c-tutorial
     
2. http://www.8051projects.net/wiki/I2C_Implementation_on_8051#Implementing_I2C_in_C
