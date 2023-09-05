#include <Adafruit_MCP4725.h> // Required for MCP4725 library
#include <SPI.h>              // Required for I2C comms
#include <Wire.h>             // Required for I2C comms
#include <SendOnlySoftwareSerial.h>
#define TOP_SWITCH 10
#define BOTTOM_SWITCH 11
#define bottomLED 12
#define middleLED 9
#define MIDI_NOTE_ON    0x90 
#define MIDI_NOTE_OFF   0x80
#define MIDI_POLYPHONIC 0xA0
#define MIDI_CC =B0


Adafruit_MCP4725 dac; // creates an instance of the DAC
Adafruit_MCP4725 dac2; // creates an instance of the DAC

int noteBuffer = 1;
int tempNoteBuffer = 0;
byte data;
int a1Raw = 0;
int lastA1Raw = 0;
int analogNote = 0;
unsigned long lastMidiNoteOn = 0;
unsigned long waitPeriod = 2000;
unsigned long blinkLength = 150;
unsigned long minPauseDAC = 80;
bool blinkBottom = false;
bool midiChanged = false;
unsigned long tempPause = 250;
unsigned long lastAnalog = 0;
int lastAnalogNote = 0;
int topSwitchStatus = 0;
int bottomSwitchStatus = 0;
int loopCounter = 0;

const PROGMEM uint16_t dac_notes[61] = 
{// C    C#     D    D#     E     F    F#     G    G#     A    A#     B 
    0,   68,  137,  205,  273,  341,  410,  478,  546,  614,  683,  751,
  819,  887,  956, 1024, 1092, 1161, 1229, 1297, 1365, 1434, 1502, 1570,
 1638, 1707, 1775, 1843, 1911, 1980, 2048, 2116, 2185, 2253, 2321, 2389, 
 2458, 2526, 2594, 2662, 2731, 2799, 2867, 2935, 3004, 3072, 3140, 3209,
 3277, 3345, 3413, 3482, 3550, 3618, 3686, 3755, 3823, 3891, 3959, 4028,
 4095};
SendOnlySoftwareSerial serialOut2(4); 
unsigned long lastMidi = 0;
void setup() {
  Serial.begin(31250);      //Midi In/Out 1
  serialOut2.begin(9600);
  serialOut2.println("DEBUG OUT");
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(bottomLED, OUTPUT);
  pinMode(middleLED, OUTPUT);
  pinMode(TOP_SWITCH, INPUT_PULLUP);
  pinMode(BOTTOM_SWITCH, INPUT_PULLUP);
  dac.begin(0x60);
  dac2.begin(0x61);
  delayMicroseconds(25);
  TWBR = ((F_CPU /400000l) - 16) / 2; // Change the i2c clock to 400KHz
  dac.setVoltage( 0 , false);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100); 
  serialOut2.println("DEBUG OUT3");   
}

void loop() {
  //readSwitches();
  loopCounter++;
  readAnalogInput();
  checkMidiIn();
  if ((millis() - lastMidiNoteOn) > waitPeriod) digitalWrite(LED_BUILTIN, LOW);
}
int returnClosestNote(int aNote){
 // serialOut2.print("aNote:");
 // serialOut2.print(aNote);
 // serialOut2.print(" ");
  int theQuantizedNote;
  int a = aNote % 12;
  if bitRead(noteBuffer, a) theQuantizedNote = aNote;
  else
  {
      bool keepGoing = true;
      while (keepGoing)
      {
        if (aNote !=0 ) 
        {
          aNote = aNote - 1;
          a = aNote % 12;
          if bitRead(noteBuffer, a) {
            theQuantizedNote = aNote;
            keepGoing = false;
          }
        } else 
        {
          theQuantizedNote = 100;
          keepGoing = false;
        }
      }
  }
  //serialOut2.print("q:");
  //serialOut2.println(theQuantizedNote);
  return theQuantizedNote;
}
void readAnalogInput() {
  //delayMicroseconds(100);
  a1Raw = analogRead(A1);
  if ((abs(a1Raw - lastA1Raw))>5) {
    analogNote = map(a1Raw, 0, 1024, 0, 61);
    analogNote = returnClosestNote(analogNote);
    if (analogNote == 100) analogNote = lastAnalogNote;
    lastA1Raw = a1Raw;
  }
  //if ((analogNote != lastAnalogNote)||midiChanged) {
  if (analogNote != lastAnalogNote) {
  
    midiChanged = false;
    lastAnalogNote = analogNote;
    //int a = analogNote % 12;
    //if bitRead(noteBuffer, a) dac.setVoltage( pgm_read_word(&(dac_notes[analogNote])), false);
    dac.setVoltage( pgm_read_word(&(dac_notes[analogNote])), false);
    int shiftedNote = 0;
    if (analogNote > 55) shiftedNote = analogNote;
    else shiftedNote = analogNote + 5;
    dac2.setVoltage( pgm_read_word(&(dac_notes[shiftedNote])), false);
    
    //serialOut2.print("NOTE: ");
    //serialOut2.print(analogNote);
    //serialOut2.print(" ");
    //serialOut2.print(lastAnalogNote);
    //serialOut2.print(" ");
    //serialOut2.println(millis()-lastAnalog);
    lastAnalog = millis();
  } 

  
  //}   
}
void checkMidiIn(){
  if(Serial.available() > 0) {
    byte data = Serial.read();
    serialOut2.println(data,HEX);
    if (data = MIDI_NOTE_ON) {
      if ((millis()- lastMidiNoteOn) > waitPeriod) {
        noteBuffer = 0;
        midiChanged = true;
        serialOut2.println("MIDI Changed");
      }
      data = Serial.read();
      serialOut2.println(data,HEX);
      int p = data % 12;
      bitSet(noteBuffer, p);
      data = Serial.read();
      serialOut2.println(data,HEX);
      lastMidiNoteOn = millis();
      printNotes();
      digitalWrite(LED_BUILTIN, HIGH);
      
            
    }
   }
}
/*
void checkMidiIn(){
  byte data1;
  byte data2;
  byte data3;
    while(Serial.available() > 0) {
    data1 = Serial.read();
    if(data1 == MIDI_NOTE_ON){
      data2 = Serial.read();
      data3 = Serial.read();
      serialOut2.print("Note On:");
      serialOut2.print(data1, HEX);
      serialOut2.print(":");
      serialOut2.print(data2, HEX);
      serialOut2.print(":");
      serialOut2.println(data3, HEX);
      if ((millis()- lastMidiNoteOn) > waitPeriod) noteBuffer = 0;
      //serialOut2.print(data,HEX);
      //serialOut2.print(" ");
           
      int p = data2 % 12;
      bitSet(noteBuffer, p);
      if (topSwitchStatus==1) noteBuffer = tempNoteBuffer;
      //serialOut2.print(tempNoteBuffer);
      //serialOut2.print("   ");
      serialOut2.print(noteBuffer);
      lastMidiNoteOn = millis();
      
      digitalWrite(LED_BUILTIN, HIGH);
      printNotes();
      
    }
       
  }
}*/
void readSwitches(){
  topSwitchStatus = digitalRead(TOP_SWITCH);
  bottomSwitchStatus = digitalRead(BOTTOM_SWITCH);
}
void printNotes() {

    if (bitRead(noteBuffer,0))  serialOut2.print("C " );
    if (bitRead(noteBuffer,1))  serialOut2.print("C# ");
    if (bitRead(noteBuffer,2))  serialOut2.print("D " );
    if (bitRead(noteBuffer,3))  serialOut2.print("D# ");
    if (bitRead(noteBuffer,4))  serialOut2.print("E " );
    if (bitRead(noteBuffer,5))  serialOut2.print("F " );
    if (bitRead(noteBuffer,6))  serialOut2.print("F# ");
    if (bitRead(noteBuffer,7))  serialOut2.print("G " );
    if (bitRead(noteBuffer,8))  serialOut2.print("G# ");
    if (bitRead(noteBuffer,9))  serialOut2.print("A " );
    if (bitRead(noteBuffer,10)) serialOut2.print("A# ");
    if (bitRead(noteBuffer,11)) serialOut2.print("B " );
    serialOut2.println();
    
}
