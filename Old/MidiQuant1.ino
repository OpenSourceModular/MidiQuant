/*
 * 
 * MidiQuantizer for Eurorack
 * Justin Ahrens
 * justin@ahrens.net 
 * 
 */

#include <Adafruit_MCP4725.h> // Required for MCP4725 library
#include <SPI.h>              // Required for I2C comms
#include <Wire.h>             // Required for I2C comms
#include <SendOnlySoftwareSerial.h>
#define TOP_SWITCH 10
#define BOTTOM_SWITCH 11
#define TOP_LED_PIN 13
#define BOTTOM_LED_PIN 12
#define MIDDLE_LED_PIN 9
#define KNOB_PIN A0
#define CV_PIN A2
#define MIDI_NOTE_ON    0x90 
#define MIDI_NOTE_OFF   0x80
#define MIDI_POLYPHONIC 0xA0
#define MIDI_CC B0
#define DEBUG_OUT 1 //change to 1 to send debug messages via debug pin
                    //debug messages can be sent to 2nd Arduino which relays them to USB Serial      

Adafruit_MCP4725 dac; // creates an instance of the DAC
Adafruit_MCP4725 dac2; // creates an instance of the DAC

bool blinkBottom = false;
bool midiChanged = false;
bool shiftChanged = false;

byte data;

int noteBuffer = 1;
int tempNoteBuffer = 0;
int a1Raw = 0;
int lastA1Raw = 0;
int a2Raw = 0;
int lastA2Raw = 0;
int analogNote = 0;
int lastAnalogNote = 0;
int analogNote2 = 0;
int lastAnalogNote2 = 0;
int topSwitchStatus = 0;
int bottomSwitchStatus = 0;
int loopCounter = 0;
int shiftAmount = 0;
int lastShiftAmount = 0; 
int shiftCV = 0;

unsigned long lastMidiNoteOn = 0;
unsigned long waitPeriod = 2000;
unsigned long blinkLength = 150;
unsigned long minPauseDAC = 80;
unsigned long tempPause = 250;
unsigned long lastAnalog = 0;
//

// There are three options for powering the DAC module(s)
// Option one: power them with the same 5v source as the ATMega chip(or NANO)
//     This voltage may vary quite a bit as your rack uses more or less power
//
// Option two: use a 5V LM4040 voltage reference with a 10k resistor to get 5.0 V. This will be very close to 5.0v and remain consistent
//
// Option three: use a 4.069V LM4040 voltage reference with a to 4.097 V. This will be very close to 4.096V and remain consistent
//
// The advantage of the 4.096 option is that it matches the 4096 step resolution of the 12 bit DAC. This means each step is .001v 
// This also means that the pitches are a bit more accurate compared to 5.0V
//
// Depending on which option you choose, measure the votage at the DAC and update the "actualVoltageAtDAC" variable
// The formula will calculate the values for each pitch based on the actual voltage.

float numNotes = 48.0; // 48.0 for 4.096v DAC -- 60.0 for 5.00v DAC
float topVolt = 4.0; // Adjust this to your voltage at the DAC. Do not go lower than 4.0 or 5.0
float actualVoltageAtDAC = 4.096;
float dacInterval = (((topVolt / actualVoltageAtDAC) * 4095.0) / numNotes);

uint32_t dac_notes[61]; // holds the calculated values for the DAC

/*const PROGMEM uint16_t dac_notes[61] = 
{// C    C#     D    D#     E     F    F#     G    G#     A    A#     B 
    0,   68,  137,  205,  273,  341,  410,  478,  546,  614,  683,  751,
  819,  887,  956, 1024, 1092, 1161, 1229, 1297, 1365, 1434, 1502, 1570,
 1638, 1707, 1775, 1843, 1911, 1980, 2048, 2116, 2185, 2253, 2321, 2389, 
 2458, 2526, 2594, 2662, 2731, 2799, 2867, 2935, 3004, 3072, 3140, 3209,
 3277, 3345, 3413, 3482, 3550, 3618, 3686, 3755, 3823, 3891, 3959, 4028,
 4095};*/
 
SendOnlySoftwareSerial serialOut2(4); 
unsigned long lastMidi = 0;
void setup() {
  for (int x = 0; x < numNotes+2; x++) {dac_notes[x] = round(x * dacInterval);}
  Serial.begin(31250);      //Midi In/Out 1
  if (DEBUG_OUT) serialOut2.begin(9600);
  if (DEBUG_OUT) serialOut2.println("DEBUG OUT");
  pinMode(TOP_LED_PIN, OUTPUT);
  pinMode(BOTTOM_LED_PIN, OUTPUT);
  pinMode(MIDDLE_LED_PIN, OUTPUT);
  pinMode(TOP_SWITCH, INPUT_PULLUP);
  pinMode(BOTTOM_SWITCH, INPUT_PULLUP);
  dac.begin(0x60);
  dac2.begin(0x61);
  delayMicroseconds(25);
  TWBR = ((F_CPU /400000l) - 16) / 2; // Change the i2c clock to 400KHz
  dac.setVoltage( 0 , false);
  for (int o = 0; o < 4; o++) // blink top led
  {
    digitalWrite(TOP_LED_PIN, HIGH);
    delay(100);
    digitalWrite(TOP_LED_PIN, LOW);
    delay(100);
  }
  
}

void loop() {
  //serialOut2.println(analogRead(KNOB_PIN));
  readSwitches();
  loopCounter++;
  readAnalogInput();
  checkMidiIn();
  if ((millis() - lastMidiNoteOn) > waitPeriod) digitalWrite(TOP_LED_PIN, LOW);
    
}
int returnClosestNote(int aNote) //simple function to go down the scale until we get a note in the chord
{
  int theQuantizedNote;
  int a = aNote % 12;                                  // get the modulo of the note (C = 0; C# = 1, etc.) 
  if bitRead(noteBuffer, a) theQuantizedNote = aNote;  // check if the bit is set for this note. If it is, we are done
  else
  {
      bool keepGoing = true; // start going down from the note until we find one
      while (keepGoing)
      {
        if (aNote !=0 )  // if aNote = 0, we hit bottom 
        {
          aNote = aNote - 1;
          a = aNote % 12;
          if bitRead(noteBuffer, a) {
            theQuantizedNote = aNote;
            keepGoing = false;
          }
        } else 
        { 
          bool test = true;
          int h = 0;
          while (test) {
            if (bitRead(noteBuffer, h))
            {
              theQuantizedNote = 2 ^ h;
              test = false;
            }
            h++;
          }
          //theQuantizedNote = 100;
          keepGoing = false;
        }
      }
  }
  return theQuantizedNote;
}

void readAnalogInput() {
  //serialOut2.println(analogRead(A2));
  shiftCV = map(analogRead(CV_PIN), 0, 1024,0,13);
  shiftAmount = map(analogRead(KNOB_PIN), 0, 1024, 0, 13);
  if (bottomSwitchStatus) shiftAmount = shiftAmount + shiftCV;
  if (shiftAmount > 12) shiftAmount = 12;
  if (shiftAmount != lastShiftAmount)
  {
      lastShiftAmount = shiftAmount;
      shiftChanged = true;
  }
  a1Raw = analogRead(A1);                                     // get the current analog in
  a2Raw = analogRead(A2);

  if (((abs(a1Raw - lastA1Raw))>5) || midiChanged || shiftChanged)                             // check if it is more than 5 steps away from last reading (debounce)     
  {
    
    analogNote = map(a1Raw, 0, 1024, 0, numNotes + 1);                  // map the new reading from 0-60 (C to C 5 octaves)(or 0-48)
    analogNote = returnClosestNote(analogNote);               // get the closest quantized note (going down)
    if (analogNote == 100) analogNote = lastAnalogNote;       // if there is no quantized note, the function returns a 100 so reset the analog note
    lastA1Raw = a1Raw;                                        // save the last ananlog reading
  }  

  if (((abs(a2Raw - lastA2Raw))>5) || midiChanged || shiftChanged)                             // check if it is more than 5 steps away from last reading (debounce)     
  {
    
    analogNote2 = map(a2Raw, 0, 1024, 0, numNotes + 1);                  // map the new reading from 0-60 (C to C 5 octaves)(or 0-48)
    analogNote2 = returnClosestNote(analogNote2);               // get the closest quantized note (going down)
    if (analogNote2 == 100) analogNote2 = lastAnalogNote2;       // if there is no quantized note, the function returns a 100 so reset the analog note
    lastA2Raw = a2Raw;                                        // save the last ananlog reading
  }
  
  if (analogNote != lastAnalogNote || analogNote2 != lastAnalogNote2 || midiChanged || shiftChanged)            // if we have a new note or if we have a new midi chord
  {
    if (midiChanged) midiChanged = false;                                               // reset midiChanged
    if (shiftChanged) shiftChanged = false;                                               // reset midiChanged
    lastAnalogNote = analogNote;                                       // save lastAnalogNote
    dac.setVoltage( dac_notes[analogNote], false);   // set the DAC voltage to the new note
    int shiftedNote = 0;                                               // shifted note for second DAC  
    if (analogNote > numNotes - shiftAmount) shiftedNote = analogNote;                     // as long as the note isn't too high 
    else shiftedNote = analogNote + shiftAmount;                                 // add the shift
    if (bottomSwitchStatus) dac2.setVoltage( dac_notes[shiftedNote], false); // set the voltage for DAC 2
    else dac2.setVoltage( dac_notes[analogNote2], false); // set the voltage for DAC 2
    lastAnalog = millis();                                             // save time of last analog change
  } 
}

void checkMidiIn(){
  if(Serial.available() > 0) {                              //if there is serial data waiting
    byte data = Serial.read();
    delay(1); //read one byte
    //if (DEBUG_OUT) serialOut2.println(data,HEX);            
    if (data == MIDI_NOTE_ON) {                              //if it is a MIDI Note On message
      if ((millis()- lastMidiNoteOn) > waitPeriod)          //our waiting period has elapsed
      {                                                     // so
        noteBuffer = 0;                                     // reset note buffer
        midiChanged = true;                                 // this lets the main loop know we have new midi input
        //if (DEBUG_OUT) serialOut2.println("MIDI Changed");
      }
      data = Serial.read();                                 // read the next byte which is the note value
      if (DEBUG_OUT) serialOut2.println(data,HEX);
      int p = data % 12;                                    // get the modulo of the note i.e. C = 0, C# = 1, D = 2, etc...
      bitSet(noteBuffer, p);                                // set the bit. So if you play a C & D together, bits 0 and 2 are set so value = 1+4:5
      data = Serial.read();                                 // read the next byte which is velocity
      if (DEBUG_OUT) serialOut2.println(data,HEX);
      lastMidiNoteOn = millis();                            // save the time in ms for when we last got a midi note
      if (DEBUG_OUT) printNotes();
      digitalWrite(TOP_LED_PIN, HIGH);                           // turn on top LED to show midi activity
    }
  }
}

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
