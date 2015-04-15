#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

#include "Adafruit_BLE_UART.h"

// **** NFC define

// If using the breakout or shield with I2C, define just the pins connected
// to the IRQ and reset lines.  Use the values below (2, 3) for the shield!
#define PN532_IRQ   (2)
#define PN532_RESET (3)  // Not connected by default on the NFC Shield

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// **** BLE define
#define ADAFRUITBLE_REQ 10
#define ADAFRUITBLE_RDY 3     // We use '3' cause '2' is occupied by the NFC shield
#define ADAFRUITBLE_RST 9

Adafruit_BLE_UART BTLEserial = Adafruit_BLE_UART(ADAFRUITBLE_REQ, ADAFRUITBLE_RDY, ADAFRUITBLE_RST);

struct track {
  uint8_t serialNumber[7];
  uint8_t routeName[44];
  uint8_t difficulty[4];
  // TODO: add Date
  struct track * next;
};

uint8_t oldUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
struct track * tracks = NULL;

// **** BLE variables
aci_evt_opcode_t laststatus = ACI_EVT_DISCONNECTED;

void setup() {
  Serial.begin(115200);
  Serial.println(F("Hello!"));

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print(F("Didn't find PN53x board"));
    while (1); // halt
  }

  // Got ok data, print it out!
  Serial.print(F("Found chip PN5")); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print(F("Firmware ver. ")); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print(F(".")); Serial.println((versiondata >> 8) & 0xFF, DEC);

  // configure board to read RFID tags
  nfc.SAMConfig();

  Serial.println(F("Waiting for an ISO14443A Card ..."));

  // BLE Setup
  BTLEserial.setDeviceName("PiPoint"); /* 7 characters max! */
  BTLEserial.begin();

  Serial.println(F("BLE initialized. Waiting for connection..."));
}

void loop() {
  uint8_t uidLength = 7;  
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID

  uint8_t routeName[44];
  uint8_t difficulty[4];
  uint8_t success = 0;
  
  // Tell the nRF8001 to do whatever it should be working on.
  BTLEserial.pollACI();

  // Ask what is our current status
  aci_evt_opcode_t status = BTLEserial.getState();
  
  // the status changed....
  if (status != laststatus) {
    printBleStatus(status);
    laststatus = status;
  }

  if (status == ACI_EVT_CONNECTED) {
    handleBtleCommands();
  }

  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  // 100 = wait 100ms and then success = 0, 0 would mean no timeout and wait forever
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100);
  if (success) {
    
    // only track route once!
    if (!areEqualArrays(uid, oldUid, uidLength)) {
      if (uidLength == 4) {
        Serial.println(F("Mifare Classic card is not supported!"));
      }
      else if (uidLength == 7) {
        Serial.println(F("Detected Mifare Ultralight tag"));

        success = readDifficulty(difficulty);
        success = success && readRouteName(routeName);

        if (success) {
          // save tracked route
          struct track * newTrack = createTrack(uid, routeName, difficulty);
          if (newTrack != NULL) {
            addTrackToTracks(newTrack);
            Serial.println(F("added track"));
            printTracks();
          }
        }
      }
    }
    
    copyArray(uid, oldUid, uidLength);
  
  } else {
    // no nfc tag detected... reset the uids and wait of one
    clearUid(uid, uidLength);
    clearUid(oldUid, uidLength);
  }
}

void handleBtleCommands() {
  if (BTLEserial.available()) {
    Serial.print(F("* ")); Serial.print(BTLEserial.available()); Serial.println(F(" bytes available from BTLE"));
  }

  while (BTLEserial.available()) {
    char command = BTLEserial.read();
    if(command == 'A') {
      // get available tracks 
      uint8_t numberOfTracks = getNumberOfTracks();
      
      Serial.print(F("* Get number of available tracks: "));
      Serial.println(numberOfTracks);
      
      BTLEserial.write(numberOfTracks); 
    }
    if(command == 'B') {
      // get next track  
      uint8_t packet[16];
      createBleTrackPacket(packet);
      
      Serial.print(F("* Get next track: "));
      //Serial.println(packet, HEX);
      nfc.PrintHex(packet, 16);
      
      BTLEserial.write(packet, 16);
    }
    
    Serial.print(command);
  }
}

void printBleStatus(aci_evt_opcode_t status) {
  if (status == ACI_EVT_DEVICE_STARTED) {
    Serial.println(F("* Advertising started"));
  }
  if (status == ACI_EVT_CONNECTED) {
    Serial.println(F("* Connected!"));
  }
  if (status == ACI_EVT_DISCONNECTED) {
    Serial.println(F("* Disconnected or advertising timed out"));
  }  
}

void createBleTrackPacket(uint8_t * packet){
  uint8_t dummyPacket[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 0x00, 0x00, 0x00, 0x00, 0x55, 0x2E, 0x1A, 0xF7, 0x00};
  copyArray(dummyPacket, packet, 16);
  
  if(tracks != NULL) {
    copyArray(tracks->serialNumber, packet, 7); 
  }
}

void addTrackToTracks(struct track * trackToAdd) {
  struct track * currentTrack;

  if (tracks == NULL) {
    tracks = trackToAdd;
  } else {
    currentTrack = tracks;
    while (currentTrack->next != NULL) {
      currentTrack = currentTrack->next;
    }

    currentTrack->next = trackToAdd;
  }
}

void printTracks() {
  struct track * currentTrack;

  Serial.println();
  Serial.println(F("stored tracks:"));

  if (tracks == NULL) {
    Serial.println(F("no tracks available"));
    return;
  }

  currentTrack = tracks;
  printTrack(currentTrack);

  while (currentTrack->next != NULL) {
    currentTrack = currentTrack->next;
    printTrack(currentTrack);
  }
}

void printTrack(struct track * trackToPrint) {
  Serial.print(F("SerialNumber: "));
  nfc.PrintHexChar(trackToPrint->serialNumber, 7);

  Serial.print(F("RouteName: "));
  nfc.PrintHexChar(trackToPrint->routeName, 44);

  Serial.print(F("Difficulty: "));
  nfc.PrintHexChar(trackToPrint->difficulty, 4);

  Serial.println(F("---------------------------"));
}

struct track * createTrack(uint8_t * serialNumber, uint8_t * routeName, uint8_t * difficulty) {
  struct track * ptr;

  ptr = (struct track *) malloc(sizeof(struct track));
  if (ptr == NULL) {
    Serial.println(F("could not allocate memory"));
    return NULL;
  }

  copyArray(serialNumber, ptr->serialNumber, 7);
  copyArray(routeName, ptr->routeName, 44);
  copyArray(difficulty, ptr->difficulty, 4);
  ptr->next = NULL;
  return ptr;
}

uint8_t getNumberOfTracks() {
  uint8_t numberOfTracks = 0;
  struct track * currentTrack;
  
  currentTrack = tracks;
  while (currentTrack != NULL) {
    numberOfTracks++;
    currentTrack = currentTrack->next;
  }
  
  return numberOfTracks;
}

void clearUid(uint8_t * uid, int uidLength) {
  uint8_t emptyUid[] = { 0, 0, 0, 0, 0, 0, 0 };
  copyArray(emptyUid, uid, uidLength);
}

void copyArray(uint8_t * src, uint8_t * dest, int arrayLength) {
  int i;

  for (i = 0; i < arrayLength; i++) {
    dest[i] = src[i];
  }
}

uint8_t areEqualArrays(uint8_t * array1, uint8_t * array2, int arrayLength) {
  int i;

  for (i = 0; i < arrayLength; i++) {
    if (array1[i] != array2[i]) {
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

    Serial.print(F("Reading Page ")); Serial.print(page); Serial.print(F(": "));
    nfc.PrintHexChar(bufferArray, dataLength);
    delay(100);
  } else {
    Serial.print(F("Could not read page ")); Serial.println(page);
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

  for (pageNumber = 5; pageNumber < 16; pageNumber++) {
    indexNumber = (pageNumber - 5) * pageSize;
    success = success && readMifareUltralightPage(pageNumber, &routeName[indexNumber]);
  }

  return success;
}
