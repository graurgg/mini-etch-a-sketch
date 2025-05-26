#include "MaxMatrix.h"
#include "Joystick.h"

// LED Matrix Pins
#define DIN_PIN 6
#define CS_PIN  5
#define CLK_PIN 4

#define BUZZER_PIN 7

#define GALLERY_BUTTON_PIN 12
#define SAVE_LOAD_BUTTON_PIN 13
#define RANDOM_BUTTON_PIN 9

//tx 10
//rx 11


// Joystick Pins
#define JOY_X  A0
#define JOY_Y  A1
#define JOY_SW 3

MaxMatrix ledMatrix(DIN_PIN, CS_PIN, CLK_PIN, 1);
Joystick joystick(JOY_X, JOY_Y, JOY_SW);

// Current cursor position
int cursorX = 3;
int cursorY = 3; // Start at center

// Gallery array now has 6 patterns (original 2 + flower + smiley + 2 save slots)
bool gallery[6][8][8] = {{
  // Original pattern 1
  {0, 0, 1, 0, 0, 1, 0, 0},
  {0, 1, 0, 1, 1, 0, 1, 0},
  {1, 0, 1, 0, 0, 1, 0, 1},
  {0, 1, 0, 1, 1, 0, 1, 0},
  {0, 0, 1, 0, 0, 1, 0, 0},
  {0, 1, 0, 1, 1, 0, 1, 0},
  {1, 0, 1, 0, 0, 1, 0, 1},
  {0, 1, 0, 1, 1, 0, 1, 0}
}, {
  // Original pattern 2
  {1, 1, 0, 0, 0, 0, 1, 1},
  {1, 0, 1, 0, 0, 1, 0, 1},
  {0, 1, 0, 1, 1, 0, 1, 0},
  {0, 0, 1, 0, 0, 1, 0, 0},
  {1, 1, 0, 0, 0, 0, 1, 1},
  {1, 0, 1, 0, 0, 1, 0, 1},
  {0, 1, 0, 1, 1, 0, 1, 0},
  {0, 0, 1, 0, 0, 1, 0, 0}
}, {
  // Flower pattern
  {0, 0, 0, 1, 1, 0, 0, 0},
  {0, 1, 1, 0, 0, 1, 1, 0},
  {0, 1, 0, 1, 1, 0, 1, 0},
  {1, 0, 1, 0, 0, 1, 0, 1},
  {1, 0, 1, 0, 0, 1, 0, 1},
  {0, 1, 0, 1, 1, 0, 1, 0},
  {0, 1, 1, 0, 0, 1, 1, 0},
  {0, 0, 0, 1, 1, 0, 0, 0}
}, {
  // Smiley face pattern
  {0, 0, 1, 1, 1, 1, 0, 0},
  {0, 1, 0, 0, 0, 0, 1, 0},
  {1, 0, 1, 0, 0, 1, 0, 1},
  {1, 0, 0, 0, 0, 0, 0, 1},
  {1, 0, 1, 0, 0, 1, 0, 1},
  {1, 0, 0, 1, 1, 0, 0, 1},
  {0, 1, 0, 0, 0, 0, 1, 0},
  {0, 0, 1, 1, 1, 1, 0, 0}
}, {
  // Empty slot for first saved drawing (index 4)
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0}
}, {
  // Empty slot for second saved drawing (index 5)
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0}
}};

bool galleryMode = false;
int currentGalleryIndex = 4;
unsigned long lastGalleryChangeTime = 0;
const int GALLERY_CHANGE_DELAY = 300; // ms between gallery changes

// Display state
bool pixels[8][8] = {false};
bool lastButtonState = false;
bool lastGalleryButtonState = false;
bool lastSaveLoadButtonState = false;
unsigned long lastMoveTime = 0;
const int MOVE_DELAY = 150; // ms between moves

void setup() {
  Serial.begin(9600);
  ledMatrix.init(1);
  ledMatrix.setIntensity(8);
  ledMatrix.clear();
  updateDisplay();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  pinMode(GALLERY_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SAVE_LOAD_BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
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
          pixels[x][y] = gallery[currentGalleryIndex][x][y];
        }
      }
    } else {
      // Save behavior: save current drawing to gallery index 4 or 5
      int saveIndex = currentGalleryIndex;
      
      for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
          gallery[saveIndex][x][y] = pixels[x][y];
        }
      }
      
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

  if (galleryMode) {
    // Gallery mode navigation
    if (millis() - lastGalleryChangeTime > GALLERY_CHANGE_DELAY) {
      int xVal = joystick.getX();
      
      if (xVal < 300) {
        currentGalleryIndex--;
        if (currentGalleryIndex < 0) currentGalleryIndex = 5;
        loadGalleryPattern(currentGalleryIndex);
        lastGalleryChangeTime = millis();
      }
      else if (xVal > 700) {
        currentGalleryIndex++;
        if (currentGalleryIndex > 5) currentGalleryIndex = 0;
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
      pixels[x][y] = gallery[index][x][y];
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