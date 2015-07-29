#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define NUM_CARDS  13
#define SS_PIN  10
MFRC522 mfrc522(SS_PIN);
String card[NUM_CARDS] = {"As","Ks","Qs","Js","Ts","9s","8s","7s","6s","5s","4s","3s","2s"};
bool asked;
byte count;

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
  count=0;
  asked = false;
  Serial.println(F("Tag ID's will be stored in the EEPROM. Player 1 reader has been initialized."));
  Serial.println(F("Deck will be scanned in this order:"));
  Serial.println(F(" As->...->2s, Ah->...->2h, Ad->...->2d, Ac->...->2c"));
}

void loop() {  
  if (count == NUM_CARDS) {return;}
  if (  !asked) {
    Serial.println();
    Serial.print(F("Scan tag attached to "));
    Serial.print(card[count]);
    Serial.println(F("."));
    asked = true;
  }
  // Look for new cards, and select one if present
  if ( ! mfrc522.PICC_IsNewCardPresent() || ! mfrc522.PICC_ReadCardSerial() ) {
    delay(50);
    return;
  }
  if (mfrc522.uid.size != 7) {
    Serial.println(F("Improperly sized UID. Scan a new card"));
    mfrc522.PICC_HaltA();
    return;
  }
  int start = (count * 7);
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    EEPROM.write(start + i, mfrc522.uid.uidByte[i]);
  }
  mfrc522.PICC_HaltA();
  
  
  count++;
  asked = false;
  if (count == NUM_CARDS) {
    for (byte i = 0; i < NUM_CARDS; i++) {
      Serial.print(card[i]);
      Serial.print(F(" key: "));
      for (byte j = i*7; j < 7*(i+1); j++) {
        Serial.print(EEPROM.read(j) < 0x10 ? " 0" : " ");
        Serial.print(EEPROM.read(j), HEX);
      }
      Serial.println();
    }
  }
}
