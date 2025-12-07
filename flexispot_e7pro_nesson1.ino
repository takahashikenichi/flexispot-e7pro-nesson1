#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>

// ======= WiFi設定 =======
const char* ssid     = "YOUR_SSID";       // ← 自宅WiFiのSSIDに変更
const char* password = "YOUR_PASSWORD";   // ← 自宅WiFiのパスワードに変更

WebServer server(80);

// D1 (GPIO7) will be TX1 -> No.6(TX) of e2pro 
// D2 (GPIO2) will be RX1 -> No.5(RX) of e2pro
// D3 (GPI06) will be No.4(DisplayPin20) of e2pro
// see https://github.com/iMicknl/LoctekMotion_IoT/tree/main

char lastResult[6] = "";   // 直前に表示した値（"1.95" など）を保存

// --- Up/Down 連続送信用 ---
bool upActive = false;
bool downActive = false;
unsigned long lastUpSend = 0;
unsigned long lastDownSend = 0;
const unsigned long CMD_INTERVAL_MS = 108;   // 約108msごとに送信

// Supported Commands
const byte cmd_wakeup[] = {0x9b, 0x06, 0x02, 0x00, 0x00, 0x6c, 0xa1, 0x9d};
const byte cmd_up[] = {0x9b, 0x06, 0x02, 0x01, 0x00, 0xfc, 0xa0, 0x9d};
const byte cmd_dwn[] = {0x9b, 0x06, 0x02, 0x02, 0x00, 0x0c, 0xa0, 0x9d};
const byte cmd_pre1[] = {0x9b, 0x06, 0x02, 0x04, 0x00, 0xac, 0xa3, 0x9d};
const byte cmd_pre2[] = {0x9b, 0x06, 0x02, 0x08, 0x00, 0xac, 0xa6, 0x9d};
const byte cmd_pre3[] = {0x9b, 0x06, 0x02, 0x10, 0x00, 0xac, 0xac, 0x9d}; // stand
const byte cmd_mem[] = {0x9b, 0x06, 0x02, 0x20, 0x00, 0xac, 0xb8, 0x9d};
const byte cmd_pre4[] = {0x9b, 0x06, 0x02, 0x00, 0x01, 0xac, 0x60, 0x9d}; // sit
const byte cmd_alarmoff[] = {0x9b, 0x06, 0x02, 0x40, 0x00, 0xaC, 0x90, 0x9d};
const byte cmd_childlock[] = {0x9b, 0x06, 0x02, 0x20, 0x00, 0xac, 0xb8, 0x9d};

// Status Protocol
// 0 byte 0x9b: Start Command
const uint8_t START_BYTE  = 0x9B;  // フレーム開始
// 1 byte 0xXX: Command Length
// 2 byte 0x12: Height Stream 
const uint8_t CMD_DISPLAY = 0x12;  // 7セグ表示, 以降は高さ表示のフォーマット
const uint8_t CMD_SLEEP = 0x13;  // スリープ
// 3 byte 0xXX: First Digit (8 Segment) / Reverse Bit
// 4 byte 0xXX: Second Digit (8 Segment) / Reverse Bit
// 5 byte 0xXX: Third Digit (8 Segment) / Reverse Bit
// 6 byte 0xXX: 16bit Modbus-CRC16 Checksum 1
// 7 byte 0xXX: 16bit Modbus-CRC16 Checksum 2
// 8 byte 0x9b: End of Command

enum RecvState {
  WAIT_START,
  WAIT_LEN,
  WAIT_PAYLOAD
};

RecvState state = WAIT_START;

uint8_t frameLen = 0;
const uint8_t MAX_FRAME_LEN = 32;  // 必要に応じて増やす
uint8_t frameBuf[MAX_FRAME_LEN];
uint8_t frameIndex = 0;

//////////////
// Commands //
//////////////
void turnon() {
  // Turn desk in operating mode by setting controller DisplayPin20 to HIGH
  Serial.println("Sending Turn on Command");
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

// ====== HTTPハンドラ ======
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ja">
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

  /* 上段：高さ + Wakeup */
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

  /* 丸ボタン */
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

  /* 段ごとのボタン配置 */
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

  <!-- 上段：高さ + Wakeup -->
  <div class="top-row">
    <div>
      <div class="height-label">現在の高さ</div>
      <div class="height-box" id="height">--.- cm</div>
    </div>
    <button class="btn-circle btn-main" onclick="fetch('/wake')">Wakeup</button>
  </div>

  <!-- 1段目：Up / Down / Memory -->
  <div class="button-row">
    <button class="btn-circle" id="btnUp">▲ Up</button>
    <button class="btn-circle" id="btnDown">▼ Down</button>
    <button class="btn-circle" onclick="fetch('/mem')">Memory</button>
  </div>

  <!-- 2段目：Preset1〜4 -->
  <div class="button-row">
    <button class="btn-circle" onclick="fetch('/pre1')">Preset 1</button>
    <button class="btn-circle" onclick="fetch('/pre2')">Preset 2</button>
    <button class="btn-circle" onclick="fetch('/pre3')">Preset 3</button>
    <button class="btn-circle" onclick="fetch('/pre4')">Preset 4</button>
  </div>

  <p class="note">Up/Downは押している間だけ動きます。</p>

  <script>
    // 高さの更新
    async function updateHeight() {
      try {
        const res = await fetch('/height');
        if (!res.ok) return;
        const data = await res.json();
        document.getElementById('height').innerText = data.height;
      } catch (e) {}
    }

    // 押しっぱなし処理
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
  // lastResult を文字列化
  String h = String(lastResult);

  // デスクがスリープ中などで "   " の場合は Sleeping... にする
  if (strcmp(lastResult, "   ") == 0 || lastResult[0] == '\0') {
    h = "Sleeping...";
  } else if (strcmp(lastResult, "5- ") == 0) {
    h = "S- ";
  } else {
    h += " cm";
  }

  // JSONで返す
  String json = String("{\"height\":\"") + h + "\"}";
  server.send(200, "application/json", json);
}

void handleUpStart() {
  Serial.println("HTTP: Up start");
  // 必要なら最初に起動
  turnon();
  upActive = true;
  downActive = false;    // 逆方向は止める
  lastUpSend = 0;        // すぐ送れるように
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
  upActive = false;      // 逆方向は止める
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
  memory();  // ← 既存の memory() を呼ぶ
  server.send(200, "text/plain", "Memory command sent.");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void setup() {
  M5.begin();
  M5.Display.setRotation(1);  // 横画面モードに設定

  // テキストのプロパティ設定
  M5.Display.setTextDatum(MC_DATUM);              // テキスト配置の中央基準
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);  // 白文字、黒背景
  M5.Display.setTextSize(2); // 大きさ2倍

  // 画面をクリアしてテキストを描画
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.drawString("Flexispot Controller", M5.Display.width() / 2, M5.Display.height() / 2);

  // デバッグシリアル設定
  Serial.begin(115200);
  Serial.println("Serial Ready.");

  // e2pro向けシリアル
  // Initialize Serial1 on D1 and D2
  // Format: Serial1.begin(baudrate, config, rxPin, txPin);
  Serial1.begin(9600, SERIAL_8N1, D2, D1);
  Serial.println("Serial1 Ready.");

  // DisplayPin20 configured as an output
  pinMode(D3, OUTPUT);
  digitalWrite(D3, LOW);

  // ====== WiFi接続 ======
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

  // 画面にIP表示（最初だけざっくり）
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(TL_DATUM); // 左上基準
  M5.Display.setTextSize(2);
  M5.Display.drawString("Flexispot Controller", 0, 0);
  M5.Display.setTextSize(1);
  M5.Display.drawString("IP: " + WiFi.localIP().toString(), 0, 30);

  // ====== HTTPサーバ設定 ======
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
  // Webサーバ処理
  server.handleClient();

  // ボタン処理
  M5.update();

  // --- Up/Down 連続送信処理 ---
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
    Serial.println("A Btn Pressed");
    wake();
  }
  if (M5.BtnA.wasReleased()) {
    Serial.println("A Btn Released");
  }
  if (M5.BtnB.wasPressed()) {
    Serial.println("B Btn Pressed");
    //memory();
    pre4();
  }
  if (M5.BtnB.wasReleased()) {
    Serial.println("B Btn Released");
  }

  // 受信データ処理
  while (Serial1.available() > 0) {
    uint8_t b = (uint8_t)Serial1.read();

    switch (state) {
      case WAIT_START:
        if (b == START_BYTE) {
          state = WAIT_LEN;
          // デバッグ用
          //Serial.println(F("START_BYTE 受信"));
        }
        break;

      case WAIT_LEN:
        frameLen = b;
        if (frameLen == 0 || frameLen > MAX_FRAME_LEN) {
          Serial.println(F("不正な長さ"));
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
          // フレームを処理
          handleFrame();
          // 次のフレームに備えて状態を戻す
          resetState();
        }
        break;
    }
  }
}

// 8bit のビット反転関数（b7..b0 -> b0..b7）
uint8_t reverseBits(uint8_t x) {
  x = (x & 0xF0) >> 4 | (x & 0x0F) << 4;
  x = (x & 0xCC) >> 2 | (x & 0x33) << 2;
  x = (x & 0xAA) >> 1 | (x & 0x55) << 1;
  return x;
}

// 7セグ＋ドットのパターンを数字に変換
// raw : 受信した生の 8bit データ（例：0x86）
// dot : その桁にドットが点灯しているかどうかを返す
char segToDigit(uint8_t raw, bool &dot) {
  // まずビット列を反転
  uint8_t r = reverseBits(raw);

  // 反転後の LSB(ビット0) をドットとみなす
  dot = (r & 0x01) != 0;

  // ドットビットを落として、数字部分だけにする
  uint8_t pat = r & 0xFE;

  // 標準的な7セグのパターン（ビット反転後）との対応表
  // 0:0xFC, 1:0x60, 2:0xDA, 3:0xF2, 4:0x66,
  // 5:0xB6, 6:0xBE, 7:0xE0, 8:0xFE, 9:0xF6
//  Serial.print(pat);
//  Serial.print(" ");
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
    case 0x00: return ' '; // 非表示化のデータ。
    case 0x02: return '-'; // ハイフン
    case 0x1C: return 'L'; // L
    case 0x3A: return 'o'; // o
    case 0x9C: return 'C'; // C
    default:   return '?';  // 不明なパターン
  }
}

// 受信した1フレームを処理
void handleFrame() {
  // 最低でも [CMD(=0x12), seg1, seg2, seg3] の4バイト必要
  if (frameLen < 4) {
    Serial.println(F("フレーム長が足りません"));
    return;
  }

  uint8_t cmd = frameBuf[0];

  if (cmd == CMD_DISPLAY) {
    // 2,3,4バイト目を7セグデータとして処理
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

    // 出力用バッファ（最大 "9.99" で5文字＋終端）
    char result[6];
    int pos = 0;

    for (int i = 0; i < 3; i++) {
      result[pos++] = digits[i];
      if (dots[i]) {
        result[pos++] = '.';
      }
    }
    result[pos] = '\0';

    Serial.print(F("表示データ: "));
    Serial.println(result);  // 例: "1.95"

    // ★ 前回表示と同じなら何もしない（点滅防止）
    if (strcmp(result, lastResult) != 0) {
      // ★ 新しい値を保存
      strncpy(lastResult, result, sizeof(lastResult));
      lastResult[sizeof(lastResult) - 1] = '\0';

      // ★ 画面更新（fillScreen はやめる）
      //   背景色付きで drawString しているので、文字の周囲は上書きされます
      M5.Display.setTextDatum(MC_DATUM);
      M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
      M5.Display.setTextFont(7); // 48ピクセル7セグ風フォント

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
    // 必ずこのパケットが飛ぶわけではなさそう
//    M5.Display.fillScreen(TFT_BLACK);
      Serial.println("Clear Display");
  } else {
    // 今回は CMD_DISPLAY 以外は無視（必要なら他コマンドも追加）
    //Serial.print(F("未知のコマンド: 0x"));
    //Serial.println(cmd, HEX);
        // CMD_DISPLAY 以外のコマンド → frameBuf をダンプ表示
    Serial.print("Unknown CMD: 0x");
    Serial.println(cmd, HEX);

    Serial.print("Raw frame: ");
    for (int i = 0; i < frameLen; i++) {
        if (frameBuf[i] < 16) Serial.print('0'); // 1桁のとき 0 を付ける
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


