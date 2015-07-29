# RFID_Poker_Table

 	This project is used to control multiple MFRC522 readers mounted beneath the
 	felt of a poker table and sends the information over serial to an attached
 	device. This project was designed around the following hardware:
 
 		Arduino Duemilanove
 		MFRC522 reader
 		Sparkfun Analog/Digital MUX Breakout - CD74HC4067
 
 	The MFRC522 library, obtained from https://github.com/miguelbalboa/rfid, was
 	modified from it's original state for my specific purpose. More specifically,
 	the use of a reset pin was removed, as this will be controlled externally,
 	with hardware. Each player will have a switch in order to turn off their own
 	reader.
 
 		Arduino pins used in this project:
 
 	D0 - Serial Rx
 	D1 - Serial Tx
 	D2 - S0, MUX1
 	D3 - S1, MUX1
 	D4 - S2, MUX1
 	D5 - S3, MUX1
 	D6 - S0, MUX1
 	D7 - S1, MUX1
 	D8 - S2, MUX1
 	D9 - S3, MUX1
 	D10 - COM, MUX1
 	D11 - MOSI
 	D12 - MISO
 	D13 - SCK
 	A0 - COM, MUX2
 	
 		MUX1 pins used in this project:
 
 	VCC - +5V
 	EN - GND
 	GND - GND
 	CH0 - MFRC522-1 (Player 1 Reader)
 	CH1 - MFRC522-2 (Player 2 Reader)
 	CH2 - MFRC522-3 (Player 3 Reader)
 	CH3 - MFRC522-4 (Player 4 Reader)
 	CH4 - MFRC522-5 (Player 5 Reader)
 	CH5 - MFRC522-6 (Player 6 Reader)
 	CH6 - MFRC522-7 (Player 7 Reader)
 	CH7 - MFRC522-8 (Player 8 Reader)
 	CH8 - MFRC522-9 (Player 9 Reader)
 	CH9 - MFRC522-10 (Player 10 Reader)
 	CH10 - MFRC522-11 (Muck 1 Reader)
 	CH11 - MFRC522-12 (Muck 2 Reader)
 	CH12 - MFRC522-13 (Board/Muck 3 Reader)
 	CH13 - MFRC522-14 (Board/Muck 4 Reader)
 	CH14 - MFRC522-15 (Board/Muck 5 Reader)
 	CH15 - MFRC522-16 (Board/Muck 6 Reader)
 
 		MUX2 pins used in this project:
 
 	VCC - 5V
 	EN - GND
 	GND - GND
 	CH0 - Switch Circuit, Player 1
 	CH1 - Switch Circuit, Player 2
 	CH2 - Switch Circuit, Player 3
 	CH3 - Switch Circuit, Player 4
 	CH4 - Switch Circuit, Player 5
 	CH5 - Switch Circuit, Player 6
 	CH6 - Switch Circuit, Player 7
 	CH7 - Switch Circuit, Player 8
 	CH8 - Switch Circuit, Player 9
 	CH9 - Switch Circuit, Player 10
 	CH10 - NC
 	CH11 - NC
 	CH12 - NC
 	CH13 - NC
 	CH14 - NC
 	CH15 - NC
 
 		Typical MFRC522 Module
 
 	SCK - Arduino 13
 	MOSI - Arduino 11
 	MISO - Arduino 12
 	SS - MUX1 CHx
 	RST - 3.3V (In case of Player reader, also connected to switch and MUX2 CHx)
 	3.3V - 3.3V
 	GND - GND
 
