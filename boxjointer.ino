#include <Adafruit_GFX.h>    // Core graphics library
//#include <Adafruit_TFTLCD.h> // Hardware-specific library
//Adafruit_TFTLCD tft(A3, A2, A1, A0, A4);
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;       // hard-wired for UNO shields anyway.
#include <TouchScreen.h>
#include <math.h>
#include <EEPROM.h>

#include <AccelStepper.h>


/* Params for Screen */
// most mcufriend shields use these pins and Portrait mode:
uint8_t YP = A1;  // must be an analog pin, use "An" notation!
uint8_t XM = A2;  // must be an analog pin, use "An" notation!
uint8_t YM = 7;   // can be a digital pin
uint8_t XP = 6;   // can be a digital pin
uint8_t SwapXY = 0;

const short TS_MINX = 150;
const short TS_MINY = 120;
const short TS_MAXX = 920;
const short TS_MAXY = 940;

const uint16_t MINPRESSURE = 20;
const uint16_t MAXPRESSURE = 1000;

#define SWAP(a, b) {uint16_t tmp = a; a = b; b = tmp;}

const uint8_t Orientation = 1;    //PORTRAIT

// Assign human-readable names to some common 16-bit color values:
const uint16_t BLACK = 0x0000;
const uint16_t BLUE = 0x001F;
const uint16_t RED = 0xF800;
const uint16_t GREEN = 0x07E0;
const uint16_t CYAN = 0x07FF;
const uint16_t MAGENTA = 0xF81F;
const uint16_t YELLOW = 0xFFE0;
const uint16_t WHITE = 0xFFFF;

/* Init. Params for UI */
const long maxBladeWidth = 500;
const long maxGlueWidth = 200;
const long maxWoodWidth = 30000;
const long maxCutWidth = 3000;
const long maxOffsetZeroPosition = 5000;

const int bladeWidthDigits = 2;
const int cutWidthDigits = 2;
const int glueWidthDigits = 2;
const int woodWidthDigits = 1;
const int offsetZeroPositionDigits = 1;

const int multiCutOverlapping = 10;

const uint8_t dirPin = 12;                  // output pin for stepper motor direction
const uint8_t stepPin = 13;
const uint8_t startLimitPin = 10;
const uint8_t endLimitPin = 11;

const float maxSpeed = 300.0;

const bool dirInverted = true;
const bool stepInverted = false;
const bool enableInverted = false;

const uint8_t stepsPerMm = 40;
const uint8_t microSteppingFactor = 4;

AccelStepper stepper(AccelStepper::DRIVER, stepPin, dirPin);


/* Variables */

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
TSPoint tp;

int16_t bladeWidth = 280;
int16_t cutWidth = 500;
int16_t glueWidth = 10;
int16_t woodWidth = 10000;
int16_t offsetZeroPosition = 500;

void setup() {
  bladeWidth = readEepromWithDefault(0, bladeWidth);
  cutWidth = readEepromWithDefault(4, cutWidth);
  glueWidth = readEepromWithDefault(8, glueWidth);
  woodWidth = readEepromWithDefault(12, woodWidth);
  offsetZeroPosition = readEepromWithDefault(16, offsetZeroPosition);

  Serial.begin(9600);
  ts = TouchScreen(XP, YP, XM, YM, 300);
  tft.begin(0x0);
  tft.setRotation(Orientation);
  tft.setTextColor(BLACK);

  stepper.setMaxSpeed(maxSpeed * microSteppingFactor);
  stepper.setAcceleration(1200.0f);
  stepper.setPinsInverted(dirInverted, stepInverted, enableInverted);

  pinMode(startLimitPin, INPUT);
  pinMode(endLimitPin, INPUT);

  showWelcome();
  calibrateStepper();
}

long readEepromWithDefault(int address, long defaultValue) {
  int value = EEPROMReadlong(address);

  return value != -1 ? value : defaultValue;
}

void clearScreen() {
  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);
}

void showWelcome() {
  clearScreen();

  printText(F("BoxJointer"), 5, 5, 5);

  printText(F("Release: v1.0.RC1"), 5, 80, 2);

  printText(F("Kalibriere Schlitten"), 5, 200, 1);
}

void startJoint() {
  clearScreen();
  
  int cutGlueWidth = glueWidth / 2.0f;
  int toothGlueWidth = glueWidth - cutGlueWidth;
  long toothWidth = cutWidth - glueWidth;

  int cutWidthWithGlue = cutWidth + glueWidth;

  Serial.print(F("cutWidthWithGlue="));
  Serial.print(cutWidthWithGlue);
  Serial.println();
      
  long nextPosition = 0;
  long remainingCutWidth = cutWidthWithGlue - bladeWidth;
  float maxFollowupCutWidth = (float) (bladeWidth - multiCutOverlapping);
  
  int additionalCuts = ceil(remainingCutWidth / maxFollowupCutWidth);
  float additionalCutWidth = remainingCutWidth / (float) additionalCuts;
  int totalCuts = ceil(woodWidth / (cutWidth * 2.0f + glueWidth));
  
  Serial.print(F("remainingCutWidth="));
  Serial.print(remainingCutWidth);
  Serial.print(F(" maxFollowupCutWidth="));
  Serial.print(maxFollowupCutWidth);
  Serial.print(F(" additionalCuts="));
  Serial.print(additionalCuts);
  Serial.print(F(" totalCuts="));
  Serial.print(totalCuts);
  Serial.println();
  
  int buttonsSize = 2;
  Adafruit_GFX_Button buttons[buttonsSize];
  const int buttonsY = 210;
  printText(F("Beginnen mit ..."), 5, 100, 2);
  buttons[0].initButton(&tft, 80, buttonsY, 150, 50, BLACK, RED, WHITE, "Schnitt", 2);
  buttons[1].initButton(&tft, 240, buttonsY, 150, 50, BLACK, RED, WHITE, "Zinken", 2);

  drawButtons(buttons, 2);

  int buttonPressed = waitForButtonPress(buttons, buttonsSize);

  if (buttonPressed == 1) {
    Serial.println(F("Starte mit Zinken"));
    nextPosition += toothWidth + toothGlueWidth;
  } else {
    nextPosition -= toothGlueWidth;
  }

  int step = 0;
  for (int i = 1; i <= totalCuts; ++i) {
    // do cut
    for (int j = 0; j <= additionalCuts; ++j) {
      long nextPartialCut = j * additionalCutWidth;
      Serial.print(F("nextPartialCut="));
      Serial.println(nextPartialCut);
      
      gotoNextCutPositionWithDialog(nextPosition + nextPartialCut);
    }
    
    nextPosition = nextPosition + cutWidth + cutWidth;

    Serial.print(F("Finished Step "));
    Serial.println(i);
  }

  tft.fillScreen(WHITE);
  tft.setTextColor(BLACK);
  renderCentered("FERTIG!", 100, 3);
  renderCentered("Tippen: Auf Start", 200, 2);
  waitForTouch();

  showInProgress();
  runToMmPositionWithinLimits(offsetZeroPosition);
}

void gotoNextCutPositionWithDialog(long positionMm) {
  showInProgress();
  runToMmPositionWithinLimits(positionMm + offsetZeroPosition);
  showDoCut();
  String text = formatNumber(positionMm, 2);
  text = String("Pos: " + text + " mm");
  
  renderCentered(text, 100, 3);
  
  waitForTouch();
}

void showInProgress() {
  tft.fillScreen(RED);
  renderCentered("Bitte warten...", 100, 3);
}

void showDoCut() {
  tft.fillScreen(GREEN);
}

void runToMmPositionWithinLimits(long mmPosition) {
  long mappedPosition = mapPosition(mmPosition);

  Serial.print("mappedPosition="); Serial.println(mappedPosition);
  
  runToNewPositionWithinLimits(mappedPosition);
}

void runToNewPositionWithinLimits(long newPosition) {
  int count = 0;

  stepper.moveTo(newPosition);

  while (1) {
    count++;

    stepper.run();
    
    long distToGo = stepper.distanceToGo();
    
    if (distToGo == 0) {
      break;
    }

    stepper.run();
    
    if (count % 100 == 0) {
      count = 0;
      
      if (distToGo > 0) {
        bool endLimitReached = digitalRead(endLimitPin);

        if (endLimitReached) {
          break;
        }
      }

      stepper.run();

      if (distToGo < 0) {
        bool startLimitReached = digitalRead(startLimitPin);

        if (startLimitReached) {
          break;
        }
      }
    }

    stepper.run();
  }
}

void setupText(int x, int y, int size) {
  tft.setCursor(x, y);
  tft.setTextSize(size);
}

void printText(int intToPrint, int x, int y, int size) {
  setupText(x, y, size);
  tft.print(intToPrint);
}

void printText(const __FlashStringHelper * text, int x, int y, int size) {
  setupText(x, y, size);
  tft.print(text);
}

void printText(String text, int x, int y, int size) {
  setupText(x, y, size);
  tft.print(text);
}

int showOverview() {
  clearScreen();

  printText(F("BoxJointer"), 5, 5, 3);

  int secondColX = 220;
  int curRow = 50;
  int rowHeight = 20;

  printText(F("Blattbreite"), 5, curRow, 2);
  printText(formatNumber(bladeWidth, bladeWidthDigits), secondColX, curRow, 2);
  tft.print("mm");

  curRow += rowHeight;
  printText(F("Zinkenbreite"), 5, curRow, 2);
  printText(formatNumber(cutWidth, cutWidthDigits), secondColX, curRow, 2);
  tft.print("mm");

  curRow += rowHeight;
  printText(F("Leimzugabe"), 5, curRow, 2);
  printText(formatNumber(glueWidth, glueWidthDigits), secondColX, curRow, 2);
  tft.print("mm");

  curRow += rowHeight;
  printText(F("Holzbreite"), 5, curRow, 2);
  printText(formatNumber(woodWidth, woodWidthDigits), secondColX, curRow, 2);
  tft.print("mm");

  curRow += rowHeight;
  printText(F("Nullposition"), 5, curRow, 2);
  printText(formatNumber(offsetZeroPosition, offsetZeroPositionDigits), secondColX, curRow, 2);
  tft.print("mm");

  int buttonsSize = 2;
  Adafruit_GFX_Button buttons[buttonsSize];

  const int buttonsY = 210;

  buttons[0].initButton(&tft, 80, buttonsY, 150, 50, BLACK, RED, WHITE, "Anpassen", 2);
  buttons[1].initButton(&tft, 240, buttonsY, 150, 50, BLACK, RED, WHITE, "Start", 2);

  drawButtons(buttons, buttonsSize);

  return waitForButtonPress(buttons, buttonsSize);
}

void loop() {
  int result = showOverview();

  if (result == 0) {
    showSetup();
  } else if (result == 1) {
    startJoint();
  }
}

void showSetup() {
  int bladeWidthResult = readNumberInput(F("Blattbreite"), bladeWidth, 0, maxBladeWidth, bladeWidthDigits, 1, 10);
  if (bladeWidthResult < 0) {
    return;
  }

  int cutWidthResult = readNumberInput(F("Zinkenbreite"), cutWidth, 0, maxCutWidth, cutWidthDigits, 5, 50);
  if (cutWidthResult < 0) {
    return;
  }

  int glueWidthResult = readNumberInput(F("Leimzugabe"), glueWidth, 0, maxGlueWidth, 2, 1, 10);
  if (glueWidthResult < 0) {
    return;
  }

  int woodWidthResult = readNumberInput(F("Holzbreite"), woodWidth, 0, maxWoodWidth, 1, 50, 1000);
  if (woodWidthResult < 0) {
    Serial.println(F("Aborting setup at woodwidth"));
    return;
  }

  int offsetZeroPositionResult = readNumberInput(F("Nullposition"), offsetZeroPosition, 0, maxOffsetZeroPosition, offsetZeroPositionDigits, 10, 100);
  if (offsetZeroPositionResult < 0) {
    return;
  }

  clearScreen();

  int changeZeroPos = offsetZeroPositionResult - offsetZeroPosition;
  if (changeZeroPos != 0) {
    long mappedValue = mapPosition(offsetZeroPositionResult);

    Serial.print(F("offsetZeroPositionResult="));
    Serial.print(offsetZeroPosition);
    Serial.print(" mappedValue=");
    Serial.println(mappedValue);

    runToNewPositionWithinLimits(mappedValue);
  }

  bladeWidth = bladeWidthResult;
  cutWidth = cutWidthResult;
  glueWidth = glueWidthResult;
  woodWidth = woodWidthResult;
  offsetZeroPosition = offsetZeroPositionResult;

  EEPROMWritelong(0, bladeWidth);
  EEPROMWritelong(4, cutWidth);
  EEPROMWritelong(8, glueWidth);
  EEPROMWritelong(12, woodWidth);
  EEPROMWritelong(16, offsetZeroPosition);
}

long readNumberInput(const __FlashStringHelper * header, long value, long minValue, long maxValue, int fractionDigits, int slowIncrement, int fastIncrement) {
  clearScreen();

  printText(header, 5, 5, 3);

  renderInputNumber(value, 90, 3, fractionDigits);

  Adafruit_GFX_Button buttons[4];
  buttons[0].initButton(&tft, 40, 100, 70, 70, BLACK, BLUE, WHITE, "-", 4);
  buttons[1].initButton(&tft, 280, 100, 70, 70, BLACK, BLUE, WHITE, "+", 4);
  buttons[2].initButton(&tft, 80, 210, 150, 50, BLACK, RED, WHITE, "Abbrechen", 2);
  buttons[3].initButton(&tft, 240, 210, 150, 50, BLACK, RED, WHITE, "OK", 2);

  drawButtons(buttons, 4);

  tft.setTextColor(BLACK, WHITE);

  renderCentered(F("mm"), 120, 2);

  const int delayTime = 100;
  long lastTouch = millis();
  int continuousTouchCount = 0;

  while (1) {
    int button = waitForButtonPress(buttons, 4);

    long thisTouch = millis();
    long diffTouch = thisTouch - lastTouch;

    if (diffTouch < 50) {
      continuousTouchCount++;
    } else {
      continuousTouchCount = 0;
    }

    int currentIncrement = (continuousTouchCount > 5 && value % fastIncrement == 0) ? fastIncrement : slowIncrement;

    if (button == 0) {
      value = max(minValue, value - currentIncrement);
      renderInputNumber(value, 90, 3, fractionDigits);
      delay(delayTime);
    } else if (button == 1) {
      value = min(maxValue, value + currentIncrement);
      renderInputNumber(value, 90, 3, fractionDigits);
      delay(delayTime);
    } else if (button == 2) {
      return -1;
    } else {
      return value;
    }

    lastTouch = millis();
  }

}

void renderInputNumber(int number, int y, int fontSize, int fractionDigits) {
  tft.fillRect(80, 65, 160, 50, WHITE);

  renderCentered(formatNumber(number, fractionDigits), y, fontSize);
}

String formatNumber(int number, int fractionDigits) {
  float numberToDisplay = (float) number / 100.0f;
  return String(numberToDisplay, fractionDigits);
}

void prepareCursorForCenteredText(int numChars, int y, int fontSize) {
  int width = 5 * numChars * fontSize;
  int startX = (tft.width() - width) / 2;

  setupText(startX, y, fontSize);
}

void renderCentered(String text, int y, int fontSize) {
  int size = text.length();
  prepareCursorForCenteredText(size, y, fontSize);

  tft.print(text);
}

void drawButtons(Adafruit_GFX_Button * buttons, int number) {
  for (int i = 0; i < number; ++i) {
    buttons[i].drawButton();
  }
}

int waitForButtonPress(Adafruit_GFX_Button * buttons, int numberOfButtons) {
  while (1) {
    TSPoint p = waitForTouch();

    for (int i = 0; i < numberOfButtons; ++i) {
      uint16_t mappedX = mapXValue(p);
      uint16_t mappedY = mapYValue(p);

      if (buttons[i].contains(mappedX, mappedY)) {
        return i;
      }
    }
  }
}

uint16_t mapXValue(TSPoint & p) {
  return map(p.x, TS_MAXX, TS_MINX, 0, tft.width());
}

uint16_t mapYValue(TSPoint &p) {
  return map(p.y, TS_MINY, TS_MAXY, 0, tft.height());
}

TSPoint waitForTouch() {
  // a point object holds x y and z coordinates

  while (1) {
    TSPoint p = ts.getPoint();

    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);
    pinMode(XP, OUTPUT);
    pinMode(YM, OUTPUT);

    if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
      Serial.print(F("X = ")); Serial.print(p.x);
      Serial.print(F("\tY = ")); Serial.print(p.y);
      Serial.print(F("\tPressure = ")); Serial.println(p.z);
      return p;
    }
  }
}

void calibrateStepper() {
  Serial.println(F("Start calibration"));

  tft.print(".");

  if (digitalRead(startLimitPin)) {
    Serial.println(F("Stepper hit startLimit"));
  } else {
    Serial.println(F("Stepper not reaching start limit"));
    runUntilLimitReachesState(startLimitPin, true, -12000, maxSpeed * microSteppingFactor);
  }

  tft.print(".");

  Serial.println(F("Stepper hitting startLimit. Starting fine calibration."));
  runUntilLimitReachesState(startLimitPin, false, 500, 25 * microSteppingFactor);
  
  stepper.setCurrentPosition(0);
  
  tft.print(".");
  stepper.setMaxSpeed(maxSpeed * microSteppingFactor);
  
  runToMmPositionWithinLimits(offsetZeroPosition);
  
  tft.print(".");  
}

long mapPosition(long position) {
  long result = position * stepsPerMm  * microSteppingFactor / 100;
  return result;
}

void runUntilLimitReachesState(int pin, bool expectedState, int moveTo, int maxSpeed) {
  stepper.setMaxSpeed(maxSpeed);

  int count = 0;

  stepper.moveTo(moveTo);

  while (1) {
    if (stepper.distanceToGo() == 0) {
      break;
    }
    
    if (count++ % 1000 == 0) {
      count = 0;
      bool state = digitalRead(pin);

      if (expectedState == state) {
        break;
      }
    }

    stepper.run();
  }
}

void EEPROMWritelong(int address, long value) {
  //Decomposition from a long to 4 bytes by using bitshift.
  //One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  //Write the 4 bytes into the eeprom memory.
  EEPROM.update(address, four);
  EEPROM.update(address + 1, three);
  EEPROM.update(address + 2, two);
  EEPROM.update(address + 3, one);
}

long EEPROMReadlong(long address) {
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

