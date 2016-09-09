#include <Adafruit_GFX.h>    // Core graphics library
//#include <Adafruit_TFTLCD.h> // Hardware-specific library
//Adafruit_TFTLCD tft(A3, A2, A1, A0, A4);
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;       // hard-wired for UNO shields anyway.
#include <TouchScreen.h>
#include <math.h>
#include <EEPROM.h>
#include <Stepper.h>

// most mcufriend shields use these pins and Portrait mode:
uint8_t YP = A1;  // must be an analog pin, use "An" notation!
uint8_t XM = A2;  // must be an analog pin, use "An" notation!
uint8_t YM = 7;   // can be a digital pin
uint8_t XP = 6;   // can be a digital pin
uint8_t SwapXY = 0;


uint16_t TS_LEFT = 920;
uint16_t TS_RT  = 150;
uint16_t TS_TOP = 940;
uint16_t TS_BOT = 120;

short TS_MINX=150;
short TS_MINY=120;
short TS_MAXX=920;
short TS_MAXY=940;

char *name = "Unknown controller";

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
TSPoint tp;

#define MINPRESSURE 20
#define MAXPRESSURE 1000

#define SWAP(a, b) {uint16_t tmp = a; a = b; b = tmp;}

int16_t BOXSIZE;
int16_t PENRADIUS = 3;
uint16_t identifier, oldcolor, currentcolor;
uint8_t Orientation = 1;    //PORTRAIT

const long maxBladeWidth = 500;
const long maxGlueWidth = 100;
const long maxWoodWidth = 3000;
const long maxCutWidth = 3000;

const int bladeWidthDigits = 2;
const int cutWidthDigits = 2;
const int glueWidthDigits = 2;
const int woodWidthDigits = 0;

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

int16_t bladeWidth = 280;
int16_t cutWidth = 500;
int16_t glueWidth = 10;
int16_t woodWidth = 1000;

void setup() {
  TS_LEFT = 893; TS_RT = 145; TS_TOP = 930; TS_BOT = 135;
  SwapXY = 1;

  bladeWidth = readEepromWithDefault(0, bladeWidth);
  cutWidth = readEepromWithDefault(4, cutWidth);
  glueWidth = readEepromWithDefault(8, glueWidth);
  woodWidth = readEepromWithDefault(12, woodWidth);

  Serial.begin(9600);
  ts = TouchScreen(XP, YP, XM, YM, 300);
  tft.begin(0x0);
  tft.setRotation(Orientation);
  tft.setTextColor(BLACK);
  showWelcome();
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

    printText(F("Release: Beta 1"), 5, 80, 2);

    printText(F("Starting"), 5,200, 1);
    
    for (int i = 0; i < 3; ++i) {
      tft.print(F("."));
      delay(200);
    }
}

void setupText(int x, int y, int size) {
  tft.setCursor(x,y);
  tft.setTextSize(size);
}

void printText(int intToPrint, int x, int y, int size) {
  setupText(x,y,size);
  tft.print(intToPrint);
}

void printText(const __FlashStringHelper * text, int x, int y, int size) {
  setupText(x,y,size);
  tft.print(text);
}

int showOverview() {
  clearScreen();
  
  printText(F("BoxJointer"), 5, 5, 3);

  int secondColX = 200;
  int curRow = 50;
  int rowHeight = 20;
  
  printText(F("Blattbreite"), 5, curRow, 2);
  printText(bladeWidth, secondColX,curRow,2);
  curRow += rowHeight;
  
  printText(F("Zinkenbreite"), 5, curRow, 2);
  printText(cutWidth, secondColX,curRow,2);
  curRow += rowHeight;

  printText(F("Leimzugabe"), 5, curRow, 2);
  printText(glueWidth, secondColX,curRow,2);
  curRow += rowHeight;
  
  printText(F("Holzbreite"), 5, curRow, 2);
  printText(woodWidth, secondColX,curRow,2);

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
  
  int cutWidthResult = readNumberInput(F("Zinkenbreite"), cutWidth, 0, maxCutWidth, cutWidthDigits, 5, 10);
  if (cutWidthResult < 0) {
    return;
  }

  int glueWidthResult = readNumberInput(F("Leimzugabe"), glueWidth, 0, maxGlueWidth, 2, 1, 10);
  if (glueWidthResult < 0) {
    return;
  }

  int woodWidthResult = readNumberInput(F("Holzbreite"), woodWidth, 0, maxWoodWidth, 1, 5, 100);
  if (woodWidthResult < 0) {
    return; 
  }

  bladeWidth = bladeWidthResult;
  cutWidth = cutWidthResult;
  glueWidth = glueWidthResult;
  woodWidth = woodWidthResult;

  EEPROMWritelong(0, bladeWidth);
  EEPROMWritelong(4, cutWidth);
  EEPROMWritelong(8, glueWidth);
  EEPROMWritelong(12, woodWidth);
  
  waitForTouch();
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

  const int delayTime = 100;
  long lastTouch = millis();
  int continuousTouchCount = 0;
  
  while (1) {
    int button = waitForButtonPress(buttons, 4);

    long thisTouch = millis();
    long diffTouch = thisTouch - lastTouch;

    Serial.print("TimeSinceLastTouch=");
    Serial.println(diffTouch);
    if (diffTouch < 50) {
      Serial.println("Continuous Touch detected");
      continuousTouchCount++;
    } else {
      Serial.println("Reset Cont. Touch Count");
      continuousTouchCount = 0;
    }

    int currentIncrement = (continuousTouchCount > 5 && value % fastIncrement == 0) ? fastIncrement : slowIncrement;
   
    if (button == 0) {
      value = max(minValue, value - currentIncrement);
      renderInputNumber(value, 90, 3, fractionDigits);
      Serial.print("decrement value="); Serial.println(value);
      delay(delayTime);
    } else if (button == 1) {
      value = min(maxValue, value + currentIncrement);
      renderInputNumber(value, 90, 3, fractionDigits);
      Serial.print("decrement value="); Serial.println(value);
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
  tft.fillRect(80, 65, 160, 70, WHITE);
  
  float numberToDisplay = (float) number;
  for (int i = 0; i < fractionDigits; ++i) {
      numberToDisplay = numberToDisplay / 10;
  }
  
  String formated = String(numberToDisplay, fractionDigits);
  renderCentered(formated, y, fontSize);
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

void startJoint() {
  
}

void drawButtons(Adafruit_GFX_Button * buttons, int number) {
  for (int i = 0; i < number; ++i) {
    buttons[i].drawButton();
    }
}

int waitForButtonPress(Adafruit_GFX_Button * buttons, int numberOfButtons) {
  while(1) {
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
  
  while(1) {
    TSPoint p = ts.getPoint();

    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);
    pinMode(XP, OUTPUT);
    pinMode(YM, OUTPUT);
    
    if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
       Serial.print("X = "); Serial.print(p.x);
       Serial.print("\tY = "); Serial.print(p.y);
       Serial.print("\tPressure = "); Serial.println(p.z);
       return p;
    }
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

