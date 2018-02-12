// ****************************************************************
// IRremote_test
//  Ver0.00 2018.01.14
//  copyright 坂柳
//  Hardware構成
//  マイコン：wroom-02
//  IO:
//   P4  赤外線LED
//   P14 IRレシーバ
// ****************************************************************
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#if DECODE_AC
#include <ir_Daikin.h>
#include <ir_Fujitsu.h>
#include <ir_Kelvinator.h>
#include <ir_Midea.h>
#include <ir_Toshiba.h>
#endif  // DECODE_AC

//*******************************************************************************
//* 定数、変数
//*******************************************************************************
//サーバー
ESP8266WebServer server(80);  //webサーバー
IPAddress HOST_IP = IPAddress(192, 168, 0, 10);
IPAddress SUB_MASK = IPAddress(255, 255, 255, 0);
MDNSResponder mdns;
//ポート
#define RECV_PIN 14
#define SEND_PIN 4
//シリアル
#define BAUD_RATE 115200
//赤外線通信
#define CAPTURE_BUFFER_SIZE 1024
#define TIMEOUT 15U  // Suits most messages, while not swallowing many repeats.
#define MIN_UNKNOWN_SIZE 20
IRrecv irrecv(RECV_PIN, CAPTURE_BUFFER_SIZE, TIMEOUT, true);
IRsend irsend(SEND_PIN);
decode_results results;
String ircode = "********";
//*******************************************************************************
//* プロトタイプ宣言
//*******************************************************************************

//*******************************************************************************
//* HomePage
//*******************************************************************************
// -----------------------------------
// Top Page
//------------------------------------
void handleRoot() {
  Serial.println("TopPage");
  if (server.hasArg("code")) {
    uint32_t code = strtoul(server.arg("code").c_str(), NULL, 0);
    irsend.sendNEC(code, 32);
  }

  File fd = SPIFFS.open("/index.html", "r");
  String html = fd.readString();
  fd.close();
  html.replace("********", ircode);
  ircode = "********";
  server.send(200, "text/html", html);
}
void handleAll() {
  Serial.println("AllPage");
  if (server.hasArg("code")) {
    uint32_t code = strtoul(server.arg("code").c_str(), NULL, 0);
    irsend.sendNEC(code, 32);
  }

  File fd = SPIFFS.open("/all.html", "r");
  String html = fd.readString();
  fd.close();
  server.send(200, "text/html", html);
}

// -----------------------------------
// Not Found
//------------------------------------
void handleNotFound() {
  Serial.println("NotFound");
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
// -----------------------------------
// Send Ir
//------------------------------------
//void handleIr() {
//  for (uint8_t i = 0; i < server.args(); i++) {
//    if (server.argName(i) == "code") {
//      uint32_t code = strtoul(server.arg(i).c_str(), NULL, 10);
//      irsend.sendNEC(code, 32);
//    }
//  }
//  handleRoot();
//}
// *****************************************************************************************
// * 関数
// *****************************************************************************************
String Uint64ToString(uint64_t number) {
  unsigned long long1 = (unsigned long)((number & 0xFFFF0000) >> 16 );
  unsigned long long2 = (unsigned long)((number & 0x0000FFFF));
  return (String(long1, HEX) + String(long2, HEX)); // six octets
}
// *****************************************************************************************
// * 初期化
// *****************************************************************************************
//------------------------------
// IO初期化
//------------------------------
void setup_io(void) {
}
// ------------------------------------
void setup_com(void) {      //通信初期化
  String ssid = "ESP_" + String(ESP.getChipId(), HEX);
  // シリアル通信
  Serial.begin(115200);

  // WiFi
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(HOST_IP, HOST_IP, SUB_MASK);
  WiFi.softAP(ssid.c_str());

  // mDNS
  mdns.begin("espir", WiFi.localIP());
}
// ------------------------------------
void setup_ir(void) {
  irsend.begin();
  irrecv.enableIRIn();
}
// ------------------------------------
void setup_ram(void) {
}
// ------------------------------------
void setup_spiffs(void) {
  SPIFFS.begin();
}
// ------------------------------------
void setup_http(void) {
  server.on("/", handleRoot);
  server.on("/index.html", handleRoot);
  server.on("/all.html", handleAll);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}
// ------------------------------------
void setup(void) {

  setup_io();
  setup_ram();
  setup_com();
  setup_ir();
  setup_spiffs();
  setup_http();
}
// *****************************************************************************************
// * Loop処理
// *****************************************************************************************
void loop_ir(void) {
  // Check if the IR code has been received.
  if (irrecv.decode(&results)) {
    if (results.decode_type == UNKNOWN) return;
    // Display a crude timestamp.
    uint32_t now = millis();
    Serial.printf("Timestamp : %06u.%03u\n", now / 1000, now % 1000);
    if (results.overflow)
      Serial.printf("WARNING: IR code is too big for buffer (>= %d). "
                    "This result shouldn't be trusted until this is resolved. "
                    "Edit & increase CAPTURE_BUFFER_SIZE.\n",
                    CAPTURE_BUFFER_SIZE);
    // Display the basic output of what we found.
    Serial.print(resultToHumanReadableBasic(&results));
    yield();  // Feed the WDT as the text output can take a while to print.


    // Output RAW timing info of the result.
    Serial.println(resultToTimingInfo(&results));
    yield();  // Feed the WDT (again)

    // Output the results as source code
    Serial.println(resultToSourceCode(&results));
    Serial.println("");  // Blank line between entries
    yield();  // Feed the WDT (again)

    //    Serial.print("Results:");
    //    Serial.println(results.value);
    if (results.value == -1) return;


    switch (results.decode_type) {
      case NEC:  ircode = "(NEC)"; break;
      case SONY: ircode = "(SONY)"; break;
      case RC5:  ircode = "(RC5)"; break;
      case RC6:  ircode = "(RC6)"; break;
      default:   ircode = "(???)"; break;
    }
    ircode += Uint64ToString(results.value);
  }
}

void loop(void) {
  server.handleClient();
  loop_ir();
}




