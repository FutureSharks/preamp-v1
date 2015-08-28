// Time based acceleration of volume changes via IR/encoder?
// Add NeoPixel code
// Tidy variable names
// Tidy all the dB variables, too many of them
// Mote from remote?
//
//////////////////////////////////////////////////////////////////////////////////////////////
// If enabled volume/input data will be printed via serial
bool debugEnabled = true;

// EEPROM related stuff to save volume level
#include "EEPROM.h"
int dBLevelEEPROMAddressBit = 0;
bool isVolumeSavedToEeprom = true;
unsigned long timeOfLastVolumeChange;
unsigned long timeBetweenVolumeSaves = 60000;
byte maximumLevelToSave = 30;

// Volume fading stuff
bool volumeFadeInProgress = true;

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

// Input selector stuff
int selectedInput = 0;
const int inputSelectorCSPin = 9;
long muteDelay = 1000;

// MDAC attenuator stuff
const int MDACCSPin = 10;
float currentDbLevel;
float max_dbLevel = -0.0001;
float min_dbLevel = -96.5;
float encoderIncrement = 2;
float iRIncrement = 3;
float currentChangeVolumeIncrement;
float targetVolumelevel;
float newVolumeSetting;

// Encoder stuff
const int encoder0GroundPin = 4;
const int encoder0PowerPin = 5;
int encoder0PinA = 6;
int encoder0PinB = 7;
int encoder0Pos = 0;
int encoder0PinALast = HIGH;
int n = LOW;

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
  // Set initial MADC volume level from EEPROM and prepare to fade to this level
  currentDbLevel = read_DbLevel();
  targetVolumelevel = currentDbLevel;
  newVolumeSetting = min_dbLevel;
  if (debugEnabled) {
    Serial.print ("Setting volume to MDAC level to minimum. Will fade to: ");
    Serial.println (currentDbLevel);
  }
  SetDac88812Volume(min_dbLevel);
  volumeFadeInProgress = true;
  // Delay before unmuting output
  delay(muteDelay);
  setMCP23S08(9, B00000001);
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to save the DB level to EERPOM
int save_DbLevel (float DbLevel) {
  byte byteToWrite = (int) DbLevel * -1;
  if (byteToWrite < maximumLevelToSave) {
    byteToWrite = maximumLevelToSave;
  }
  if (debugEnabled) {
    Serial.print ("Writing byte to EEPROM: ");
    Serial.println (byteToWrite);
  }
  isVolumeSavedToEeprom = true;
  EEPROM.write(dBLevelEEPROMAddressBit, byteToWrite);
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to read the DB level to EERPOM
int read_DbLevel (){
  byte byteFromEeprom = EEPROM.read(dBLevelEEPROMAddressBit);
  return (float) (-1 * byteFromEeprom);
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
      setMCP23S08(9, B00000001);
      break;
    case 2:
      setMCP23S08(9, B00110011);
      delay(5);
      setMCP23S08(9, B00000001);
      break;
    case 3:
      setMCP23S08(9, B00101101);
      delay(5);
      setMCP23S08(9, B00000001);
      break;
  }
  if (debugEnabled) {
    Serial.print ("Selected Input: ");
    Serial.println (selectedInput);
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Functions to change volume
int changeVolume(float increment) {
  float newDbLevel = currentDbLevel + increment;
  currentChangeVolumeIncrement = increment;
  volumeFadeInProgress = false;
  if (newDbLevel < max_dbLevel && newDbLevel > min_dbLevel) {
    SetDac88812Volume(newDbLevel);
  } else if (newDbLevel >= max_dbLevel && currentDbLevel != max_dbLevel) {
    SetDac88812Volume(max_dbLevel);
  } else if (newDbLevel <= min_dbLevel && currentDbLevel != min_dbLevel) {
    SetDac88812Volume(min_dbLevel);
  } else {
    if (debugEnabled) {
      Serial.println ("No volume change");
    }
  }
}
int SetDac88812Volume(float newDbLevel) {
  unsigned int newDacR2Rvalue = 65536*(pow(10, (newDbLevel/20)));
  unsigned int currentDacR2Rvalue = 65536*(pow(10, (currentDbLevel/20)));
  if (currentDacR2Rvalue == newDacR2Rvalue && currentChangeVolumeIncrement) {
    while (currentDacR2Rvalue == newDacR2Rvalue && newDbLevel > (min_dbLevel - currentChangeVolumeIncrement)) {
      newDbLevel = newDbLevel + currentChangeVolumeIncrement;
      newDacR2Rvalue = 65536*(pow(10, (newDbLevel/20)));
    }
  }
  if (newDacR2Rvalue <= 65536 && newDacR2Rvalue >= 0) {
    currentDbLevel = newDbLevel;
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
      Serial.print (newDbLevel);
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
    if (results.value == 2011291790) {
      lastIRoperation = "changeInputUp";
      changeInput("up");
      delay(100);
    }
    if (results.value == 2011238542) {
      lastIRoperation = "changeInputDown";
      changeInput("down");
      delay(100);
    }
    if (results.value == 2011287694) {
      lastIRoperation = "volumeUp";
      changeVolume(iRIncrement);
    }
    if (results.value == 2011279502) {
      lastIRoperation = "volumeDown";
      changeVolume(-iRIncrement);
    }
    if (results.value == 4294967295) {
      if (lastIRoperation == "changeInputUp") { delay(500); changeInput("up"); }
      if (lastIRoperation == "changeInputDown") { delay(500); changeInput("down"); }
      if (lastIRoperation == "volumeUp") { changeVolume(iRIncrement); }
      if (lastIRoperation == "volumeDown") { changeVolume(-iRIncrement); }
    }
    irrecv.resume(); // Receive the next value
  }
  // Read encoder
  n = digitalRead(encoder0PinA);
  if ((encoder0PinALast == LOW) && (n == HIGH)) {
      if (digitalRead(encoder0PinB) == LOW) {
        changeVolume(encoderIncrement);
      } else {
        changeVolume(-encoderIncrement);
      }
  }
  encoder0PinALast = n;
  // Save volume level
  unsigned long CurrentTime = millis();
  if (!isVolumeSavedToEeprom && (CurrentTime - timeOfLastVolumeChange) > timeBetweenVolumeSaves) {
    save_DbLevel(currentDbLevel);
  }
  // Fade to a set volume level
  if (volumeFadeInProgress) {
    newVolumeSetting++;
    SetDac88812Volume(newVolumeSetting);
    newVolumeSetting = currentDbLevel;
    delay(50);
    if (newVolumeSetting >= targetVolumelevel) {
      volumeFadeInProgress = false;
      if (debugEnabled) {
        Serial.println ("Volume fade complete");
      }
    }
  }
}
