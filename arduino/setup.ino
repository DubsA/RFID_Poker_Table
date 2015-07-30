#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define NUM_RANKS  13
#define NUM_SUITS  4
#define SS_PIN  10
#define WIPER A1
MFRC522 mfrc522(SS_PIN);
String ranks[13] = {"A","K","Q","J","T","9","8","7","6","5","4","3","2"};
String suits[4] = {"s","h","d","c"};
bool successWrite;
bool successRead;
bool finished=false;
byte readid[4];
byte masterid[4];
byte storedid[4];

void setup() {
  Serial.begin(9600);
  while(!Serial);
  SPI.begin();
  // Select Player 1 reader as the location to read in data
  for (byte x = 0; x <=3; x++){
    int ss = x+2;
    pinMode(ss,OUTPUT);
    digitalWrite(ss, (0 >> x) & 01);
  }
  delay(250);
  mfrc522.PCD_Init();
  pinMode(WIPER, INPUT_PULLUP);
  if (digitalRead(WIPER) == LOW) {	// when button pressed pin should get low, button connected to ground
    wipe_eeprom();
  }
  // Check if master card defined, if not let user choose a master card
  // This also useful to just redefine Master Card
  // You can keep other EEPROM records just write other than 143 to EEPROM address 1
  // EEPROM address 1 should hold magical number which is '143'
  if (EEPROM.read(0) != 143) {  		
    define_master();
  }
  for (int i=0; i<4; i++) {
    masterid[i] = EEPROM.read(i+1);
  }
  Serial.println(F("-------------------"));
  Serial.println(F("Cards should be scanned using Player 1's reader."));
  Serial.println(F("Deck will be scanned in this order:"));
  Serial.println(F(" As->...->2s, Ah->...->2h, Ad->...->2d, Ac->...->2c"));
  Serial.println();
}

void loop() {  
  if (finished) return;
  for (int suit=0; suit<NUM_SUITS; suit++) {
    for (int rank=0; rank<NUM_RANKS; rank++) {
      Serial.println();
      Serial.print(F("Scan tag attached to "));
      Serial.print(ranks[rank] + suits[suit]);
      Serial.println(F("."));
      do {
        do {
          successRead = getID();            // sets successRead to true when we get read from reader otherwise false
        }
        while (!successRead);                  // Program will not go further while you not get a successful read
  
        if (mfrc522.uid.size < 4) {
          Serial.print(F("Improperly sized UID. Scan a new card for the "));
          Serial.println(ranks[rank] + suits[suit]);
          successWrite = false;
        }
        else if (isMaster(readid)){
          Serial.print(F("Skipping eeprom write for card: "));
          Serial.println(ranks[rank] + suits[suit]);
          successWrite = true;
        }
        else if (isDuplicate(readid,(suit+1),rank)) {
          Serial.print(F("You scanned a duplicate tag. Please scan another tag for card: "));
          Serial.println(ranks[rank] + suits[suit]);
          successWrite = false;
        }
        else {
          int start = 4*((suit+1)*(rank)+1)+1;
          for (byte i = 0; i < 4; i++) {
            EEPROM.write(start + i, mfrc522.uid.uidByte[i]);
          }
          successWrite = true;
        }
        mfrc522.PICC_HaltA();
      }
      while(!successWrite);
    }
  }
  for (byte i = 0; i < NUM_SUITS; i++) {
    for (byte j = 0; j < NUM_RANKS; j++) {
      Serial.print(ranks[j] + suits[i]);
      Serial.print(F(" key: "));
      int start = 4*((i+1)*j+1)+1;
      for (byte k = start; k < start+4; k++) {
        Serial.print(EEPROM.read(k) < 0x10 ? " 0" : " ");
        Serial.print(EEPROM.read(k), HEX);
      }
      Serial.println();
    }
  }
  finished=true;
}

bool getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return false;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return false;
  }
  Serial.println(F("Scanned PICC's UID:"));
  for (int i = 0; i < 4; i++) {  //
    readid[i] = mfrc522.uid.uidByte[i];
    Serial.print(readid[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA();
  return true;
}

void wipe_eeprom() {
  Serial.println(F("Wipe Button Pressed"));
  Serial.println(F("You have 5 seconds to Cancel"));
  Serial.println(F("This will be remove all records and cannot be undone"));
  delay(5000);                        // Give user enough time to cancel operation
  if (digitalRead(WIPER) == LOW) {    // If button still be pressed, wipe EEPROM
    Serial.println(F("Starting Wiping EEPROM"));
    for (int x = 0; x < EEPROM.length(); x = x + 1) {    //Loop end of EEPROM address
      if (EEPROM.read(x) == 0) {              //If EEPROM address 0
        // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
      }
      else {
        EEPROM.write(x, 0); 			// if not write 0 to clear, it takes 3.3mS
      }
    }
    Serial.println(F("EEPROM Successfully Wiped"));
  }
  else {
    Serial.println(F("Wiping Cancelled"));
  }
}

void define_master() {
  Serial.println(F("No Master Card Defined"));
  Serial.println(F("Scan A PICC to Define as Master Card"));
  do {
    successRead = getID();            // sets successRead to 1 when we get read from reader otherwise 0
  }
  while (!successRead);                  // Program will not go further while you not get a successful read
  for ( int j = 0; j < 4; j++ ) {        // Loop 4 times
    EEPROM.write( 1 + j, readid[j] );  // Write scanned PICC's UID to EEPROM, start from address 1
    masterid[j] = readid[j];
  }
  EEPROM.write(0, 143);                  // Write to EEPROM we defined Master Card.
  Serial.println(F("Master Card Defined"));
}

boolean isMaster(byte test[]) {
  if (checkTwo(test,masterid))
    return true;
  else
    return false;
}

boolean isDuplicate(byte test[],int a, int b) {
  for (int i=0; i<a*b; i++) {
    int start = 4*(i+1)+1;
    for (int j=0; j<4; j++) {
      storedid[j] = EEPROM.read(start+j);
    }
    if (checkTwo(test,storedid)) return true;
  }
  return false;
}

boolean checkTwo(byte a[], byte b[]) {
  boolean match = true;
  for ( int i = 0; i < 4; i++) {
    if (a[i] != b[i]) match = false;
  }
  if (match) return true;
  else return false;
}
