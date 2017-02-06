/* Sump Pump Control Software

Dave Wine

Module sources:
WiFi Web Server: https://www.arduino.cc/en/Tutorial/Wifi101WiFiWebServer
SD Card Controller:  Std Arduino library
RTC Zero Module: https://www.arduino.cc/en/Tutorial/SimpleRTC
Time Setup Code: http://playground.arduino.cc/code/time

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

 created 13 July 2010
 by dlf (Metodo2 srl)
 modified 31 May 2012
 by Tom Igoe

 */

//INITIALIZATIONS

// WiFi Server:
#include <SPI.h>
#include <WiFi101.h>

// SD Card:
#include <SPI.h> 
#include <SD.h>

// RTC Zero:
#include <RTCZero.h>

//Time Setup:
//#include <TimeLib.h>

// Initializations

//
//WiFi Server:
//
char ssid[] = "CELLAR";      // your network SSID (name)
char pass[] = "PuckStepsOnMushrooms";   // your network password
int keyIndex = 0;                 // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;

WiFiServer server(80);

//
//SD Card
//
const int chipSelect = 4;

//RTC Zero

/* Create an rtc object */
 RTCZero rtc;
/* Change these values to set the current initial time */
 const byte seconds = 0;
 const byte minutes = 0;
 const byte hours = 12;

/* Change these values to set the current initial date */
 const byte day = 5;
 const byte month = 2;
 const byte year = 17;

//MKR Digital Pins
const int FloatSensor = 2;
const int CD = 4;
const int CS = 7;
const int CLK = 8;
const int DO = 9;

//MKR Analog Pins
const int Milone = 1;

//MAIN SETUP

void setup() {
  
  //WiFi Server
  
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

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
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  server.begin();
  // you're connected now, so print out the status:
  printWiFiStatus();
  
  //
  //SD Card
  //
/*
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
*/
  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");
  
 //Initialize time and other variables
 long timestamp=WiFi.getTime();
    Serial.print("Initial Time: ");
    Serial.println(timestamp);
 unsigned long systime=millis()  ; 

 // Calculate time for RTC based on WiFi:
// this is a standalone project for later...

//RTC Zero

   rtc.begin(); // initialize RTC

   // Set the time
   rtc.setHours(hours);
   rtc.setMinutes(minutes);
   rtc.setSeconds(seconds);

   // Set the date
   rtc.setDay(day);
   rtc.setMonth(month);
   rtc.setYear(year);

   // you can use also
   //rtc.setTime(hours, minutes, seconds);
   //rtc.setDate(day, month, year);


   
//Time between reads in milliseconds
int readtime = 60000; //Start at one minute

}

//MAIN LOOP

void loop() {
  // Main loop just sits and watches stuff....
delay(10000);
 long timestamp=WiFi.getTime();

  Serial.println(timestamp); 

//Always check the float sensor!
Serial.println(digitalRead(FloatSensor));

CallWiFi();

SDCardWrite(timestamp);
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
    Serial.println("client disonnected");
  }
}


//WiFi
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

//Put SD Card Functions here

void SDCardWrite(long time) {
  // make a string for assembling the data to log:
  
  Serial.println("Writing data...");
  
  String dataString = String(time)+",";
  dataString += String(digitalRead(FloatSensor)) + ",";

  // read three sensors and append to the string:
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    dataString += String(sensor);
    if (analogPin < 2) {
      dataString += ",";
    }
  }
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

//RTC Functions

void GetRTCTime(){
   // Print date...
   print2digits(rtc.getDay());
   Serial.print("/");
   print2digits(rtc.getMonth());
   Serial.print("/");
   print2digits(rtc.getYear());
   Serial.print(" ");

   // ...and time
   print2digits(rtc.getHours());
   Serial.print(":");
   print2digits(rtc.getMinutes());
   Serial.print(":");
   print2digits(rtc.getSeconds());

   Serial.println();

   delay(1000);
}


void print2digits(int number) {
   if (number < 10) {
     Serial.print("0"); // print a 0 before if the number is < than 10
   }
   Serial.print(number);
}

