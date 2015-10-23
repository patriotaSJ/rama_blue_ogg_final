// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// define the pins used
#define RESET 9      // VS1053 reset pin (output)
#define CS 10        // VS1053 chip select pin (output)
#define DCS 8        // VS1053 Data/command select pin (output)
#define CARDCS A0     // Card chip select pin
#define DREQ A1       // VS1053 Data request, ideally an Interrupt pin
#define REC_BUTTON 7

//ambiente bluetooth
int led = 4; //led Rojo de prueba de conexión
String voltageValue[4] = {"result_LED","result_dht11","result_vs1053","result_almacenameinto"}; 
char inbyte = 0; //Char para leer el led
//fin ambiente bluetooth

Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(RESET, CS, DCS, DREQ, CARDCS);

File recording;  // the file we will save our recording to
#define RECBUFFSIZE 128  // 64 or 128 bytes.
uint8_t recording_buffer[RECBUFFSIZE];

void setup() {
  Serial.begin(9600);
  Serial.println("Adafruit VS1053 Ogg Recording Test");
  voltageValue[1]="Adafruit VS1053 Ogg Recording Test";
  sendAndroidValues();                    // *****Adafruit VS1053 Ogg Recording Test**** //
  
  //bluetooth
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  // vs1053
  // initialise the music player
  if (!musicPlayer.begin()) {
    Serial.println("VS1053 not found");
    while (1);  // don't do anything more
  }

  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
 
  if (!SD.begin(CARDCS)) {
    Serial.println("SD failed, or not present");
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  voltageValue[1]="SD OK!";
  sendAndroidValues();                    // ******SD OK!!*** //
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(10,10);
  
  // when the button is pressed, record!
  pinMode(REC_BUTTON, INPUT);
  digitalWrite(REC_BUTTON, HIGH);
  
  // load plugin from SD card! We'll use mono 44.1KHz, high quality
  if (! musicPlayer.prepareRecordOgg("v44k1q05.img")) {
     Serial.println("Couldn't load plugin!");
     while (1);    
  }
}

uint8_t isRecording = false;

void loop() {  
  if (!isRecording && !digitalRead(REC_BUTTON)) {
    Serial.println("Begin recording");
    voltageValue[1] = "Begin recording";
    sendAndroidValues();                    // ****Begin recording***** //
    isRecording = true;
    
    // Check if the file exists already
    char filename[15];
    strcpy(filename, "RECORD00.OGG");
    for (uint8_t i = 0; i < 100; i++) {
      filename[6] = '0' + i/10;
      filename[7] = '0' + i%10;
      // create if does not exist, do not open existing, write, sync after write
      if (! SD.exists(filename)) {
        break;
      }
    }
    Serial.print("Recording to "); Serial.println(filename);
    recording = SD.open(filename, FILE_WRITE);
    if (! recording) {
       Serial.println("Couldn't open file to record!");
       while (1);
    }
    musicPlayer.startRecordOgg(true); // use microphone (for linein, pass in 'false')
  }
  if (isRecording)
    saveRecordedData(isRecording);
  if (isRecording && !digitalRead(REC_BUTTON)) {
    Serial.println("End recording");
    voltageValue[1] = "End recording";
    sendAndroidValues();                    // ****End recording***** //
    musicPlayer.stopRecordOgg();
    isRecording = false;
    // flush all the data!
    saveRecordedData(isRecording);
    // close it up
    recording.close();
    delay(1000);
  }

   //bluetooth
  //when serial values have been received this will be true
  if (Serial.available() > 0)
  {
    inbyte = Serial.read();
    Serial.println(inbyte);
    if (inbyte == '2')
    {
      digitalWrite(led, LOW); //LED off
      //voltageValue[0] = 0;
    }
    if (inbyte == '1')
    {
      digitalWrite(led, HIGH); //LED on
      //voltageValue[0] = 1;
    }
  }//fin looop
  
}//fin loop()

uint16_t saveRecordedData(boolean isrecord) {
  uint16_t written = 0;
  
    // read how many words are waiting for us
  uint16_t wordswaiting = musicPlayer.recordedWordsWaiting();
  
  // try to process 256 words (512 bytes) at a time, for best speed
  while (wordswaiting > 256) {
    //Serial.print("Waiting: "); Serial.println(wordswaiting);
    // for example 128 bytes x 4 loops = 512 bytes
    for (int x=0; x < 512/RECBUFFSIZE; x++) {
      // fill the buffer!
      for (uint16_t addr=0; addr < RECBUFFSIZE; addr+=2) {
        uint16_t t = musicPlayer.recordedReadWord();
        //Serial.println(t, HEX);
        recording_buffer[addr] = t >> 8; 
        recording_buffer[addr+1] = t;
      }
      if (! recording.write(recording_buffer, RECBUFFSIZE)) {
            Serial.print("Couldn't write "); Serial.println(RECBUFFSIZE); 
            while (1);
      }
    }
    // flush 512 bytes at a time
    recording.flush();
    written += 256;
    wordswaiting -= 256;
  }
  
  wordswaiting = musicPlayer.recordedWordsWaiting();
  if (!isrecord) {
    Serial.print(wordswaiting); Serial.println(" remaining");
    // wrapping up the recording!
    uint16_t addr = 0;
    for (int x=0; x < wordswaiting-1; x++) {
      // fill the buffer!
      uint16_t t = musicPlayer.recordedReadWord();
      recording_buffer[addr] = t >> 8; 
      recording_buffer[addr+1] = t;
      if (addr > RECBUFFSIZE) {
          if (! recording.write(recording_buffer, RECBUFFSIZE)) {
                Serial.println("Couldn't write!");
                while (1);
          }
          recording.flush();
          addr = 0;
      }
    }
    if (addr != 0) {
      if (!recording.write(recording_buffer, addr)) {
        Serial.println("Couldn't write!"); while (1);
      }
      written += addr;
    }
    musicPlayer.sciRead(VS1053_SCI_AICTRL3);
    if (! (musicPlayer.sciRead(VS1053_SCI_AICTRL3) & _BV(2))) {
       recording.write(musicPlayer.recordedReadWord() & 0xFF);
       written++;
    }
    recording.flush();
  }

  return written;
}

//enviar valores del celular al arduino
void sendAndroidValues()
 {
  Serial.print('#'); //hay que poner # para el comienzo de los datos, así Android sabe que empieza el String de datos
  for(int k=0; k<4; k++)
  {
    Serial.print(voltageValue[k]);
    Serial.print('@'); //separamos los datos con el +, así no es más fácil debuggear la información que enviamos
  }
 Serial.print('~'); //con esto damos a conocer la finalización del String de datos
 Serial.println();
 delay(10);        //agregamos este delay para eliminar tramisiones faltantes
}
//----------------- fin bluetooth -------------------//


