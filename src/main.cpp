#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
#include <Adafruit_PCD8544.h>
#include <Adafruit_GFX.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())
#define BUTTON_PIN_BITMASK 0x200000004 // GMPIO33 and GPIO2
#define CHECK 2
#define STP_STRT 33
#define buzer 25
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60 * 5        /* Time ESP32 will go to sleep (in seconds) */
#define RED 18
#define GREEN 5 
#define LCDON 34
#define RTCON 19

const int oneWireBus = 4;
const int battery_channel = 35;

//battery variables 
float adc_value = 0.0;
float voltage_value = 0.0;
float display_btryvoltage = 0.0;

//Times
const long intervalFile = 3000;  //millisecond
unsigned long previousMillisFile = 0;
unsigned long currentMillisFile;

const long intervalsleep = 1000 * 2;  //millisecond deep sleep for 60 seconds
unsigned long previousMillisleep = 0;
unsigned long currentMillisleep;

const int LONG_PRESS_TIME  = 150; // 1000 milliseconds

//Warning millis
unsigned long warning_checktime = 0;
unsigned long warning_delay = 1000 * 10;

//Logging millis
unsigned int command_timeout = 0; 
unsigned int command_delay = 1000 * 3; // 3s

unsigned int checkmode_timemout = 0;
unsigned int checkmode_delay = 1000 * 10;

// setting PWM properties
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// long press btn variable
unsigned long pressedTime  = 0;
byte lastState = HIGH;  // the previous state from the input pin
byte stp_strt_currentState;     // the current reading from the input pin
byte check_currentState;
bool isPressing = false;
bool isLongDetected = false;
int x=0, z=0, t=0;
bool checkmodeflag = false;

//BLE variables
int man_code = 0x02E5;//manufacturer code (0x02E5 for Espressif)
int p=0;

RTC_DATA_ATTR bool flagBTN = false;
RTC_DATA_ATTR int flag = 2;
RTC_DATA_ATTR bool timeflag = false;

RTC_DATA_ATTR bool memoflag = false;
bool flagbreak = false;
bool flagalarm = false;

int ii = 0;

String MAC ="";

File templog;
File Dangerlog;
File serial;
BLEAdvertisementData advert;
BLEAdvertising *pAdvertising;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
RTC_DS1307 rtc;
Adafruit_PCD8544 display = Adafruit_PCD8544(14, 13, 27, 15, 26);

String YEAR="";
String MONTH="";
String DAY="";
String SEC="";
String MIN="";
String HOUR="";
String DATE__ ="";
bool timeoutflag = false;

String ID = "NXT1" + String(ESP_getChipId(),HEX);
//TODO: UPLOAD File systems before uploading code
//TODO: Remove all Serial.println(""); Very important !!!!!!
//TODO: Discuss LED displaying ........
void setup() 
{
  Serial.begin(115200);
  Serial.println("[F] Firmware V1 LOGGER-NXT");
  pinMode(RED, OUTPUT);
  pinMode(GREEN,OUTPUT);
  digitalWrite(GREEN, HIGH); //turn off GREEN LED
  digitalWrite(RED, HIGH); //turn off RED LED

  digitalWrite(GREEN, LOW); //turn on GREEN LED
  
  BLEDevice::init("NXT");
  pinMode(LCDON, OUTPUT);
  pinMode(RTCON, OUTPUT);
  BLEAddress mAddress = BLEDevice::getAddress();
  Serial.printf("[I] Device address: %s \n", mAddress.toString().c_str());

  digitalWrite(RTCON, HIGH);

  previousMillisFile = millis();
  previousMillisleep = millis();  
  digitalWrite(LCDON, HIGH);
  display.begin();
  display.setContrast(60);
  display.setTextColor(BLACK);
  display.clearDisplay();
  display.display();

  if (! rtc.begin()) 
  {
    Serial.println("[W] Couldn't find RTC");
  }
  if(!SPIFFS.begin(true))
  {
    Serial.println("Error while mounting SPIFFS");
  }
  pinMode(STP_STRT, INPUT_PULLUP);
  pinMode(CHECK, INPUT_PULLUP);
  sensors.begin();
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(buzer, ledChannel);
  if(timeflag == false)
  {
    // TODO : uncoment first upload and comment 2nd upload
    //rtc.adjust(DateTime(__DATE__, __TIME__));
    timeflag = true;
  }
}

float battery_lvl()
{
  adc_value = analogRead(battery_channel);
  voltage_value = (2.21 * adc_value * 3.33)/4095.0;
  return voltage_value;
}


/********************** BLE sending function *******************************/
void setManData(String c, int c_size, BLEAdvertisementData &adv, int m_code)
{ 
  adv.setManufacturerData(c.c_str());
  //Serial.print("data seny by BLE: ");Serial.println(s.c_str());
}

//Function that logs Date/Time from RTC to file
void loggingDisp_date()
{ 
  DateTime now = rtc.now();
  templog = SPIFFS.open("/data.txt",FILE_APPEND);
  if(templog)
  {
    templog.print(now.year(), DEC); // DATE
    templog.print("/");
    templog.print(now.month(), DEC);
    templog.print("/");
    templog.print(now.day(), DEC);
    templog.print(" | "); // TIME
    templog.print(now.hour(), DEC);
    templog.print(':');
    templog.print(now.minute(), DEC);
    templog.print(':');
    templog.print(now.second(), DEC);
    templog.print(" |   ");
  }
  templog.close();
}

//Function that log temperature values on file
void loggingDisp_Temp()
{
  sensors.requestTemperatures(); 
  float temperatureC = roundf(sensors.getTempCByIndex(0)*100)/100;

  // store in file 
  templog = SPIFFS.open("/data.txt",FILE_APPEND);
  if (templog) 
  {
    templog.println(temperatureC);
    //templog.println("°C");
  }
  templog.close();

  /**SEND temperature to scanner BLE*/
  String a = String(temperatureC, 2);
  advert.setManufacturerData(a.c_str());
  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();
  advert.setName(ID.c_str());
  pAdvertising->setAdvertisementData(advert);
  pAdvertising->start();
  //BLEAdvertisementData scan_response;
  /*setManData(a, a.length() , advert, man_code);
  pAdvertising->stop();
  pAdvertising->setScanResponseData(advert);
  pAdvertising->start();*/
}


// Function that display main icons on the PCD display
void Display()
{
  DateTime now = rtc.now();
  display_btryvoltage = battery_lvl();
  unsigned int usedBytes = SPIFFS.usedBytes();
  sensors.requestTemperatures(); 
  float temperatureC = roundf(sensors.getTempCByIndex(0)*100)/100;
  display.setTextColor(BLACK);
  display.setTextSize(1);
  display.setCursor(72,0);
  display.println(now.minute(), DEC);
  display.setCursor(68,0);
  display.println(":");
  display.setTextSize(1);
  display.setCursor(58,0);
  display.println(now.hour(), DEC);
  
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println(now.day(), DEC);
  
  display.setTextSize(1);
  display.setCursor(10,0);
  display.println("/");
  
  display.setTextSize(1);
  display.setCursor(15,0);
  display.println(now.month(), DEC);

  Serial.print("[d] Battery LVL: ");Serial.println(battery_lvl());
  //Battry icon
  if (display_btryvoltage >= 5.00)
  {
    display.drawRect(39,0,13,7 ,BLACK);
    display.drawRect(52,2,2,3,BLACK );
    display.fillRect(41,2,9,3,BLACK);
  } //>  558579 && usedBytes <=
  else if (display_btryvoltage < 5.00 && display_btryvoltage >= 4.50)
  {
    display.drawRect(39,0,13,7 ,BLACK);
    display.drawRect(52,2,2,3,BLACK );
    display.fillRect(41,2,7,3,BLACK);
  }
  else if (display_btryvoltage < 4.50 && display_btryvoltage >= 4.00)
  {
    display.drawRect(39,0,13,7 ,BLACK);
    display.drawRect(52,2,2,3,BLACK );
    display.fillRect(41,2,5,3,BLACK);
  }
  else if (display_btryvoltage < 4.00 && display_btryvoltage >= 3.5)
  {
    display.drawRect(39,0,13,7 ,BLACK);
    display.drawRect(52,2,2,3,BLACK );
    display.fillRect(41,2,3,3,BLACK);
  }
  else if (display_btryvoltage < 3.5)
  {
    display.drawRect(39,0,13,7 ,BLACK);
    display.drawRect(52,2,2,3,BLACK );
    display.fillRect(41,2,1,3,BLACK);
  }
  
  //display.display();
  
  //display temp logo
  display.drawRect(3,11,5,14,BLACK);
  display.drawCircle(5,29,5,BLACK);
  display.drawCircle(5,29,4,BLACK);
  display.fillCircle(5,29,2,BLACK);
  display.drawLine(5,11,5,29,BLACK);
  display.fillCircle(5,11,2,BLACK);
  display.drawFastHLine(7,13,5,BLACK);
  display.drawFastHLine(7,16,4,BLACK);
  display.drawFastHLine(7,19,4,BLACK);
  display.drawFastHLine(7,22,5,BLACK);
  //End of logo
  
  display.setTextSize(2);
  display.setCursor(13,15);
  display.printf("%.2f",temperatureC);
  
  display.setTextSize(2);
  display.setCursor(60,15);
  display.print(" C");

  display.setTextSize(1);
  display.setCursor(20,38);
  display.print("Memo:"); 

  display.setTextSize(1);
  display.setCursor(0,40);
  display.drawRect(52,38,32,6,BLACK);
  //Serial.println(usedBytes);
  
  if(usedBytes<=558579) 
  {
    display.fillRect(55,41,5,5,BLACK);
  }
  else if (usedBytes > 558579 && usedBytes <= 830080 )  
  {
    display.fillRect(55,41,5,5,BLACK);
    display.fillRect(61,41,5,5,BLACK);
  }

  else if (usedBytes > 830080 && usedBytes <= 1395120 )  
  {
    display.fillRect(55,41,5,5,BLACK);
    display.fillRect(61,41,5,5,BLACK);
    display.fillRect(67,41,5,5,BLACK);
  }
  else if (usedBytes>1395120 && usedBytes <= 1700000)
  {
    display.fillRect(55,41,5,5,BLACK);
    display.fillRect(61,41,5,5,BLACK);
    display.fillRect(67,41,5,5,BLACK);
    display.fillRect(73,41,5,5,BLACK);
    display.setTextSize(1);
    display.setCursor(0,38);
    display.print("Full:");
  }

  display.display();
  display.clearDisplay(); 
}

/***************loop*********************/
void loop()
{
  Serial.println("inside loop");
  BLEAddress mAddress = BLEDevice::getAddress();
  unsigned int totalBytes = SPIFFS.totalBytes();
  unsigned int usedBytes = SPIFFS.usedBytes();
  currentMillisleep = millis();
  currentMillisFile = millis();

  stp_strt_currentState = digitalRead(STP_STRT);
  check_currentState = digitalRead(CHECK);

  if(check_currentState == LOW)
  {
    flagBTN = true;
    Serial.println("[i] Check btn pressed");
    checkmode_timemout = millis();
  }
 //check button is pressed
  if(flagBTN == true )
  {
    if(checkmodeflag == false) // WHY this : this to display check mode just once for 500 ms
    {
      String RAW = mAddress.toString().c_str();
      display.clearDisplay();
      display.display();
      display.setTextSize(2);
      display.setCursor(10,0);
      display.print("INFO");
      display.setTextSize(1);
      display.setCursor(15,25);
      display.print(RAW.substring(0,8));
      display.setTextSize(1);
      display.setCursor(15,35);
      display.print(RAW.substring(9, RAW.length()));

      display.display();
      ledcWriteTone(ledChannel,4020);
      delay(100);
      ledcWriteTone(ledChannel,0);
      delay(100);
      ledcWriteTone(ledChannel,4005);
      delay(100);
      ledcWriteTone(ledChannel,0);
      delay(100);
      ledcWriteTone(ledChannel,4005);
      delay(100);
      ledcWriteTone(ledChannel,0);
      delay(1000);
      display.clearDisplay();
      display.display();
      checkmodeflag = true;
    }
    Display();
  }

  /*************** TODO: modifying this logic***************/
  /**********BTN start long press ******************/

  if(lastState == HIGH && stp_strt_currentState == LOW)
  {        // button is pressed
    Serial.println("[d] STP is pressed ");
    pressedTime = millis();
    isPressing = true;
    isLongDetected = false;
  }
  else if(lastState == LOW && stp_strt_currentState == HIGH) 
  {  // button is released
    isPressing = false;
    Serial.println("[d] STP is released");
  }
  lastState = stp_strt_currentState;

  if(isPressing == true && isLongDetected == false) 
  {
    long pressDuration = millis() - pressedTime;
    if( pressDuration > LONG_PRESS_TIME ) 
    {
      Serial.println("[d] A long press is detected");
      isLongDetected = true;
      flag = 0;
    }
  }
  
  //enter sleep mode if btn stop clicked for 150 ms
  if(flag == 0)
  {
    display.clearDisplay(); 
    display.display();
    flag = 1;
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0); //1 = High, 0 = Low
    //Serial.println("Going to sleep now");
    ledcWriteTone(ledChannel,4005);
    delay(100);
    ledcWriteTone(ledChannel,0);
    delay(200);
    ledcWriteTone(ledChannel,4005);
    delay(100);
    ledcWriteTone(ledChannel,0);
    display.setTextSize(2);
    display.setCursor(10,0);
    display.print("GOOD");

    display.setTextSize(2);
    display.setCursor(0,20);
    display.print("BYE..!");
    digitalWrite(GREEN, HIGH);
    digitalWrite(RED, HIGH);
    display.display();
    Serial.println("[d] Good BYE");
    flagBTN = false;
    delay(2000);
    display.clearDisplay();
    display.display();
    esp_deep_sleep_start();
  }
  
  //if(usedBytes > 418417 && usedBytes <=1800000)
  //check if we are at the limits of memory
  if(usedBytes > 1750000 && usedBytes <=1800000)
  {
    memoflag = true;
  }

  sensors.requestTemperatures(); 
  float temperatureC = roundf(sensors.getTempCByIndex(0)*100)/100;
 
  warning_checktime = millis();  //TODO: check this code 'millis()'
  
  while ((temperatureC>30.00) && stp_strt_currentState == HIGH && (Serial.available() <= 0) && !flagbreak)
  //while ((temperatureC>8.00 || temperatureC<2.00) && (Serial.available() <= 0) && stp_strt_currentState == HIGH && !flagbreak)
  //while ((temperatureC<= -127.00) && stp_strt_currentState == LOW && (Serial.available() <= 0) && !flagbreak)
  {
    display_btryvoltage = battery_lvl();
    Serial.println("[d] inside warning");
    flagalarm = true;
    if(millis() >  warning_checktime + warning_delay)
    {
      // timer update
      warning_checktime = millis();
      flagbreak = true;
    }
    stp_strt_currentState = digitalRead(STP_STRT);
    if(stp_strt_currentState == LOW)
    {
      currentMillisleep = millis();
      previousMillisleep = millis(); 
      Serial.println("[i] breaking now warning while");
      break;
    }
    DateTime now = rtc.now();
    sensors.requestTemperatures(); 
    temperatureC = sensors.getTempCByIndex(0);

    display.setTextSize(1);
    display.setCursor(72,0);
    display.println(now.minute(), DEC);
    display.setCursor(68,0);
    display.println(":");
    display.setTextSize(1);
    display.setCursor(58,0);
    display.println(now.hour(), DEC);

    display.setTextSize(1);
    display.setCursor(0,0);
    display.println(now.day(), DEC);

    display.setTextSize(1);
    display.setCursor(10,0);
    display.println("/");

    display.setTextSize(1);
    display.setCursor(15,0);
    display.println(now.month(), DEC);

    Serial.print("[d] Warning Battery LVL: ");Serial.println(display_btryvoltage);
    //Battry icon
    if (display_btryvoltage >= 5.00)
    {
      display.drawRect(39,0,13,7 ,BLACK);
      display.drawRect(52,2,2,3,BLACK );
      display.fillRect(41,2,9,3,BLACK);
    } //>  558579 && usedBytes <=
    else if (display_btryvoltage < 5.00 && display_btryvoltage >= 4.50)
    {
      display.drawRect(39,0,13,7 ,BLACK);
      display.drawRect(52,2,2,3,BLACK );
      display.fillRect(41,2,7,3,BLACK);
    }
    else if (display_btryvoltage < 4.50 && display_btryvoltage >= 4.00)
    {
      display.drawRect(39,0,13,7 ,BLACK);
      display.drawRect(52,2,2,3,BLACK );
      display.fillRect(41,2,5,3,BLACK);
    }
    else if (display_btryvoltage < 4.00 && display_btryvoltage >= 3.5)
    {
      display.drawRect(39,0,13,7 ,BLACK);
      display.drawRect(52,2,2,3,BLACK );
      display.fillRect(41,2,3,3,BLACK);
    }
    else if (display_btryvoltage < 3.5)
    {
      display.drawRect(39,0,13,7 ,BLACK);
      display.drawRect(52,2,2,3,BLACK );
      display.fillRect(41,2,1,3,BLACK);
    }
    display.setTextSize(1);
    display.setCursor(0,10);
    display.print("T ");
    display.setTextSize(2);
    display.setCursor(10,10);
    display.printf("%.2f",temperatureC);
    display.setTextSize(2);
    display.setCursor(60,10);
    display.print(" C");
    display.drawTriangle(43, 25, 30, 42, 56, 42, BLACK);
    display.fillTriangle(43, 25, 30, 42, 56, 42, BLACK);
    display.drawLine(42,30,42,35,WHITE);
    display.drawLine(43,30,43,35,WHITE);
    display.drawLine(44,30,44,35,WHITE);
    display.drawLine(42,38,42,39,WHITE);
    display.drawLine(43,38,43,39,WHITE);
    display.drawLine(44,38,44,39,WHITE);

    display.display();
    delay(500);
    display.clearDisplay();
    String a = String(temperatureC, 2);   // just added it 
    advert.setManufacturerData(a.c_str());
    BLEServer *pServer = BLEDevice::createServer();
    pAdvertising = pServer->getAdvertising();
    advert.setName(ID.c_str());
    pAdvertising->setAdvertisementData(advert);
    pAdvertising->start();
    
    ledcWriteTone(ledChannel,4000);
    digitalWrite(RED, LOW);
    delay(200);
    ledcWriteTone(ledChannel, 0);
    digitalWrite(RED, HIGH);
    display.display();
    if(x < 2)
    {
      Serial.println("[d] logging into warning..");
      Dangerlog = SPIFFS.open("/danger.txt",FILE_APPEND);
      if (Dangerlog) 
      {
        Dangerlog.print(now.year(), DEC); // DATE
        Dangerlog.print("/");
        Dangerlog.print(now.month(), DEC);
        Dangerlog.print("/");
        Dangerlog.print(now.day(), DEC);
        Dangerlog.print(" | "); // TIME
        Dangerlog.print(now.hour(), DEC);
        Dangerlog.print(':');
        Dangerlog.print(now.minute(), DEC);
        Dangerlog.print(':');
        Dangerlog.print(now.second(), DEC);
        Dangerlog.print(" |   ");
        Dangerlog.println(temperatureC);
        //Dangerlog.println("°C");
        Dangerlog.close();
        x++;
        }
     }
   }

  if(flagBTN == false && memoflag == false && flagalarm == false) // if check is LOW and memory not full
  {
    //Display();
    display.setTextSize(2); // modified 02/12/2020  10:30
    display.setCursor(0,0);
    display.print("LOGGING");

    display.setTextSize(2);
    display.setCursor(10,25);
    display.print("DATA..");
    display.display();
    delay(500);
    //display.clearDisplay();
    //display.display();

    if(ii < 1)
    {
      Serial.println("[d] logging into file");
      loggingDisp_date();
      loggingDisp_Temp();
      ii++;
    }
  }

    //if memory is full and checkstate is low
  if(memoflag == true && flagBTN == false )
  { 
    t++;
    display.setTextSize(2); // modified 02/12/2020  10:30
    display.setCursor(0,0);
    display.print("UPLOAD");
    display.setTextSize(2);
    display.setCursor(5,25);
    display.print("ME..!");
    display.display();
    delay(500);
    display.clearDisplay();
    display.display();
    if(t>=3)
    {
      loggingDisp_date();
      loggingDisp_Temp();
      t=0;
    }
  }
    //if memory full and nothing uploaded the TAG so remove files to avoid memory error
  if(usedBytes >= 1850000)
  {
    SPIFFS.remove("/data.txt");
    memoflag = false;
  }  
    /**if TAG is uploaded to PC:
     * display UPLOAD icons
     * Read commands from host
     * Treat commands:
     * READ == read data in normal mode
     * READ WARNING == read warning data content 
     * DELETE == DELETE files
     * */
  if(Serial.available() > 0)
  {
    display.clearDisplay();
    display.display();
    display.setTextSize(2);
    display.setCursor(0,0);
    display.print("UPLOAD");
    display.setTextSize(2);
    display.setCursor(0,25);
    display.print("TO PC..");
    display.display();
    String command = Serial.readStringUntil('\n');
    Serial.print("[d] command:");serial.println(command.c_str());
    if (command == "READ")
    {
      templog = SPIFFS.open("/data.txt",FILE_READ);
      Serial.println("@");
      Serial.println("@");
      Serial.println("@");
      Serial.println(mAddress.toString().c_str());
      while(templog.available())
      {
        Serial.write(templog.read());
      }
      Serial.println("!");
      templog.close();
      //previousMillisFile = currentMillisFile;
    }
    else if (command == "READ WARNING")
    {
      Dangerlog = SPIFFS.open("/danger.txt",FILE_READ);
      Serial.println("@");
      Serial.println("@");
      Serial.println("@");
      Serial.println(mAddress.toString().c_str());
      
      while(Dangerlog.available())
      {
        Serial.write(Dangerlog.read());
      }
      Serial.println("!");
      Dangerlog.close();
    }

    else if (command == "DELETE")
    {
      //Serial.println("Removing File");
      SPIFFS.remove("/data.txt");
      SPIFFS.remove("/danger.txt");
      display.fillRect(55,41,5,5,BLACK); // clear Memo on display!!
      display.display();
    }

    else if (command == "READ SERIAL")
    {
      Serial.println("@");
      Serial.println("@");
      Serial.println("@");
      Serial.println(mAddress.toString().c_str());
      Serial.println("!");
      //Serial.printf("[d] address: %s \n", mAddress.toString().c_str());
    }

    else if(command == "TIME")
    {
      //display_USB();
      Serial.println("date and time format ISO 8601 : 2000-01-01 00:00:00");
      Serial.println("enter DAY 01");
      while(DAY == "")
      {
        DAY = Serial.readStringUntil('\n');
      }
      Serial.println("enter MONTH 01");
      while(MONTH == "")
      {
        MONTH = Serial.readStringUntil('\n');
      }
      Serial.println("enter YEAR 2000");
      while(YEAR == "")
      {
        YEAR = Serial.readStringUntil('\n');
      }
      Serial.println("enter time HOUR 00");
      while(HOUR == "")
      {
        HOUR = Serial.readStringUntil('\n');
      }
      Serial.println("enter MIN 00");
      while(MIN == "")
      {
        MIN = Serial.readStringUntil('\n');
      }
      Serial.println("enter SEC 00 ");
      while(SEC == "")
      {
        SEC = Serial.readStringUntil('\n');
      }
        //"2000-01-01T00:00:00"
      DATE__ = YEAR + "-" + MONTH + "-" + DAY + "T" + HOUR + ":"+ MIN + ":" + SEC;
      rtc.adjust(DateTime(DATE__.c_str()));
      //Serial.print("rtc adjust time : ");Serial.println(DATE__.c_str());
      DAY = "";
      MONTH ="";
      YEAR="";
      HOUR="";
      MIN="";
      SEC="";
      DATE__ = "";
    }
  
    else if(command == "#CMD_TIME!")
    {
      while(DATE__ == "")
      {
        DATE__ = Serial.readStringUntil('\n');
      }
      rtc.adjust(DateTime(DATE__.c_str()));
      DATE__ = "";
    }
    command_timeout = millis();
    display.clearDisplay();
    display.display();
    Serial.println("[d] waiting 5s..");
  }

  if(millis() > command_timeout + command_delay)
  {
    Serial.println("[d] Inside timout....");
    // timer update
    timeoutflag = true;
    command_timeout = millis();
  }

  if ((currentMillisleep - previousMillisleep >= intervalsleep) && timeoutflag && !flagBTN) // timeout normal mode
  {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0); //1 = High, 0 = Low
    //esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0);
    flagBTN = false;
    ii=0;
    x=0;
    timeoutflag = false;
    Serial.println("Going to sleep now for 60s");
    digitalWrite(LCDON, LOW);
    digitalWrite(RTCON, LOW);
    digitalWrite(RED, HIGH);
    digitalWrite(GREEN, HIGH);
    display.clearDisplay();
    display.display();
    previousMillisleep = currentMillisleep;
    esp_deep_sleep_start();
  }

  else if ((millis()  > checkmode_timemout + checkmode_delay) && flagBTN && timeoutflag) //timeout checkmode
  {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 0);
    flagBTN = false;
    ii=0;
    x=0;
    timeoutflag = false;
    Serial.println("Going to sleep from flagBTN now for 60s");
    digitalWrite(LCDON, LOW);
    digitalWrite(RTCON, LOW);
    digitalWrite(RED, HIGH);
    digitalWrite(GREEN, HIGH);
    display.clearDisplay();
    display.display();
    previousMillisleep = currentMillisleep;
    esp_deep_sleep_start();
  }
  //Serial.print("totalBytes loop end : ");Serial.println(totalBytes);
  Serial.print("usedBytes loop end : ");Serial.println(usedBytes); //!!!!!!!!!!!!!!!!!!
}