//////////////////////////////////////////////////////////////////////////////////////////////
#include "SPI.h"
// Arduino pin 9 & 10 = CS
// Arduino pin 11 = SDI
// Arduino pin 13 = CLK

// IR stuff
#include "IRremote.h"
int RECV_PIN = A0;  // IR Receiver pin
IRrecv irrecv(RECV_PIN);
decode_results results;
String lastIRoperation;

// Input selector stuff
int selectedInput = 0;
const int inputSelectorCSPin = 9;

// MDAC attenuator stuff
const int MDACCSPin = 10;
double dbLevel = -30;
double dbIncrement = 0.5;
unsigned int dac_level;
// Address code 3 = Both A and B DACs
int addr_decode = 3;

// Encoder stuff
const int encoderPowerPin = 4;
const int encoderGroundPin = 5;
int encoder0PinA = 6;
int encoder0PinB = 7;
int encoder0Pos = 0;
int encoder0PinALast = LOW;
int n = LOW;

//////////////////////////////////////////////////////////////////////////////////////////////
// Setup
//////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  // set the CS pins as output:
  pinMode (inputSelectorCSPin, OUTPUT);
  digitalWrite(inputSelectorCSPin,HIGH);
  pinMode (MDACCSPin, OUTPUT);
  digitalWrite(MDACCSPin,HIGH);
  // Serial
  Serial.begin (9600);
  // Set up pins for encoder:
  pinMode (encoder0PinA,INPUT);
  pinMode (encoder0PinB,INPUT);
  pinMode (encoderPowerPin, OUTPUT);
  pinMode (encoderGroundPin, OUTPUT);
  digitalWrite(encoderPowerPin,HIGH);
  digitalWrite(encoderGroundPin,LOW);
  // Start serial
  Serial.println ("Starting SPI..");
  SPI.begin();
  Serial.println ("Setting control registers..");
  // Set SPI selector IO direction to output for all pinds
  digitalWrite(inputSelectorCSPin,LOW);
  SPI.transfer(B01000000); // Send Device Opcode
  SPI.transfer(0); // Select IODIR register
  SPI.transfer(0); // Set register
  digitalWrite(inputSelectorCSPin,HIGH);
  // Sleep for start-up mute
  delay(1000);
  setMCP23S08(9, B00000001);  
  // IR
  irrecv.enableIRIn(); // Start the receiver
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  digitalWrite(A1, LOW); // GND for the IR
  digitalWrite(A2, HIGH); // Power for the IR
  // Set MADC volume to 0
  Serial.println ("Setting volume to 0..");
  DACWrite(addr_decode, 0);
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Function to send data to the MCP23S08
// select_register: 9 = GPIO
int setMCP23S08 (int select_register, int register_value) {
  digitalWrite(inputSelectorCSPin,LOW);
  SPI.transfer(B01000000); // Send Device Opcode
  SPI.transfer(select_register); // Select register
  SPI.transfer(register_value); // Set register
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
int WriteAndPrintLevel(){
  Serial.print ("dB: ");
  Serial.print (dbLevel);
  Serial.print (" / DAC Level: ");
  Serial.println (dac_level);
  DACWrite(addr_decode, dac_level);
}
int VolumeUp() {
  if (dbLevel < (0-dbIncrement)) {
    dbLevel = dbLevel + dbIncrement;
    dac_level = 65536*(pow(10, (dbLevel/20)));
    WriteAndPrintLevel();
  }
}
int VolumeDown() {
  if (dbLevel > -102.5) {
    dbLevel = dbLevel - dbIncrement;
    dac_level = 65536*(pow(10, (dbLevel/20)));
    WriteAndPrintLevel();
  }
}
void DACWrite(int addr_decode, unsigned int level) {
  digitalWrite(MDACCSPin, LOW);
  SPI.transfer(addr_decode);
  byte highByte = level >> 8;  
  unsigned int lowBytetemp = level << 8;
  byte lowByte = lowBytetemp >> 8;
  SPI.transfer(highByte);
  SPI.transfer(lowByte);
  digitalWrite(MDACCSPin, HIGH);
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
      VolumeUp();
    }
    if(results.value == 2011279502) {
      lastIRoperation = "volumeDown";
      VolumeDown();
    }
    if(results.value == 4294967295) {
      if (lastIRoperation == "changeInputUp") { delay(500); changeInput("up"); }
      if (lastIRoperation == "changeInputDown") { delay(500); changeInput("down"); }
      if (lastIRoperation == "volumeUp") { VolumeUp(); }
      if (lastIRoperation == "volumeDown") { VolumeDown(); }
    }
    irrecv.resume(); // Receive the next value
  }
  // Read encoder
  n = digitalRead(encoder0PinA);
  if ((encoder0PinALast == LOW) && (n == HIGH)) {
    if (digitalRead(encoder0PinB) == LOW) {
      VolumeUp();
    } else {
      VolumeDown();
    }
  }
  encoder0PinALast = n;
}
