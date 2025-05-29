#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h> // Library for the BMP180 sensor
#include <MPU6050.h>         // Library for the MPU6050
#include <SD.h>              // Library for the SD card
#include <ESP32Servo.h>      // Library for the servo motor on ESP32

// Replace with your network credentials
const char* ssid = "InfinityOne"; // Your Wi-Fi network name
const char* password = "12345678"; // Your Wi-Fi password

// Web server port number
WiFiServer server(80);

Adafruit_BMP085 bmp; // BMP180 sensor object
MPU6050 mpu;         // MPU6050 sensor object
Servo myServo;       // Servo motor object

#define SD_CS_PIN 5  // Chip select pin for the SD module
#define SERVO_PIN 2  // Servo motor pin

float initialAltitude = 0.0;      // Initial altitude relative to the ground
float minAltitudeParachute = -3.00; // Minimum altitude to deploy the parachute (in meters)
File dataFile;                      // Object for handling the file on the SD card

// Variables for sensor status
bool bmpSensorOk = false;
bool mpuSensorOk = false;
bool sdCardOk = false;    

// Variable to hold the current system output message for JSON
String currentSystemOutput = "System Nominal";

unsigned long previousDataLogMillis = 0; // Variable to store the last time data was written to SD
const long dataLogInterval = 500;        // Logging interval in milliseconds (0.5 seconds)

void setup() {
  Serial.begin(115200);

  // Configure ESP32 as an access point
  Serial.print("Setting up AP (Access Point)...");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Initialize BMP180 sensor
  bmpSensorOk = bmp.begin();
  if (!bmpSensorOk) {
    Serial.println("Could not find a valid BMP180 sensor, check connection!");
  }

  // Initialize MPU6050 sensor
  Wire.begin();       // Initialize I2C bus
  mpu.initialize();   // Initialize MPU6050

  // Check if MPU6050 is available
  mpuSensorOk = mpu.testConnection();
  if (!mpuSensorOk) {
    Serial.println("MPU6050 connection failed");
  }

  // Initialize SD module
  Serial.println("Initializing SD module...");
  sdCardOk = SD.begin(SD_CS_PIN);
  if (sdCardOk) {
    Serial.println("SD module initialized successfully.");
  } else {
    Serial.println("Failed to initialize SD module.");
  }

  // Collect initial altitude (relative to the ground)
  if (bmpSensorOk) {
    // Read a few times to stabilize if necessary, though Adafruit_BMP085 usually gives a stable reading quickly.
    delay(100); // Small delay before reading initial altitude
    initialAltitude = bmp.readAltitude();
    Serial.print("Initial altitude relative to ground: ");
    Serial.print(initialAltitude);
    Serial.println(" meters");
  }

  // Start web server
  server.begin();
  Serial.println("Web server started. Waiting for clients...");

  // Configure servo motor
  myServo.setPeriodHertz(50);             // Set servo motor frequency to 50 Hz
  myServo.attach(SERVO_PIN, 500, 2400); // Servo motor pin, min and max PWM
  myServo.write(0);                       // Set servo to initial position
  Serial.println("Servo motor configured.");
}

void loop() {
  // Update sensor data
  float temperature = bmpSensorOk ? bmp.readTemperature() : 0.0;
  float pressure = bmpSensorOk ? bmp.readPressure() / 100.0F : 0.0; // Converting to hPa
  float altitudeGround = bmpSensorOk ? (bmp.readAltitude() - initialAltitude) : 0.0; // Altitude relative to ground

  // Read MPU6050 data
  int16_t ax_raw, ay_raw, az_raw;
  float accelX = 0.0, accelY = 0.0, accelZ = 0.0;
  float pitch = 0.0, roll = 0.0;

  if (mpuSensorOk) {
    mpu.getAcceleration(&ax_raw, &ay_raw, &az_raw);
    // Convert raw acceleration to G's (MPU6050 default sensitivity is +/- 2g, 16384 LSB/g)
    accelX = ax_raw / 16384.0F;
    accelY = ay_raw / 16384.0F;
    accelZ = az_raw / 16384.0F;
    pitch = atan2(accelX, sqrt(accelY * accelY + accelZ * accelZ)) * 180.0 / PI;
    roll = atan2(accelY, sqrt(accelX * accelX + accelZ * accelZ)) * 180.0 / PI;
  }


  // Update system output status based on parachute logic
  bool parachuteDeployedThisCycle = false;
  if (mpuSensorOk && (pitch >= 45.0 || pitch <= -45.0)) {
    currentSystemOutput = "Parachute Deployed (High Pitch/Roll)";
    myServo.write(120); // Trigger servo to 120 degrees
    // delay(500); // Delaying here blocks all other operations, including web server responses.
    // Consider a non-blocking way or simply set status and let servo stay if it's a one-time deploy.
    // For now, will keep original servo logic but be mindful of delays.
    // If parachute is a one-time action, perhaps don't return it to 0 immediately or handle state.
    parachuteDeployedThisCycle = true;
  } else if (bmpSensorOk && altitudeGround <= minAltitudeParachute) {
    currentSystemOutput = "Parachute Deployed (Min Altitude)";
    Serial.println("Minimum altitude reached. Deploying parachute!");
    myServo.write(120);
    parachuteDeployedThisCycle = true;
  } else if (!parachuteDeployedThisCycle) { // Only reset if no parachute condition met this cycle
    currentSystemOutput = "System Nominal";
  }

  // If parachute deployed, it might return to 0 after a delay as per original logic.
  // This part might need refinement based on desired servo behavior post-deployment.
  if (parachuteDeployedThisCycle) {
    delay(500);         // Keep servo for a bit
    myServo.write(0);   // Return servo to 0 degrees
    delay(500);         // Delay after returning
  }


  // Write data to SD card at every interval
  unsigned long currentMillis = millis();
  if (sdCardOk && (currentMillis - previousDataLogMillis >= dataLogInterval)) {
    previousDataLogMillis = currentMillis;

    dataFile = SD.open("/datalog.txt", FILE_APPEND); // Open file to append data
    if (dataFile) {
      // Format data to be written
      String dataString = "Time: " + String(millis() / 1000.0, 2) + " s, ";
      dataString += "Temp: " + String(temperature, 2) + " C, ";
      dataString += "Press: " + String(pressure, 2) + " hPa, ";
      dataString += "Alt_G: " + String(altitudeGround, 2) + " m, ";
      dataString += "Pitch: " + String(pitch, 2) + " deg, ";
      dataString += "Roll: " + String(roll, 2) + " deg, ";
      dataString += "AccX: " + String(accelX, 2) + "G, ";
      dataString += "AccY: " + String(accelY, 2) + "G, ";
      dataString += "AccZ: " + String(accelZ, 2) + "G, ";
      dataString += "Output: " + currentSystemOutput + "\n";

      dataFile.print(dataString); // Write data to file
      dataFile.close();
      // Serial.println("Data written to SD."); // Can be too verbose
    } else {
      Serial.println("Error opening datalog.txt for writing.");
    }
  }

  // Handle HTTP client requests
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected.");
    String requestFirstLine = ""; // To store the first line of the HTTP request
    boolean firstLineRead = false;

    // An extra long timeout to make sure the client sends data
    unsigned long clientTimeout = millis() + 200; // 200ms timeout for request
    String currentLine = ""; // To read line by line

    while (client.connected() && millis() < clientTimeout) {
      if (client.available()) {
        char c = client.read();
        // Serial.write(c); // Echo request to Serial for debugging
        if (c == '\n') { // End of a line
          if (!firstLineRead) {
            requestFirstLine = currentLine;
            firstLineRead = true;
            // Serial.print("Request: "); Serial.println(requestFirstLine); // Debug: print first line
          }
          if (currentLine.length() == 0) { // An empty line indicates end of client HTTP request
            // Check if the request is for the root path ("GET / HTTP/1.1")
            if (requestFirstLine.startsWith("GET /")) {
              // Send HTTP headers for JSON response
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println("Access-Control-Allow-Origin: *"); // For CORS, helpful for web apps
              client.println("Connection: close"); // Advise client to close connection after response
              client.println();                   // Blank line required to end headers

              // Create the JSON response string
              String jsonResponse = "{";
              jsonResponse += "\"BMPStatus\":\"" + String(bmpSensorOk ? "online" : "offline") + "\",";
              jsonResponse += "\"MPUStatus\":\"" + String(mpuSensorOk ? "online" : "offline") + "\",";
              jsonResponse += "\"SDStatus\":\"" + String(sdCardOk ? "online" : "offline") + "\",";
              jsonResponse += "\"MPU_X\":" + String(accelX, 2) + ","; // Using 2 decimal places
              jsonResponse += "\"MPU_Y\":" + String(accelY, 2) + ",";
              jsonResponse += "\"MPU_Z\":" + String(accelZ, 2) + ",";
              jsonResponse += "\"MPU_Pitch\":" + String(pitch, 2) + ",";
              jsonResponse += "\"MPU_Roll\":" + String(roll, 2) + ",";
              jsonResponse += "\"BMP_Temp\":" + String(temperature, 2) + ",";
              jsonResponse += "\"BMP_Pressure\":" + String(pressure, 2) + ",";
              jsonResponse += "\"BMP_Altitude\":" + String(altitudeGround, 2) + ",";
              jsonResponse += "\"Output\":\"" + currentSystemOutput + "\"";
              jsonResponse += "}";

              client.println(jsonResponse); // Send the JSON data
              Serial.println("JSON data sent to client.");
            } else {
              // Respond with 404 Not Found for other paths
              client.println("HTTP/1.1 404 Not Found");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println("Not Found");
              Serial.println("Sent 404 Not Found to client for path: " + requestFirstLine);
            }
            break; // Exit the while loop once response is sent
          }
          currentLine = ""; // Reset for next line
        } else if (c != '\r') { // If it's not a carriage return
          currentLine += c; // Add it to the current line
        }
        clientTimeout = millis() + 200; // Reset timeout as we are receiving data
      } // if client.available()
    }   // while client.connected()

    // Give the client time to receive the data and then close the connection.
    delay(10); // Small delay before closing, helps ensure data is sent.
    client.stop();
    Serial.println("Client disconnected.");
  } // if (client)
}