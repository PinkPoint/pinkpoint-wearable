/**************************************************************************/
/*! 
    @file     readMifare.pde
    @author   Adafruit Industries
	@license  BSD (see license.txt)

    This example will wait for any ISO14443A card or tag, and
    depending on the size of the UID will attempt to read from it.
   
    If the card has a 4-byte UID it is probably a Mifare
    Classic card, and the following steps are taken:
   
    - Authenticate block 4 (the first block of Sector 1) using
      the default KEYA of 0XFF 0XFF 0XFF 0XFF 0XFF 0XFF
    - If authentication succeeds, we can then read any of the
      4 blocks in that sector (though only block 4 is read here)
	 
    If the card has a 7-byte UID it is probably a Mifare
    Ultralight card, and the 4 byte pages can be read directly.
    Page 4 is read by default since this is the first 'general-
    purpose' page on the tags.


This is an example sketch for the Adafruit PN532 NFC/RFID breakout boards
This library works with the Adafruit NFC breakout 
  ----> https://www.adafruit.com/products/364
 
Check out the links above for our tutorials and wiring diagrams 
These chips use SPI or I2C to communicate.

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

*/
/**************************************************************************/
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

// Uncomment just _one_ line below depending on how your breakout or shield
// is connected to the Arduino:

// Use this line for a breakout with a software SPI connection (recommended):
//Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// Use this line for a breakout with a hardware SPI connection.  Note that
// the PN532 SCK, MOSI, and MISO pins need to be connected to the Arduino's
// hardware SPI SCK, MOSI, and MISO pins.  On an Arduino Uno these are
// SCK = 13, MOSI = 11, MISO = 12.  The SS line can be any digital IO pin.
//Adafruit_PN532 nfc(PN532_SS);

// Or use this line for a breakout or shield with an I2C connection:
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

void setup(void) {
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


void loop(void) {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  uint8_t * routeName;
  uint8_t * difficulty;
  
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  if (success) {
    // Display some basic information about the card
    Serial.println("Found an ISO14443A card");
    Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("  UID Value: ");
    nfc.PrintHex(uid, uidLength);
    Serial.println("");
    
    Serial.println("Want do you want to do? C = Clear / R = Read");
   
    int incomingByte = 0;   // for incoming serial data
  
    while(!Serial.available());

      // read the incoming byte:
      incomingByte = Serial.read();

      // say what you got:
      //Serial.print("I received: ");
      //Serial.println(incomingByte, HEX);
      
      //CLEAR
      if(incomingByte == 0x43) {
        incomingByte = Serial.parseInt();
        
        Serial.print("you want to clear page ");
        Serial.println(incomingByte);
        
        ClearPage(incomingByte);
      }
      // READ 
      else if(incomingByte == 0x52) {
        incomingByte = Serial.parseInt();
        
        Serial.print("you want to read page ");
        Serial.println(incomingByte);
        
        ReadPage(incomingByte);
      } 
      // WRITE 
      else if(incomingByte == 0x57) {
        Serial.print("you want to write a route. Please enter the route name (max 44 characters): ");
        Serial.readBytesUntil(10, routeName, 44);

        Serial.print("please enter the difficulty (max 4 characters): ");
        Serial.readBytesUntil(10, difficulty, 4);

        WritePage(routeName, difficulty);
      } else {
        Serial.println("nothing...");
      }
  }
}

void WritePage(uint8_t * routeName, uint8_t * difficulty) {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {
    if (uidLength == 4) {
      //mifareclassic_WritePage(page, data);
    } 
    else if(uidLength == 7) {      
      mifareultralight_WritePage(routeName, difficulty);
    }  
  } else {
    Serial.print("no tag detected");  
  }
}

void ClearPage(uint8_t page) {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {
    if (uidLength == 4) {
      mifareclassic_ClearPage(page);
    } 
    else if(uidLength == 7) {
      mifareultralight_ClearPage(page);
    }  
  } else {
    Serial.print("no tag detected");  
  }
}

void ReadPage(uint8_t page) {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {
    if (uidLength == 4) {
      mifareclassic_ReadPage(page);
    } 
    else if(uidLength == 7) {
      mifareultralight_ReadPage(page);
    }  
  } else {
    Serial.print("no tag detected");  
  }
}

void mifareclassic_ClearPage(uint8_t page) {
  uint8_t dataEmpty[16] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
  uint8_t success;
  uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  // Now we need to try to authenticate it for read/write access
  // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
  success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, page - (page % 4), 0, keya);
  if (success)
  {
    Serial.println("authenticated...");
    	
    success = nfc.mifareclassic_WriteDataBlock(page, dataEmpty);
    if (success)
    {
      Serial.print("cleared block ");  
      Serial.println(page);  
    }
    else
    {
      Serial.print("could not clear block ");
      Serial.println(page);
    }
  }
  else
  {
    Serial.println("Ooops ... authentication failed: Try another key?");
  }
}

void mifareclassic_WritePage(uint8_t page, uint8_t * data) {
  uint8_t success;
  uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  // Now we need to try to authenticate it for read/write access
  // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
  success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, page - (page % 4), 0, keya);
  if (success)
  {
    Serial.println("authenticated...");
    	
    success = nfc.mifareclassic_WriteDataBlock(page, data);
    if (success)
    {
      Serial.print("wrote block ");  
      Serial.print(page);
      Serial.print(": ");
      nfc.PrintHexChar(data, 4);
    }
    else
    {
      Serial.print("could not write block ");
      Serial.println(page);
    }
  }
  else
  {
    Serial.println("Ooops ... authentication failed: Try another key?");
  }
}


void mifareclassic_ReadPage(uint8_t page) {
  uint8_t data[16];
  uint8_t success;
  uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  // Now we need to try to authenticate it for read/write access
  // Try with the factory default KeyA: 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
  success = nfc.mifareclassic_AuthenticateBlock(uid, uidLength, page - (page % 4), 0, keya);
  if (success)
  {
    Serial.println("authenticated...");

    success = nfc.mifareclassic_ReadDataBlock(page, data);
    if (success)
    {  
      // Data seems to have been read ... spit it out
      nfc.PrintHexChar(data, 16);
      Serial.println("");
  		
      // Wait a bit before reading the card again
      delay(1000);
    } else {
      Serial.println("Ooops ... unable to read the requested page!?");
    }
  }
  else
  {
    Serial.println("Ooops ... authentication failed: Try another key?");
  }
}

void mifareultralight_ClearPage(uint8_t page) {
  uint8_t dataEmpty[4] = {0x0, 0x0, 0x0, 0x0};
  uint8_t success;
  
  success = nfc.mifareultralight_WritePage(page, dataEmpty);
  if(success) {
    Serial.print("cleared page ");  
    Serial.println(page);  
  } else {
    Serial.print("could not clear page ");
    Serial.println(page);
  }
}

void mifareultralight_WritePage(uint8_t * routeName, uint8_t * difficulty) {
  uint8_t success;
  int pageCount;
  int arraySize;
  int i;
  int pageSize = 4;
  int maxPageCount = 16;
  int difficultyStartPage = 4;
  int routeNameStartPage = 5;
  
  // write diffculty to page 4
  success = nfc.mifareultralight_WritePage(difficultyStartPage, difficulty);
  if(success) {
    Serial.print("wrote difficulty ");
    Serial.print(": ");
    nfc.PrintHexChar(difficulty, pageSize);
 } 
  // write route name to page 5-15
  arraySize = sizeof(routeName)/sizeof(routeName[0]);
  
  if(arraySize > 44) {
      Serial.println("The route name contains more than 44 characters. Only 44 character will be written to the NFC tag.");
  }
  
  pageCount = (arraySize / pageSize);
  if(arraySize % pageSize > 0) {
    pageCount++;
  }

  // write all characters of the route to the respecting pages. Break, if max number of pages exceeded  
  success = 1;
  for(i = routeNameStartPage; i < (pageCount + routeNameStartPage) && i < maxPageCount; i++){
    success = success && nfc.mifareultralight_WritePage(i, &routeName[(i-routeNameStartPage) * pageSize]);    
  }
  
  if(success) {
    Serial.print("wrote route name ");
    Serial.print(": ");
    nfc.PrintHexChar(routeName, arraySize);
  }  else {
    Serial.print("could not write route name ");
    nfc.PrintHexChar(routeName, arraySize);
  }
}

void mifareultralight_ReadPage(uint8_t page) {
  uint8_t success;  
  uint8_t data[16];
  
  Serial.print("reading page ");  
  Serial.println(page);
    
  success = nfc.mifareultralight_ReadPage(page, data);
  if (success)
  {  
    // Data seems to have been read ... spit it out
    nfc.PrintHexChar(data, 4);
    Serial.println("");
		
    // Wait a bit before reading the card again
    delay(1000);
  } else {
    Serial.println("Ooops ... unable to read the requested page!?");
  }
}

