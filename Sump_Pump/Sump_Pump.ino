/* Sump Pump Control Software

Dave Wine
2/16/2017 Rev 0.2: Trying to get RTCZero/TimeLib working....
2/19/2017 Rev 0.3: Using RBD Timer instead of RTCZero - works OK despite being for AVR boards.

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


//RBD Timer:
#include <RBD_Timer.h>

// Initializations

//
//WiFi Server:
//
char ssid[] = "CELLAR";      // your network SSID (name)
char pass[] = "xxs";   // your network password
int keyIndex = 0;                 // your network key Index number (needed only for WEP)
int status = WL_IDLE_STATUS;

WiFiServer server(80);

//Initialize an RBD Timer
RBD::Timer timer;
//
//SD Card
//
const int chipSelect = 4;

//MKR Digital Pins
const int FloatSensor = 2;
const int CD = 4;
const int CS = 7;
const int CLK = 8;
const int DO = 9;

//MKR Analog Pins
const int Milone = 1;

// Variables

long ReadInt=10000; // Main interval variable used between sensor reads (milliseconds)
long timestamp=1487523237; // 2/19/2017 8:51 am

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

//Initialize the RBD Timer
timer.setTimeout(ReadInt);
timer.restart();
}

//MAIN LOOP

void loop() {
  // Main loop just sits and watches stuff....

  if(timer.onRestart()){
  // Read sensors, etc.
  
  Serial.print(ReadInt/1000);
  Serial.println(" seconds passed");
  timestamp=WiFi.getTime();

//Always check the float sensor!
  Serial.print("Float Sensor State: ");
  Serial.println(digitalRead(FloatSensor));

  // Read sensors and write to card
  SDCardWrite(timestamp);
  //delay(1000);
  } 

//Do other stuff


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

void SDCardWrite(long currenttime) {
  // make a string for assembling the data to log:
  
  Serial.println("Writing data to card...");
  
  String dataString = String(currenttime)+",";
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


void print2digits(int number) {
   if (number < 10) {
     Serial.print("0"); // print a 0 before if the number is < than 10
   }
   Serial.print(number);
}



