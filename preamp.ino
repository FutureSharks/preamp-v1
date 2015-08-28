///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// To do:
// Add NeoPixel code, fix crossover point (should be very red at -30 or 1847)
// Change mute to slowly pulse green
// First remote button doesn't do anything when volume is fading, it just stops the fade
// Pulse red at higher volumes
// Don't use negative dB as a counter, just use 0-96.5. Double negatives etc are too confusing.
// Possible to automatically enable debug when USB is connected?
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// If enabled volume/input data will be printed via serial
bool debugEnabled = false;

// EEPROM related stuff to save volume level
#include "EEPROM.h"
int dBLevelEEPROMAddressBit = 0;
bool isVolumeSavedToEeprom = true;
unsigned long timeOfLastVolumeChange;
unsigned long timeBetweenVolumeSaves = 60000;
float maximumLevelToSave = -30.0;
float currentSavedDbLevel;

// SPI library
#include "SPI.h"
// Arduino pin 9 & 10 = inputSelectorCSPin & MDACCSPin
// Arduino pin 11 = SDI
// Arduino pin 13 = CLK

// IR stuff
#include "IRremote.h"
int RECV_PIN = A2;  // IR Receiver pin
const int IRGroundPin = A0;
const int IRPowerPin = A1;
IRrecv irrecv(RECV_PIN);
decode_results results;
String lastIRoperation;
float iRIncrement = 2;
unsigned long timeOfLastIRChange;

// NeoPixel stuff
#include "Adafruit_NeoPixel.h"
const int NeoPixelPin = 3;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, NeoPixelPin, NEO_GRB + NEO_KHZ800);
bool neopixelIsRed;
bool neopixelIsBlue;
unsigned long timeOfLastNeopixelColourChange;

// Input selector stuff
int selectedInput = 0;
const int inputSelectorCSPin = 9;
long muteDelay = 1000;
bool muteEnabled = true;

// MDAC attenuator stuff
const int MDACCSPin = 10;
float currentDbLevel;
float max_dbLevel = -0.0001;
float min_dbLevel = -96.5;
float currentChangeVolumeIncrement;
unsigned int currentDacR2Rvalue;

// Volume fading stuff
bool volumeFadeInProgress = true;
float targetVolumelevel;
float fadeInProgressLevel = -96.0;

// Encoder stuff
const int encoder0GroundPin = 4;
const int encoder0PowerPin = 5;
int encoder0PinA = 6;
int encoder0PinB = 7;
int encoder0Pos = 0;
int encoder0PinALast = HIGH;
int n = LOW;
float encoderIncrement = 0.5;

//////////////////////////////////////////////////////////////////////////////////////////////
// Setup
//////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  // Serial
  if (debugEnabled) {
    Serial.begin (9600);
  }
  // SPI
  // set the CS pins as output:
  pinMode (inputSelectorCSPin, OUTPUT);
  digitalWrite(inputSelectorCSPin,HIGH);
  pinMode (MDACCSPin, OUTPUT);
  digitalWrite(MDACCSPin,HIGH);
  // Start SPI
  if (debugEnabled) {
    Serial.println ("Starting SPI..");
  }
  SPI.begin();
  // Set SPI selector IO direction to output for all pinds
  if (debugEnabled) {
    Serial.println ("Setting SPI selector IO direction control registers..");
  }
  digitalWrite(inputSelectorCSPin,LOW);
  SPI.transfer(B01000000); // Send Device Opcode
  SPI.transfer(0); // Select IODIR register
  SPI.transfer(0); // Set register
  digitalWrite(inputSelectorCSPin,HIGH);
  // Set up pins for encoder:
  pinMode (encoder0PinA,INPUT);
  pinMode (encoder0PinB,INPUT);
  pinMode (encoder0GroundPin,OUTPUT);
  pinMode (encoder0PowerPin,OUTPUT);
  digitalWrite(encoder0GroundPin,LOW);
  digitalWrite(encoder0PowerPin,HIGH);
  // IR
  irrecv.enableIRIn(); // Start the receiver
  pinMode(IRPowerPin, OUTPUT);
  pinMode(IRGroundPin, OUTPUT);
  digitalWrite(IRPowerPin, HIGH); // Power for the IR
  digitalWrite(IRGroundPin, LOW); // GND for the IR
  // NeoPixel
  strip.begin();
  // Set initial MADC volume level from EEPROM and prepare to fade to this level
  targetVolumelevel = read_DbLevel();
  if (debugEnabled) {
    Serial.print ("Setting volume to minimum. Will fade to: ");
    Serial.println (targetVolumelevel);
  }
  SetDac88812Volume(min_dbLevel);
  // Delay before unmuting output
  delay(muteDelay);
  changeMute();
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to save the DB level to EERPOM
int save_DbLevel (float DbLevel) {
  byte byteToWrite = (int) DbLevel * -1;
  if (debugEnabled) {
    Serial.print ("Writing byte to EEPROM: ");
    Serial.println (byteToWrite);
  }
  isVolumeSavedToEeprom = true;
  currentSavedDbLevel = DbLevel;
  EEPROM.write(dBLevelEEPROMAddressBit, byteToWrite);
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to read the DB level to EERPOM
int read_DbLevel (){
  byte byteFromEeprom = EEPROM.read(dBLevelEEPROMAddressBit);
  currentSavedDbLevel = (float) (-1 * byteFromEeprom);
  return currentSavedDbLevel;
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to set NeoPixel colour to match current DAC level
int setNeoPixelColourFromDac (float DACLevel) {
  byte redPixelColor = (log(DACLevel) / log(2)) * 16;
  byte bluePixelColor = 255 - redPixelColor;
  setNeoPixelColour(redPixelColor, 0, bluePixelColor);
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to multiple NeoPixels to a colour simultaneously
int setNeoPixelColour (byte Red, byte Green, byte Blue) {
  strip.setPixelColor(0, Red, Green, Blue);
  strip.setPixelColor(1, Red, Green, Blue);
  strip.setPixelColor(2, Red, Green, Blue);
  strip.setPixelColor(3, Red, Green, Blue);
  strip.setPixelColor(4, Red, Green, Blue);
  strip.setPixelColor(5, Red, Green, Blue);
  strip.show();
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to send data to the MCP23S08
// select_register: 9 = GPIO
int setMCP23S08 (int select_register, int register_value) {
  digitalWrite(inputSelectorCSPin,LOW);
  SPI.transfer(B01000000);       // Send Device Opcode
  SPI.transfer(select_register); // Select register
  SPI.transfer(register_value);  // Set register
  digitalWrite(inputSelectorCSPin,HIGH);
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to change to next input
int changeInput(String direction) {
  if (direction == "up") { selectedInput++; }
  if (direction == "down") { selectedInput--; }
  if (selectedInput > 3) { selectedInput = 1; }
  if (selectedInput < 1) { selectedInput = 3; }
  switch (selectedInput) {
    case 1:
      setMCP23S08(9, B01001011);
      delay(5);
      break;
    case 2:
      setMCP23S08(9, B00110011);
      delay(5);
      break;
    case 3:
      setMCP23S08(9, B00101101);
      delay(5);
      break;
  }
  setMCP23S08(9, B00000001);
  muteEnabled = false;
  setNeoPixelColour(255, 0, 0);
  delay(40);
  setNeoPixelColour(0, 255, 0);
  delay(40);
  setNeoPixelColour(0, 0, 255);
  delay(40);
  setNeoPixelColourFromDac(currentDacR2Rvalue);
  if (debugEnabled) {
    Serial.print ("Selected Input: ");
    Serial.println (selectedInput);
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to change mute on input selector
int changeMute() {
  if (muteEnabled) {
    setMCP23S08(9, B00000001);
    muteEnabled = false;
    setNeoPixelColourFromDac(currentDacR2Rvalue);
    if (debugEnabled) {
      Serial.println ("Mute disabled");
    }
  } else {
    setMCP23S08(9, B0000000);
    muteEnabled = true;
    if (debugEnabled) {
      Serial.println ("Mute enabled");
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Functions to change volume
int changeVolume(float increment) {
  currentChangeVolumeIncrement = 0;
  float newDbLevel = currentDbLevel + increment;
  if (newDbLevel < max_dbLevel && newDbLevel > min_dbLevel) {
    currentChangeVolumeIncrement = increment;
    SetDac88812Volume(newDbLevel);
  } else if (newDbLevel >= max_dbLevel && (max_dbLevel - currentDbLevel) > 0.01) {
    SetDac88812Volume(max_dbLevel);
  } else if (newDbLevel <= min_dbLevel && (currentDbLevel - min_dbLevel) > 0.01) {
    SetDac88812Volume(min_dbLevel);
  } else {
    if (debugEnabled) {
      Serial.println ("No volume change");
    }
  }
}
int SetDac88812Volume(float newDbLevel) {
  unsigned int newDacR2Rvalue = 65536*(pow(10, (newDbLevel/20)));
  currentDacR2Rvalue = 65536*(pow(10, (currentDbLevel/20)));
  if (currentDacR2Rvalue == newDacR2Rvalue && currentChangeVolumeIncrement) {
    // This skips volume level steps that are too small or identical.
    while (currentDacR2Rvalue == newDacR2Rvalue && newDbLevel > (min_dbLevel - currentChangeVolumeIncrement)) {
      newDbLevel = newDbLevel + currentChangeVolumeIncrement;
      newDacR2Rvalue = 65536*(pow(10, (newDbLevel/20)));
      if (debugEnabled) {
        Serial.println ("Auto skipping steps");
      }
    }
  }
  if (newDacR2Rvalue <= 65536 && newDacR2Rvalue >= 0) {
    setNeoPixelColourFromDac(newDacR2Rvalue);
    currentDbLevel = newDbLevel;
    currentDacR2Rvalue = newDacR2Rvalue;
    isVolumeSavedToEeprom = false;
    timeOfLastVolumeChange = millis();
    byte highByte = newDacR2Rvalue >> 8;
    unsigned int lowBytetemp = newDacR2Rvalue << 8;
    byte lowByte = lowBytetemp >> 8;
    digitalWrite(MDACCSPin, LOW);
    SPI.transfer(3); // This is the address code for setting both DACs, ie left and right
    SPI.transfer(highByte);
    SPI.transfer(lowByte);
    digitalWrite(MDACCSPin, HIGH);\
    // Print levels
    if (debugEnabled) {
      Serial.print ("dB: ");
      Serial.print (newDbLevel, 7);
      Serial.print (" / DAC Attenuation Level: ");
      Serial.println (newDacR2Rvalue);
    }
  } else {
    if (debugEnabled) {
      Serial.println ("SetDac88812Volume level ERROR");
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Main loop
//////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  // Decode the IR if recieved
  if (irrecv.decode(&results)) {
    volumeFadeInProgress = false;
    if (results.value == 2011291790) {
      lastIRoperation = "changeInputUp";
      if (muteEnabled) { changeMute(); }
      changeInput("up");
      timeOfLastIRChange = millis();
      delay(100);
    }
    if (results.value == 2011238542) {
      lastIRoperation = "changeInputDown";
      if (muteEnabled) { changeMute(); }
      changeInput("down");
      timeOfLastIRChange = millis();
      delay(100);
    }
    if (results.value == 2011287694) {
      lastIRoperation = "volumeUp";
      if (muteEnabled) { changeMute(); }
      changeVolume(iRIncrement);
      timeOfLastIRChange = millis();
    }
    if (results.value == 2011279502) {
      lastIRoperation = "volumeDown";
      if (muteEnabled) { changeMute(); }
      changeVolume(-iRIncrement);
      timeOfLastIRChange = millis();
    }
    if (results.value == 2011265678) {
      lastIRoperation = "playPause";
      changeMute();
      timeOfLastIRChange = millis();
    }
    if (results.value == 2011250830) {
      //lastIRoperation = "menu";
    }
    if (results.value == 4294967295 && lastIRoperation != "None") {
      if (lastIRoperation == "changeInputUp") { delay(500); changeInput("up"); timeOfLastIRChange = millis(); }
      if (lastIRoperation == "changeInputDown") { delay(500); changeInput("down"); timeOfLastIRChange = millis(); }
      if (lastIRoperation == "volumeUp") { changeVolume(iRIncrement); timeOfLastIRChange = millis(); }
      if (lastIRoperation == "volumeDown") { changeVolume(-iRIncrement); timeOfLastIRChange = millis(); }
      timeOfLastIRChange = millis();
    }
    irrecv.resume(); // Receive the next value
  }
  // Read encoder
  n = digitalRead(encoder0PinA);
  if ((encoder0PinALast == LOW) && (n == HIGH)) {
    if (muteEnabled) { changeMute(); }
    volumeFadeInProgress = false;
    if (digitalRead(encoder0PinB) == LOW) {
      changeVolume(encoderIncrement);
    } else {
      changeVolume(-encoderIncrement);
    }
  }
  encoder0PinALast = n;
  // Set the time. This is used by other functions
  unsigned long currentTime = millis();
  // Save volume level
  if (!isVolumeSavedToEeprom && (currentTime - timeOfLastVolumeChange) > timeBetweenVolumeSaves) {
    if (currentDbLevel >= maximumLevelToSave && currentSavedDbLevel != maximumLevelToSave) {
      save_DbLevel(maximumLevelToSave);
    } else if (currentDbLevel < maximumLevelToSave && currentDbLevel != currentSavedDbLevel) {
      save_DbLevel(currentDbLevel);
    }
  }
  // Fade to a set volume level
  if (volumeFadeInProgress) {
    fadeInProgressLevel++;
    SetDac88812Volume(fadeInProgressLevel);
    fadeInProgressLevel = currentDbLevel;
    delay(50);
    if (fadeInProgressLevel >= targetVolumelevel) {
      volumeFadeInProgress = false;
      if (debugEnabled) {
        Serial.println ("Volume fade complete");
      }
    }
  }
  // Flash NeoPixel Red/Blue if mute is enabled
  if (muteEnabled) {
    if (!neopixelIsRed && (currentTime - timeOfLastNeopixelColourChange) > 300) {
      setNeoPixelColour(0, 0, 0);
      neopixelIsRed = true;
      neopixelIsBlue = false;
      timeOfLastNeopixelColourChange = millis();
    } else if (!neopixelIsBlue && (currentTime - timeOfLastNeopixelColourChange) > 300) {
      setNeoPixelColour(0, 255, 0);
      neopixelIsBlue = true;
      neopixelIsRed = false;
      timeOfLastNeopixelColourChange = millis();
    }
  }
  // Stop IR repeating after timeout to stop interference from other remotes
  if ((currentTime - timeOfLastIRChange) > 1000 && lastIRoperation != "None") {
    lastIRoperation = "None";
  }
}
