#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

struct track {
  uint8_t serialNumber[7];
  uint8_t routeName[44];
  uint8_t difficulty[4];
  // TODO: add Date
  struct track * next;
};

uint8_t success;
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
uint8_t oldUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
uint8_t uidLength = 7;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

struct track * tracks = NULL;

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
  success = 0;
  uidLength = 7;
  
  uint8_t routeName[44];
  uint8_t difficulty[4];
  
  copyArray(uid, oldUid, uidLength);
  
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  // 100 = wait 100ms and then success = 0, 0 would mean no timeout and wait forever
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100);
  if (success) {
    // only track route once!
    if(!areEqualUids(uid, oldUid, uidLength)) {
      if (uidLength == 4) {
        Serial.println("Mifare Classic card is not supported!");
      }
      else if(uidLength == 7) {
        Serial.println("Detected Mifare Ultralight tag");
        
        success = readDifficulty(difficulty);
        success = success && readRouteName(routeName);
        
        if(success) {
          // save tracked route  
          struct track * newTrack = createTrack(uid, routeName, difficulty);
          if(newTrack != NULL) {
            addTrackToTracks(newTrack);  
            Serial.println("added track");
            printTracks();
          }
        }
      }
    }
  } else {
    // no nfc tag detected... reset the uids and wait of one
    clearUid(uid, uidLength);
    clearUid(oldUid, uidLength);
  }
}

void addTrackToTracks(struct track * trackToAdd) {
  struct track * currentTrack;
  
  if(tracks == NULL) {
    tracks = trackToAdd;  
  } else {
     currentTrack = tracks;
     while(currentTrack->next != NULL) {
       currentTrack = currentTrack->next;  
     }
     
     currentTrack->next = trackToAdd;
  }
}

void printTracks() {
  struct track * currentTrack;
  
  Serial.println();
  Serial.println("stored tracks:");
            
  if(tracks == NULL) {
    Serial.println("no tracks available");
    return;
  }
  
  currentTrack = tracks;
  printTrack(currentTrack);
  
   while(currentTrack->next != NULL) {
     currentTrack = currentTrack->next;  
     printTrack(currentTrack);
   }
}

void printTrack(struct track * trackToPrint) {
  Serial.print("SerialNumber: ");
  nfc.PrintHexChar(trackToPrint->serialNumber, 7);
  
  Serial.print("RouteName: ");
  nfc.PrintHexChar(trackToPrint->routeName, 44);
  
  Serial.print("Difficulty: ");
  nfc.PrintHexChar(trackToPrint->difficulty, 4);
  
  Serial.println("---------------------------");
}

struct track * createTrack(uint8_t * serialNumber, uint8_t * routeName, uint8_t * difficulty) {
  struct track * ptr;

  ptr = (struct track *) malloc(sizeof(struct track));
  if(ptr == NULL) {
    Serial.println("could not allocate memory");
    return NULL;  
  }
  
  copyArray(serialNumber, ptr->serialNumber, 7);
  copyArray(routeName, ptr->routeName, 44);
  copyArray(difficulty, ptr->difficulty, 4);
  ptr->next = NULL;
  return ptr;  
}

void clearUid(uint8_t * uid, int uidLength) {
  uint8_t emptyUid[] = { 0, 0, 0, 0, 0, 0, 0 };
  copyArray(emptyUid, uid, uidLength);
}

void copyArray(uint8_t * src, uint8_t * dest, int arrayLength) {
  int i;
  
  for(i = 0; i < arrayLength; i++) {
    dest[i] = src[i];  
  }
}

int areEqualUids(uint8_t * uid1, uint8_t * uid2, int uidLength) {
    int i;
    
    for(i = 0; i < uidLength; i++) {
      if(uid1[i] != uid2[i]) {
        return 0;  
      }
    }
    
    return 1;
}

uint8_t readMifareUltralightPage(uint8_t page, uint8_t * bufferArray) {
  uint8_t success;  
  int dataLength = 4;
  uint8_t data[dataLength];

  success = nfc.mifareultralight_ReadPage(page, data);
  if (success) {  
    copyArray(data, bufferArray, dataLength);
    
    Serial.print("Reading Page "); Serial.print(page); Serial.print(": ");
    nfc.PrintHexChar(bufferArray, dataLength);
    delay(100);
  } else {
    Serial.print("Could not read page "); Serial.println(page);
  }  
  
  return success;
}

uint8_t readDifficulty(uint8_t * difficulty) {
  return readMifareUltralightPage(4, difficulty);
}

uint8_t readRouteName(uint8_t * routeName) {
  uint8_t success = 1;  
  int pageNumber = 5;
  int pageSize = 4;
  int indexNumber;
  
  for(pageNumber = 5; pageNumber < 16; pageNumber++) {
    indexNumber = (pageNumber -5) * pageSize;
    success = success && readMifareUltralightPage(pageNumber, &routeName[indexNumber]);
  }
  
  return success;
}
