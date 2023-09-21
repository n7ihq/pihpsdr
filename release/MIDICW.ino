/*
 * define four I/O lines for the four inputs
 */
 
#define CWLEFT_IO   0   // Dot paddle
#define CWRIGHT_IO  1   // Dash paddle or StraightKey
#define CWKEY_IO    2   // from Keyer: CW
#define PTT_IO      3   // from Keyer: PTT


/*
 * define MIDI channel four MIDI notes for these four events
 */

#define MIDI_CHANNEL 10
#define CWLEFT_NOTE  10
#define CWRIGHT_NOTE 11
#define CWKEY_NOTE   12
#define PTT_NOTE     13

/*
 * define a debouncing time for CWLEFT/CWRIGHT
 * and a (possibly shorter) one for CWKEY/PTT
 */

#define DEBOUNCE_PADDLE 15
#define DEBOUNCE_KEYER   5

#include "MIDIUSB.h"

/*
 * Debouncing timers
 */

static uint8_t CWLEFT_time;
static uint8_t CWRIGHT_time;
static uint8_t CWKEY_time;
static uint8_t PTT_time; 

/*
 * last reported states
 */
 
static uint8_t CWLEFT_last;
static uint8_t CWRIGHT_last; 
static uint8_t CWKEY_last;
static uint8_t PTT_last;

void setup() {
  pinMode(CWLEFT_IO,  INPUT_PULLUP);
  pinMode(CWRIGHT_IO, INPUT_PULLUP);
  pinMode(CWKEY_IO,   INPUT_PULLUP);
  pinMode(PTT_IO,     INPUT_PULLUP);
  CWLEFT_time =255;
  CWRIGHT_time=255;
  CWKEY_time  =255;
  PTT_time    =255;
  CWLEFT_last =0;
  CWRIGHT_last=0;
  CWKEY_last  =0;
  PTT_last    =0;
}

/*
 * Send MIDI NoteOn/Off message
 */
 
void SendOnOff(int note, int state) {
  midiEventPacket_t event;
  if (state) {
    // Note On
    event.header = 0x09;
    event.byte1  = 0x90 | MIDI_CHANNEL;
    event.byte2  = note;
    event.byte3  = 127;
  } else {
    // NoteOff, but we use NoteOn with velocity=0
    event.header = 0x09;
    event.byte1  = 0x90 | MIDI_CHANNEL;
    event.byte2  = note;
    event.byte3  = 0;
  }
  MidiUSB.sendMIDI(event);
  // this is CW, so flush each single event
  MidiUSB.flush(); 
}

/*
 * Process an input line.
 * Note that HIGH input means "inactive"
 * and LOW input means "active"
 * This function does noting during the debounce
 * settlement time.
 * After the settlement time, if the input state
 * has changed, the settlement time is reset and
 * a MIDI message (with the new state) sent.
 */
 
void process(uint8_t *time, uint8_t *oldstate,
             uint8_t ioline, uint8_t note,
             uint8_t debounce) {
  if (*time < 255) (*time)++;
  if (*time > debounce) {
    uint8_t newstate = !digitalRead(ioline);
    if (newstate != *oldstate) {  
      *time=0;
      *oldstate = newstate;
      SendOnOff(note, newstate);
    }
  }
}

void loop() {
  uint8_t state;
 /*
  * For each line, do nothing during the debounce settlement
  * time. After that, look at the input line, if it changed,
  * reset debounce timer and report value
  */

  process (&CWLEFT_time,
           &CWLEFT_last,
           CWLEFT_IO,
           CWLEFT_NOTE,
           DEBOUNCE_PADDLE);
           
  process (&CWRIGHT_time,
           &CWRIGHT_last,
            CWRIGHT_IO,
            CWRIGHT_NOTE,
            DEBOUNCE_PADDLE);
            
  process (&CWKEY_time,
           &CWKEY_last,
           CWKEY_IO,
           CWKEY_NOTE,
           DEBOUNCE_KEYER);
  
  process (&PTT_time,
           &PTT_last,
           PTT_IO,
           PTT_NOTE,
           DEBOUNCE_KEYER);


 /*
  * Execute loop() approximately (!) once per milli-second
  */
 delayMicroseconds(950);
}
