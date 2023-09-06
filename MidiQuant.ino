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
#define bottomLED 12
#define middleLED 9
#define MIDI_NOTE_ON    0x90 
#define MIDI_NOTE_OFF   0x80
#define MIDI_POLYPHONIC 0xA0
#define MIDI_CC B0
#define DEBUG_OUT 0 //change to 1 to send debug messages via debug pin
                    //debug messages can be sent to 2nd Arduino which relays them to USB Serial      

Adafruit_MCP4725 dac; // creates an instance of the DAC
Adafruit_MCP4725 dac2; // creates an instance of the DAC

bool blinkBottom = false;
bool midiChanged = false;

byte data;

int noteBuffer = 1;
int tempNoteBuffer = 0;
int a1Raw = 0;
int lastA1Raw = 0;
int analogNote = 0;
int lastAnalogNote = 0;
int topSwitchStatus = 0;
int bottomSwitchStatus = 0;
int loopCounter = 0;

unsigned long lastMidiNoteOn = 0;
unsigned long waitPeriod = 2000;
unsigned long blinkLength = 150;
unsigned long minPauseDAC = 80;
unsigned long tempPause = 250;
unsigned long lastAnalog = 0;

float topVolt = 5.0; // Adjust this to your voltage at the DAC. Do not go lower than 5.0
float dacInterval = (((5.0 / topVolt) * 4095.0) / 60.0);

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
  for (int x = 0; x < 62; x++) {dac_notes[x] = round(x * dacInterval);}
  Serial.begin(31250);      //Midi In/Out 1
  if (DEBUG_OUT) serialOut2.begin(9600);
  if (DEBUG_OUT) serialOut2.println("DEBUG OUT");
  pinMode(topLED, OUTPUT);
  pinMode(bottomLED, OUTPUT);
  pinMode(middleLED, OUTPUT);
  pinMode(TOP_SWITCH, INPUT_PULLUP);
  pinMode(BOTTOM_SWITCH, INPUT_PULLUP);
  dac.begin(0x60);
  dac2.begin(0x61);
  delayMicroseconds(25);
  TWBR = ((F_CPU /400000l) - 16) / 2; // Change the i2c clock to 400KHz
  dac.setVoltage( 0 , false);
  for (int o = 0; 0 < 4; o++) // blink top led
  {
    digitalWrite(topLED, HIGH);
    delay(100);
    digitalWrite(topLED, LOW);
    delay(100);
  }
  
}

void loop() {
  //readSwitches();
  loopCounter++;
  readAnalogInput();
  checkMidiIn();
  if ((millis() - lastMidiNoteOn) > waitPeriod) digitalWrite(topLED, LOW);
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
          theQuantizedNote = 100;
          keepGoing = false;
        }
      }
  }
  return theQuantizedNote;
}
void readAnalogInput() {
  a1Raw = analogRead(A1);                                     // get the current analog in
  if ((abs(a1Raw - lastA1Raw))>5)                             // check if it is more than 5 steps away from last reading (debounce)     
  {
    analogNote = map(a1Raw, 0, 1024, 0, 61);                  // map the new reading from 0-60 (C to C 5 octaves)
    analogNote = returnClosestNote(analogNote);               // get the closest quantized note (going down)
    if (analogNote == 100) analogNote = lastAnalogNote;       // if there is no quantized note, the function returns a 100 so reset the analog note
    lastA1Raw = a1Raw;                                        // save the last ananlog reading
  }
  
  if ((analogNote != lastAnalogNote)||midiChanged)            // if we have a new note or if we have a new midi chord
  {
    midiChanged = false;                                               // reset midiChanged
    lastAnalogNote = analogNote;                                       // save lastAnalogNote
    dac.setVoltage( pgm_read_word(&(dac_notes[analogNote])), false);   // set the DAC voltage to the new note
    int shiftedNote = 0;                                               // shifted note for second DAC  
    if (analogNote > 55) shiftedNote = analogNote;                     // as long as the note isn't too high 
    else shiftedNote = analogNote + 5;                                 // add the shift
    dac2.setVoltage( pgm_read_word(&(dac_notes[shiftedNote])), false); // set the voltage for DAC 2
    lastAnalog = millis();                                             // save time of last analog change
  } 
}

void checkMidiIn(){
  if(Serial.available() > 0) {                              //if there is serial data waiting
    byte data = Serial.read();                              //read one byte
    if (DEBUG_OUT) serialOut2.println(data,HEX);            
    if (data = MIDI_NOTE_ON) {                              //if it is a MIDI Note On message
      if ((millis()- lastMidiNoteOn) > waitPeriod)          //our waiting period has elapsed
      {                                                     // so
        noteBuffer = 0;                                     // reset note buffer
        midiChanged = true;                                 // this lets the main loop know we have new midi input
        if (DEBUG_OUT) serialOut2.println("MIDI Changed");
      }
      data = Serial.read();                                 // read the next byte which is the note value
      if (DEBUG_OUT) serialOut2.println(data,HEX);
      int p = data % 12;                                    // get the modulo of the note i.e. C = 0, C# = 1, D = 2, etc...
      bitSet(noteBuffer, p);                                // set the bit. So if you play a C & D together, bits 0 and 2 are set so value = 1+4:5
      data = Serial.read();                                 // read the next byte which is velocity
      if (DEBUG_OUT) serialOut2.println(data,HEX);
      lastMidiNoteOn = millis();                            // save the time in ms for when we last got a midi note
      if (DEBUG_OUT) printNotes();
      digitalWrite(topLED, HIGH);                           // turn on top LED to show midi activity
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
