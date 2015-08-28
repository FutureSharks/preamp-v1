// Tidy IR repeat code
// Time based acceleration of volume changes via IR
// Test time based acceleration of volume changes via encoder
// Remember volume level after power off
// Volume is changed on bootup due to encoder code
// Verify dB curve, try proper dB curve
// Skip repeated dacAttenuationLevel steps
// Remove IR power
// Need to remove serial for speed?
//////////////////////////////////////////////////////////////////////////////////////////////
#include "SPI.h"
// Arduino pin 9 & 10 = inputSelectorCSPin & MDACCSPin
// Arduino pin 11 = SDI
// Arduino pin 13 = CLK

// IR stuff
#include "IRremote.h"
int RECV_PIN = 5;  // IR Receiver pin
// Temporary power
const int IRPowerPin = A0;
const int IRGroundPin = A1;
IRrecv irrecv(RECV_PIN);
decode_results results;
String lastIRoperation;

// Input selector stuff
int selectedInput = 0;
const int inputSelectorCSPin = 9;

// MDAC attenuator stuff
const int MDACCSPin = 10;
double currentDbLevel = -30;
double max_dbLevel = -0.01;
double min_dbLevel = -97;
double encoderIncrement = 1;
double iRIncrement = 3;

// Encoder stuff
int encoder0PinA = 6;
int encoder0PinB = 7;
int encoder0Pos = 0;
int encoder0PinALast = LOW;
int n = LOW;

//////////////////////////////////////////////////////////////////////////////////////////////
// Setup
//////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  // Serial
  Serial.begin (9600);
  // Set up pins for encoder:
  pinMode (encoder0PinA,INPUT);
  pinMode (encoder0PinB,INPUT);
  // SPI Stuff
  // set the CS pins as output:
  pinMode (inputSelectorCSPin, OUTPUT);
  digitalWrite(inputSelectorCSPin,HIGH);
  pinMode (MDACCSPin, OUTPUT);
  digitalWrite(MDACCSPin,HIGH);
  // Start SPI
  Serial.println ("Starting SPI..");
  SPI.begin();
  // Set SPI selector IO direction to output for all pinds
  Serial.println ("Setting SPI selector IO direction control registers..");
  digitalWrite(inputSelectorCSPin,LOW);
  SPI.transfer(B01000000); // Send Device Opcode
  SPI.transfer(0); // Select IODIR register
  SPI.transfer(0); // Set register
  digitalWrite(inputSelectorCSPin,HIGH);
  // IR
  irrecv.enableIRIn(); // Start the receiver
  pinMode(IRPowerPin, OUTPUT);
  pinMode(IRGroundPin, OUTPUT);
  digitalWrite(IRPowerPin, HIGH); // Power for the IR
  digitalWrite(IRGroundPin, LOW); // GND for the IR
  // Set MADC volume to 0
  Serial.println ("Setting volume to initial value..");
  SetDac88812Volume(currentDbLevel);
  // Sleep for start-up mute
  delay(1000);
  setMCP23S08(9, B00000001);
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
  Serial.print ("Selected Input: ");
  Serial.println (selectedInput);
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Functions to change volume
int ChangeVolume(double increment) {
  double newDbLevel = currentDbLevel + increment;
  if (newDbLevel < max_dbLevel && newDbLevel > min_dbLevel) {
    currentDbLevel = newDbLevel;
    SetDac88812Volume(currentDbLevel);
  } else if (newDbLevel >= max_dbLevel && currentDbLevel != max_dbLevel) {
    SetDac88812Volume(max_dbLevel);
    currentDbLevel = max_dbLevel;
  } else if (newDbLevel <= min_dbLevel && currentDbLevel != min_dbLevel) {
    SetDac88812Volume(min_dbLevel);
    currentDbLevel = min_dbLevel;
  } 
}
int SetDac88812Volume(double currentDbLevel) {
  unsigned int dacAttenuationLevel = 65536*(pow(10, (currentDbLevel/20)));
  if (dacAttenuationLevel <= 65536 || dacAttenuationLevel >= 0) {
    byte highByte = dacAttenuationLevel >> 8;
    unsigned int lowBytetemp = dacAttenuationLevel << 8;
    byte lowByte = lowBytetemp >> 8;
    digitalWrite(MDACCSPin, LOW);
    SPI.transfer(3); // This is the address code for setting both DACs, ie left and right
    SPI.transfer(highByte);
    SPI.transfer(lowByte);
    digitalWrite(MDACCSPin, HIGH);
    // Print levels
    Serial.print ("dB: ");
    Serial.print (currentDbLevel);
    Serial.print (" / DAC Attenuation Level: ");
    Serial.println (dacAttenuationLevel);
  } else {
    Serial.println ("SetDac88812Volume ERROR");
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Main loop
//////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  // Decode the IR if recieved
  if (irrecv.decode(&results)) {
    if(results.value == 2011291790) {
      lastIRoperation = "changeInputUp";
      changeInput("up");
      delay(100);
    }
    if(results.value == 2011238542) {
      lastIRoperation = "changeInputDown";
      changeInput("down");
      delay(100);
    }
    if(results.value == 2011287694) {
      lastIRoperation = "volumeUp";
      ChangeVolume(iRIncrement);
    }
    if(results.value == 2011279502) {
      lastIRoperation = "volumeDown";
      ChangeVolume(-iRIncrement);
    }
    if(results.value == 4294967295) {
      if (lastIRoperation == "changeInputUp") { delay(500); changeInput("up"); }
      if (lastIRoperation == "changeInputDown") { delay(500); changeInput("down"); }
      if (lastIRoperation == "volumeUp") { ChangeVolume(iRIncrement); }
      if (lastIRoperation == "volumeDown") { ChangeVolume(-iRIncrement); }
    }
    irrecv.resume(); // Receive the next value
  }
  // Read encoder
  n = digitalRead(encoder0PinA);
  if ((encoder0PinALast == LOW) && (n == HIGH)) {
    if (digitalRead(encoder0PinB) == LOW) {
      ChangeVolume(encoderIncrement);
    } else {
      ChangeVolume(-encoderIncrement);
    }
  }
  encoder0PinALast = n;
}
