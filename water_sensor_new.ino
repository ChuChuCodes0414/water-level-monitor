/*  
This code makes use of an ultrasonic sensor to monitor water level via distance and calculated velocity.
The constants for detection section (velocityMax, distanceMax, distanceMin) can all be configured and change on a needed basis.
*/
 
// Include the Necessary library
#include <SoftwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>

// Define constants for detection
float velocityMax = 0.015; // velocity measured in mm/ms
float distanceMax = 180; // max distance the sensor should be from the water
float distanceMin = 40; // min distance the sensor should be from the water
 
// Define connections to sensor
int pinRX = 10;
int pinTX = 11;
int pinBZ = 8;

// Velocity storage variables
float distanceMOne, distanceMTwo, distanceMThree, velocity, miliOne, miliTwo,miliThree;
unsigned long last;
int trials;

// Wifi Storage Variables
const char* ssid = "DukeOpen";
const char* serverName = "";
unsigned long lastTime = 0;
unsigned long timerDelay = 30000; // min time between slack notifications, in ms
 
// Array to store incoming serial data
unsigned char data_buffer[4] = {0};
 
// Integer to store distance
int distance = 0;
 
// Variable to hold checksum
unsigned char CS;
 
// Object to represent software serial port
SoftwareSerial mySerial(pinRX, pinTX);

// Define LCD 
LiquidCrystal_I2C lcd(0x27, 16, 2);
 
void setup() {
  // Set up serial monitor
  Serial.begin(115200);
  Serial.println("Beginning setup...");
  // Set up software serial port
  mySerial.begin(9600);
  // Establish wifi connection
  WiFi.begin(ssid);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  last = millis();

  lcd.init(); // initialize the lcd
  lcd.backlight();
}
 
void loop() {
  // Run if data available
  if (mySerial.available() > 0) {
    delay(50);
    // Check for packet header character 0xff
    if (mySerial.read() == 0xff) {
      // Insert header into array
      data_buffer[0] = 0xff;
      // Read remaining 3 characters of data and insert into array
      for (int i = 1; i < 4; i++) {
        data_buffer[i] = mySerial.read();
      }
 
      //Compute checksum
      CS = data_buffer[0] + data_buffer[1] + data_buffer[2];
      // If checksum is valid compose distance from data
      if (data_buffer[3] == CS) {
        distance = (data_buffer[1] << 8) + data_buffer[2];     
        // Ensures that the sensor will only make a reading once every 500ms
        if (millis() - last >= 500){
          // If the distance is out of range
          if (distance <= distanceMin || distance >= distanceMax){ 
            // Print distance and sound buzzer
            Serial.print("distance: ");
            Serial.print(distance);
            Serial.println(" mm");
            Serial.println(" Out of range");
            tone(pinBZ, 1000);
            lcd.clear();
            lcd.setCursor(0, 0);         
            lcd.print("Distance: " + String(distance) + "mm");       
            lcd.setCursor(0, 1);
            lcd.print("Out of Range!");   
            // If it has been long enough since the last slack notification
            if ((millis() - lastTime) > timerDelay){
              if(WiFi.status()== WL_CONNECTED){
                // Process and send request for notification
                Serial.println("Sending HTTP Request!");
                HTTPClient http;

                http.begin(serverName);
                http.addHeader("Content-Type", "application/json");
                int httpResponseCode = http.POST("{\"blocks\": [{\"type\": \"header\",\"text\": {\"type\": \"plain_text\",\"text\": \"Water Level Out of Bounds\",\"emoji\": true}},{\"type\": \"section\",\"fields\": [{\"type\": \"mrkdwn\",\"text\": \"*Type:*\nOut of Defined Range\"},{\"type\": \"mrkdwn\",\"text\": \"*Indicated By:*\nWater is " + String(distance) + "mm from the sensor \"}]}]}");
                Serial.print("HTTP Response code: ");
                Serial.println(httpResponseCode);
                // Free resources
                http.end();
                lastTime = millis();
              } else {
                Serial.println("Failed to send message due to wifi connection issue.");
              }
            }
          } else {
            // If enough time measurements have been made to make a velocity calculation
            if (trials >=4){
              // Calculate velocity via previously measured distances at time intervals
              velocity = ((distanceMOne-distance)/(miliThree-miliTwo) + (distanceMTwo-distanceMOne)/(miliTwo-miliOne) + (distanceMThree-distanceMTwo)/(miliOne-millis()))/3;
              // If velocity is positive and too high, the water is moving away from the sensor, indicating a leak
              if (velocity >= velocityMax){
                Serial.print(String(velocity) + " is the current calculated velocity.");
                Serial.println(" Indicating an imminent leak!");
                tone(pinBZ, 1000);
                lcd.clear();
                lcd.setCursor(0, 0);         
                lcd.print("Distance: " + String(distance) + "mm");       
                lcd.setCursor(0, 1);
                lcd.print("Leak Soon!");
                if ((millis() - lastTime) > timerDelay){
                  if(WiFi.status()== WL_CONNECTED){
                    // Process and send the request for a notification
                    Serial.println("Sending HTTP Request!");
                    HTTPClient http;

                    http.begin(serverName);
                    http.addHeader("Content-Type", "application/json");
                    int httpResponseCode = http.POST("{\"blocks\": [{\"type\": \"header\",\"text\": {\"type\": \"plain_text\",\"text\": \"Water Level Velocity Out of Bounds\",\"emoji\": true}},{\"type\": \"section\",\"fields\": [{\"type\": \"mrkdwn\",\"text\": \"*Type:* Leakage\n\"},{\"type\": \"mrkdwn\",\"text\": \"*Indicated By:*\nWater level is changing at " + String(-1 * velocity) + "mm/ms\"}]}]}");
                    Serial.print("HTTP Response code: ");
                    Serial.println(httpResponseCode);
                    // Free resources
                    http.end();
                    lastTime = millis();
                  } else {
                    Serial.println("Failed to send message due to wifi connection issue.");
                  }
                }
              // If the velocity is too high in the wrong direction, the water is moving torwards the sensor, meaning a oerflow is about to occur. 
              } else if (velocity <= -1 * velocityMax){
                Serial.print(String(velocity) + " is the current calculated velocity.");
                Serial.println(" Indicating a overflow!");
                tone(pinBZ, 1000);
                lcd.clear();
                lcd.setCursor(0, 0);         
                lcd.print("Distance: " + String(distance) + "mm");       
                lcd.setCursor(0, 1);
                lcd.print("Overflow Soon!");
                if ((millis() - lastTime) > timerDelay){
                  if(WiFi.status()== WL_CONNECTED){
                    // Process and send the request for a notification
                    HTTPClient http;

                    http.begin(serverName);
                    http.addHeader("Content-Type", "application/json");
                    int httpResponseCode = http.POST("{\"blocks\": [{\"type\": \"header\",\"text\": {\"type\": \"plain_text\",\"text\": \"Water Level Velocity Out of Bounds\",\"emoji\": true}},{\"type\": \"section\",\"fields\": [{\"type\": \"mrkdwn\",\"text\": \"*Type:* Overflow\n\"},{\"type\": \"mrkdwn\",\"text\": \"*Indicated By:*\nWater level is changing at " + String(-1 * velocity) + "mm/ms\"}]}]}");
                    Serial.print("HTTP Response code: ");
                    Serial.println(httpResponseCode);
                    // Free resources
                    http.end();
                    lastTime = millis();
                  } else {
                    Serial.println("Failed to send message due to wifi connection issue.");
                  }
                }
              } else {
                // Turn off the buzzer
                noTone(pinBZ);
                lcd.clear();
                lcd.setCursor(0, 0);         
                lcd.print("Distance: " + String(distance) + "mm"); 
                lcd.setCursor(0, 1); 
                lcd.print("Status Normal");
              }
            } else {
              // Turn off the Buzzer
              noTone(pinBZ);
              lcd.clear();
              lcd.setCursor(0, 0);         
              lcd.print("Distance: " + String(distance) + "mm"); 
              lcd.setCursor(0, 1); 
              lcd.print("Initiating...");
            }
          }
          // Store distances and times to calculate velocities
          distanceMThree = distanceMTwo;
          distanceMTwo = distanceMOne;
          distanceMOne = distance;
          miliThree = miliTwo;
          miliTwo = miliOne;
          miliOne = millis();
          last = millis();
          trials+=1; 
        }
      } else {
        Serial.println("Invalid computation");
      }
    }
  }
}
