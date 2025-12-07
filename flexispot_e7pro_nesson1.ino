#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>

// ======= WiFi configuration =======
const char* ssid     = "YOUR_SSID";       // Your Wi-Fi SSID
const char* password = "YOUR_PASSWORD";   // Your Wi-Fi password

WebServer server(80);

// D1 (GPIO7) will be TX1 -> No.6(TX) of E7 pro 
// D2 (GPIO2) will be RX1 -> No.5(RX) of E7 pro
// D3 (GPI06) will be No.4(DisplayPin20) of E7 pro
// see https://github.com/iMicknl/LoctekMotion_IoT/tree/main

char lastResult[6] = "";   // Stores the last displayed value (e.g. "1.95")

// --- Continuous Up/Down command sending ---
bool upActive = false;
bool downActive = false;
unsigned long lastUpSend = 0;
unsigned long lastDownSend = 0;
const unsigned long CMD_INTERVAL_MS = 108;   // Send command every ~108ms

// Supported Commands
const byte cmd_wakeup[]   = {0x9b, 0x06, 0x02, 0x00, 0x00, 0x6c, 0xa1, 0x9d};
const byte cmd_up[]       = {0x9b, 0x06, 0x02, 0x01, 0x00, 0xfc, 0xa0, 0x9d};
const byte cmd_dwn[]      = {0x9b, 0x06, 0x02, 0x02, 0x00, 0x0c, 0xa0, 0x9d};
const byte cmd_pre1[]     = {0x9b, 0x06, 0x02, 0x04, 0x00, 0xac, 0xa3, 0x9d};
const byte cmd_pre2[]     = {0x9b, 0x06, 0x02, 0x08, 0x00, 0xac, 0xa6, 0x9d};
const byte cmd_pre3[]     = {0x9b, 0x06, 0x02, 0x10, 0x00, 0xac, 0xac, 0x9d}; // stand
const byte cmd_mem[]      = {0x9b, 0x06, 0x02, 0x20, 0x00, 0xac, 0xb8, 0x9d};
const byte cmd_pre4[]     = {0x9b, 0x06, 0x02, 0x00, 0x01, 0xac, 0x60, 0x9d}; // sit
const byte cmd_alarmoff[] = {0x9b, 0x06, 0x02, 0x40, 0x00, 0xaC, 0x90, 0x9d};
const byte cmd_childlock[] = {0x9b, 0x06, 0x02, 0x20, 0x00, 0xac, 0xb8, 0x9d};

// Status Protocol
// byte 0: 0x9b: Start Command
const uint8_t START_BYTE  = 0x9B;  // Frame start
// byte 1: 0xXX: Command Length
// byte 2: 0x12: Height Stream 
const uint8_t CMD_DISPLAY = 0x12;  // 7-seg display, height format
const uint8_t CMD_SLEEP   = 0x13;  // Sleep
// byte 3: 0xXX: First Digit (8 Segment) / Reverse Bit
// byte 4: 0xXX: Second Digit (8 Segment) / Reverse Bit
// byte 5: 0xXX: Third Digit (8 Segment) / Reverse Bit
// byte 6: 0xXX: 16bit Modbus-CRC16 Checksum 1
// byte 7: 0xXX: 16bit Modbus-CRC16 Checksum 2
// byte 8: 0x9b: End of Command

enum RecvState {
  WAIT_START,
  WAIT_LEN,
  WAIT_PAYLOAD
};

RecvState state = WAIT_START;

uint8_t frameLen = 0;
const uint8_t MAX_FRAME_LEN = 32;  // Increase if needed
uint8_t frameBuf[MAX_FRAME_LEN];
uint8_t frameIndex = 0;

//////////////
// Commands //
//////////////
void turnon() {
  // Put desk in operating mode by setting controller DisplayPin20 to HIGH
  Serial.println("Sending Turn On Command");
  digitalWrite(D3, HIGH);
  delay(1000);
  digitalWrite(D3, LOW);
}

void wake() {
  turnon();

  Serial.println("Sending Wakeup Command");
  Serial1.flush();
  Serial1.write(cmd_wakeup, sizeof(cmd_wakeup));
}

void memory() {
  turnon();

  Serial.println("Sending Memory Command");
  Serial1.flush();
  Serial1.write(cmd_mem, sizeof(cmd_mem));
}

void sendUpOnce() {
  Serial.println("Sending Up Command");
  Serial1.flush();
  Serial1.write(cmd_up, sizeof(cmd_up));
}

void sendDownOnce() {
  Serial.println("Sending Down Command");
  Serial1.flush();
  Serial1.write(cmd_dwn, sizeof(cmd_dwn));
}

void pre1() {
  turnon();

  Serial.println("Sending Preset 1 Command");
  Serial1.flush();
  Serial1.write(cmd_pre1, sizeof(cmd_pre1));
}

void pre2() {
  turnon();

  Serial.println("Sending Preset 2 Command");
  Serial1.flush();
  Serial1.write(cmd_pre2, sizeof(cmd_pre2));
}

void pre3() {
  turnon();

  Serial.println("Sending Preset 3 Command");
  Serial1.flush();
  Serial1.write(cmd_pre3, sizeof(cmd_pre3));
}

void pre4() {
  turnon();

  Serial.println("Sending Preset 4 Command");
  Serial1.flush();
  Serial1.write(cmd_pre4, sizeof(cmd_pre4));
}

// ====== HTTP handlers ======
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Flexispot Web Controller</title>
<style>
  body {
    font-family: sans-serif;
    text-align: center;
    margin-top: 40px;
  }
  h1 {
    margin-bottom: 10px;
  }

  /* Top row: height + Wakeup */
  .top-row {
    display: flex;
    justify-content: center;
    align-items: center;
    gap: 20px;
    margin-bottom: 20px;
    flex-wrap: wrap;
  }

  .height-label {
    font-size: 1rem;
  }

  .height-box {
    margin: 10px auto;
    font-size: 3rem;
    font-weight: bold;
    border: 1px solid #ccc;
    display: inline-block;
    padding: 10px 30px;
    border-radius: 8px;
    min-width: 140px;
  }

  /* Round buttons */
  .btn-circle {
    font-size: 1.1rem;
    padding: 0.8rem 1.6rem;
    margin: 0.3rem;
    border-radius: 999px;
    border: 1px solid #888;
    cursor: pointer;
    min-width: 140px;
  }

  .btn-main {
    font-size: 1.2rem;
    padding: 0.9rem 1.8rem;
    font-weight: bold;
  }

  /* Button rows */
  .button-row {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    max-width: 700px;
    margin: 10px auto;
  }

  p.note {
    margin-top: 10px;
    font-size: 0.9rem;
    color: #555;
  }
</style>
</head>
<body>
  <h1>Flexispot Web Controller</h1>

  <!-- Top row: height + Wakeup -->
  <div class="top-row">
    <div>
      <div class="height-label">Current Height</div>
      <div class="height-box" id="height">--.- cm</div>
    </div>
    <button class="btn-circle btn-main" onclick="fetch('/wake')">Wakeup</button>
  </div>

  <!-- Row 1: Up / Down / Memory -->
  <div class="button-row">
    <button class="btn-circle" id="btnUp">▲ Up</button>
    <button class="btn-circle" id="btnDown">▼ Down</button>
    <button class="btn-circle" onclick="fetch('/mem')">Memory</button>
  </div>

  <!-- Row 2: Preset 1–4 -->
  <div class="button-row">
    <button class="btn-circle" onclick="fetch('/pre1')">Preset 1</button>
    <button class="btn-circle" onclick="fetch('/pre2')">Preset 2</button>
    <button class="btn-circle" onclick="fetch('/pre3')">Preset 3</button>
    <button class="btn-circle" onclick="fetch('/pre4')">Preset 4</button>
  </div>

  <p class="note">Up/Down moves the desk only while the button is pressed.</p>

  <script>
    // Update height display
    async function updateHeight() {
      try {
        const res = await fetch('/height');
        if (!res.ok) return;
        const data = await res.json();
        document.getElementById('height').innerText = data.height;
      } catch (e) {}
    }

    // Hold-to-move behavior
    function setupHoldButton(btnId, startUrl, stopUrl) {
      const btn = document.getElementById(btnId);
      if (!btn) return;

      const start = (e) => { e.preventDefault(); fetch(startUrl); };
      const stop  = (e) => { e.preventDefault(); fetch(stopUrl); };

      btn.addEventListener('mousedown', start);
      btn.addEventListener('mouseup', stop);
      btn.addEventListener('mouseleave', stop);

      btn.addEventListener('touchstart', start);
      btn.addEventListener('touchend', stop);
      btn.addEventListener('touchcancel', stop);
    }

    updateHeight();
    setInterval(updateHeight, 1000);

    setupHoldButton('btnUp', '/up/start', '/up/stop');
    setupHoldButton('btnDown', '/down/start', '/down/stop');
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleWake() {
  wake();
  server.send(200, "text/plain", "Wake command sent.");
}

void handlePre1() {
  pre1();
  server.send(200, "text/plain", "Preset 1 command sent.");
}

void handlePre2() {
  pre2();
  server.send(200, "text/plain", "Preset 2 command sent.");
}

void handlePre3() {
  pre3();
  server.send(200, "text/plain", "Preset 3 command sent.");
}

void handlePre4() {
  pre4();
  server.send(200, "text/plain", "Preset 4 command sent.");
}

void handleHeight() {
  // Convert lastResult to String
  String h = String(lastResult);

  // If the desk is sleeping or display is blank, show "Sleeping..."
  if (strcmp(lastResult, "   ") == 0 || lastResult[0] == '\0') {
    h = "Sleeping...";
  } else if (strcmp(lastResult, "5- ") == 0) {
    // Example of handling a special pattern, can be adjusted as needed
    h = "S- ";
  } else {
    h += " cm";
  }

  // Return JSON
  String json = String("{\"height\":\"") + h + "\"}";
  server.send(200, "application/json", json);
}

void handleUpStart() {
  Serial.println("HTTP: Up start");
  // Ensure desk is awake
  turnon();
  upActive = true;
  downActive = false;   // Stop opposite direction
  lastUpSend = 0;       // Allow immediate send
  server.send(200, "text/plain", "Up start");
}

void handleUpStop() {
  Serial.println("HTTP: Up stop");
  upActive = false;
  server.send(200, "text/plain", "Up stop");
}

void handleDownStart() {
  Serial.println("HTTP: Down start");
  turnon();
  downActive = true;
  upActive = false;     // Stop opposite direction
  lastDownSend = 0;
  server.send(200, "text/plain", "Down start");
}

void handleDownStop() {
  Serial.println("HTTP: Down stop");
  downActive = false;
  server.send(200, "text/plain", "Down stop");
}

void handleMem() {
  Serial.println("HTTP: Memory");
  memory();
  server.send(200, "text/plain", "Memory command sent.");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  M5.begin();
  M5.Display.setRotation(1);  // Landscape mode

  // Text properties
  M5.Display.setTextDatum(MC_DATUM);              // Center-based text alignment
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);  // White text on black background
  M5.Display.setTextSize(2);                      // 2x size

  // Initial screen
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.drawString("Flexispot Controller", M5.Display.width() / 2, M5.Display.height() / 2);

  // Debug serial
  Serial.begin(115200);
  Serial.println("Serial Ready.");

  // Serial for E7 pro
  // Initialize Serial1 on D1 and D2
  // Format: Serial1.begin(baudrate, config, rxPin, txPin);
  Serial1.begin(9600, SERIAL_8N1, D2, D1);
  Serial.println("Serial1 Ready.");

  // DisplayPin20 configured as an output
  pinMode(D3, OUTPUT);
  digitalWrite(D3, LOW);

  // ====== WiFi connection ======
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Show IP address on screen (first time only, simple layout)
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(TL_DATUM); // Top-left origin
  M5.Display.setTextSize(2);
  M5.Display.drawString("Flexispot Controller", 0, 0);
  M5.Display.setTextSize(1);
  M5.Display.drawString("IP: " + WiFi.localIP().toString(), 0, 30);

  // ====== HTTP server routes ======
  server.on("/", handleRoot);
  server.on("/wake", handleWake);
  server.on("/pre1", handlePre1);
  server.on("/pre2", handlePre2);
  server.on("/pre3", handlePre3);
  server.on("/pre4", handlePre4);
  server.on("/height", handleHeight);
  server.on("/up/start", handleUpStart);
  server.on("/up/stop",  handleUpStop);
  server.on("/down/start", handleDownStart);
  server.on("/down/stop",  handleDownStop);
  server.on("/mem", handleMem);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle HTTP requests
  server.handleClient();

  // Handle M5 buttons
  M5.update();

  // --- Continuous Up/Down command sending ---
  unsigned long now = millis();

  if (upActive && (now - lastUpSend >= CMD_INTERVAL_MS)) {
    lastUpSend = now;
    sendUpOnce();
  }

  if (downActive && (now - lastDownSend >= CMD_INTERVAL_MS)) {
    lastDownSend = now;
    sendDownOnce();
  }

  if (M5.BtnA.wasPressed()) {
    Serial.println("Button A Pressed");
    wake();
  }
  if (M5.BtnA.wasReleased()) {
    Serial.println("Button A Released");
  }
  if (M5.BtnB.wasPressed()) {
    Serial.println("Button B Pressed");
    //memory();
    pre4();
  }
  if (M5.BtnB.wasReleased()) {
    Serial.println("Button B Released");
  }

  // Handle incoming status frames from desk controller
  while (Serial1.available() > 0) {
    uint8_t b = (uint8_t)Serial1.read();

    switch (state) {
      case WAIT_START:
        if (b == START_BYTE) {
          state = WAIT_LEN;
          // Debug:
          // Serial.println(F("START_BYTE received"));
        }
        break;

      case WAIT_LEN:
        frameLen = b;
        if (frameLen == 0 || frameLen > MAX_FRAME_LEN) {
          Serial.println(F("Invalid frame length"));
          resetState();
        } else {
          frameIndex = 0;
          state = WAIT_PAYLOAD;
          // Serial.print(F("LEN=")); Serial.println(frameLen);
        }
        break;

      case WAIT_PAYLOAD:
        frameBuf[frameIndex++] = b;
        if (frameIndex >= frameLen) {
          // Process full frame
          handleFrame();
          // Reset for next frame
          resetState();
        }
        break;
    }
  }
}

// 8-bit bit-reverse function (b7..b0 -> b0..b7)
uint8_t reverseBits(uint8_t x) {
  x = (x & 0xF0) >> 4 | (x & 0x0F) << 4;
  x = (x & 0xCC) >> 2 | (x & 0x33) << 2;
  x = (x & 0xAA) >> 1 | (x & 0x55) << 1;
  return x;
}

// Convert 7-seg pattern + dot to a digit character
// raw : received 8-bit data (e.g. 0x86)
// dot : will be set to true if the decimal point is on for this digit
char segToDigit(uint8_t raw, bool &dot) {
  // First, reverse bits
  uint8_t r = reverseBits(raw);

  // LSB (bit 0) after reversal is treated as the dot
  dot = (r & 0x01) != 0;

  // Mask out the dot bit, leave only segment bits
  uint8_t pat = r & 0xFE;

  // Standard 7-seg patterns (after bit reversal)
  // 0:0xFC, 1:0x60, 2:0xDA, 3:0xF2, 4:0x66,
  // 5:0xB6, 6:0xBE, 7:0xE0, 8:0xFE, 9:0xF6
  // Serial.print(pat);
  // Serial.print(" ");
  switch (pat) {
    case 0xFC: return '0';
    case 0x60: return '1';
    case 0xDA: return '2';
    case 0xF2: return '3';
    case 0x66: return '4';
    case 0xB6: return '5';
    case 0xBE: return '6';
    case 0xE0: return '7';
    case 0xFE: return '8';
    case 0xF6: return '9';
    case 0x00: return ' '; // Blanked digit
    case 0x02: return '-'; // Hyphen
    case 0x1C: return 'L'; // L
    case 0x3A: return 'o'; // o
    case 0x9C: return 'C'; // C
    default:   return '?'; // Unknown pattern
  }
}

// Process one received frame
void handleFrame() {
  // At minimum we need [CMD(=0x12), seg1, seg2, seg3]
  if (frameLen < 4) {
    Serial.println(F("Frame length too short"));
    return;
  }

  uint8_t cmd = frameBuf[0];

  if (cmd == CMD_DISPLAY) {
    // Bytes 1,2,3 are 7-seg data
    uint8_t segBytes[3] = {
      frameBuf[1],
      frameBuf[2],
      frameBuf[3]
    };

    char digits[3];
    bool dots[3];

    for (int i = 0; i < 3; i++) {
      digits[i] = segToDigit(segBytes[i], dots[i]);
    }

    // Output buffer (up to "9.99" = 4 chars + null)
    char result[6];
    int pos = 0;

    for (int i = 0; i < 3; i++) {
      result[pos++] = digits[i];
      if (dots[i]) {
        result[pos++] = '.';
      }
    }
    result[pos] = '\0';

    Serial.print(F("Display data: "));
    Serial.println(result);  // e.g. "1.95"

    // Avoid flickering: only update when the value changes
    if (strcmp(result, lastResult) != 0) {
      // Save new value
      strncpy(lastResult, result, sizeof(lastResult));
      lastResult[sizeof(lastResult) - 1] = '\0';

      // Update screen (no full clear; background color in drawString is enough)
      M5.Display.setTextDatum(MC_DATUM);
      M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
      M5.Display.setTextFont(7); // 48px 7-seg-like font

      if(strcmp(result, "   ") == 0) {
        M5.Display.fillScreen(TFT_BLACK);
        Serial.println("Clear Display");
      } else {
        M5.Display.drawString(
          lastResult,
          M5.Display.width() / 2,
          M5.Display.height() / 2
        );
      }
    }
  } else if (cmd == CMD_SLEEP) {
    // Not always sent, but indicates going to sleep
    // M5.Display.fillScreen(TFT_BLACK);
    Serial.println("Clear Display (Sleep)");
  } else {
    // For now ignore non-display commands (but dump for debugging)
    Serial.print("Unknown CMD: 0x");
    Serial.println(cmd, HEX);

    Serial.print("Raw frame: ");
    for (int i = 0; i < frameLen; i++) {
        if (frameBuf[i] < 16) Serial.print('0'); // leading zero for single hex digit
        Serial.print(frameBuf[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
  }
}

void resetState() {
  state = WAIT_START;
  frameLen = 0;
  frameIndex = 0;
}
