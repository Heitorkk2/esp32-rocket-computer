#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>  // Library for the BMP180 sensor
#include <MPU6050.h>          // Library for the MPU6050
#include <SD.h>               // Library for the SD card
#include <ESP32Servo.h>       // Library for the servo motor on ESP32

// Replace with your network credentials
const char* ssid = "InfinityOne";
const char* password = "12345678";

// Web server port number
WiFiServer server(80);

Adafruit_BMP085 bmp;  // BMP180 sensor object
MPU6050 mpu;          // MPU6050 sensor object
Servo myServo;        // Servo motor object

#define SD_CS_PIN 5   // Chip select pin for the SD module
#define SERVO_PIN 2   // Servo motor pin

float initialAltitude = 0.0; // Initial altitude relative to the ground
float minAltitudeParachute = -3.00; // Minimum altitude to deploy the parachute (in meters)
File dataFile;              // Object for handling the file on the SD card

// Variables for sensor status
bool bmpStatus = false;
bool mpuStatus = false;
bool sdStatus = false;

unsigned long previousMillis = 0; // Variable to store the last time data was written
const long interval = 500;        // Logging interval in milliseconds (0.5 seconds)

void setup() {
  Serial.begin(115200);

  // Configure ESP32 as an access point
  Serial.print("Setting up AP...");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Initialize BMP180 sensor
  bmpStatus = bmp.begin();
  if (!bmpStatus) {
    Serial.println("Could not find a valid BMP180 sensor, check connection!");
  }

  // Initialize MPU6050 sensor
  Wire.begin();  // Initialize I2C bus
  mpu.initialize(); // Initialize MPU6050

  // Check if MPU6050 is available
  mpuStatus = mpu.testConnection();
  if (!mpuStatus) {
    Serial.println("MPU6050 connection failed");
  }

  // Initialize SD module
  Serial.println("Initializing SD module...");
  sdStatus = SD.begin(SD_CS_PIN);
  if (sdStatus) {
    Serial.println("SD module initialized successfully.");
  } else {
    Serial.println("Failed to initialize SD module.");
  }

  // Collect initial altitude (relative to the ground)
  if (bmpStatus) {
    initialAltitude = bmp.readAltitude();
    Serial.print("Initial altitude relative to ground: ");
    Serial.print(initialAltitude);
    Serial.println(" meters");
  }

  // Start web server
  server.begin();
  Serial.println("Web server started.");

  // Configure servo motor
  myServo.setPeriodHertz(50);  // Set servo motor frequency to 50 Hz
  myServo.attach(SERVO_PIN, 500, 2400);  // Servo motor pin, min and max PWM
  myServo.write(0); // Set servo to initial position
  Serial.println("Servo motor configured.");
}

void loop() {
  // Update sensor data
  float temperature = bmpStatus ? bmp.readTemperature() : 0.0;
  float pressure = bmpStatus ? bmp.readPressure() / 100.0F : 0.0;  // Converting to hPa
  float altitudeGround = bmpStatus ? bmp.readAltitude() - initialAltitude : 0.0;  // Altitude relative to ground

  // Read MPU6050 data
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // Calculate pitch and roll angles
  float pitch = atan2(ax, sqrt(ay*ay + az*az)) * 180.0 / PI;
  float roll = atan2(ay, sqrt(ax*ax + az*az)) * 180.0 / PI;
  if (pitch >= 45.0 || pitch <= -45.0) {
      // Trigger servo to 120 degrees
      myServo.write(120);
      delay(500);
      // Return servo to 0 degrees
      myServo.write(0);
      delay(500);
    }

    // Check altitude to deploy parachute
    if (altitudeGround <= minAltitudeParachute) {
      Serial.println("Minimum altitude reached. Deploying parachute!");
      myServo.write(120);
      delay(500);
      // Return servo to 0 degrees
      myServo.write(0);
      delay(500);
    }
  // Write data to SD card at every interval
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Open file to append data
    dataFile = SD.open("/datalog.txt", FILE_APPEND);
    if (dataFile) {
      // Format data to be written
      String dataString = "Time: " + String(millis() / 1000.0) + " seconds\n";
      dataString += "Temperature: " + String(temperature) + " C\n";
      dataString += "Pressure: " + String(pressure) + " hPa\n";
      dataString += "Altitude relative to ground: " + String(altitudeGround) + " meters\n";
      dataString += "Pitch: " + String(pitch) + " degrees\n";
      dataString += "Roll: " + String(roll) + " degrees\n\n";

      // Write data to file
      dataFile.print(dataString);
      dataFile.close();
      Serial.println("Data written to SD.");
    } else {
      Serial.println("Error opening file for writing.");
    }
  }

  // Handle HTTP requests here
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client.");

    // Read the request
    String request = client.readStringUntil('\r');
    Serial.println(request);
    client.flush();

    // Prepare HTML page with current sensor data
    String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<meta http-equiv=\"refresh\" content=\"1\">"; // Refresh page every 1 second
    html += "<style>body { background-color: black; color: white; text-align: center; font-family: Arial, sans-serif; }</style>";
    html += "</head><body><h1>InfinityOne Telemetry</h1>";
    html += "<h2>Sea Level</h2>";
    html += "<p>Temperature: " + String(temperature) + " C</p>";
    html += "<p>Pressure: " + String(pressure) + " hPa</p>";
    html += "<p>Altitude relative to ground: " + String(altitudeGround) + " meters</p>";
    html += "<h2>MPU6050 Data</h2>";
    html += "<p>Acceleration X: " + String(ax / 16384.0) + " g</p>";
    html += "<p>Acceleration Y: " + String(ay / 16384.0) + " g</p>";
    html += "<p>Acceleration Z: " + String(az / 16384.0) + " g</p>";
    html += "<h2>Angles</h2>";
    html += "<p>Pitch: " + String(pitch) + " degrees</p>";
    html += "<p>Roll: " + String(roll) + " degrees</p>";
    html += "<h2>Sensor Status</h2>";
    html += "<p>BMP180: " + String(bmpStatus ? "Connected" : "Disconnected") + "</p>";
    html += "<p>MPU6050: " + String(mpuStatus ? "Connected" : "Disconnected") + "</p>";
    html += "<p>SD: " + String(sdStatus ? "Connected" : "Disconnected") + "</p>";
    html += "</body></html>";

    // HTTP Response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=UTF-8");
    client.println("Connection: close");
    client.println();
    client.println(html);

    client.stop();
    Serial.println("Client disconnected.");
  }
}