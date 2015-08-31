#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define EMPTY                           0xFF
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
#define CARD_ADD                        0
#define CARD_REMOVE                     1
#define CARD_FOLD                       2
#define HAND_END                        3
#define LED_PIN                         6
boolean VERBOSE;

byte mux_pin[2][4] = {{2,3,4,5},{6,7,8,9}};
int On_Readers;  // Equivalent to B11110000000000. The first 4 bits represent the muck and board readers. The remaining 10 bits will store the state of the player switches.
int Game_State; //  Will be used to store the presence of cards in specified locations as a 13 bit binary number. From left to right bit: River, Turn, Flop, Player 10, Player 9, ...  Will also be used to determine game state.
int Previous_Game_State;
byte Player_Cards[NUM_PLAYERS][2];
byte Board_Cards[5];
float game_summary[2*NUM_PLAYERS+5][3];
byte readid[4];
byte storedid[4];

String ranks[13] = {"A","K","Q","J","T","9","8","7","6","5","4","3","2"};
String suits[4] = {"s","h","d","c"};

MFRC522 mfrc522(SS_PIN);	// Create MFRC522 instance

void setup() {
	Serial.begin(9600);		// Initialize serial communications with the PC
	while (!Serial);		// Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
	SPI.begin();			// Init SPI bus
        for (byte x = 0; x <=3; x++) {
          pinMode(mux_pin[RFID_MUX][x],OUTPUT);
          pinMode(mux_pin[SWITCH_MUX][x],OUTPUT); 
        }
        pinMode(LED_PIN,OUTPUT);
        VERBOSE = digitalRead(A1);
	Reset();
        // Initialize the board readers
        for (byte x = FIRST_BOARD_CHANNEL; x < (FIRST_BOARD_CHANNEL + NUM_BOARD_READERS); x++) {
          Select_Channel(x, RFID_MUX);
          mfrc522.PCD_Init();
          mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
          if (VERBOSE) {Serial.print(F("Board "));}
          if (VERBOSE) {Serial.print(x-FIRST_BOARD_CHANNEL+1);}
          if (VERBOSE) {Serial.print(F(" initialized. Antenna Gain = "));}
          if (VERBOSE) {Serial.println(mfrc522.PCD_GetAntennaGain());}
        }
}

void loop() {
        VERBOSE = digitalRead(A1);
	On_Readers = Update_On_Readers(On_Readers);
	// Loop through player readers and look for new cards
	while (((Game_State + 15360) ^ On_Readers) != 0 && !Card_On_Board()) {  // Wait until all players have cards to move to next phase of game
		for (byte i = 0; i < NUM_PLAYERS; i++) {
			if (bitRead(On_Readers, i)){// & ~bitRead(Game_State, i)) {  // Player switch is on and antenna has been initialized. Look for new cards.
				// Look for new cards
				Select_Channel(i, RFID_MUX);
                                
				if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                                        Blink_LED();
					if (VERBOSE) {Serial.print(F("Player "));}
					if (VERBOSE) {Serial.print(i+1);}
					if (VERBOSE) {Serial.print(F(" Card: "));}
					for (byte j = 0; j < 4; j++) {
						readid[j] = mfrc522.uid.uidByte[j];
					}
                                        int position = findposition(readid);
                                        if (position == -1) {
                                          if (VERBOSE) {Serial.print(F("Card Unknown"));}
                                        }
                                        else {
                                          int suit = position / 13;
                                          int rank = position % 13;
                                          if (VERBOSE) {Serial.print(ranks[rank]+suits[suit]);}
                                          if (VERBOSE) {Serial.print(millis());}
                                          Store_Player_Card(i,position);
                                        }
					if (VERBOSE) {Serial.println();}
                                        mfrc522.PICC_HaltA();
				}				
			}
		}
	}
        if (VERBOSE) {Serial.println(F("All players have 2 cards"));}
        DumpCardsToSerial();
        Previous_Game_State = Game_State;
	while(!Hand_Complete()) {
	// Look for cards in muck/board readers. Check if cards match players cards. Otherwise, add new cards to board.
		for (byte x = FIRST_BOARD_CHANNEL; x < (FIRST_BOARD_CHANNEL + NUM_BOARD_READERS); x++){
			Select_Channel(x,RFID_MUX);
			if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
                                Blink_LED();
				// New card found in muck/board pile. Compare to player cards to determine if a player has folded
				// If new card does not match a player card and card appears on the board reader, then card should be added to board.
				for (byte j = 0; j < 4; j++) {
				  readid[j] = mfrc522.uid.uidByte[j];
				}
                                int position = findposition(readid);
                                if (position != -1) {
                                  if (!Player_Folded(position)) {
					if (x > (FIRST_BOARD_CHANNEL + NUM_BOARD_READERS - 2)) { // Location is not one of the muck readers. This prevents burn cards from appearing on the board.
						for (byte j = 0; j < 5; j++) {
                                                        if (Board_Cards[j] == position) {break;} // Read card has already been stored.
                                                        if (j > 1) { bitSet(Game_State, (FLOP + j - 2)); }
							if (Board_Cards[j] == EMPTY) { 
                                                          Add_Board_Card(j,position);
                                                          break;
                                                        }
						}
					}
				  }
                                }
                                mfrc522.PICC_HaltA();
			}	
		}
		if(Game_State_Change()) {
                  DumpCardsToSerial();
		}
	}
	Game_Summary_DumpToSerial();
	if (VERBOSE) {Serial.println(F("Hand Complete"));}
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
                        if (VERBOSE) {Serial.print(F("Player "));}
                        if (VERBOSE) {Serial.print(x+1);}
                        if (VERBOSE) {Serial.print(F(" reader initialized. Antenna Gain = "));}
                        if (VERBOSE) {Serial.println(mfrc522.PCD_GetAntennaGain());}
		}
		bitWrite(num, x, state);
	}
        return num;
}

bool Player_Folded(byte test_position) {
        for (byte i = 0; i < NUM_PLAYERS; i++) {
		if (test_position == Player_Cards[i][0] || test_position == Player_Cards[i][1]) { // Player folded.
			bitClear(Game_State, i);
                        Update_Game_Summary(2*i, Player_Cards[i][0], CARD_FOLD, millis()/1000.);
                        Update_Game_Summary(2*i+1, Player_Cards[i][1], CARD_FOLD, millis()/1000.);
			return(true);
		}
	}
	return(false);
}

byte Get_Number_Of_Board_Cards() {
  byte num_board_cards = 0;
  if (bitRead(Game_State,FLOP) == 1) num_board_cards = 3;
  if (bitRead(Game_State,TURN) == 1) num_board_cards = 4;
  if (bitRead(Game_State,RIVER) == 1) num_board_cards = 5;
  return num_board_cards;
}

bool Hand_Complete() {
        int count = 0;
        float end_time = millis()/1000. + 5.;
	if (bitRead(Game_State, RIVER)) { // River card has been dealt.
          for (byte i=0; i<NUM_PLAYERS; i++) {
            if (bitRead(Game_State,i)) {
              Update_Game_Summary(2*i,EMPTY,HAND_END,end_time);
              Update_Game_Summary(2*i+1,EMPTY,HAND_END,end_time);
            }
          }
          for (byte i=0; i<Get_Number_Of_Board_Cards(); i++) {
            Update_Game_Summary(2*NUM_PLAYERS+i,EMPTY,HAND_END,end_time);
          }
          return(true);
        }  
	else {
                int winner=0;
		for (byte j = 0; j < NUM_PLAYERS; j++) {
			if (bitRead(Game_State, j)) {
                           count++;
                           winner=j+1;
                           Update_Game_Summary(2*j,EMPTY,HAND_END,end_time);
                           Update_Game_Summary(2*j+1,EMPTY,HAND_END,end_time);
                        }
		}
                if (count < 2) { // Only 1 player has cards.
                  if (VERBOSE) {Serial.print(F("Player "));}
                  if (VERBOSE) {Serial.print(winner);}
                  if (VERBOSE) {Serial.println(F(" wins! Everyone else folded."));}
                  for (byte i=0; i<Get_Number_Of_Board_Cards(); i++) {
                    Update_Game_Summary(2*NUM_PLAYERS+i,EMPTY,HAND_END,end_time);
                  }
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
  Previous_Game_State = 0;
  On_Readers = 15360;
  memset(Player_Cards, EMPTY, sizeof(Player_Cards));
  memset(Board_Cards, EMPTY, sizeof(Board_Cards));
  memset(game_summary, 0., sizeof(game_summary));
}

int findposition(byte test[]) {
  for (int i=0; i<52; i++) {
    int start = 4*(i+1)+1;
    for (int j=0; j<4; j++) {
      storedid[j] = EEPROM.read(start+j);
    }
    if (checkTwo(test,storedid)) return i;
  }
  return -1;
}
boolean checkTwo(byte a[], byte b[]) {
  boolean match = true;
  for ( int i = 0; i < 4; i++) {
    if (a[i] != b[i]) match = false;
  }
  if (match) return true;
  else return false;
}

void DumpCardsToSerial() {
  for (byte i = 0; i < NUM_PLAYERS; i++) {
    if (bitRead(Game_State,i) == 1) {
      if (VERBOSE) {Serial.print(F("Player "));}
      if (VERBOSE) {Serial.print(i+1);}
      if (VERBOSE) {Serial.print(F("'s Cards: "));}
      if (VERBOSE) {Serial.print(ranks[Player_Cards[i][0] % 13]+suits[Player_Cards[i][0] / 13]);}
      if (VERBOSE) {Serial.println(ranks[Player_Cards[i][1] % 13]+suits[Player_Cards[i][1] / 13]);}
    }
  }
  if (VERBOSE) {Serial.print(F("Board Cards: "));}
  for (byte i = 0; i < Get_Number_Of_Board_Cards(); i++) {
    if (VERBOSE) {Serial.print(ranks[Board_Cards[i] % 13]+suits[Board_Cards[i] / 13]);}
  }
  if (VERBOSE) {Serial.println();}
}

void Game_Summary_DumpToSerial() {
  for (byte i=0; i<2*NUM_PLAYERS; i++) {
    if ((byte)game_summary[i][0] >= 1) {  // Only output cards that were read
      Serial.print(i);
      Serial.print(F(" "));
      Serial.print((byte)game_summary[i][0]);
      Serial.print(F(" "));
      Serial.print(ranks[(byte)game_summary[i][0] % 13]+suits[(byte)game_summary[i][0] / 13]);
      Serial.print(F(" "));
      Serial.print(game_summary[i][1]);
      Serial.print(F(" "));
      Serial.println(game_summary[i][2]);
    }
  }
  for (byte i = 0; i < Get_Number_Of_Board_Cards(); i++) {
    Serial.print(2*NUM_PLAYERS+i);
    Serial.print(F(" "));
    Serial.print((byte)game_summary[2*NUM_PLAYERS+i][0]);
    Serial.print(F(" "));
    Serial.print(ranks[Board_Cards[i] % 13]+suits[Board_Cards[i] / 13]);
    Serial.print(F(" "));
    Serial.print(game_summary[2*NUM_PLAYERS+i][1]);
    Serial.print(F(" "));
    Serial.println(game_summary[2*NUM_PLAYERS+i][2]);
  }
  if (VERBOSE) {Serial.println();}
}

void Update_Game_Summary(byte position, byte card_id, byte instruction, float time) {
  switch (instruction) {
    case CARD_ADD:
      game_summary[position][0] = card_id;
      game_summary[position][1] = time;
      break;
    case CARD_REMOVE:
      for (byte i=0; i<3; i++) {
        game_summary[position][i] = EMPTY;
      }
      break;
    case CARD_FOLD:
      game_summary[position][2] = time;
      break;
    case HAND_END:
      game_summary[position][2] = time;
      break;
  }
}

void Store_Player_Card(byte player_id, byte card_id) {
  int duplicate = Duplicate_Player_Card(card_id); // Stores the player_id of a duplicate location.
  switch (duplicate) {
    case -1: // No duplicate found.
      Add_Player_Card(player_id,card_id);
      break;
    default:
      if (duplicate != player_id) { // Player has a card that was previously at another location
        // Remove card from duplicate location
        if (Player_Cards[duplicate][0] == card_id) {
          // Move duplicate player's 2nd card to 1st location
          Player_Cards[duplicate][0] = Player_Cards[duplicate][1];
          Update_Game_Summary(2*duplicate,game_summary[2*duplicate+1][0],CARD_ADD,game_summary[2*duplicate+1][1]);
        }
        // Remove duplicate player's 2nd card
        Player_Cards[duplicate][1] = EMPTY;
        Update_Game_Summary(2*duplicate+1,EMPTY,CARD_REMOVE,EMPTY);
        bitClear(Game_State,duplicate);
        Add_Player_Card(player_id,card_id);
      }
  }
}

void Add_Player_Card(byte player_id, byte card_id) {
  if (Player_Cards[player_id][0] == EMPTY) {  // Store card value in memory
    Player_Cards[player_id][0] = card_id;
    Update_Game_Summary(2*player_id,card_id,CARD_ADD,millis()/1000.);
  }
  else if (Player_Cards[player_id][1] == EMPTY) {  // Player has a second card
    Player_Cards[player_id][1] = card_id;
    bitSet(Game_State, player_id);  //  No longer need to read data from player. No provision for misdelt cards. Need a verification function to check cards again.
    Update_Game_Summary(2*player_id+1,card_id,CARD_ADD,millis()/1000.);
  }
  else { // Player received an additional card. Card may belong to another player. Or one of the original cards may have flipped during the deal and the player received a replacement.
    Serial.print(F("What do I do with this card"));
  }
}

void Add_Board_Card(byte board_id, byte card_id) {
  Board_Cards[board_id] = card_id;
  Update_Game_Summary(2*NUM_PLAYERS+board_id,card_id,CARD_ADD,millis()/1000.);
}

int Duplicate_Player_Card(byte card_id) {
  for (byte i=0; i<NUM_PLAYERS; i++) {
    if (Player_Cards[i][0] == card_id || Player_Cards[i][1] == card_id) { // Return player that had duplicate card
      return i;
    }
  }
  return -1; // No duplicate found
}

void Blink_LED() {
  for (byte i=0; i<16; i++) {
    analogWrite(LED_PIN,(i+1)*16);
    delay(31);
  }
  for (byte i=16; i>0; i--) {
    analogWrite(LED_PIN,(i-1)*16);
    delay(31);
  }
}

boolean Card_On_Board() {
  byte readboardid[4];
  Select_Channel(FIRST_BOARD_CHANNEL,RFID_MUX);
  mfrc522.PCD_Init();
  delay(10);
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return false;
  }
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);
  for (byte j = 0; j < 4; j++) {
    readboardid[j] = mfrc522.uid.uidByte[j];
  }
  int test_id = findposition(readboardid);
  static int board_card_id = 0;
  static unsigned long card_time_on_board;
  if (test_id == board_card_id) {
    if (millis() - card_time_on_board > 5000) {
      board_card_id = 0;
      return (true);
    }
  }
  else {
    board_card_id = test_id;
    card_time_on_board = millis();
  }
  return(false);
}
