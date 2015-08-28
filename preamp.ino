// Tidy IR repeat code
// Time based acceleration of volume changes via IR/encoder?
// Remember volume level after power off
// Volume is changed on bootup due to encoder code
// Verify dB curve, try proper dB curve
// Skip repeated dacAttenuationLevel steps
//
//
//////////////////////////////////////////////////////////////////////////////////////////////
#include "SPI.h"
// Arduino pin 9 & 10 = inputSelectorCSPin & MDACCSPin
// Arduino pin 11 = SDI
// Arduino pin 13 = CLK

// If enabled volume/input data will be printed via serial
String debugEnabled = "True";

// Neopixel Stuff
#include "Adafruit_NeoPixel.h"
#define neopixelPin 8
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(8, neopixelPin, NEO_GRB + NEO_KHZ800);

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

// MDAC attenuator stuff
const int MDACCSPin = 10;
double currentDbLevel = -30;
double max_dbLevel = -0.01;
double min_dbLevel = -97;
double encoderIncrement = 1;
double iRIncrement = 3;

// Encoder stuff
const int encoder0GroundPin = 4;
const int encoder0PowerPin = 5;
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
  if (debugEnabled == "True") {
    Serial.begin (9600);
  }
  // Neopixel
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  // SPI
  // set the CS pins as output:
  pinMode (inputSelectorCSPin, OUTPUT);
  digitalWrite(inputSelectorCSPin,HIGH);
  pinMode (MDACCSPin, OUTPUT);
  digitalWrite(MDACCSPin,HIGH);
  // Start SPI
  if (debugEnabled == "True") {
    Serial.println ("Starting SPI..");
  }
  SPI.begin();
  // Set SPI selector IO direction to output for all pinds
  Serial.println ("Setting SPI selector IO direction control registers..");
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
  // Set MADC volume to 0
  if (debugEnabled == "True") {
    Serial.println ("Setting volume to initial value..");
  }
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
  if (debugEnabled == "True") {
    Serial.print ("Selected Input: ");
    Serial.println (selectedInput);
  }
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
    digitalWrite(MDACCSPin, HIGH);\
    // Print levels
    if (debugEnabled == "True") {
      Serial.print ("dB: ");
      Serial.print (currentDbLevel);
      Serial.print (" / DAC Attenuation Level: ");
      Serial.println (dacAttenuationLevel);
      int BlueNeopixelBrightness = (currentDbLevel * -2.6);
      int RedNeopixelBrightness = (255 - (currentDbLevel * -2.6));
      SetPixelColour(RedNeopixelBrightness, 0, BlueNeopixelBrightness);
    }
  } else {
    if (debugEnabled == "True") {
      Serial.println ("SetDac88812Volume ERROR");
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////
// Functions to change Neopixel colour
int SetPixelColour(int red, int green, int blue) {
  strip.setPixelColor(0, red, green, blue);
  strip.setPixelColor(1, red, green, blue);
  strip.setPixelColor(2, red, green, blue);
  strip.setPixelColor(3, red, green, blue);
  strip.setPixelColor(4, red, green, blue);
  strip.setPixelColor(5, red, green, blue);
  strip.setPixelColor(6, red, green, blue);
  strip.setPixelColor(7, red, green, blue);
  strip.show();
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
