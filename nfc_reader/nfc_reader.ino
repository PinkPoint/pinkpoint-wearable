#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

// If using the breakout with SPI, define the pins for SPI communication.
#define PN532_SCK  (2)
#define PN532_MOSI (3)
#define PN532_SS   (4)
#define PN532_MISO (5)

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void setup() {
  Serial.begin(115200);
  Serial.println("Hello!");

  nfc.begin();
  
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
  while (1); // halt
  }
  
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // configure board to read RFID tags
  nfc.SAMConfig();
  
  Serial.println("Waiting for an ISO14443A Card ...");
}

void loop() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
    
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {
    if (uidLength == 4) {
      Serial.println("Detected Mifare Classic card");
      int i = 4;
      for(i = 4; i < 64; i++) {
        ReadMifareClassicBlock(i, uid, uidLength);
      }
    }
    else if(uidLength == 7) {
      Serial.println("Detected Mifare Ultralight tag");
      int i = 4;
      for(i = 4; i < 50; i++) {
        ReadMifareUltralightPage(i);
      }
    }
  }
}

void ReadMifareClassicBlock(uint8_t block, uint8_t * uid, uint8_t uidLength) {
  uint8_t success;  
  uint8_t data[16];
  
  success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block - (block % 4), 0, keya);
  if(success) {
    success = nfc.mifareclassic_ReadDataBlock(block, data);
    if(success) {
      Serial.print("Reading Block "); Serial.print(block); Serial.print(": ");
      nfc.PrintHexChar(data, 16);
      delay(100);
    } else {
      Serial.print("Could not read block "); Serial.println(block);
    }  
  } else {
    Serial.println("Could not authenticate Mifare Classic blocks. Please try another key.");  
  }
}

void ReadMifareUltralightPage(uint8_t page) {
  uint8_t success;  
  uint8_t data[32];
  success = nfc.mifareultralight_ReadPage(page, data);
  if (success)
  {  
    Serial.print("Reading Page "); Serial.print(page); Serial.print(": ");
    nfc.PrintHexChar(data, 4);
    delay(100);
  } else {
    Serial.print("Could not read page "); Serial.println(page);
  }  
}
