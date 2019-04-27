//removed fried gps and record time from switch on 

#include <Adafruit_VC0706.h>
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>  
#include  <OneWire.h>               
#include  <DallasTemperature.h>  

//Library Stuff
SoftwareSerial       ss_cam(5, 6);                       // camera - RX pin 5, TX pin 6
Adafruit_VC0706      cam = Adafruit_VC0706(&ss_cam);
OneWire              ds(2);                 
DallasTemperature    DS18B20(&ds);               
DeviceAddress        DALL_ADDR = {0x28, 0x2C, 0x58, 0x7C, 0x05, 0x00, 0x00, 0xF1};

//Define constants
const int CAM_PIN = 7;                // pin high to turn camera on, pin low to turn camera off
const int LED_PIN = 8;                // pin to control LED state
const int PIR_PIN = 9;                // pin to read PIR state
const int CSL_PIN = 10;               // pin to SD_card chip select pin
const int TMP_PIN = A0;               // pin to measure temperature

const long MILLIS_DAY = 86400000;     // 86400000 milliseconds in a day
const long MILLIS_HR  = 3600000;      // 3600000 milliseconds in an hour
const long MILLIS_MIN = 60000;        // 60000 milliseconds in a minute
const long MILLIS_SEC = 1000;         // 1000 milliseconds in a second

const unsigned int  CAM_SETTLE_DELAY = 1000;       // time to wait before taking pic after turning camera on - millis
const unsigned int  PIR_SETTLE_DELAY = 5000;      // time at start to allow pir to settle (and gps to start recieving data)
const unsigned long LED_DARK_DELAY = 120000;      // time after which led no longer lights (so as not to disturb any animals) - 

//Define variables
int           tmpC;                 // temperature in degs C * 10
unsigned long runtime;              // variable to hold current time in millis
char          img_fname[13];        // filename for saving image
char          log_fname[13];        // filename for log file
char          buff[80];             // char buffer for log file data
unsigned int  jpglen;               // size of jpg to download from camera
//======SETUP======

void setup() {

  Serial.begin(9600);                      // for debugging
  
//Set up pin modes
  pinMode(CSL_PIN, OUTPUT); 
  pinMode(PIR_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(CAM_PIN, OUTPUT);
  pinMode(TMP_PIN, INPUT);
  analogReference(EXTERNAL);
  
//Turn on LED and wait a bit
  digitalWrite(LED_PIN, HIGH);
  delay(PIR_SETTLE_DELAY);
  
//Camera setup
  cam.begin();                             //start camera
  cam.setImageSize(VC0706_640x480);        //set picture size
 
//SD card setup
  while (!SD.begin(CSL_PIN)); 

//Get log filename
  strcpy(log_fname, "log000.txt");
  for (int i = 0; i < 1000; i++) {
    log_fname[3] = '0' + i/100;
    log_fname[4] = '0' + (i%100)/10;
    log_fname[5] = '0' + (i%100)%10;
    if (! SD.exists(log_fname)) break;
  } 
      
//Tidy up
  digitalWrite(LED_PIN, LOW);              // turn off led
  digitalWrite(CAM_PIN, LOW);              //turn off camera
}

//======LOOP======

void loop() {

  //If movement is detected after a delay of 5secs....
  if (digitalRead(PIR_PIN)) { 
    
    //Turn camera on, wait for 1sec - v important
    digitalWrite(CAM_PIN, HIGH);
    runtime = millis();
    if (runtime < LED_DARK_DELAY) digitalWrite(LED_PIN, HIGH);  

    //Take picture - 1sec delay is important to allow cam to warm up
    delay(CAM_SETTLE_DELAY);
    while (!cam.takePicture());          // be careful - this could cause hang

    //Get temperature
    DS18B20.requestTemperatures();
    tmpC = DS18B20.getTempC(DALL_ADDR)*10; 
    //Serial.println(tmpC);
    
    //Get image filename
    //This approach is handy for selecting unique filename, but can get slow when there ar elots of files.
    strcpy(img_fname, "image000.jpg");
    for (int i = 0; i < 1000; i++) {
      img_fname[5] = '0' + i/100;
      img_fname[6] = '0' + (i%100)/10;
      img_fname[7] = '0' + (i%100)%10;
      if (! SD.exists(img_fname)) break;   // create if does not exist, do not open existing, write, sync after write
    } 
    File imgFile = SD.open(img_fname, FILE_WRITE);    // open file on disk

    //Download image and save to file
    jpglen = cam.frameLength();
    while (jpglen > 0) {
      if (runtime < LED_DARK_DELAY) digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      uint8_t *buffer;
      uint8_t bytesToRead = min(32, jpglen);    // read 32 bytes at a time;
      buffer = cam.readPicture(bytesToRead);
      imgFile.write(buffer, bytesToRead);
      jpglen -= bytesToRead;
    } 
    imgFile.close();              //close file on disk
    cam.resumeVideo();            //tell camera to pay attention

    //Convert runtime to useful units
    int days = runtime / MILLIS_DAY;                                
    int hrs = (runtime % MILLIS_DAY) / MILLIS_HR;              
    int mins = ((runtime % MILLIS_DAY) % MILLIS_HR) / MILLIS_MIN;
    int secs = (((runtime % MILLIS_DAY) % MILLIS_HR) % MILLIS_MIN) / MILLIS_SEC;
    
    //Write to the log file
    File logFile = SD.open(log_fname, FILE_WRITE);
    snprintf(buff, sizeof(buff), "%s, %02d:%02d:%02d:%02d, %i\n", img_fname, days, hrs, mins, secs, tmpC);
    Serial.write(buff);        // debug
    logFile.write(buff);          //write data to log file
    logFile.close();              //close file on disk

  } //close if

  //If movement is not detected (or we are within settle period....)
  else {
    //nothing is happening, so turn off camera & led
    digitalWrite(LED_PIN, LOW);
    digitalWrite(CAM_PIN, LOW);
  }

}

