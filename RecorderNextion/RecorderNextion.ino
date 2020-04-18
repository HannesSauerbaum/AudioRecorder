// Version von Martin (für eigenen Branch)
// Erfordert als zusätzliche Library RMSLevel (von mir)
//
// Editiertes Beispiel-File.
// Die Eingaben sollen ueber ein Nextion-Display gemacht werden.
// Es wird zum Ausfuehren noch die Nextion-Library benoetigt.
// Es werden 3 Knöpfe auf dem Nextion erstellt und die Funktionen
// durch Callbacks (ganz untern zu finden) aufgerufen.
// Neben dem Beispielprojekt wurde das folgende Video verwendet:
// https://www.youtube.com/watch?v=mdkUBB60HoI
//
// Record sound as raw data to a SD card, and play it back.
//
// Requires the audio shield:
//   http://www.pjrc.com/store/teensy3_audio.html
//
// This example code is in the public domain.

#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Nextion.h>
#include <RMSLevel.h>
#include <string>

// GUItool: begin automatically generated code
AudioInputI2S            i2s2 ;           //xy=105,63
AudioAnalyzePeak         peak1;          //xy=278,108
// AUDIO ANALYZE RMS HINZUFUEGEN
AudioAnalyzeRMS          rms_mono;
AudioRecordQueue         queue1;         //xy=281,63
AudioPlaySdRaw           playRaw1;       //xy=302,157
AudioOutputI2S           i2s1;           //xy=470,120
AudioConnection          patchCord1(i2s2, 0, queue1, 0);
AudioConnection          patchCord2(i2s2, 0, peak1, 0);
AudioConnection          patchCord3(playRaw1, 0, i2s1, 0);
AudioConnection          patchCord4(playRaw1, 0, i2s1, 1);
AudioConnection          patchCord6(i2s2, 0, i2s1, 0);
AudioConnection          patchCord7(i2s2, 0, i2s1, 1);
AudioConnection          patchCord5(i2s2, 0, rms_mono, 0);
AudioControlSGTL5000     sgtl5000_1;     //xy=265,212
IntervalTimer            TimerRec;
// GUItool: end automatically generated code

// Nextion Buttons: NexButton(int page, int objectID, string name)
NexButton buttonRecord = NexButton(0,3,"Record");
NexButton buttonStop = NexButton(0,2,"Stop");
NexButton buttonPlay = NexButton(0,1,"Play");
//NexButton buttonInput = NexButton(0,12,"Input");

/* ES MUSS EIN NEUER SLIDER ERSTELLT WERDEN (PAGE UND ID
   INDIVIDUELL ANPASSEN). DIE SKALA DES SLIDERS MUSS
   ZWISCHEN 0 und 63 LIEGEN!
   
   ES WURDE GANZ UNTEN EIN CALLBACK HINZUGEFÜGT.
   DER SLIDER WURDE DER NEX_LISTEN_LIST HINZUGEFÜGT.
   IN DER SETUP WURDE EIN POP CALLBACK ATTACHED.
   FALLS FEHLER AUFTRETEN, IST WOHL AN DIESEN STELLEN
   NACHZUGUCKEN.

   FUNKTIONSWEISE:
   DER GAIN DES INPUTS KANN ZWISCHEN 0 UND 63 DB EINGESTELLT
   WERDEN (LAUT DOC). DAZU WIRD AUF DAS sgtl5000_1-OBJEKT
   ZUGEGRIFFEN, WELCHES DIE FUNKTION micGain BESITZT.
   DIESE WIRD IN DER SLIDER-CALLBACK FUNKTION AUF DEN WERT
   DES SLIDERS GESETZT.
   */
NexSlider sliderGain = NexSlider(0,11,"Gain");
NexProgressBar ProgBarLevel = NexProgressBar(0,10,"Pegel");
NexText textTimer = NexText(0,7,"Timer");

// Liste mit Buttons
NexTouch *nex_listen_list[] =
{
  &buttonRecord,
  &buttonStop,
  &buttonPlay,
//  &uttonInput,
  &sliderGain,
  NULL
};

// which input on the audio shield will be used?
int inputMode = 1;
int myInput = AUDIO_INPUT_LINEIN;

// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  11
#define SDCARD_SCK_PIN   13

// Remember which mode we're doing
int mode = 0;  // 0=stopped, 1=recording, 2=playing

// The file where data is recorded
File frec;

// Initialising rms level meter
// Vielleicht noch 512 als Variable anlegen, so BlockLength oder so?
double tau = 0.125;
double fs = 44100/512; //divided by block length, because
// rmsMeter calculates level from 512 samples
RMSLevel rmsMeter(tau,fs);

uint32_t sliderValue = 50;

//variables for Timer
int counterSec = 0;
int counterMin = 0;
int counterHr = 0;
char TimerVal[] = "00:00:00";



void setup() {

  nexInit();
  Serial7.print("baud=115200");
  Serial7.write(0xff);
  Serial7.write(0xff);
  Serial7.write(0xff);
  Serial7.end();

  Serial7.begin(115200);
//  sliderGain.setValue(sliderValue);
  
  // Audio connections require memory, and the record queue
  // uses this memory to buffer incoming audio.
  AudioMemory(256);

  // Enable the audio shield, select input, and enable output
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(myInput);
  sgtl5000_1.volume(0.5);

  // Initialize the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here if no SD card, but print a message
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

  // Link Callbacks
  //buttonInput.attachPush(InputButtonCallback);
  buttonRecord.attachPush(RecordButtonCallback);
  buttonStop.attachPush(StopButtonCallback);
  buttonPlay.attachPush(PlayButtonCallback);
//  sliderGain.attachPop(sliderGainCallback);
}


void loop() {

  // Respond to button presses
  nexLoop(nex_listen_list);

  // If we're playing or recording, carry on...
  if (mode == 1) {
    continueRecording();
  }
  if (mode == 2) {
    continuePlaying();
  }

  // when using a microphone, continuously adjust gain
  if (myInput == AUDIO_INPUT_MIC) adjustMicLevel();
}


void changeInput(){
if (inputMode == 1){
  myInput = AUDIO_INPUT_LINEIN;
  Serial.println("Input: line");
}
if (inputMode == 2) {
  myInput = AUDIO_INPUT_MIC;
  Serial.println("Input: mic");
}
}

void startRecording() {
  Serial.println("startRecording");
  TimerRec.begin(TimerUpdate,1000000);
  if (SD.exists("RECORD.RAW")) {
    // The SD library writes new data to the end of the
    // file, so to start a new recording, the old file
    // must be deleted before new data is written.
    SD.remove("RECORD.RAW");
  }
  frec = SD.open("RECORD.RAW", FILE_WRITE);
  if (frec) {
    queue1.begin();
    mode = 1;
  }
}

void continueRecording() {
  if (queue1.available() >= 2) {

    //Serial.println(rmsMeter.updateRMS(double(rms_mono.read())));
    uint32_t ProgBarVal = uint32_t(100*(1-(rmsMeter.updateRMS(double(rms_mono.read()))/-80)));
    //Serial.println(ProgBarVal);
    ProgBarLevel.setValue(ProgBarVal); 
    byte buffer[512];
    // Fetch 2 blocks from the audio library and copy
    // into a 512 byte buffer.  The Arduino SD library
    // is most efficient when full 512 byte sector size
    // writes are used.
    memcpy(buffer, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    memcpy(buffer+256, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    // write all 512 bytes to the SD card
    //elapsedMicros usec = 0;
    frec.write(buffer, 512);
    // Uncomment these lines to see how long SD writes
    // are taking.  A pair of audio blocks arrives every
    // 5802 microseconds, so hopefully most of the writes
    // take well under 5802 us.  Some will take more, as
    // the SD library also must write to the FAT tables
    // and the SD card controller manages media erase and
    // wear leveling.  The queue1 object can buffer
    // approximately 301700 us of audio, to allow time
    // for occasional high SD card latency, as long as
    // the average write time is under 5802 us.
    //Serial.print("SD write, us=");
    //Serial.println(usec);
  }
}

void stopRecording() {
  Serial.println("stopRecording");
  TimerRec.end();
  resetTimer();
  queue1.end();
  if (mode == 1) {
    while (queue1.available() > 0) {
      frec.write((byte*)queue1.readBuffer(), 256);
      queue1.freeBuffer();
    }
    frec.close();
  }
  mode = 0;
}


void startPlaying() {
  Serial.println("startPlaying");
  playRaw1.play("RECORD.RAW");
  mode = 2;
}

void continuePlaying() {
  if (!playRaw1.isPlaying()) {
    playRaw1.stop();
    mode = 0;
  }
}

void stopPlaying() {
  Serial.println("stopPlaying");
  if (mode == 2) playRaw1.stop();
  mode = 0;
}

void adjustMicLevel() {
  // TODO: read the peak1 object and adjust sgtl5000_1.micGain()
  // if anyone gets this working, please submit a github pull request :-)
}

void TimerUpdate(){
  counterSec += 1;
  if(counterSec >= 60){
    counterSec = 0;
    counterMin += 1;
    if (counterMin >= 60){
      counterMin = 0;
      counterHr += 1;
    }
  } 
  TimerVal[0] = char(int(floor(counterHr/10))+'0');
  TimerVal[1] = char(int(counterHr%10)+'0');
  TimerVal[3] = char(int(floor(counterMin/10))+'0');
  TimerVal[4] = char(int(counterMin%10)+'0');
  TimerVal[6] = char(int(floor(counterSec/10))+'0');
  TimerVal[7] = char(int(counterSec%10)+'0');
  

  Serial.println(TimerVal);
  textTimer.setText(TimerVal);

}

void resetTimer(){
  counterSec = 0;
  counterMin = 0;
  counterHr = 0;
}

// PUSH CALLBACKS
void RecordButtonCallback(void *ptr)
{;
  Serial7.print("Record");
  if (mode == 2) stopPlaying();
  if (mode == 0) 
  {
    startRecording();
  }
  
}

void StopButtonCallback(void *ptr)
{
  Serial.println("Stop Button Press");
  if (mode == 1) stopRecording();
  if (mode == 2) stopPlaying();
}

void PlayButtonCallback(void *ptr)
{
  Serial.println("Play Button Press");
  if (mode == 1) stopRecording();
  if (mode == 0) startPlaying();
}

//void InputButtonCallback(void *ptr)
//{
//  Serial.println("Input Button Press");
//  if (inputMode == 1)
//  {
//    changeInput();
//    inputMode = 2;
//    return;
//  }
//  if (inputMode == 2)
//  {
//    changeInput();
//    inputMode = 1;
//    return;
//  }
//  sgtl5000_1.inputSelect(myInput);
//}

void sliderGainCallback(void *ptr)
{
  sgtl5000_1.micGain(sliderGain.getValue(&sliderValue));
}
