#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN				10		
#define SWITCH_PIN			A0
#define RFID_MUX			0
#define SWITCH_MUX			1
#define NUM_PLAYERS			2
#define NUM_BOARD_READERS	        1
#define FIRST_BOARD_CHANNEL             10
#define FLOP				12		// Bit location in Game_State
#define TURN				13		// Bit location in Game_State
#define RIVER				14		// Bit location in Game_State

byte mux_pin[2][4] = {{2,3,4,5},{6,7,8,9}};
int On_Readers = 15360;  // Equivalent to B11110000000000. The first 4 bits represent the muck and board readers. The remaining 10 bits will store the state of the player switches.
int Game_State = 0; //  Will be used to store the presence of cards in specified locations as a 13 bit binary number. From left to right bit: River, Turn, Flop, Player 10, Player 9, ...  Will also be used to determine game state.
int Previous_Game_State = 0;
byte Player_Cards[NUM_PLAYERS][2];
byte Board_Cards[5];

MFRC522 mfrc522(SS_PIN);	// Create MFRC522 instance

void setup() {
	Serial.begin(9600);		// Initialize serial communications with the PC
	while (!Serial);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
	SPI.begin();			// Init SPI bus
        for (byte x = 0; x <=3; x++) {
          pinMode(mux_pin[0][x],OUTPUT);
          pinMode(mux_pin[1][x],OUTPUT); 
        }
	memset(Player_Cards, 0x00, sizeof(Player_Cards));
	memset(Board_Cards, 0x00, sizeof(Board_Cards));
        // Initialize the board readers
        for (byte x = FIRST_BOARD_CHANNEL; x < (FIRST_BOARD_CHANNEL + NUM_BOARD_READERS); x++) {
          Select_Channel(x, RFID_MUX);
          mfrc522.PCD_Init();
          mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
          Serial.print(F("Board "));
          Serial.print(x-FIRST_BOARD_CHANNEL+1);
          Serial.println(F(" initialized."));
        }
}

void loop() {
	On_Readers = Update_On_Readers(On_Readers);
	// Loop through player readers and look for new cards
	while (((Game_State + 15360) ^ On_Readers) != 0) {  // Wait until all players have cards to move to next phase of game
		for (byte i = 0; i < NUM_PLAYERS; i++) {
			if (bitRead(On_Readers, i) & ~bitRead(Game_State, i)) {  // Player switch is on and antenna has been initialized. Look for new cards.
				// Look for new cards
				Select_Channel(i, RFID_MUX);
                                
				if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
					Serial.print(F("Player "));
					Serial.print(i+1);
					Serial.print(F(" Card UID:"));
					for (byte j = 0; j < mfrc522.uid.size; j++) {
						Serial.print(mfrc522.uid.uidByte[j] < 0x10 ? " 0" : " ");
						Serial.print(mfrc522.uid.uidByte[j], HEX);
					}
					if (Player_Cards[i][0] == 0x00) {  // Store card value in memory
						Player_Cards[i][0] = mfrc522.uid.uidByte[0];
					}
					else {  // Player has a second card
						Player_Cards[i][1] = mfrc522.uid.uidByte[0];
						bitSet(Game_State, i);  //  No longer need to read data from player. No provision for misdelt cards. Need a verification function to check cards again.
					}
					Serial.println();
                                        mfrc522.PICC_HaltA();
				}				
			}
		}
	}
        Serial.println(F("All players have 2 cards"));
        for (byte i = 0; i < NUM_PLAYERS; i++) {
          Serial.print(F("Player "));
          Serial.print(i+1);
          Serial.print(F("'s Cards: "));
          Serial.print(Player_Cards[i][0], HEX);
          Serial.println(Player_Cards[i][1], HEX);
        }
        Previous_Game_State = Game_State;
	while(!Hand_Complete()) {
	// Look for cards in muck/board readers. Check if cards match players cards. Otherwise, add new cards to board.
		for (byte x = FIRST_BOARD_CHANNEL; x < (FIRST_BOARD_CHANNEL + NUM_BOARD_READERS); x++){
			Select_Channel(x,RFID_MUX);
			if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
				// New card found in muck/board pile. Compare to player cards to determine if a player has folded
				// If new card does not match a player card and card appears on the board reader, then card should be added to board.
				if (!Player_Folded()) {
					if (x > (FIRST_BOARD_CHANNEL + NUM_BOARD_READERS - 2)) {
						for (byte j = 0; j < 5; j++) {
                                                        if (j > 1) { bitSet(Game_State, (FLOP + j - 2)); }
							if (Board_Cards[j] == 0x00) { Board_Cards[j] = mfrc522.uid.uidByte[0]; break;}
						}
					}
				}
                                mfrc522.PICC_HaltA();
			}	
		}
		if(Game_State_Change()) {
			for (byte i = 0; i < NUM_PLAYERS; i++) {
				Serial.print(F("Player "));
				Serial.print(i+1);
				Serial.print(F(" Cards: "));
				Serial.print(Player_Cards[i][0],HEX);
				Serial.print(Player_Cards[i][1],HEX);
				Serial.println();
			}
			Serial.print(F("Board Cards: "));
			for (byte i = 0; i < 5; i++) {
				if (Board_Cards[i] == 0x00) {break;}
				Serial.print(Board_Cards[i], HEX);
			}
                        Serial.println();
		}
	}
	
	Serial.println(F("Hand Complete"));
        Reset();
}

void Select_Channel(int channel, int mux_select){	
	for (byte x = 0; x <= 3; x++){
		int ss = mux_pin[mux_select][x];
		digitalWrite(ss, (channel >> x) & 01);
	}
        delay(25);
}

int Update_On_Readers(int num) {
	// Loop through all channels and read switch state. Store current state in On_Readers
	for (byte x = 0; x < NUM_PLAYERS; x++) {
		Select_Channel(x, SWITCH_MUX);
		byte state = digitalRead(A0);
		if (bitRead(num, x) == 0 && state == 1) {  // Player switch was changed to ON since last check. Need to re-Init the reader.
			Select_Channel(x, RFID_MUX);
			mfrc522.PCD_Init();
                        mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
                        Serial.print(F("Player "));
                        Serial.print(x+1);
                        Serial.println(F(" reader initialized."));
		}
		bitWrite(num, x, state);
	}
        return num;
}

bool Player_Folded() {
	for (byte i = 0; i < NUM_PLAYERS; i++) {
		if (mfrc522.uid.uidByte[0] == Player_Cards[i][0] || mfrc522.uid.uidByte[0] == Player_Cards[i][1]) { // Player folded.
			bitClear(Game_State, i);
			return(true);
		}
	}
	return(false);
}

bool Hand_Complete() {
        int count = 0;
	if (bitRead(Game_State, RIVER)) { return(true);}  // River card has been dealt.
	else {
                int winner=0;
		for (byte j = 0; j < NUM_PLAYERS; j++) {
			if (bitRead(Game_State, j)) {
                           count++;
                           winner=j+1;
                        }
		}
                if (count < 2) { // Only 1 player has cards.
                  Serial.print(F("Player "));
                  Serial.print(winner);
                  Serial.println(F(" wins! Everyone else folded."));
                  return(true);
                }
		else {return(false);}
	}
}

bool Game_State_Change() {
	if ((Previous_Game_State >> NUM_PLAYERS) != (Game_State >> NUM_PLAYERS)) {
		Previous_Game_State = Game_State;
		return(true);
	}
	else return(false);
}

void Reset() {
  Game_State = 0;
  memset(Player_Cards, 0x00, sizeof(Player_Cards));
  memset(Board_Cards, 0x00, sizeof(Board_Cards));
}
