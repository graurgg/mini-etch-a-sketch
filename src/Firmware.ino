#include "MaxMatrix.h"
#include "Joystick.h"
#include <SoftwareSerial.h>
#include <Arduino.h>
#include "Ch376msc.h"


// LED Matrix Pins
#define DIN_PIN 6
#define CS_PIN  5
#define CLK_PIN 4

#define BUZZER_PIN 7

#define GALLERY_BUTTON_PIN 12
#define SAVE_LOAD_BUTTON_PIN 13
#define RANDOM_BUTTON_PIN 9


#define CH376S_RX 11  // Arduino RX (connect to CH376S TXD)
#define CH376S_TX 10  // Arduino TX (connect to CH376S RXD)

SoftwareSerial CH376S(CH376S_RX, CH376S_TX);

Ch376msc flashDrive(CH376S); // Create a CH376MSC object using SoftwareSerial

byte computerByte;           //used to store data coming from the computer
byte USB_Byte;               //used to store data coming from the USB stick
int timeOut = 2000;
 
bool ch376Detected = false;
bool usbMounted = false;

// Joystick Pins
#define JOY_X  A0
#define JOY_Y  A1
#define JOY_SW 3

MaxMatrix ledMatrix(DIN_PIN, CS_PIN, CLK_PIN, 1);
Joystick joystick(JOY_X, JOY_Y, JOY_SW);


// Current cursor position
int cursorX = 3;
int cursorY = 3; // Start at center

// Gallery array now has 5 patterns (original 2 + flower + smiley + 1 save slot)
// Each pattern is stored as 8 bytes (one per row), each bit represents a pixel
uint8_t gallery[5][8] = {
  { // Original pattern 1
    0b10000000,
    0b00000100,
    0b00100000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  },
  { // Original pattern 2
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  },
  { // Flower pattern
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  },
  { // Smiley face pattern
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  },
  { // Empty slot for saved drawing (index 4)
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
  }
};

bool galleryMode = false;
int currentGalleryIndex = 4;
unsigned long lastGalleryChangeTime = 0;
const int GALLERY_CHANGE_DELAY = 300; // ms between gallery changes

// Display state
bool pixels[8][8] = {false};
bool lastButtonState = false;
bool lastGalleryButtonState = false;
bool lastSaveLoadButtonState = false;
bool lastRandomButtonState = false;
unsigned long lastMoveTime = 0;
const int MOVE_DELAY = 150; // ms between moves

void setup() {
  ledMatrix.init(1);
  ledMatrix.setIntensity(8);
  ledMatrix.clear();
  updateDisplay();

  Serial.begin(9600);
  CH376S.begin(9600); // Initialize SoftwareSerial for CH376S
  flashDrive.init(); // Initialize the CH376S USB controller

  pinMode(GALLERY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SAVE_LOAD_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RANDOM_BUTTON_PIN, INPUT_PULLUP);

  loadGalleryFromUsb(); // Load gallery patterns from USB on startup
  // Serial.println(flashDrive.getChipVer(), HEX); // Print the CH376S chip version
}

// Modified saveGalleryToUSB function for SPI
void saveGalleryToUSB() {

  flashDrive.setFileName("test.txt"); // Set the file name for the first pattern
  flashDrive.openFile(); 
  flashDrive.writeChar('H'); // Write a test character to the file
  flashDrive.writeChar('e');
  flashDrive.writeChar('l');
  flashDrive.writeChar('l');
  flashDrive.writeChar('o');
  flashDrive.writeChar(' ');
  flashDrive.writeChar('U');
  flashDrive.writeChar('S');
  flashDrive.writeChar('B');
  flashDrive.writeChar('\n'); // Newline character
  flashDrive.closeFile(); // Close the file after writing

  Serial.println("Starting USB save process...");

  // for (int i = 0; i < 5; i++) {
  //   String fileName = "nPAT" + String(i);
  //   flashDrive.setFileName(fileName.c_str());
    
  //   // Check if file already exists
  //   if (flashDrive.openFile()) {
  //     flashDrive.closeFile(); // Close the file if it exists
  //     flashDrive.deleteFile(); // Delete the existing file
  //     Serial.print("Deleted existing file: ");
  //     Serial.println(fileName);
  //   }
  // }

  // Save each pattern
  for (int i = 0; i < 5; i++) {
    String fileName = "NPAT" + String(i) + ".TXT";
    uint8_t fileContent[8];
    for (int x = 0; x < 8; x++) {
      fileContent[x] = gallery[i][x]; // Copy the pattern row
    }
  flashDrive.setFileName(fileName.c_str());
  flashDrive.openFile();
  for (int x = 0; x < 8; x++) {
    // Write each row of the pattern to the file
    flashDrive.writeNum(fileContent[x]);
    flashDrive.writeChar('\n'); // Add newline after each row
    Serial.print("Row ");
    Serial.print(x);
    Serial.print(": ");
    for (int y = 0; y < 8; y++) {
      Serial.print((fileContent[x] >> (7 - y)) & 1); // Print each bit
    }
    Serial.println();
  }

  flashDrive.saveFileAttrb(); // Save file attributes
  flashDrive.closeFile();
  Serial.print("Saved pattern ");
  Serial.print(i);
  Serial.print(" to ");
  Serial.println(fileName);
  }
  while(flashDrive.listDir()) { // reading next file
    if (flashDrive.getFileAttrb() == CH376_ATTR_DIRECTORY) {//directory
      Serial.print('/');
      Serial.println(flashDrive.getFileName()); // get the actual file name
    } else {
      Serial.print(flashDrive.getFileName()); // get the actual file name
      Serial.print(" : ");
      Serial.print(flashDrive.getFileSize()); // get the actual file size in bytes
      Serial.print(" >>>\t");
      Serial.println(flashDrive.getFileSizeStr()); // get the actual file size in formatted string
    }
  }
  Serial.println("USB save process completed.");
  for (int i = 0; i < 5; i++) {
    String fileName = "NPAT" + String(i) + ".TXT";
    flashDrive.setFileName(fileName.c_str());
    flashDrive.openFile();                //open the file
    bool readMore = true;
            //read data from flash drive until we reach EOF
    uint8_t fileContent[8] = {0};
    for (int x = 0; x < 8; x++) {
      uint8_t buffer = 0; // Buffer to read one character
      flashDrive.readFileUntil('\n', (char*)&buffer, sizeof(buffer)); // Read until newline
      fileContent[x] = buffer; // Store the read character
      Serial.print(fileContent[x]);
    }
    flashDrive.closeFile();
  }
}

void loadGalleryFromUsb() {
  for (int i = 0; i < 5; i++) {
    String fileName = "NPAT" + String(i) + ".TXT";
    flashDrive.setFileName(fileName.c_str());
    flashDrive.openFile();                //open the file
    bool readMore = true;
            //read data from flash drive until we reach EOF
    uint8_t fileContent[8] = {0};
    for (int x = 0; x < 8; x++) {
      uint8_t buffer = 0; // Buffer to read one character
      flashDrive.readFileUntil('\n', (char*)&buffer, sizeof(buffer)); // Read until newline
      fileContent[x] = buffer; // Store the read character
      Serial.print(fileContent[x]);
    }
    for (int x = 0; x < 8; x++) {
        gallery[i][x] = fileContent[x];
    }
    flashDrive.closeFile();              //close the file
    Serial.print("Loaded pattern ");
    Serial.print(i);
    Serial.print(" from ");
    Serial.println(fileName);
    for (int x = 0; x < 8; x++) {
      Serial.print("Row ");
      Serial.print(x);
      Serial.print(": ");
      for (int y = 0; y < 8; y++) {
        Serial.print((gallery[i][x] >> (7 - y)) & 1);
      }
      Serial.println();
    }
  }

}

void loop() { 
  if(flashDrive.checkIntMessage()){
		if(flashDrive.getDeviceStatus()){
			Serial.println(F("Flash drive attached!"));
		} else {
			Serial.println(F("Flash drive detached!"));
		}
	}
  
  // Check gallery button (D12)
  bool currentGalleryButtonState = digitalRead(GALLERY_BUTTON_PIN) == LOW;
  
  if (currentGalleryButtonState && !lastGalleryButtonState) {
    galleryMode = !galleryMode;
    
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    
    if (galleryMode) {
      loadGalleryPattern(currentGalleryIndex);
    }
    
    delay(100); // Debounce
  }
  lastGalleryButtonState = currentGalleryButtonState;

  // Check save/load button (D13)
  bool currentSaveLoadButtonState = digitalRead(SAVE_LOAD_BUTTON_PIN) == LOW;
  
  if (currentSaveLoadButtonState && !lastSaveLoadButtonState) {
    if (galleryMode) {
      // Load behavior: exit gallery mode and load selected pattern
      galleryMode = false;
      for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
          pixels[x][y] = (gallery[currentGalleryIndex][x] >> (7 - y)) & 1;
        }
      }
    } else {
      // Save behavior: save current drawing to gallery index 4
      uint8_t pattern[8] = {0};
      for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
          if (pixels[x][y]) {
            pattern[x] |= (1 << (7 - y));
          }
        }
      }
      memcpy(gallery[currentGalleryIndex], pattern, sizeof(pattern));
      
      // Provide feedback
      for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(50);
        digitalWrite(BUZZER_PIN, LOW);
        delay(50);
      }
    }
    
    updateDisplay();
    delay(100); // Debounce
  }
  lastSaveLoadButtonState = currentSaveLoadButtonState;

  bool currentRandomButtonState = digitalRead(RANDOM_BUTTON_PIN) == LOW;
  if (currentRandomButtonState && !lastRandomButtonState) {
    saveGalleryToUSB();
    delay(100); // Debounce
  }
  lastRandomButtonState = currentRandomButtonState;

  if (galleryMode) {
    // Gallery mode navigation
    if (millis() - lastGalleryChangeTime > GALLERY_CHANGE_DELAY) {
      int xVal = joystick.getX();
      
      if (xVal < 300) {
        currentGalleryIndex--;
        if (currentGalleryIndex < 0) currentGalleryIndex = 4;
        loadGalleryPattern(currentGalleryIndex);
        lastGalleryChangeTime = millis();
      }
      else if (xVal > 700) {
        currentGalleryIndex++;
        if (currentGalleryIndex > 4) currentGalleryIndex = 0;
        loadGalleryPattern(currentGalleryIndex);
        lastGalleryChangeTime = millis();
      }
    }
  } else {
    // Drawing mode
    bool currentButtonState = joystick.getSW();
    if (currentButtonState && !lastButtonState) {
      pixels[cursorX][cursorY] = !pixels[cursorX][cursorY];
      updateDisplay();
      
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);
      
      delay(100);
    }
    lastButtonState = currentButtonState;

    if (millis() - lastMoveTime > MOVE_DELAY) {
      int xVal = joystick.getX();
      int yVal = joystick.getY();
      
      if (xVal < 300) cursorX--;
      else if (xVal > 700) cursorX++;
      
      if (yVal < 300) cursorY++;
      else if (yVal > 700) cursorY--;
      
      // Wrap around
      cursorX = constrain(cursorX, 0, 7);
      cursorY = constrain(cursorY, 0, 7);
      
      updateDisplay();
      lastMoveTime = millis();
    }
  }
}

void loadGalleryPattern(int index) {
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      pixels[x][y] = (gallery[index][x] >> (7 - y)) & 1;
    }
  }
  updateDisplay();
}

void updateDisplay() {
  ledMatrix.clear();
  
  for (int x = 0; x < 8; x++) {
    for (int y = 0; y < 8; y++) {
      if (pixels[x][y]) {
        ledMatrix.setDot(x, y, 1);
      }
    }
  }
  
  if (!galleryMode) {
    ledMatrix.setDot(cursorX, cursorY, 1);
  }
}
