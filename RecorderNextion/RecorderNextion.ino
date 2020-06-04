// Master-Version
// Dieses File enthält bislang nur die Grundfunktionen.
// Sobald ein weiteres Feature effizient genug ausgearbeitet worden ist (idealerweise outsourcen in andere Files),
// kann es hier eingebaut werden. Am besten sollten aber alle davon wissen, was hier eingebaut wird, also
// idealerweise erst in der Besprechung erklären, wie das Feature zu nutzen ist.
//
// Es handelt sich um ein editiertes Beispiel-File!
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

// eigene Files
#include <RMSLevel.h>
#include <AutomaticGainControl.h>

//-----------------------------------------------------------------------------------------
// AUDIO
//-----------------------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------------------
// TIMING
//-----------------------------------------------------------------------------------------
elapsedMillis            TimerLvl;

//-----------------------------------------------------------------------------------------
//BUTTONS
//-----------------------------------------------------------------------------------------
NexButton buttonRecord = NexButton(0,3,"Record");
NexButton buttonStop = NexButton(0,2,"Stop");
NexButton buttonPlay = NexButton(0,1,"Play");
NexProgressBar ProgBarLevel = NexProgressBar(0,10,"Pegel");
NexButton buttonCheckLvl = NexButton(0,12,"CheckLvl");
NexButton buttonAGC = NexButton(0, 18, "AGC");
NexSlider sliderVolume = NexSlider(0, 15, "Volume");
NexSlider sliderGain = NexSlider(0, 11, "Gain");
NexSlider sliderAGChang = NexSlider(2, 26, "AGChang");
NexSlider sliderAGCslopeInc = NexSlider(2, 25, "AGCslopeInc");
NexSlider sliderAGCslopeDec = NexSlider(2, 24, "AGCslopeDec");
NexSlider sliderAGCtresh = NexSlider(2, 7, "AGCtresh");

NexTouch *nex_listen_list[] =
{
  &buttonRecord,
  &buttonStop,
  &buttonPlay,
  &buttonCheckLvl,
  &buttonAGC,
  &sliderVolume,
  &sliderGain,
  &sliderAGChang,
  &sliderAGCslopeInc,
  &sliderAGCslopeDec,
  &sliderAGCtresh,
  NULL
};

int myInput = AUDIO_INPUT_LINEIN;

#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  11
#define SDCARD_SCK_PIN   13

int mode = 0;  // 0=stopped, 1=recording, 2=playing

File frec;

//-----------------------------------------------------------------------------------------
// RMS-METER
//-----------------------------------------------------------------------------------------
double tau = 0.125;
double f_refresh = 4;
RMSLevel rmsMeter(tau,f_refresh);
bool checkLvl = false;
int dispDelay = 1000/f_refresh;

//-----------------------------------------------------------------------------------------
// Automatic Gain Control
//-----------------------------------------------------------------------------------------
int AGCMode = 1;                   
int AGChangtime = 500;               
double AGCtresh = 0.95;             
double AGCslopeIncrease  = 0.1;         
double AGCslopeDecrease  = 1.0;      
double peak;                         
elapsedMillis MilliSec=0;
double AGCvalue;

//-----------------------------------------------------------------------------------------
// SETUP
//-----------------------------------------------------------------------------------------
void setup() {

  nexInit();
  Serial7.print("baud=115200");
  Serial7.write(0xff);
  Serial7.write(0xff);
  Serial7.write(0xff);
  Serial7.end();
  Serial7.begin(115200);
  
  AudioMemory(256);

  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(myInput);
  sgtl5000_1.volume(0.5);

  // Initialize the SD card
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

  // Link Button Callbacks
  buttonRecord.attachPush(RecordButtonCallback);
  buttonStop.attachPush(StopButtonCallback);
  buttonPlay.attachPush(PlayButtonCallback);
  buttonCheckLvl.attachPush(buttonCheckLvlCallback);
  buttonAGC.attachPush(AGCButtonCallback);
  sliderVolume.attachPop(sliderVolumeCallback);
  sliderGain.attachPop(sliderGainCallback);
  sliderAGChang.attachPop(sliderAGChangCallback);
  sliderAGCslopeInc.attachPop(sliderAGCslopeIncCallback);
  sliderAGCslopeDec.attachPop(sliderAGCslopeDecCallback);
  sliderAGCtresh.attachPop(sliderAGCtreshCallback);
}

//-----------------------------------------------------------------------------------------
// LOOP
//-----------------------------------------------------------------------------------------
void loop() {
  nexLoop(nex_listen_list);

  if (mode == 1) {
    continueRecording();
  }
  if (mode == 2) {
    continuePlaying();
  }
  if (checkLvl){
    if(TimerLvl >=dispDelay){
      displayLvl();
      TimerLvl-=dispDelay;
    }
  }
  if (AGCMode == 2){
  AutoGain();
  }
}

//-----------------------------------------------------------------------------------------
// BASIS FUNKTIONEN
//-----------------------------------------------------------------------------------------
void startRecording() {
  Serial.println("startRecording");
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
    checkLvl = true;
  }
}

void continueRecording() {
  if (queue1.available() >= 2) {
    byte buffer[512];
    memcpy(buffer, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    memcpy(buffer+256, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    frec.write(buffer, 512);                          // Addiert in jedem Durchlauf 512 Bytes   
  }
}

void stopRecording() {
  Serial.println("stopRecording");
  queue1.end();
  if (mode == 1) {
    while (queue1.available() > 0) {
      frec.write((byte*)queue1.readBuffer(), 256);
      queue1.freeBuffer();
    }
  }
  mode = 0;
  checkLvl = false;
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

void AutoGain() 
{
  AutomaticGainControl agc(AGChangtime,AGCtresh,AGCslopeIncrease,AGCslopeDecrease,peak);
  if (MilliSec > 2)
  {
    peak = peak1.read(); 
    double AGCgain = double(agc.AGC(double(AGCvalue)));
    sgtl5000_1.micGain(AGCgain);
    MilliSec = 0;
  }
}

//-----------------------------------------------------------------------------------------
// ANZEIGE FUNKTIONEN
//-----------------------------------------------------------------------------------------
void displayLvl() {
  uint32_t ProgBarVal = uint32_t(100*(1-(rmsMeter.updateRMS(double(rms_mono.read()))/-80)));
  Serial.println(ProgBarVal);
  ProgBarLevel.setValue(ProgBarVal); 
}

//-----------------------------------------------------------------------------------------
// PUSH CALLBACKS
//-----------------------------------------------------------------------------------------
// für Basis-Funktionen
void RecordButtonCallback(void *ptr)
{
  Serial.print("Record");
  if (mode == 1) {
    stopRecording();
    //break;
  }
  if (mode == 0) 
  {
    startRecording();
    TimerLvl=0;
  }  
}

void StopButtonCallback(void *ptr)
{
  Serial.println("Stop Button Press");
  if (mode == 1) stopRecording();
  if (mode == 2) stopPlaying();
  checkLvl = false;
}

void PlayButtonCallback(void *ptr)
{
  Serial.println("Play Button Press");
  if (mode == 1) stopRecording();
  if (mode == 0) startPlaying();
}

void buttonCheckLvlCallback(void *ptr)
{
  if (!checkLvl)
  {
    Serial.println("Checking Level");
    TimerLvl = 0;
    checkLvl = true;
  }
  else
  {
    Serial.println("Stop Checking Level");
    checkLvl = false;
  }
}  

void sliderVolumeCallback(void *ptr)
{
  uint32_t slideValue = 0;
  sliderVolume.getValue(&slideValue);
  double volumeValue = slideValue /100.0;
  sgtl5000_1.volume(volumeValue);
}

void sliderGainCallback(void *ptr)
{
  uint32_t slideGainValue = 0;
  sliderGain.getValue(&slideGainValue);
  double gainValue = slideGainValue; 
  sgtl5000_1.micGain(gainValue);
}

void AGCButtonCallback(void *ptr)
{
  Serial.println("AGC Button Press");
    if (AGCMode == 1)
      {
      AGCMode = 2;                             
      Serial.println("AGC On");
      }      
    else 
      {
      AGCMode = 1;
      Serial.println("AGC Off");                 
      }       
}

void sliderAGChangCallback(void *ptr)
{
  uint32_t hangSetting = 0;
  sliderAGChang.getValue(&hangSetting);
  if (hangSetting == 1)
  {
    AGChangtime = 0;           //Off
    Serial.println("AGC Hangtime Off");
  }

  if (hangSetting == 2)
  {
    AGChangtime = 250;         //500ms
    Serial.println("AGC Hangtime 500ms");
  }

  if (hangSetting == 3)
  {
    AGChangtime = 500;         //1000ms
    Serial.println("AGC Hangtime 1000ms");
  }

  if (hangSetting == 4)
  {
    AGChangtime = 1000;        //2000ms
    Serial.println("AGC Hangtime 2000ms");
  }
}

void sliderAGCslopeIncCallback(void *ptr)
{
  uint32_t slopeIncSetting = 0;
  sliderAGCslopeInc.getValue(&slopeIncSetting);
  if (slopeIncSetting == 1)
  {
    AGCslopeIncrease = 0.05;        
    Serial.println("AGCslopeIncrease = 0.05");
  }

  if (slopeIncSetting == 2)
  {
    AGCslopeIncrease = 0.1;         
    Serial.println("AGCslopeIncrease = 0.1");
  }

  if (slopeIncSetting == 3)
  {
    AGCslopeIncrease = 0.5;         
    Serial.println("AGCslopeIncrease = 0.5");
  }

  if (slopeIncSetting == 4)
  {
    AGCslopeIncrease = 1.0;        
    Serial.println("AGCslopeIncrease = 1");
  }
}
void sliderAGCslopeDecCallback(void *ptr)
{
  uint32_t slopeDecSetting = 0;
  sliderAGCslopeDec.getValue(&slopeDecSetting);
  if (slopeDecSetting == 1)
  {
    AGCslopeDecrease = 0.1;         
    Serial.println("AGCslopeDecrease = 0.1");
  }

  if (slopeDecSetting == 2)
  {
    AGCslopeDecrease = 0.5;         
    Serial.println("AGCslopeDecrease = 0.5");
  }

  if (slopeDecSetting == 3)
  {
    AGCslopeDecrease = 1.0;         
    Serial.println("AGCslopeDecrease = 1.0");
  }

  if (slopeDecSetting == 4)
  {
    AGCslopeDecrease = 2.0;        
    Serial.println("AGCslopeDecrease = 2.0");
  }
}
void sliderAGCtreshCallback(void *ptr)
{
  uint32_t treshSetting = 0;
  sliderAGCtresh.getValue(&treshSetting);
  double treshSet = (treshSetting + 60);
  AGCtresh = treshSet/100;
  Serial.println("AGCtresh = ");
  Serial.println(AGCtresh);
}
