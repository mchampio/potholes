// this program accesses the wfi network to send gps data to thingspeak
//  Mark Champion 2016

#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <Time.h>  
                                                     // NOTE - This code is for the NODEMCU and the pins on that device
                                                      
#define RXPIN 12                                     // Define the pins for TX and RX for the GPS unit
#define TXPIN 13

int redled = 2;                                      // setup the led pin numbers
int greenled = 0;
int blueled = 4;

const byte interruptPin = 5;                         // Setup the pin5 (D1) to be used for the pushbutton interrupt
volatile boolean send_data = false;

char EEPROM_data = '*';                              // Indicator that data is stored in the EEPROM
char EEPROM_star = '*';                              // constant to test the data indicator against
byte EEPROM_pointer;                                 // pointer into the EEPROM to store/read the location data 

float Lat;                                           // define the variable to hold Latitude
float Long;                                          // define the variable to hold Longitude 
char outstr[15];                                     // string to hold the Lat, Long converted from float to string 
                  
String apiKey = "YourThingSpeakKey";                  // replace with your channel’s thingspeak API key, SSID and Password
const char* ssid = "SSID";                            //replace with the SSID of your router or other network device
const char* password = "password";                    //replace with the password of your router or other network device

const char* server = "api.thingspeak.com";            // this is the Thingspeak web site URL

SoftwareSerial gpsSerial(RXPIN, TXPIN);               // create gps sensor connection
TinyGPSPlus gps;                                      // create gps object

WiFiClient client;                                    // create WIFI Client

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Thingspeak_Send(float Lat, float Long) {         // function to send GPS Location to Thingspeak
  
  digitalWrite(redled, HIGH);                         //turn on the Red Led  to say location processing is starting
  String postStr = apiKey;                            // build the string to send
  postStr +="&field1=";
  dtostrf(Lat,10, 5, outstr);                         //convert float to string
  postStr += outstr;
  postStr +="&field2="; 
  dtostrf(Long,10, 5, outstr);                        //convert float to string
  postStr += outstr;
  postStr += "\r\n\r\n";

  client.connect(server,80);
  client.print("POST /update HTTP/1.1\n");           // send the data to Thingspeak
  client.print("Host: api.thingspeak.com\n");
  client.print("Connection: close\n");
  client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
  client.print("Content-Type: application/x-www-form-urlencoded\n");
  client.print("Content-Length: ");
  client.print(postStr.length());
  client.print("\n\n");
  client.print(postStr);
  
  delay(20000);                                     // thingspeak needs minimum 15 sec delay between updates
  digitalWrite(redled, LOW);                        //turn off the Red Led  to say location processing is finished
  client.stop();
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Store_In_EEPROM(float Lat, float Long) {      // function to store GPS Location in EEPROM for later processing when WIFI is available
                                                   // the structure of the EEPROM is:  byte 0 - an * if data is stored, byte 1 - counter for the number of locations stored,
                                                   //      the rest of the EEPROM;  8 bytes for each location
  if (EEPROM_pointer > 250 ) return;               // if the number of stored GPS locations is greater than 250, then don't store
                                                   //    this will prevent possible overflow issues
  digitalWrite(redled, HIGH);                      //turn on the Red Led to indicate the pushbutton was pressed
  EEPROM.begin(2048);                                              
  if (EEPROM.get(0,EEPROM_data) != EEPROM_star) {  // if '*' is not found in byte 0 of EEPROM (ie no data is stored)
    EEPROM.put(0, EEPROM_star);                    // write a '*' to byte 0 to indicate there is now data in the EEPROM
    EEPROM_pointer = 0;
    EEPROM.put(1, EEPROM_pointer);                 // write a 0 to byte 1 to initialise the location counter
    EEPROM.end();  
  }
  EEPROM.begin(2048);  
  
  EEPROM_pointer = EEPROM.get(1,EEPROM_pointer);  // get the stored pointer
  EEPROM.put((EEPROM_pointer*8)+ 2, Lat);         // write the Latitude at position counter*8 (every location is 8 bytes) + 2 (to get past the first two bytes)
  EEPROM.put((EEPROM_pointer*8)+ 2+4 , Long);     // write the Longitude at position counter*8 (every location is 8 bytes) + 2 (to get past the first two bytes) 
                                                  //    + 4 (to write after the Lat)
  EEPROM.put((EEPROM_pointer*8)+ 2, Lat);         // write the Latitude at position counter*8 (every location is 8 bytes) + 2 (to get past the first two bytes)
  EEPROM.put((EEPROM_pointer*8)+ 2+4 , Long);     // write the Longitude at position counter*8 (every location is 8 bytes) + 2 (to get past the first two bytes) 
  EEPROM_pointer = EEPROM_pointer + 1;            // Increment the pointer                    
  EEPROM.write(1, EEPROM_pointer);                // write the pointer back to the EEPROM
  digitalWrite(redled, LOW);                      //turn off the Red Led to say location processing is finished
  EEPROM.commit(); //note here the commit! 
  EEPROM.end();  
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Send_Location() {                           // function to process the pushbutton interrupt and set the variable to send or store the data
  send_data = true;
  noInterrupts();                                // stop interrupts until data processing is complete,
                                                 //      this is to prevent switch bounce causing multiple interrupts

                                                  
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Process_EEPROM() {

 // if  there are pothole locations in EEPROM then process them up to ThingSpeak
  EEPROM.begin(2048);              

 if (EEPROM.get(0,EEPROM_data) == EEPROM_star) { // if '*' is  found in byte 0 of EEPROM then process the data
     EEPROM_pointer = EEPROM.read(1);            // get the stored pointer

  for (int i=0; i < EEPROM_pointer; i++){
   Lat = EEPROM.get((i*8)+ 2, Lat);              // read the Latitude at position counter*8 (every location is 8 bytes) + 2 (to get past the first two bytes)
   Long = EEPROM.get((1*8)+ 2+4 , Long);         // read the Longitude at position counter*8 (every location is 8 bytes) + 2 (to get past the first two bytes) 
                                                 //    + 4 (to write after the Lat)
  
   Thingspeak_Send(Lat, Long);                   //    Call ThingSpeak_send to put the locations  into Thingspeak
 
  }
  EEPROM.put(0, " ");                            // write space to byte 0 to indicate there is now NO data in the EEPROM

 }
  EEPROM.commit();                               //note here the commit!
  EEPROM.end();  
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void setup(){
  
  gpsSerial.begin(9600);                       // connect gps sensor
 
  pinMode(4, OUTPUT);                          // initialize digital pin 4 (D2, Blue)as an output to indicate GPS data is incoming.
  pinMode(0, OUTPUT);                          // initialize digital pin 0 (D3, Green)as an output to indicate Internet is available.
  pinMode(redled, OUTPUT);                     // initialize digital pin 2 (D4, Red)as an output to indicate pushbutton and processing of the GPS data.
 
  WiFi.begin(ssid, password);                  // start the wifi connection

  for (int x = 0; x < 20; x ++)                // for 10 seconds (20 loops) test to see if WIFI is connected
  {
    if (WiFi.status() == WL_CONNECTED){
      digitalWrite(greenled, HIGH);            // light the Green Led to indicate WIFI is available 
      Process_EEPROM();                        // if WIFI is connected, then see if there are any locations in the EEPROM to be sent
      
      break;
    }  
    delay(500);
  }
  
  pinMode(5,INPUT_PULLUP);                    // set Pin as Input and set pullup resister on
  
  attachInterrupt(digitalPinToInterrupt(5), Send_Location, FALLING);    // define interrupt on pin 5, looking for Rising Input and calling Send_Location when interrupt is detected


  
 }


//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void loop(){                                // the main processing loop. On the pushbutton interrup, it will get the GPS data and either 
                                            //       send to thingspeak if wifi is available, or store in the EEPROM

 
 while(gpsSerial.available() > 0)          // check for gps data
 gps.encode(gpsSerial.read());             // encode gps data
  // display position
  Lat = gps.location.lat();
  Long = gps.location.lng();
  
  if (gps.location.lat() != 0) {           // test for gps data coming through
    digitalWrite(blueled, HIGH);           // light the Blue Led to indicate GPS is sending valid location data
  }

  if (WiFi.status() == WL_CONNECTED) {     // if WIFI available
     digitalWrite(greenled, HIGH);         // light the Green Led to indicate WIFI is available
  }
  else {
     digitalWrite(greenled, LOW);                                           
  }
 
 if (send_data) {                         // if pushbutton has been pushed
   if (WiFi.status() == WL_CONNECTED) {   // if Web Site available
       Thingspeak_Send(Lat,Long);         // send the data to ThingSpeak
      }
   else {   
       Store_In_EEPROM(Lat,Long);         // otherwise store the data in EEPROM for later processing
   }
 }  
 send_data = false;                       // stop the GPS data being sent more than once for each pushbutton press
 interrupts();                            // re-enable interrupts for the pushbutton

}

