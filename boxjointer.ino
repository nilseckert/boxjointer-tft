#include <Adafruit_GFX.h>    // Core graphics library
//#include <Adafruit_TFTLCD.h> // Hardware-specific library
//Adafruit_TFTLCD tft(A3, A2, A1, A0, A4);
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;       // hard-wired for UNO shields anyway.
#include <TouchScreen.h>

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

int woodWidth = 2000;
int bladeWidth = 280;
int cutWidth = 500;
int glueWidth = 10;

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

void setup() {
  TS_LEFT = 893; TS_RT = 145; TS_TOP = 930; TS_BOT = 135;
  SwapXY = 1;

  Serial.begin(9600);
  ts = TouchScreen(XP, YP, XM, YM, 300);
  tft.begin(0x0);
  tft.setRotation(Orientation);
  tft.setTextColor(BLACK);
  showWelcome();
  showOverview();
}

void clearScreen() {
  tft.fillScreen(WHITE);
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

void showOverview() {
  clearScreen();
 
  printText(F("Einstellungen"), 5, 5, 3);

  int secondColX = 200;
  int curRow = 50;
  int rowHeight = 20;
  
  printText(F("Blattbreite"), 5, curRow, 2);
  printText(bladeWidth, secondColX,curRow,2);
  curRow += rowHeight;
  
  printText(F("Schnittbreite"), 5, curRow, 2);
  printText(cutWidth, secondColX,curRow,2);
  curRow += rowHeight;

  printText(F("Leimzugabe"), 5, curRow, 2);
  printText(glueWidth, secondColX,curRow,2);
  curRow += rowHeight;
  
  printText(F("Holzbreite"), 5, curRow, 2);
  printText(woodWidth, secondColX,curRow,2);

  Adafruit_GFX_Button buttons[2];

  int buttonsY = 210; 

  buttons[0].initButton(&tft, 80, buttonsY, 150, 50, BLACK, RED, WHITE, "Anpassen", 2);
  buttons[1].initButton(&tft, 240, buttonsY, 150, 50, BLACK, RED, WHITE, "Start", 2);

  buttons[0].drawButton();
  buttons[1].drawButton();
}

void loop() {
  waitForTouch();
  tft.print(F("."));
  delay(200);
}

void waitForTouch() {
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
       break;
    }
  }
}

