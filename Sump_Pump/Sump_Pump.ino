/* Sump Pump Control Software

Dave Wine
2/16/2017 Rev 0.2: Trying to get RTCZero/TimeLib working....
2/19/2017 Rev 0.3: Using RBD Timer instead of RTCZero - works OK despite being for AVR boards.
4/8/2017: Rev 0.4: Wired up with custom interconnect board and Milone 24" sensor attached.
4/11/2017: Rev 0.41: Validated all sensor inputs - OK.  Milone very sensitive to orientation and touch.
4/16/2017: Rev 0.5: Added ThingSpeak code from WriteMultipleVoltages example
4/23/2017: Rev 0.6: Started scaling outputs to ThingSpeak
6/18/2017: Rev 0.7: Cleaned up code; got float switch working, but have weird impedance issue on AD590
6/28/2017: Rev 0.8: Switched out MKR1000 - now using 'B'.  Fixed many issues and got initial SFs set up.  All sensors working!
6/30/2017: Rev 0.9: Added averaging, alert, rearranged outputs
7/1/2017: Rev 1.0: Got alarm logic and dynamic interval working; did initial calibration.  Yay!
7/3/2017: Rev 1.1: Added Temboo Email service
7/5/2017: Rev 1.2: Cleaned up alert logic and went to 8 hour updates

Module sources:
WiFi Web Server: https://www.arduino.cc/en/Tutorial/Wifi101WiFiWebServer
SD Card Controller:  Std Arduino library
RBD Timer Code: http://robotsbigdata.com/docs-arduino-timer.html#example-setup
Temboo Source Code:https://temboo.com/arduino/others/send-an-email
*/

/*
WiFi Web Server

A simple web server that shows the value of the analog input pins.
using a WiFi shield.

This example is written for a network using WPA encryption. For
WEP or WPA, change the WiFi.begin() call accordingly.

Circuit:
* WiFi shield attached
* Analog inputs attached to pins A0 through A5 (optional)

Created 13 July 2010
by dlf (Metodo2 srl)
modified 31 May 2012
by Tom Igoe

 */

//LIBRARIES

//WiFi Server:
#include <SPI.h>
#include <WiFi101.h>

//SD Card:
#include <SPI.h> 
#include <SD.h>

//RBD Timer:
#include <RBD_Timer.h>

//ThingSpeak:
#include "ThingSpeak.h"

//Temboo:
#include <SPI.h>
#include <WiFi101.h>
#include <WiFiSSLClient.h>
#include <TembooSSL.h>
#include "TembooAccount.h" // Contains Temboo account information

//INITIALIZATIONS

//WiFi Server:
char ssid[] = "CELLAR";      // your network SSID (name)
char pass[] = "dasf";   // your network password
int keyIndex = 0;                 // your network key Index number (needed only for WEP)
int status = WL_IDLE_STATUS; 
WiFiServer server(80);
byte mac[6];

//RBD Timer
RBD::Timer tSensor;
RBD::Timer tUpdate;

//SD Card
const int chipSelect = 4;

//MKR Digital Pins
const int FloatSensor = 2;
const int CD = 4;
const int CS = 7;
const int CLK = 8;
const int DO = 9;

//MKR Analog Pins
const int Milone = 5;
const int AD590 = 6;
const int V_board = 4;

//Readings
byte FS_Read;
float Milone_Read;
float AD590_Read;
float BoardV_Read;
short Alert;

//Calculated Values
float Milone_Calc;
float AD590_Calc;

// Scale Factors
const float Temp_SF = 3.22265e-1 * 0.9846;  //3.3V/1024*100 deg/V * calibration coeff
const float Temp_Bias = 273.1; // Convert from Kelvin to Celsius

const float Milone_Bias = 95;
const float Milone_SF = 0.0406;  //counts/cm
const float BoardV_SF = 3.3/1024*2; //Board V in counts * 2 for voltage divider

const short Samp = 100; //Number of samples for averaging
const short SampInt = 10; //Sampling interval for averaging in ms

//Misc
short i;
String AlertString = "OK";

// Constants
long ReadInt = 30000; // Main interval variable used between sensor reads (milliseconds)
long ReadUpd = ReadInt; //Update rate for ThingSpeak
short ReadExp = 0; //Scaling exponent for ThingSpeak
short MaxExp = 10; //Maximum exponent allowed (sets update email interval (2^MaxExp*Readint milliseconds)
long timestamp = 1498916136; // 7/1/2017 6:35 am PDT
long DryThres = 100; // This is the Milone count level that decides whether there is water in the sump or not.
short Max_cm = 45; //45 is a guess - set this to the FS level above ground
short Min_cm = 3; //3 is a guess - set this to the FS level above ground
//ThingSpeak:
// ***********************************************************************************************************

//#define USE_WIFI101_SHIELD
//#define USE_ETHERNET_SHIELD
#if !defined(USE_WIFI101_SHIELD) && !defined(USE_ETHERNET_SHIELD) && !defined(ARDUINO_SAMD_MKR1000) && !defined(ARDUINO_AVR_YUN) && !defined(ARDUINO_ARCH_ESP8266)
  #error "Uncomment the #define for either USE_WIFI101_SHIELD or USE_ETHERNET_SHIELD"
#endif

#if defined(ARDUINO_AVR_YUN)
    #include "YunClient.h"
    YunClient client;
#else
  #if defined(USE_WIFI101_SHIELD) || defined(ARDUINO_SAMD_MKR1000) || defined(ARDUINO_ARCH_ESP8266)
    // Use WiFi
    #ifdef ARDUINO_ARCH_ESP8266
      #include <ESP8266WiFi.h>
    #else
      #include <SPI.h>
      #include <WiFi101.h>
    #endif
//   char ssid[] = "<YOURNETWORK >";    //  your network SSID (name) 
//   char pass[] = "<YOURPASSWORD>";   // your network password
//    int status = WL_IDLE_STATUS;
    WiFiClient  client;
  #elif defined(USE_ETHERNET_SHIELD)
    // Use wired ethernet shield
    #include <SPI.h>
    #include <Ethernet.h>
    byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
    EthernetClient client;
  #endif
#endif

#ifdef ARDUINO_ARCH_AVR
  // On Arduino:  0 - 1023 maps to 0 - 5 volts
  #define VOLTAGE_MAX 5.0
  #define VOLTAGE_MAXCOUNTS 1023.0
#elif ARDUINO_SAMD_MKR1000
  // On MKR1000:  0 - 1023 maps to 0 - 3.3 volts
  #define VOLTAGE_MAX 3.3
  #define VOLTAGE_MAXCOUNTS 1023.0
#elif ARDUINO_SAM_DUE
  // On Due:  0 - 1023 maps to 0 - 3.3 volts
  #define VOLTAGE_MAX 3.3
  #define VOLTAGE_MAXCOUNTS 1023.0  
#elif ARDUINO_ARCH_ESP8266
  // On ESP8266:  0 - 1023 maps to 0 - 1 volts
  #define VOLTAGE_MAX 1.0
  #define VOLTAGE_MAXCOUNTS 1023.0
#endif
/*
  *****************************************************************************************
  **** Visit https://www.thingspeak.com to sign up for a free account and create
  **** a channel.  The video tutorial http://community.thingspeak.com/tutorials/thingspeak-channels/ 
  **** has more information. You need to change this to your channel, and your write API key
  **** IF YOU SHARE YOUR CODE WITH OTHERS, MAKE SURE YOU REMOVE YOUR WRITE API KEY!!
  *****************************************************************************************/
unsigned long myChannelNumber = 229595;
const char * myWriteAPIKey = "WWG74T7EB0L3UO1X";

//Temboo:

WiFiSSLClient SSLclient;
int calls = 1;   // Execution count, so this doesn't run forever
int maxCalls = 10;   // Maximum number of times the Choreo should be executed

//MAIN SETUP
void setup() {
  //LED for battery signal
  pinMode(LED_BUILTIN, OUTPUT);
  
  //WiFi Server
  //Initialize serial and wait for port to open:
  Serial.begin(9600);

  delay(5000);
  /* Took this out and replaced with static delay since it won't be using the serial port normally  
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  */

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  // attempt to connect to WiFi network:
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);

  // Print out MAC address
    WiFi.macAddress(mac);
    Serial.print("MAC: ");
    Serial.print(mac[5],HEX);
    Serial.print(":");
    Serial.print(mac[4],HEX);
    Serial.print(":");
    Serial.print(mac[3],HEX);
    Serial.print(":");
    Serial.print(mac[2],HEX);
    Serial.print(":");
    Serial.print(mac[1],HEX);
    Serial.print(":");
    Serial.println(mac[0],HEX);
    
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  server.begin();
  // you're connected now, so print out the status:
  printWiFiStatus();
  
  //SD Card
  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");
  
 //Initialize time and other variables
  timestamp=WiFi.getTime();
  Serial.print("Initial Time: ");
  Serial.println(timestamp);
  
  //Initialize the RBD Timers
  TimerSet();

//ThingSpeak:
  #ifdef ARDUINO_AVR_YUN
    Bridge.begin();
  #else   
    #if defined(ARDUINO_ARCH_ESP8266) || defined(USE_WIFI101_SHIELD) || defined(ARDUINO_SAMD_MKR1000)
      WiFi.begin(ssid, pass);
    #else
      Ethernet.begin(mac);
    #endif
  #endif

  ThingSpeak.begin(client);

  Email_Temboo("Rebooted and Connected OK");
}
  //Temboo:
// Not sure I needed this stuff...

//MAIN LOOP
void loop() {
// Main loop just sits and watches stuff....

  if(tSensor.onExpired()){
    // Read sensors, etc.
    Serial.print(ReadInt/1000);
    Serial.println(" seconds passed");
    Serial.print(ReadUpd/1000);
    Serial.println(" second update interval");
    timestamp=WiFi.getTime();

    //Always check the float sensor!
    Serial.print("Float Sensor State: ");
    Serial.println(digitalRead(FloatSensor));

    //Read sensors and write to card
    SensorRead();
    SDCardWrite(timestamp);
    AlertString = "Milone Read: " + String(Milone_Read);
    
    //Update Interval Check
    //If dry, lengthen interval, otherwise shorten it
    if (Milone_Read < DryThres)
    {
      ReadExp++;  //if dry, lengthen sample time
      Serial.println("Incremented update exponent");
      AlertString = AlertString + " " + "Less than " + String(Min_cm) + " cm water";
    }
    else
    {
      //Decrement if it gets wet..
      Serial.println("Decrement exponent - wet");
      ReadExp--;        
      AlertString = AlertString + " " + String(Milone_Calc) + " cm water (xx cm triggers float)";
      
      if  (ReadUpd != ReadInt){
        ReadUpd = ReadInt;
        tUpdate.setTimeout(ReadUpd);
        tUpdate.restart();
      }
    }  
    // Limit to 2^MaxExp milliseconds
    if (ReadExp > MaxExp)
    {
      ReadExp=MaxExp;
    }

    if (ReadExp < 0)
    {
      ReadExp = 0;
    }

    // Snap back if on alert
    if (Alert != 0)
    {
      ReadExp=0;
      
      if (Alert == 1){
        AlertString = "Running on battery: "+ AlertString;
      }
      else {
        AlertString = "Float Sensor Triggered! " + AlertString;
      }  
          
      Serial.println("Reset update exponent to 0 - problem");
      if  (ReadUpd != ReadInt){
        ReadUpd = ReadInt;
        tUpdate.setTimeout(ReadUpd);
        tUpdate.restart();
      }
    }
    
    ReadUpd = ReadInt * pow(2,ReadExp);  //Scale update rate relative to update interval
  
  // Restart sensor timer
  tSensor.restart();
  } 

  if(tUpdate.onExpired()){
  
    //ThingSpeak:
    // Read the input on each pin, convert the reading, and set each field to be sent to ThingSpeak.
    // On Uno,Mega,Yun:  0 - 1023 maps to 0 - 5 volts
    // On ESP8266:  0 - 1023 maps to 0 - 1 volts
    // On MKR1000,Due: 0 - 4095 maps to 0 - 3.3 volts
    //  float pinVoltage = analogRead(A1) * (VOLTAGE_MAX / VOLTAGE_MAXCOUNTS);
    
    // Send timestamp

      #ifndef ARDUINO_ARCH_ESP8266
        // The ESP8266 only has one analog input, so skip this
        ThingSpeak.setField(1,Alert);   
        // Send raw Milone output
        //pinVoltage = analogRead(Milone) * (VOLTAGE_MAX / VOLTAGE_MAXCOUNTS);
        ThingSpeak.setField(2,Milone_Calc);
        
        //Send Float Sensor
        //pinVoltage = digitalRead(FloatSensor); //* (VOLTAGE_MAX / VOLTAGE_MAXCOUNTS);
        ThingSpeak.setField(3,FS_Read);    
            
        // Send AD590
        //pinVoltage = analogRead(AD590) * (VOLTAGE_MAX / VOLTAGE_MAXCOUNTS);
        ThingSpeak.setField(4,AD590_Calc);
         
        //Send Board Voltage
        //pinVoltage = analogRead(V_board); //* (VOLTAGE_MAX / VOLTAGE_MAXCOUNTS);
        ThingSpeak.setField(5,BoardV_Read);
        
        //pinVoltage = analogRead(A5) * (VOLTAGE_MAX / VOLTAGE_MAXCOUNTS);
        ThingSpeak.setField(6,Milone_Read);
        //pinVoltage = analogRead(A6) * (VOLTAGE_MAX / VOLTAGE_MAXCOUNTS);
        ThingSpeak.setField(7,AD590_Read);
        //pinVoltage = analogRead(A7) * (VOLTAGE_MAX / VOLTAGE_MAXCOUNTS);
        ThingSpeak.setField(8,timestamp);
      #endif
    
      // Write the fields that you've set all at once.
      ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey); 

      tUpdate.setTimeout(ReadUpd);
      tUpdate.restart();

      //Send email through Temboo
      //AlertString = "Running OK";
      Email_Temboo(AlertString);
  }
  // Vary this based on history
  // delay(ReadUpd); // ThingSpeak will only accept updates every 15 seconds. 
  
  //Send data to server
  CallWiFi();
  //Serial.print("doing other stuff ");
}

// Functions
void CallWiFi(){  
  //WiFi Server
  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin
          for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
            int sensorReading = analogRead(analogChannel);
            client.print("analog input ");
            client.print(analogChannel);
            client.print(" is ");
            client.print(sensorReading);
            client.println("<br />");
          }
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

//WiFi Status
void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void TimerSet(){
  tSensor.setTimeout(ReadInt); //Set timeout for sensor updates
  tSensor.restart();
  
  tUpdate.setTimeout(ReadUpd); //Set timeout for update timer - will vary with water level
  tUpdate.restart();
}

void SensorRead(){
  //Read all sensors and average...
i=0;
  while(i<Samp){
    FS_Read += digitalRead(FloatSensor);
    Milone_Read += analogRead(Milone); 
    AD590_Read += analogRead(AD590);
    BoardV_Read += analogRead(V_board);
  i++;
  delay (SampInt);
  }
  
  FS_Read = FS_Read/Samp;
  Milone_Read = Milone_Read/Samp;
  AD590_Read = AD590_Read/Samp;
  BoardV_Read = BoardV_Read/Samp;
  
  //Calculate averages and do calculations
  Milone_Calc = (Milone_Read - Milone_Bias)*Milone_SF; 
  AD590_Calc = AD590_Read * Temp_SF - Temp_Bias; 
  BoardV_Read = BoardV_Read * BoardV_SF;
   
  //Do checks and flag alert if needed

  if(Milone_Read < 10 || FS_Read == 1 || AD590_Read > 999) {
    Alert = 2;  // Lost Milone Sensor, FS triggered
  }
  else if (BoardV_Read<4) {
    Alert = 1;  // On battery
    digitalWrite(LED_BUILTIN,HIGH);
  }
  else {
    Alert = 0;
    digitalWrite(LED_BUILTIN,LOW);
  }

}
void SDCardWrite(long currenttime) {
  //Make a string for assembling the data to log:
  Serial.println("Writing data to card...");

  //Alert
  String dataString = String(Alert) + ", ";

  //Float Sensor
  dataString += String(Milone_Calc) + ", ";
  
  //Milone
  dataString += String(FS_Read) + ", ";
  dataString += String(AD590_Calc) + ", ";
  
  //Temperature
  dataString += String(BoardV_Read) + ", ";
  dataString += String(Milone_Read) + ", ";

  //Board Voltage
  dataString += String(AD590_Read) + ", ";

  //Timestamp
  dataString += String(currenttime);

 /* read three sensors and append to the string:
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    dataString += String(sensor);
    if (analogPin < 2) {
      dataString += ",";
    }
  }
*/
// open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}

void print2digits(int number) {
   if (number < 10) {
     Serial.print("0"); // print a 0 before if the number is < than 10
   }
   Serial.print(number);
}

//Temboo email
void Email_Temboo(String Message){

  Serial.println("Running SendEmail - Run #" + String(calls++));

  TembooChoreoSSL SendEmailChoreo(SSLclient);

  // Invoke the Temboo client
  SendEmailChoreo.begin();

  // Set Temboo account credentials
  SendEmailChoreo.setAccountName(TEMBOO_ACCOUNT);
  SendEmailChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
  SendEmailChoreo.setAppKey(TEMBOO_APP_KEY);
  SendEmailChoreo.setDeviceType(TEMBOO_DEVICE_TYPE);

  // Set Choreo inputs
  String FromAddressValue = "dwine66@gmail.com";
  SendEmailChoreo.addInput("FromAddress", FromAddressValue);
  String UsernameValue = "dwine66@gmail.com";
  SendEmailChoreo.addInput("Username", UsernameValue);
  String ToAddressValue = "dwine66@gmail.com";
  SendEmailChoreo.addInput("ToAddress", ToAddressValue);
  String SubjectValue = "Sump Pump Update";
  SendEmailChoreo.addInput("Subject", SubjectValue);
  String PasswordValue = "znthlrcmkypmqeji";
  SendEmailChoreo.addInput("Password", PasswordValue);
  String MessageBodyValue = Message;
  SendEmailChoreo.addInput("MessageBody", MessageBodyValue);

  // Identify the Choreo to run
  SendEmailChoreo.setChoreo("/Library/Google/Gmail/SendEmail");

  // Run the Choreo; when results are available, print them to serial
  SendEmailChoreo.run();

  while(SendEmailChoreo.available()) {
    char c = SendEmailChoreo.read();
//    Serial.print(c);
  }
  SendEmailChoreo.close();
}

  //Serial.println("\nWaiting...\n");
  //delay(30000); // wait 30 seconds between SendEmail calls
