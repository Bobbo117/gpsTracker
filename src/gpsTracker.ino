
const char* APP = "gpsTracker ";
const char* VERSION = "2023 v0611";

/////////////////////////////////////////////////////////////////////////////////////
//
// gpsTracker:
//
//  1. A stand-alone GPS Tracker which transmits location to an adafruit.io user dashboard map via IoT cellular connection.
//
///////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 Bob Jessup  MIT License:
//  https://github.com/Bobbo117/gpsTraacker/blob/main/LICENSE
//
//  Certain code is adapted from the following:
//  https://github.com/adafruit/Adafruit_FONA
//
//////////////////////////////////////////////////////////////  
// *******     Setup secrets.h file for:        *********** //
//             adafruit mqtt username & key
//////////////////////////////////////////////////////////////

  //  AdaFruitIO MQTT OPTIONS
  //         1. Replace YOUR_USERNAME phrase in the #define SECRET_adaMQTT_USERNAME statement below,
  //            AND
  //         2. Replace the YOUR_KEY field in the #define SECRET_adaMQTT_KEY statement below,  
  //            AND
  //         3. Uncomment the following 2 lines:
  //  #define SECRET_adaMQTT_USERNAME    "YOUR ADAFRUITIO USERNAME HERE"
  //  #define SECRET_adaMQTT_KEY         "YOUR ADAFRUITIO MQTT KEY HERE"
  //            AND
  //         4. Comment the following line to disable it:
      #include <secrets.h> 
  //     
  //            OR
  // 
  //         5. include the #define SECRET_adaMQTT_USERNAME... statement in the secrets.h file. 
  //            AND
  //         6. include the #define SECRET_adaMQTT_KEY... statement in the secrets.h file. 
  //        
  // 
//////////////////////////////////////////////////////////////  
//*******         Compile-time Options           ***********//
//////////////////////////////////////////////////////////////

  //*******************************   
  // 1. Select optional debug aids/monitors:
  //*******************************
  #define OLED_               // 64x128 pixel OLED display (optional - comment out if no OLED)
  #define printMode           // comment out if you don't want brief status display of sensor readings, threshholds and cellular interactions to serial monitor

  //*****************
  // 2. Timing Parameters
  //*****************

  const int espSleepSeconds = 15*60; //or 0; heartbeat period OR wakes up out of sleepMode after sleepMinutes           
  const long espAwakeSeconds = 3*60; // interval in seconds to stay awake; you dont need to change this if espSleepSeconds = 0 indicating no sleep  
  uint8_t postTries = 3;  //allowed data post attempts
  uint8_t gpsTries = 10;
  uint8_t gpsDelay = 10;  //seconds delay between successive attempts
  float targetAccuracy = 1.0;  //range 1-4 (lower is more accurate).
  
  //*****************
  // 3. Miscellaneous
  //*****************

  uint64_t uS_TO_S_FACTOR = 1000000;  // Conversion factor for micro seconds to seconds  
  char buf[200],buf2[20];             // formatGPSData() sets up buf with data to be sent to the cloud. Buf2 is working buffer.
  uint8_t type;                       // fona.type
  char imei[16] = {0};                // 16 character buffer for IMEI

  //*****************
  // Timer stuff
  //*****************
  
  extern "C" {
    #include "freertos/FreeRTOS.h"
    #include "freertos/timers.h" 
  }
  unsigned long currentMillis =0;
  unsigned long priorMillis=0;

  //*****************
  // LLilyGO ESP32 pin definitions
  //*****************  
  #define UART_BAUD   9600
  #define pinBoardLED 12    
  #define PIN_DTR     25   
  #define pinFONA_TX  26 
  #define pinFONA_RX  27     
  #define pinFONA_PWRKEY 4 

  //*****************
  // OLED Libraries for optional OLED
  //*****************
  #ifdef OLED_
    #include "SSD1306Ascii.h"     // low overhead library text only
    #include "SSD1306AsciiWire.h"
    #define I2C_ADDRESS 0x3C      // 0x3C+SA0 - 0x3C or 0x3D
    #define OLED_RESET -1         // Reset pin # (or -1 if sharing Arduino reset pin)
    SSD1306AsciiWire oled;
  #endif   

  //*****************
  // FONA Library
  //*****************

  #include "Adafruit_FONA.h"  //IMPORTANT! get it from https://github.com/botletics/SIM7000-LTE-Shield/tree/master/Code
  Adafruit_FONA_LTE fona = Adafruit_FONA_LTE();
  #include <HardwareSerial.h>
  HardwareSerial fonaSS(1);
  #define SIMCOM_7000                // SIM7000A/C/E/G

  //*****************
  // GPS Stuff
  //*****************

  uint8_t gpsDataValid = 1;     //1=invalid, 0 = valid data
  typedef struct gps {  // create a definition of paarameters
    float lat;             
    float lon;            
    float spd;            
    float hdg;
    float altitude;       
    int vsat;            
    int usat;             // # used satelites
    float accuracy;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int sec;
  } gps;
  
  gps gpsData ={0,0,0,0,0,0,0,0,0,0,0,0,0};    

  float priorLat, priorLon, priorSpd,priorAccuracy;
  #define TINY_GSM_MODEM_SIM7000

  #include <TinyGsmClient.h>

  // Set serial for AT commands
  #define SerialAT  Serial1

  TinyGsm modem(SerialAT);

  //*****************
  //  MQTT PARAMETERS
  //*****************

  #include "Adafruit_MQTT.h"
  #include "Adafruit_MQTT_FONA.h"

  #define adaMQTT_SERVER      "io.adafruit.com"    
  #define adaMQTT_PORT        1883    //insecure port

  //IMPORTANT - See Secrets file setup instructions above for mqtt credential options                  
  // Setup the FONA MQTT class by passing in the FONA class and MQTT server and login details.
  Adafruit_MQTT_FONA mqtt(&fona, adaMQTT_SERVER, adaMQTT_PORT, SECRET_adaMQTT_USERNAME, SECRET_adaMQTT_KEY);
  Adafruit_MQTT_Publish feed_gps = Adafruit_MQTT_Publish(&mqtt, SECRET_adaMQTT_USERNAME "/f/gps/csv");  

//*********************************
int activateCellular(){
  //Active cellular connecton for transmission of gps info
  #ifdef printMode
    Serial.println(F("*activateCellular*"));
  #endif

  for (int i = 0;i<2;i++){       // try again if unsuccessful
    powerUpSimModule();          // Power on the SIM7000 module by goosing the PWR pin 
    delay (1000); //9/19/2000 
    if(setupSimModule()==0){       // Establish serial communication and fetch IMEI
      activateModem();             // Set modem to full functionality ready for Hologram network
      if(activateGPRS()==0){       // Activate General Packet Radio Service
        connectToCellularNetwork();  // Connect to cell network
        uint8_t SS = fona.getRSSI();  //read sig strength
        #ifdef printMode
          Serial.print(F("Sig Str = "));Serial.println(SS);
        #endif
        delay (5000);
        if (SS>0){                    // exit if good signal
          return (0);
        }
      }
    }
  }  
}

//******************************
int activateGPRS(){             
  // Activate General Packet Radio Service
  #ifdef printMode
    Serial.println(F("*activateGPRS*")); 
    Serial.println(F("fona.enableGPRS(true)"));
  #endif
  // Turn on GPRS
  int i = 0;
  while (!fona.enableGPRS(true)) {
    //failed to turn on; delay and retry
    #ifdef printMode
        Serial.print(F("."));
    #endif
    delay(2000); // Retry every 2s
    i ++;
    if (i>3){
      #ifdef printMode
          Serial.println(F("Failed to enable GPRS"));
      #endif
      return -1;  //failure exit
    }
  }

  #ifdef printMode
      Serial.println(F("Enabled GPRS!"));
  #endif
  return 0;  //success exit
}

//*********************************
void activateModem(){              
  // Set modem to full functionality
  #ifdef printMode
    Serial.println(F("*activateModem*"));
    Serial.println(F("fona.setFuntionality(1)")); 
    Serial.println(F("fona.setNetworkSettings('hologram')"));
    Serial.println(F("fona.setPreferredMode(38)"));
    Serial.println(F("fona.setPreferredLTEMode(1)"));
    //Serial.println(F("setOperatingBand('CAT-M', 12)"));
  #endif  
  /*
    0 - Minimum functionality
    1 - Full functionality
    4 - Disable RF
    5 - Factory test mode
    6 - Restarts module
    7 - Offline mode
  */
  fona.setFunctionality(1);        // AT+CFUN=1
  fona.setNetworkSettings(F("hologram"));
  /*
    2 Automatic
    13 GSM only
    38 LTE only
    51 GSM and LTE only
  */
  fona.setPreferredMode(38);
  /*
    1 CAT-M  //requires least power
    2 NB-Iot
    3 CAT-M and NB-IoT
  */
  fona.setPreferredLTEMode(1);
}

//*********************************
int connectToCellularNetwork() {  
  // Connect to cell network and verify connection every 2s until a connection is made
  #ifdef printMode
    Serial.println(F("*connectToCellularNetwork*"));
  #endif
    int i=0;
    while (!readNetStatus()) {
      Serial.print(F("."));
      delay(2000); // Retry every 2s
      i++;
      if (i==5){
        #ifdef printMode
            Serial.println(F("Failed"));
        #endif    
        return -1;
      }
    }
    #ifdef printMode
      Serial.println(F("Connected to cell network!"));
    #endif
    return 0;
    
}
//*************************
  void displayGPS(){
    // Display time, speed, lat, lon, accuracy, and # satelites used on optional OLED display
    #ifdef printMode
      Serial.println(F("*displayGPS*"));
    #endif  
    #ifdef OLED_
      char buf0[6], buf1[6];
      //oled.clear(); 
      oled.setFont(fixed_bold10x15);
      //oled.setFont(Arial_14);
      oled.setCursor(0,0); 

        int hr = gpsData.hour - 5;
        if(hr<0){
          hr=hr+24;
        }
        if(hr<10){oled.print("0");}
        oled.print(hr);
        oled.print(":");
        if(gpsData.minute<10){oled.print("0");}
        oled.print(gpsData.minute);

        oled.print(" ");
        oled.print(int(gpsData.spd*.621371192));
        oled.println(" mph");
        
        if(gpsData.lat < 0){
          oled.println("S " + String(gpsData.lat*(-1), 6) );
        }else{
          oled.println("N " + String(gpsData.lat, 6) );
        }
        if(gpsData.lon < 0){
          oled.println("W " + String(gpsData.lon*(-1), 6) );
        }else{
          oled.println("E " + String(gpsData.lon, 6) );
        }  
        oled.print("Acc " + String(gpsData.accuracy,1) + " " );
        oled.println("# " + String(gpsData.usat));
        
      //oled.println("  ");  
    #endif    //OLED_
}

//***********************************
static int espAwakeTimeIsUp(long msec){ 
  //Determine if timer msec has elapsed      
  static unsigned long previousMillis = 0;   
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= msec) {
    previousMillis = currentMillis; 
    return(1);
  }else{
    return(0);
  }
} 

//*********************************
int8_t formatData(){       
  //format gps data for upload to  adaFruit

  #ifdef printMode 
    Serial.println(F("*formatGPSData*"));
  #endif

  strcpy(buf,""); 
  dtostrf(gpsData.accuracy, 2, 2, buf2); 
  strcat(buf, buf2);
  strcat(buf,",");
  dtostrf(gpsData.lat, 2, 7, buf2); 
  strcat(buf, buf2);
  strcat(buf,",");
  dtostrf(gpsData.lon, 2, 7, buf2); 
  strcat(buf, buf2);
  strcat(buf,",");
  dtostrf(gpsData.spd*.621371192, 2, 0, buf2);  //*.621371192 for mph
  strcat(buf, buf2);
  //dtostrf(gpsData.accuracy, 2, 2, buf2); 
  //strcat(buf, buf2);
  buf2[0]=0;
  strcat(buf,buf2);  //null terminate

  #ifdef printMode
    Serial.print(F("buf: "));Serial.println(buf);
  #endif
}

//*********************************
int8_t MQTT_connect() {
  // Connect and reconnect as necessary to the MQTT server.
  int8_t retn = -1;  //returns 0 if connection success
  
  #ifdef printMode
    Serial.println(F("*MQTT_connect*"));
  #endif
  int8_t ret;

  // Exit if already connected.
  if (mqtt.connected()) {
    return 0;
  }
  #ifdef printMode
    Serial.println("Connecting to MQTT... ");
  #endif

  int8_t i = 0;
  while (i<postTries &&((ret = mqtt.connect()) != 0)) { // connect will return 0 for connected
    #ifdef printMode
      Serial.println(mqtt.connectErrorString(ret));
      Serial.println("Retrying MQTT connection in 5 seconds...");
    #endif
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    i++;
  }
  if(i<postTries){
    retn=0;
    #ifdef printMode
      Serial.println("MQTT Connected!");
    #endif
  }else{
    #ifdef printMode
      Serial.println("MQTT Connection failed!");  
    #endif  
  } 
  return retn;
}

//*********************************
int8_t MQTT_publish_checkSuccess(Adafruit_MQTT_Publish &feed, const char *feedContent) {
  int8_t retn = -1;  //returns 0 if connection success
  #ifdef printMode
    Serial.println(F("*MQTT_publish_checkSuccess*"));
    Serial.println(F("Sending data..."));
  #endif  
  uint8_t i=0;
  while (i<postTries &&(! feed.publish(feedContent))) {
    i++;
    delay(5000);
  }  
  if(i<postTries){
    retn=0;
    #ifdef printMode
      Serial.print(F("Publish succeeded in tries: "));Serial.println(i+1);
    #endif
  }else{
    #ifdef printMode
      Serial.println(F("Publish failed!"));  
    #endif        
  } 
  return retn;
}

//************************************
void napTime(){
  //Determine if awake time has elapsed and if so, put processor to sleep
  if (espSleepSeconds >0 && espAwakeSeconds >0 && espAwakeTimeIsUp(espAwakeSeconds*1000) ==1 ){
    #ifdef printMode
      Serial.println(F(" "));Serial.print(F("sleeping "));Serial.print(espSleepSeconds);Serial.println(F(" seconds..zzzz"));
    #endif
    simModuleOff();                 // power off the SIM7000 module                                   
    ESP.deepSleep(espSleepSeconds * uS_TO_S_FACTOR);
  }
}
    
//*********************************
void powerUpSimModule() {                
  // Power on the SIM module
  pinMode(pinFONA_PWRKEY,OUTPUT);
  digitalWrite(pinFONA_PWRKEY, LOW);
  delay(1000);                           // datasheet ton = 1 sec
  digitalWrite(pinFONA_PWRKEY, HIGH);
  delay(5000);
}     
                 
//***************************
uint8_t readGPS(){
  //Fetch the GPS data
  float priorAccuracy=100.0;
  float priorLat, priorLon, priorSpeed;
  gpsDataValid = 1;
  uint8_t gpsCount = 0;
  modem.sendAT("+SGPIO=0,4,1,1");
  if (modem.waitResponse(10000L) != 1) {
    #ifdef printMode
      Serial.println(" SGPIO=0,4,1,1 false ");
    #endif  
  }
  modem.enableGPS();
  
  for (int8_t i = gpsTries; i; i--) { 
    #ifdef printMode
      Serial.print("Reading GPS..");
    #endif
    if (modem.getGPS(&gpsData.lat, &gpsData.lon, &gpsData.spd, &gpsData.hdg, 
      &gpsData.vsat, &gpsData.usat, &gpsData.accuracy,
      &gpsData.year, &gpsData.month, &gpsData.day, 
      &gpsData.hour, &gpsData.minute, &gpsData.sec)) 
    {
      gpsCount=gpsCount+1;
      gpsDataValid = 0; 
      #ifdef printMode
        Serial.println("");//Serial.println("");
        if (gpsCount==1){
          Serial.println(F("LAT              LON            KPH     HDG     ACC     SAT     uSAT"));
        }  
        Serial.print(String(gpsData.lat,8));Serial.print("\t");
        Serial.print(String(gpsData.lon,8));Serial.print("\t");
        Serial.print(gpsData.spd);Serial.print("\t");
        //Serial.print(gpsData.alt);Serial.print("\t");
        Serial.print(gpsData.hdg);Serial.print("\t");
        Serial.print(gpsData.accuracy);Serial.print("\t");
        Serial.print(gpsData.vsat);Serial.print("\t");
        Serial.print(gpsData.usat);Serial.print("\t");
      #endif   

      if ( gpsData.accuracy<=targetAccuracy ){
        break; 
      }
      if (gpsData.accuracy<priorAccuracy){
        priorAccuracy=gpsData.accuracy;
        priorLat=gpsData.lat;
        priorLon = gpsData.lon;
        priorSpeed = gpsData.spd;
      }  
    }
    #ifdef printMode
      //Serial.println("Retrying in 6 sec.");
    #endif  
    delay(6000);
  }
  if (gpsDataValid == 0){
    gpsData.accuracy = priorAccuracy;
    gpsData.lat = priorLat;
    gpsData.lon = priorLon;
    gpsData.spd = priorSpeed;
  }
  #ifdef printMode
    Serial.println("Disabling GPS");
  #endif
  modem.disableGPS();

  // turn off GPS power (version 20200415)
  modem.sendAT("+SGPIO=0,4,1,0");
  if (modem.waitResponse(10000L) != 1) {
  #ifdef printMode
    Serial.println(" SGPIO=0,4,1,0 false ");
  #endif
  }
  return(gpsDataValid);
}
//**********************************        
bool readNetStatus() {
  //read the cellular net parameters
  int i = fona.getNetworkStatus();
  #ifdef printMode
    const char* networkStatus[]={"Not registered","Registered (home)","Not Registered (searching)","Denied","Unknown","Registered roaming"};    
    Serial.print(F("fona.getNetworkStatus")); Serial.print(i); Serial.print(F(": "));Serial.println(F(networkStatus[i]));
    Serial.println(F("fona.getNetworkInfo()"));
    fona.getNetworkInfo();  
  #endif    
  
  if (!(i == 1 || i == 5)) return false;
  else return true;
}

//*********************************
void setupGPS(){
  //Turn on the modem
  pinMode(pinFONA_PWRKEY, OUTPUT);
  digitalWrite(pinFONA_PWRKEY, HIGH);
  delay(300);
  digitalWrite(pinFONA_PWRKEY, LOW);

  delay(1000);
  
  // Set module baud rate and UART pins
  SerialAT.begin(UART_BAUD, SERIAL_8N1, pinFONA_TX, pinFONA_RX);
  #ifdef printMode
    Serial.println("Initializing modem...");
  #endif
  if (!modem.restart()) {
    #ifdef printMode
      Serial.println("Failed to restart modem, attempting to continue without restarting");
    #endif  
  }
  
  // Print modem info
  String modemName = modem.getModemName();
  delay(500);
  String modemInfo = modem.getModemInfo();
  delay(500);
  #ifdef printMode
    Serial.print("Modem Name: " + modemName);
    Serial.println(";    Modem Info: " + modemInfo);  
  #endif  
}

//*********************************  
void setupOledDisplay(){
  #ifdef OLED_
    Wire.begin();
    oled.begin(&Adafruit128x64,I2C_ADDRESS); 
  #endif  
}

//*********************************
int setupSimModule() {  
  #ifdef printMode
    Serial.println(F("*setupSimModule*"));
    Serial.println(F("fonaSS.begin(115200, SERIAL_8N1, pinFONA_TX, pinFONA_RX)"));
    Serial.println(F("fonaSS.println('AT+IPR=9600')"));    
    Serial.println(F("fonaSS.begin(9600, SERIAL_8N1, pinFONA_TX, pinFONA_RX)"));
    Serial.println(F("fona.begin(fonaSS)"));
  #endif  
  
  fonaSS.begin(UART_BAUD, SERIAL_8N1, pinFONA_TX, pinFONA_RX); // Switch to 9600
  if (! fona.begin(fonaSS)) {
    #ifdef printMode
      Serial.println(F("Couldn't find FONA"));
    #endif
    return(-1);
  }else{
    #ifdef printMode
        Serial.println(F("found FONA"));
    #endif
  }

  type = fona.type(); // read sim type
  #ifdef printMode
    Serial.print(F("fona.type: "));Serial.println(type);
    Serial.print(F("fona.getIMEI(imei)"));
  #endif
  uint8_t imei1 = fona.getIMEI(imei);
  if (imei1 > 0) {
    #ifdef printMode
      Serial.print(F(" Module IMEI: ")); Serial.println(imei);
    #endif
  }
  return(0);
}

//*********************************
void simModuleOff(){    
  // Power off the SIM7000 module by goosing the PWR pin 
  #ifdef printMode
    Serial.println(F("*simModuleOff*"));  
  #endif    

  digitalWrite(pinFONA_PWRKEY, LOW);   // turn it on
  delay(1500);                          
  digitalWrite(pinFONA_PWRKEY, HIGH); 

 //delay(5000);                         // give it time to take hold - long delay   
}

//**************************************
void turnOnBoardLED(){              
  // Turn ESP32 Wrover LED on 
  digitalWrite(pinBoardLED, LOW);
} 

//*********************************    
void turnOffBoardLED(){             
  // Turn ESP32 Wrover LED off
  digitalWrite(pinBoardLED, HIGH);
}    

///////////////////////
// *** Setup() ***
//////////////////////

void setup() {
  //This runs each time the ESP32 awakens
  pinMode(pinBoardLED,OUTPUT);
  pinMode(pinFONA_PWRKEY, OUTPUT);
  #ifdef printMode                 // if test mode
    Serial.begin(115200);          // Initialize Serial Monitor 
    turnOnBoardLED();  
    Serial.println(F("-----------------------"));
    Serial.print("Application: "); Serial.println(APP);
    Serial.print("Version: "); Serial.println(VERSION);
  #endif
  setupOledDisplay();             // Clears the OLED display if there is one
  setupGPS();                     // Turn on the modem                  
  turnOffBoardLED();
}

/////////////////
// ***  Loop()  ***
////////////////// 
void loop() {
  //This runs a continuous loop in accordance with the following steps:

  // 1. Read GPS location and publish it to adafruit.io gps feed via mqtt
  //    successful? 
  //    1a. yes - go to sleep 
  // 2. no  - is there still some awake time (espAwakeSeconds) available?
  //      2a. no  - go to sleep
  // 3.   yes - try again from the beginning of the loop
  
// 1. Fetch GPS reading 
  if (readGPS()==0){                    // continue if valid GPS reading exists
    activateCellular();                 // activate cellular communication 
    if(MQTT_connect()==0){              // continue if MQTT connection exists
      formatData();                     // format the GPS data into buf
      // publish the GPS info and verify success
      if(MQTT_publish_checkSuccess(feed_gps, buf)==0){ 
        //Success!
        displayGPS();                   // update OLED display if enabled
        simModuleOff();                 // power off the SIM7000 module                                   
        #ifdef printMode              
          Serial.println(F(" "));Serial.print(F("sleeping "));Serial.print(espSleepSeconds);Serial.println(F(" seconds..zzzz"));
          delay(1000);
        #endif
        //GPS successfully read and transmitted; Go to sleep
        ESP.deepSleep(espSleepSeconds * uS_TO_S_FACTOR); 
      }
    }
  }

// 2. Step 1 failed; Power off SIM 7000 module and go to sleep if espAwakeSeconds have elapsed
  napTime();

// 3. Loop back to step 1 to try again after delay
  delay(gpsDelay*1000);    
}
