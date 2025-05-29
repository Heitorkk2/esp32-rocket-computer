const http = require('http');

// --- Simulation State ---
let telemetryData = {
  BMPStatus: "online",
  MPUStatus: "online",
  SDStatus: "online",
  MPU_X: 0.01,
  MPU_Y: -0.02,
  MPU_Z: 0.98,
  MPU_Pitch: 1.5,
  MPU_Roll: -0.5,
  BMP_Temp: 24.5,
  BMP_Pressure: 1012.50,
  BMP_Altitude: 10.0,
  Output: "System Nominal"
};

let outputMessages = [
  "System Nominal",
  "Ascending",
  "Max Altitude Reached",
  "Descending",
  "Parachute Deployed (Simulated)",
  "Landed"
];
let currentOutputIndex = 0;
let altitudeDirection = 1; // 1 for increasing, -1 for decreasing

// --- Function to update simulated data ---
function updateTelemetryData() {
  // Simulate sensor status (occasional offline)
  telemetryData.BMPStatus = Math.random() > 0.05 ? "online" : "offline";
  telemetryData.MPUStatus = Math.random() > 0.03 ? "online" : "offline";
  telemetryData.SDStatus = Math.random() > 0.02 ? "online" : "online"; // SD card is usually more stable

  // Simulate MPU data (small random variations around a baseline)
  telemetryData.MPU_X = parseFloat((Math.random() * 0.2 - 0.1).toFixed(2)); // Small G variations
  telemetryData.MPU_Y = parseFloat((Math.random() * 0.2 - 0.1).toFixed(2));
  telemetryData.MPU_Z = parseFloat((1.0 + (Math.random() * 0.1 - 0.05)).toFixed(2)); // Around 1G

  // Simulate Pitch and Roll (slow drift)
  telemetryData.MPU_Pitch = parseFloat((telemetryData.MPU_Pitch + (Math.random() * 1 - 0.5)).toFixed(2));
  if (telemetryData.MPU_Pitch > 90) telemetryData.MPU_Pitch = 90;
  if (telemetryData.MPU_Pitch < -90) telemetryData.MPU_Pitch = -90;

  telemetryData.MPU_Roll = parseFloat((telemetryData.MPU_Roll + (Math.random() * 1 - 0.5)).toFixed(2));
  if (telemetryData.MPU_Roll > 180) telemetryData.MPU_Roll = 180;
  if (telemetryData.MPU_Roll < -180) telemetryData.MPU_Roll = -180;


  // Simulate BMP data
  telemetryData.BMP_Temp = parseFloat((20 + Math.random() * 10).toFixed(2)); // Temp between 20-30 C
  telemetryData.BMP_Pressure = parseFloat((1000 + Math.random() * 20 - 10).toFixed(2)); // Pressure around 1000-1020 hPa

  // Simulate Altitude (going up and down)
  if (telemetryData.BMP_Altitude > 500) altitudeDirection = -1; // Start descending
  if (telemetryData.BMP_Altitude < 0 && altitudeDirection === -1) { // Landed or went below start
      altitudeDirection = 0; // Stop changing if landed
      telemetryData.Output = "Landed (Simulated)";
  } else if (telemetryData.BMP_Altitude <=0 && altitudeDirection === 0 && Math.random() < 0.1) {
      altitudeDirection = 1; // Chance to "relaunch"
      telemetryData.Output = "System Nominal";
      currentOutputIndex = 0;
  }


  if (altitudeDirection !== 0) {
    telemetryData.BMP_Altitude = parseFloat((telemetryData.BMP_Altitude + altitudeDirection * (Math.random() * 5 + 1)).toFixed(2));
  }
   if (telemetryData.BMP_Altitude < 0) telemetryData.BMP_Altitude = 0.00;


  // Cycle through output messages if not landed or specific event
  if (altitudeDirection !== 0 && telemetryData.Output.indexOf("Landed") === -1) {
    if (Math.random() < 0.15) { // Change output message sometimes
        currentOutputIndex = (currentOutputIndex + 1) % outputMessages.length;
        telemetryData.Output = outputMessages[currentOutputIndex];
    }
    // Specific output for altitude milestones
    if (telemetryData.BMP_Altitude > 480 && telemetryData.Output !== outputMessages[2] && altitudeDirection === 1) {
        telemetryData.Output = outputMessages[2]; // Max Altitude
    } else if (telemetryData.BMP_Altitude < 50 && telemetryData.BMP_Altitude > 1 && altitudeDirection === -1 && telemetryData.Output.indexOf("Parachute") === -1) {
        if(Math.random() < 0.5) telemetryData.Output = outputMessages[4]; // Parachute
    }
  }
}

// --- HTTP Server Setup ---
const server = http.createServer((req, res) => {
  // Set CORS headers to allow requests from any origin (useful for Flutter web or other clients)
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Request-Method', '*');
  res.setHeader('Access-Control-Allow-Methods', 'OPTIONS, GET');
  res.setHeader('Access-Control-Allow-Headers', '*');

  // Handle OPTIONS request for CORS preflight
  if (req.method === 'OPTIONS') {
    res.writeHead(204); // No Content
    res.end();
    return;
  }

  // Only respond to GET requests on the root path "/"
  if (req.method === 'GET' && req.url === '/') {
    updateTelemetryData(); // Update data on each request

    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(telemetryData));
    console.log('Telemetry data sent:', telemetryData);
  } else {
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Not Found. Use GET /');
    console.log(`404 - Not Found for ${req.method} ${req.url}`);
  }
});

const PORT = 3000; // You can change this port if needed
const HOST = '0.0.0.0'; // Listen on all available network interfaces

server.listen(PORT, HOST, () => {
  console.log(`Mock Flight Computer server running at http://${HOST}:${PORT}/`);
  console.log(`You can also try accessing it via http://localhost:${PORT}/ or http://<your-local-ip>:${PORT}/ from your Flutter app.`);
});