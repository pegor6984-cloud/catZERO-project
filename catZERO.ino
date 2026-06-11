/*
 * ============================================================================
 * catZERO v3.0 – ПОЛНАЯ ВЕРСИЯ С WI-FI ЧАТОМ
 * Аналоговые кнопки (GPIO0): UP, DOWN, OK.
 * 
 * ПЕРВАЯ ЧАСТЬ (1200 строк) – все глобальные определения, базовые функции,
 * меню, консоль, админка, Preferences, отладка, веб‑чат (страница, WebSocket).
 * ВТОРАЯ ЧАСТЬ (1400 строк) – дополнительные функции отрисовки, updateDisplay,
 * setup, loop, обработка навигации.
 * 
 * ----------------------------------------------------------------------------
 * ВОЗМОЖНОСТИ:
 *   - Сохранение до 5 Wi-Fi сетей (Preferences).
 *   - Кнопка "Save" в меню Actions (сохраняет сеть без подключения).
 *   - Асинхронное автоподключение (не блокирует меню).
 *   - Встроенный веб-чат (WebSocket) – обмен сообщениями между браузером и ESP.
 *   - Сканер и спектроанализатор Wi-Fi.
 *   - Анализатор спектра и глушилка nRF24 (BLE/Bluetooth).
 *   - ИК-пульт (4 слота, TV-B-Gone).
 *   - Консоль с виртуальной клавиатурой (6x8), команды info, def, ap, sd, fs.
 *   - Административная панель.
 *   - Индикатор подключения Wi-Fi в правом верхнем углу.
 * ============================================================================
 */

// ----------------------------------------------------------------------------
// БИБЛИОТЕКИ
// ----------------------------------------------------------------------------
#include <Preferences.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RF24.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <vector>
#include <SD.h>
#include <LittleFS.h>

// ----------------------------------------------------------------------------
// КОНСТАНТЫ (объявляются до использования в структурах)
// ----------------------------------------------------------------------------
#define NRF_CHANNELS        80
#define WIFI_CHANNELS       14
#define IR_SLOTS_COUNT      4
#define IR_BUFFER_SIZE      512
#define JAM_INTERVAL_MS     5
#define LONG_PRESS_MS       800
#define TVBGONE_CARRIER_HZ  38000
#define TVBGONE_CODE_GAP    200
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define MAX_SAVED_NETWORKS  5
#define TVBGONE_DEFAULT_MS  30000
#define EEPROM_SIZE_BYTES   1024
#define CONSOLE_AUTOREPEAT_DELAY  80   // уменьшите с 150 до 80 для быстрой реакции
// ----------------------------------------------------------------------------
// СТРУКТУРЫ ДАННЫХ
// ----------------------------------------------------------------------------
struct IrCode {
  uint8_t freq;
  uint8_t numpairs;
  uint8_t comp;
  const uint16_t *times;
  const uint8_t *codes;
};

struct WiFiNetworkInfo {
  String ssid;
  int rssi;
  int channel;
  uint8_t bssid[6];
  String bssidString;
  String encryptionType;
};

struct IrSignalData {
  uint16_t rawBuffer[IR_BUFFER_SIZE];
  uint16_t rawLength;
  String protocolName;
  bool isValid;
};

// ----------------------------------------------------------------------------
// TV-B-Gone (ПРИМЕРНЫЕ КОДЫ ДЛЯ SONY TV)
// ----------------------------------------------------------------------------
const uint16_t sony_times[] = {240, 600, 1200, 600};
const uint8_t sony_codes[] = {0x01, 0x00, 0x00, 0x00};
const IrCode sony_code = {0x13, 2, 2, sony_times, sony_codes};
const IrCode* const NApowerCodes[] = {&sony_code};
const IrCode* const EUpowerCodes[] = {&sony_code};
const uint8_t num_NAcodes = 1;
const uint8_t num_EUcodes = 1;

// ----------------------------------------------------------------------------
// ПРОТОТИПЫ ФУНКЦИЙ (полный список для корректной компиляции)
// ----------------------------------------------------------------------------
void drawWifiConnecting();
void drawTvbgone();
void adminPanel();
void sdCardInfo();
void saveWiFiCredentials(String ssid, String password);
void loadWiFiCredentials();
void showBootLogo();
void initSDCard();
void fsManager();
void attemptAutoConnect();
void drawNrfSpectrum();
void drawNrfJammer();
void drawIrCapture();
void drawIrTransmit();
void drawTimeoutMenu();
void drawResetConfirm();
void drawRebootConfirm();
void consoleInfoMode();
void consoleDevMode();
void consoleLoop();
void updateDisplay();
void handleButtons();
void drawWiFiIcon(int x, int y);
void drawTopBarWiFiIcon();
void drawMainMenu();
void drawWifiMenu();
void drawWifiScan();
void drawWifiSpectrum();
void drawWifiActions();
void drawWifiInfo();
void drawWifiConnecting();
void drawNrfMenu();
void drawIrMenu();
void drawSettingsMenu();
void drawSysInfo();
void drawKeyboard();
void drawChatKeyboard();
void drawTopBarIcons();
String inputStringWithKeyboard(bool allowExit, const char* title);
void showCommandOutput(String output, int timeoutSeconds);
void executeCommand(String cmd);
int editPinValue(const char* prompt);
void changePins();
void setBrightness();
void setInvert();
void showProcesses();
void reinitHardware();
void drawAdminMenuHighlight(int sel);
void startWifiScan();
void updateWifiSpectrum();
void sendDeauth();
void updateNrfSpectrum();
void startJammer();
void stopJammer();
void updateJammer();
void processIrCapture();
void sendIr(int slot);
void recheckNRF24();
void saveIrSlots();
void loadIrSlots();
void eraseAllIrSlots();
void tvbgone_menu();
void tvbgone_send_all(const IrCode* const* codes, uint8_t num);
void setPower(bool on);
void showMsg(const char* msg);
void loadTimeout();
void saveTimeout();
void disconnectWiFi();
void debugPrint(const char* msg);
void startWiFiChat();
void handleChat();
void drawWifiChat();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

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
#define CMD_FS      "fs"

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

#define RESET_BUTTON_PIN 10

#define OLED_SDA        8
#define OLED_SCL        9
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_ADDR       0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ----------------------------------------------------------------------------
// КНОПКИ (АНАЛОГОВЫЕ)
// ----------------------------------------------------------------------------
enum Button {
  BTN_NONE,
  BTN_UP,
  BTN_DOWN,
  BTN_OK
};

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
// ТАБЛИЦЫ КАНАЛОВ ДЛЯ ГЛУШЕНИЯ (Bluetooth и BLE)
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
// ГРАФИЧЕСКИЕ ДАННЫЕ (ИКОНКА WI-FI 16x8)
// ----------------------------------------------------------------------------
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
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (СОСТОЯНИЯ, ДАННЫЕ)
// ----------------------------------------------------------------------------
int displayTimeoutSec = 30;
unsigned long lastActivityTime = 0;
bool displayOn = true;
bool needRedraw = true;
unsigned long deviceBootTime = 0;
String statusMsg = "";
unsigned long statusMsgTime = 0;

unsigned long resetButtonPressStart = 0;
bool resetButtonLongPressTriggered = false;

std::vector<WiFiNetworkInfo> wifiList;
int wifiSelectedIdx = 0;
int wifiScrollOffset = 0;
bool wifiScanning = false;
int wifiPackets[WIFI_CHANNELS + 1] = {0};
int wifiSmooth[WIFI_CHANNELS + 1] = {0};
int currentWifiChan = 1;
bool wifiSpectrumActive = false;
unsigned long totalPackets = 0;
unsigned long lastChanSwitch = 0;
uint8_t targetBSSID[6];
int targetChan = 0;
bool deauthActive = false;
unsigned long lastDeauthTime = 0;

RF24 radio(RF_CE_PIN, RF_CSN_PIN);
int nrfSmooth[NRF_CHANNELS];
int nrfRaw[NRF_CHANNELS];
int nrfFloor[NRF_CHANNELS];
int currentNrfChan = 0;
bool nrfOk = false;
bool nrfCalibrated = false;
unsigned long lastNrfScan = 0;
String nrfErrorMsg = "";

enum BleJammerMode {
  BLE_JAMMER_OFF,
  BLE_JAMMER_MODE,
  BLUETOOTH_JAMMER_MODE
};
BleJammerMode jammerMode = BLE_JAMMER_MODE;
bool jamming = false;
unsigned long lastJamTime = 0;

IRrecv* irRx = nullptr;
IRsend* irTx = nullptr;
IrSignalData irSlots[IR_SLOTS_COUNT];
int curIrSlot = 0;
int irTxScroll = 0;
bool irCapturing = false;
bool irTempReady = false;
bool irRxOk = false;
unsigned long irTimeout = 0;
uint16_t tempRaw[IR_BUFFER_SIZE];
uint16_t tempRawLen = 0;
String tempProto;
decode_results irResult;
unsigned long lastScrollTime = 0;
int scrollOffset = 0;
// ----------------------------------------------------------------------------
// ПЕРЕЧИСЛЕНИЕ СОСТОЯНИЙ ПРИЛОЖЕНИЯ
// ----------------------------------------------------------------------------
enum AppState {
  STATE_MAIN_MENU,
  STATE_WIFI_MENU,
  STATE_WIFI_SCAN,
  STATE_WIFI_SPECTRUM,
  STATE_WIFI_ACTIONS,
  STATE_WIFI_INFO,
  STATE_NRF24_MENU,
  STATE_NRF24_SPECTRUM,
  STATE_NRF24_JAMMER,
  STATE_IR_MENU,
  STATE_IR_CAPTURE,
  STATE_IR_TRANSMIT,
  STATE_IR_ERASE,
  STATE_TVBGONE,
  STATE_SETTINGS_MENU,
  STATE_SYSTEM_INFO,
  STATE_TIMEOUT,
  STATE_RESET_CONFIRM,
  STATE_REBOOT_CONFIRM,
  STATE_FACTORY_RESET_CONFIRM,
  STATE_CONSOLE,
  STATE_CONSOLE_COMMAND_OUTPUT,
  STATE_WIFI_CONNECTING,
  STATE_WIFI_CHAT
};
AppState appState = STATE_MAIN_MENU;

// ----------------------------------------------------------------------------
// ПЕРЕМЕННЫЕ ДЛЯ МЕНЮ (индексы выбранных пунктов)
// ----------------------------------------------------------------------------
int mainIdx = 0;
int wifiMenuIdx = 0;
int nrfMenuIdx = 0;
int irMenuIdx = 0;
int settingsIdx = 0;
int wifiActionIdx = 0;
int timeoutVal = 30;

const int MAIN_SIZE = 5;
const int WIFI_MENU_SIZE = 4;        // Scan, Spectrum, WiFi Chat, Back
const int NRF_MENU_SIZE = 4;
const int IR_MENU_SIZE = 5;
const int SETTINGS_SIZE = 5;
const int WIFI_ACTIONS_SIZE = 7;    // Info, Deauth, Stop, Connect, Save, Disconnect, Back

unsigned long okPressStart = 0;
bool longPressDetected = false;
uint8_t tvbgone_region = 0;

// ----------------------------------------------------------------------------
// Wi-Fi ЧАТ (веб-сервер и WebSocket)
// ----------------------------------------------------------------------------
WebServer server(80);
WebSocketsServer webSocket(81);
String lastIncomingMsg = "";
bool chatActive = false;
bool chatKeyboardActive = false;   // активна ли виртуальная клавиатура в чате
String chatMessage = "";           // текст сообщения, набираемого на клавиатуре
String chatHistory[10];   // Хранит последние 10 сообщений от сервера
int chatHistoryCount = 0; // Количество сообщений в истории
bool webSocketConnected = false;   // флаг, что WebSocket соединение установлено
bool confirmExit = false;   // флаг диалога подтверждения выхода
String lastConnectedSSID = "";
String lastConnectedPassword = "";
// Для бегущей строки в сообщениях (showMsg)
unsigned long lastMsgScrollTime = 0;
int msgScrollOffset = 0;
String lastLongMsg = "";
// ----------------------------------------------------------------------------
// КОНСОЛЬ: РЕЖИМЫ, ВИРТУАЛЬНАЯ КЛАВИАТУРА 6x8 (48 КЛАВИШ)
// ----------------------------------------------------------------------------
enum ConsoleMode {
  CONSOLE_KEYBOARD,
  CONSOLE_INFO,
  CONSOLE_DEV
};
ConsoleMode consoleMode = CONSOLE_KEYBOARD;

bool capsLock = false;
String consoleText = "";

// Раскладка для нижнего регистра (48 элементов)
String lowerKeys[] = {
  "a","b","c","d","e","f","g","j",
  "i","h","k","l","m","n","o","p",
  "q","r","s","t","u","v","w","x",
  "y","z","1","2","3","4","5","6",
  "7","8","9","0","Space","Caps","Del","OK",
  "Exit","","","","","","",""
};

// Раскладка для верхнего регистра (48 элементов)
String upperKeys[] = {
  "A","B","C","D","E","F","G","J",
  "I","H","K","L","M","N","O","P",
  "Q","R","S","T","U","V","W","X",
  "Y","Z","!","@","#","$","%","&",
  "*","(",")","0","Space","Caps","Del","OK",
  "Exit","","","","","","",""
};
// ----------------------------------------------------------------------------
// ПРЕДУСТАНОВЛЕННЫЕ СЕТИ (вписываете свои до 5 штук)
// ----------------------------------------------------------------------------
const char* preset_ssid[] = {
  "FRITZ!Box 6591 Cabel MK",      // ваша первая сеть
  "moto e13",        // вторая
  "",                // третья (пусто – не используется)
  "",
  ""
};
const char* preset_pass[] = {
  "43481208496765148415",
  "12345678",
  "",
  "",
  ""
};
int selectedKey = 0;
const int totalKeys = 48;
const int rowsKeyboard = 6;
const int colsKeyboard = 8;

// Строки информации для команды info
String infoLines[] = {
  "CPU: ESP32-C3",
  "Flash:" + String(ESP.getFlashChipSize() / 1048576) + "MB",
  "Heap:" + String(ESP.getFreeHeap() / 1024) + "KB",
  "Chip Rev:" + String(ESP.getChipRevision()),
  "Freq:" + String(ESP.getCpuFreqMHz()) + "MHz",
  "Uptime:" + String(millis() / 1000) + "s"
};
int infoPage = 0;
const int totalInfo = 6;

// Секретный код для выхода из DEV-режима
int secretCode[] = {1, 2, 3, 1, 2, 3, 1, 2, 3};
int enteredCode[9];
int codePos = 0;

// Переменные для вывода результата команд
String commandOutput = "";
unsigned long commandStartTime = 0;
int commandTimeoutSec = 0;
bool waitingForOk = false;

// Административная панель
bool adminMode = false;
String adminPassword = ADMIN_PASSWORD;
int displayBrightness = 128;
bool displayInvert = false;
int irBufferSize = 512;
float batteryCapacity = 650.0;      // мА·ч
float batteryRemaining = 650.0;     // текущий остаток
unsigned long lastBatteryUpdate = 0;
float estimatedCurrentMA = 150.0;   // средний ток потребления (мА)
// ----------------------------------------------------------------------------
// WI-FI: ХРАНЕНИЕ 5 СЕТЕЙ, АСИНХРОННОЕ АВТОПОДКЛЮЧЕНИЕ
// ----------------------------------------------------------------------------
String connectSsid = "";
String connectPassword = "";
bool connectInProgress = false;
unsigned long connectStartTime = 0;

String savedSSID[MAX_SAVED_NETWORKS];
String savedPassword[MAX_SAVED_NETWORKS];
int savedNetworksCount = 0;

bool autoConnectInProgress = false;
bool autoConnectScheduled = false;
unsigned long autoConnectTime = 0;
unsigned long autoConnectStartTime = 0;
Preferences pref;

// ----------------------------------------------------------------------------
// ОТЛАДОЧНЫЕ ФУНКЦИИ (ВСЁ ВЫВОДЯТ В SERIAL)
// ----------------------------------------------------------------------------
void addChatMessage(String msg) {
  // Сдвигаем старые сообщения вверх
  for (int i = 9; i > 0; i--) {
    chatHistory[i] = chatHistory[i-1];
  }
  chatHistory[0] = msg;
  if (chatHistoryCount < 10) chatHistoryCount++;
}
void debugPrint(const char* msg) {
  Serial.print("[DEBUG] ");
  Serial.println(msg);
}

void debugPrintState() {
  Serial.print("AppState: ");
  Serial.println(appState);
  Serial.print("WiFi status: ");
  Serial.println(WiFi.status());
  Serial.print("Saved networks: ");
  Serial.println(savedNetworksCount);
}

void debugButton(const char* btn) {
  Serial.print("Button: ");
  Serial.println(btn);
}

// ----------------------------------------------------------------------------
// РАБОТА С PREFERENCES (СОХРАНЕНИЕ ТАЙМАУТА И КРЕДЕНЦИАЛОВ)
// ----------------------------------------------------------------------------
void saveTimeout() {
  pref.begin("settings", false);
  pref.putInt("timeout", displayTimeoutSec);
  pref.end();
}

void loadTimeout() {
  pref.begin("settings", true);
  displayTimeoutSec = pref.getInt("timeout", 30);
  pref.end();
  if (displayTimeoutSec == 0) displayTimeoutSec = 30;
}

void saveWiFiCredentials(String ssid, String password) {
  Serial.print("Saving: ");
  Serial.println(ssid);
  loadWiFiCredentials();
  
  // Проверяем, нет ли уже такой сети
  for (int i = 0; i < savedNetworksCount; i++) {
    if (savedSSID[i] == ssid) {
      savedPassword[i] = password;
      pref.begin("wifi", false);
      pref.putInt("count", savedNetworksCount);
      for (int j = 0; j < savedNetworksCount; j++) {
        pref.putString(("ssid" + String(j)).c_str(), savedSSID[j]);
        pref.putString(("pass" + String(j)).c_str(), savedPassword[j]);
      }
      pref.end();
      return;
    }
  }
  
  // Добавляем новую сеть
  if (savedNetworksCount < MAX_SAVED_NETWORKS) {
    savedSSID[savedNetworksCount] = ssid;
    savedPassword[savedNetworksCount] = password;
    savedNetworksCount++;
  } else {
    // Список полон – сдвигаем влево, последнюю заменяем
    for (int i = 0; i < MAX_SAVED_NETWORKS - 1; i++) {
      savedSSID[i] = savedSSID[i + 1];
      savedPassword[i] = savedPassword[i + 1];
    }
    savedSSID[MAX_SAVED_NETWORKS - 1] = ssid;
    savedPassword[MAX_SAVED_NETWORKS - 1] = password;
  }
  
  pref.begin("wifi", false);
  pref.putInt("count", savedNetworksCount);
  for (int i = 0; i < savedNetworksCount; i++) {
    pref.putString(("ssid" + String(i)).c_str(), savedSSID[i]);
    pref.putString(("pass" + String(i)).c_str(), savedPassword[i]);
  }
  pref.end();
  Serial.println("Credentials saved");
}

void loadWiFiCredentials() {
  pref.begin("wifi", true);
  savedNetworksCount = pref.getInt("count", 0);
  if (savedNetworksCount > MAX_SAVED_NETWORKS) savedNetworksCount = MAX_SAVED_NETWORKS;
  for (int i = 0; i < savedNetworksCount; i++) {
    savedSSID[i] = pref.getString(("ssid" + String(i)).c_str(), "");
    savedPassword[i] = pref.getString(("pass" + String(i)).c_str(), "");
    Serial.print("Loaded: ");
    Serial.println(savedSSID[i]);
  }
  pref.end();
}

void disconnectWiFi() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_OFF);
  pref.begin("wifi", false);
  pref.clear();
  pref.end();
  savedNetworksCount = 0;
  showMsg("All Wi-Fi credentials cleared");
  Serial.println("All credentials cleared");
}

void clearAllPreferences() {
  pref.begin("wifi", false);
  pref.clear();
  pref.end();
  pref.begin("settings", false);
  pref.clear();
  pref.end();
  displayTimeoutSec = 30;
  savedNetworksCount = 0;
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
  Serial.print("Message: ");
  Serial.println(msg);

  String s = String(msg);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  if (w > 120) {
    lastLongMsg = s;
    msgScrollOffset = 0;
    lastMsgScrollTime = millis();
  } else {
    lastLongMsg = "";
  }
}

// ----------------------------------------------------------------------------
// IR: СОХРАНЕНИЕ/ЗАГРУЗКА СЛОТОВ (EEPROM ДЛЯ ПРОСТОТЫ)
// ----------------------------------------------------------------------------
#include <EEPROM.h>

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
    Serial.println("nRF24 OK");
  } else {
    nrfOk = false;
    nrfErrorMsg = "not found";
    showMsg("nRF24 not found");
    Serial.println("nRF24 FAIL");
  }
  needRedraw = true;
}

void startJammer() {
  if (!nrfOk) {
    showMsg(nrfErrorMsg.c_str());
    return;
  }
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
  Serial.println("Jammer started");
}

void stopJammer() {
  jamming = false;
  radio.powerDown();
  showMsg("Jammer OFF");
  Serial.println("Jammer stopped");
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

// ----------------------------------------------------------------------------
// Wi-Fi: СКАНИРОВАНИЕ, СПЕКТР, ДЕАУТ, ПОДКЛЮЧЕНИЕ
// ----------------------------------------------------------------------------
void startWifiScan()
{
    wifiScanning=true;

    wifiList.clear();

    wifiSelectedIdx=0;
    wifiScrollOffset=0;

    display.clearDisplay();

    display.setCursor(20,30);
    display.print("Scanning...");
    display.display();

    Serial.println("Scanning...");

    // Полный сброс Wi-Fi
    WiFi.disconnect(true,true);

    delay(300);

    WiFi.mode(WIFI_STA);

    WiFi.setSleep(false);

    delay(100);

    int found=WiFi.scanNetworks();

    Serial.print("Found: ");
    Serial.println(found);

    if(found<=0)
    {
        showMsg("No WiFi found");

        wifiScanning=false;
        needRedraw=true;

        return;
    }

    for(int i=0;i<found && i<50;i++)
    {
        WiFiNetworkInfo net;

        net.ssid=WiFi.SSID(i);

        if(net.ssid=="")
            net.ssid="<hidden>";

        net.rssi=WiFi.RSSI(i);

        wifiList.push_back(net);
    }

    WiFi.scanDelete();

    wifiScanning=false;

    needRedraw=true;
}

void sendDeauth() {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(targetChan, WIFI_SECOND_CHAN_NONE);
  uint8_t deauth[26] = {
    0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    targetBSSID[0], targetBSSID[1], targetBSSID[2], targetBSSID[3], targetBSSID[4], targetBSSID[5],
    targetBSSID[0], targetBSSID[1], targetBSSID[2], targetBSSID[3], targetBSSID[4], targetBSSID[5],
    0x00, 0x00, 0x07, 0x00
  };
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  for (int i = 0; i < 6; i++) deauth[10 + i] = mac[i];
  esp_wifi_80211_tx(WIFI_IF_STA, deauth, sizeof(deauth), false);
}

bool connectToWiFi(String ssid, String password) {
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) {
    showMsg("Already connected");
    return true;
  }
  WiFi.disconnect(true);
  delay(500);
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long startTime = millis();
  int dots = 0;
  while (millis() - startTime < WIFI_CONNECT_TIMEOUT_MS) {
    if (WiFi.status() == WL_CONNECTED) {
      saveWiFiCredentials(ssid, password);
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(15, 25);
      display.println("Connected!");
      display.display();
      delay(1500);
      Serial.println("Manual connect success");
      return true;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(25, 20);
    display.print("Connect");
    for (int i = 0; i < dots; i++) display.print(".");
    display.display();
    dots++;
    if (dots > 3) dots = 0;
    delay(500);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 25);
  display.println("Wrong password");
  display.display();
  delay(2000);
  WiFi.disconnect();
  Serial.println("Manual connect fail");
  return false;
}

void attemptAutoConnect() {
  if (autoConnectInProgress) return;
  loadWiFiCredentials();
  if (savedNetworksCount == 0) {
    Serial.println("No saved networks, auto-connect skipped");
    return;
  }
  autoConnectInProgress = true;
  autoConnectStartTime = millis();
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found during auto-connect");
    autoConnectInProgress = false;
    return;
  }
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    for (int j = 0; j < savedNetworksCount; j++) {
      if (ssid == savedSSID[j]) {
        Serial.print("Auto-connecting to ");
        Serial.println(ssid);
        WiFi.begin(savedSSID[j].c_str(), savedPassword[j].c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
          delay(200);
          Serial.print(".");
        }
        Serial.println();
if (WiFi.status() == WL_CONNECTED) {
  lastConnectedSSID = savedSSID[j];
  lastConnectedPassword = savedPassword[j];
  String msg = "Auto-connected to " + savedSSID[j];
  showMsg(msg.c_str());
  Serial.println(msg);
  WiFi.scanDelete();
  autoConnectInProgress = false;
  return;
}
      }
    }
  }
  WiFi.scanDelete();
  autoConnectInProgress = false;
  Serial.println("Auto-connect: no suitable network found");
}

// ----------------------------------------------------------------------------
// TV-B-Gone (с процентом от времени)
// ----------------------------------------------------------------------------
unsigned long tvbgoneDurationMs = TVBGONE_DEFAULT_MS;

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
  unsigned long duration = tvbgoneDurationMs;
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
    int elapsed = now - startTime;
    int percent = (elapsed * 100) / duration;
    if (percent > 100) percent = 100;
    display.fillRect(11, 46, (percent * 108) / 100, 6, SSD1306_WHITE);
    display.setCursor(55, 35);
    display.print(percent);
    display.print("%");
    display.display();
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
// ЗАСТАВКА И ИНИЦИАЛИЗАЦИЯ SD-КАРТЫ
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
  display.setTextSize(1);
  display.setCursor(15, 50);
  display.println("by Egorus");
  display.display();
  delay(1000);
  display.setTextSize(1);
}

void initSDCard() {
  Serial.println("Initializing SD card...");
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS_PIN);
  SPI.setFrequency(4000000);  // снижаем частоту для стабильности

  unsigned long start = millis();
  bool sdOk = false;
  
  // Таймаут 2 секунды на инициализацию
  while (millis() - start < 2000) {
    if (SD.begin(SD_CS_PIN, SPI, 4000000)) {
      sdOk = true;
      break;
    }
    delay(100);
    Serial.print(".");
  }
  
  if (!sdOk) {
    Serial.println("SD Card init failed (timeout or absent)");
    display.clearDisplay();
    display.setCursor(10, 28);
    display.println("SD Card error!");
    display.display();
    delay(1000);
    return;
  }
  
  Serial.println("SD Card OK");
  uint8_t cardType = SD.cardType();
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
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
  display.print("Size: ");
  display.print(cardSize);
  display.println(" MB");
  display.display();
  delay(1500);
}//----------------------------------------------------------------------------
// БАЗОВЫЕ ФУНКЦИИ ОТРИСОВКИ (ОСНОВНЫЕ МЕНЮ)
// ----------------------------------------------------------------------------
void drawHeader(const char* title) {
  display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_BLACK);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 3);
  display.print(title);
  display.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);
}

void drawItem(const char* text, bool sel, int line) {
  int y = 14 + line * 10;
  if (sel) {
    display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(4, y);
    display.print(">");
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
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
  for (int i = 0; i < MAIN_SIZE; i++) {
    drawItem(items[i], i == mainIdx, i);
  }
  drawTopBarIcons();
  display.display();
}

void drawWifiMenu() {
  display.clearDisplay();
  drawHeader("WiFi");
  const char* items[] = {"Scan", "Spectrum", "WiFi Chat", "Back"};
  for (int i = 0; i < WIFI_MENU_SIZE; i++) {
    drawItem(items[i], i == wifiMenuIdx, i);
  }
  drawTopBarIcons();
  display.display();
}

void drawWifiActions() {
  display.clearDisplay();
  drawHeader("Actions");
  const char* acts[] = {"Info", "Deauth", "Stop", "Connect", "Save", "Disconnect", "Back"};
  for (int i = 0; i < WIFI_ACTIONS_SIZE; i++) {
    drawItem(acts[i], i == wifiActionIdx, i);
  }
  if (deauthActive) {
    display.fillRect(0, 55, SCREEN_WIDTH, 9, WHITE);
    display.setTextColor(BLACK);
    display.setCursor(15, 56);
    display.print("DEAUTH ON");
  }
  drawTopBarIcons();
  display.display();
}

void drawWifiChat() {
  display.clearDisplay();
  drawHeader("WiFi Chat");

  if (chatKeyboardActive) {
    // Верхняя часть: две колонки (сжатые)
    int leftColX = 2;
    int rightColX = 64;
    int startY = 13;
    
    display.setCursor(leftColX, startY);
    display.print("You:");
    display.setCursor(leftColX, startY + 8);
    String yourMsg = chatMessage;
    if (yourMsg.length() > 14) yourMsg = yourMsg.substring(0, 14);
    display.print(yourMsg);
    
    display.setCursor(rightColX, startY);
    display.print("Friend:");
    display.setCursor(rightColX, startY + 8);
    String friendMsg = (chatHistoryCount > 0) ? chatHistory[0] : "";
    if (friendMsg.length() > 14) friendMsg = friendMsg.substring(0, 14);
    display.print(friendMsg);
    
    // Компактная клавиатура, поднята выше (Y=23)
    int keyW = 14, keyH = 8;
    int stepX = 14, stepY = 9;
    int startX = 0;
    int startYKbd = 24;   // подняли с 28 на 24
    
    for (int row = 0; row < rowsKeyboard; row++) {
      for (int col = 0; col < colsKeyboard; col++) {
        int index = row * colsKeyboard + col;
        if (index >= totalKeys) break;
        int x = startX + col * stepX;
        int y = startYKbd + row * stepY;
        if (index == selectedKey) {
          display.fillRect(x, y, keyW, keyH, WHITE);
          display.setTextColor(BLACK);
        } else {
          display.setTextColor(WHITE);
        }
        String key = capsLock ? upperKeys[index] : lowerKeys[index];
        if (key.length() > 0) {
          // Для спецклавиш можно сократить отображение
          if (key == "Space") display.setCursor(x + 2, y + 1), display.print("SP");
          else if (key == "Caps") display.setCursor(x + 2, y + 1), display.print(capsLock ? "CAP" : "Cps");
          else if (key == "Del") display.setCursor(x + 2, y + 1), display.print("DEL");
          else if (key == "OK") display.setCursor(x + 3, y + 1), display.print("OK");
          else if (key == "Exit") display.setCursor(x + 2, y + 1), display.print("EXT");
          else display.setCursor(x + 2, y + 1), display.print(key);
        }
      }
    }
  } else {
    display.setCursor(2, 14);
    display.print("IP: ");
    display.print(WiFi.localIP());
    display.setCursor(2, 26);
    display.print("Status: ");
    display.print(webSocketConnected ? "Connected" : "Waiting");
    display.setCursor(2, 38);
    display.print("OK=keyboard");
    display.setCursor(2, 48);
    display.print("Exit=back");
  }
  display.display();
}

void drawNrfMenu() {
  display.clearDisplay();
  char buf[30];
  sprintf(buf, "nRF24 (%s)", nrfOk ? "OK" : nrfErrorMsg.c_str());
  drawHeader(buf);
  const char* items[] = {"Spectrum", "Jammer", "Recheck", "Back"};
  for (int i = 0; i < NRF_MENU_SIZE; i++) {
    drawItem(items[i], i == nrfMenuIdx, i);
  }
  drawTopBarIcons();
  display.display();
}

void drawIrMenu() {
  display.clearDisplay();
  drawHeader("IR");
  const char* items[] = {"Capture", "Transmit", "Erase All", "TV-B-Gone", "Back"};
  for (int i = 0; i < IR_MENU_SIZE; i++) {
    drawItem(items[i], i == irMenuIdx, i);
  }
  drawTopBarIcons();
  display.display();
}

void drawSettingsMenu() {
  display.clearDisplay();
  drawHeader("Settings");
  const char* items[] = {"Info", "Timeout", "Reset", "Reboot", "Back"};
  for (int i = 0; i < SETTINGS_SIZE; i++) {
    drawItem(items[i], i == settingsIdx, i);
  }
  drawTopBarIcons();
  display.display();
}

void drawSysInfo() {
  display.clearDisplay();
  drawHeader("System Info");
  display.setCursor(2, 14);
  display.printf("Chip: ESP32");
  display.setCursor(2, 24);
  display.printf("Flash: %d MB", ESP.getFlashChipSize() / 1048576);
  display.setCursor(2, 34);
  display.printf("Heap: %d KB", ESP.getFreeHeap() / 1024);
  display.setCursor(2, 44);
  display.printf("Uptime: %ds", (millis() - deviceBootTime) / 1000);
  display.setCursor(2, 54);
  display.printf("nRF24: %s", nrfOk ? "OK" : "FAIL");
  display.setCursor(2, 60);
  display.println("Press=back");
  drawTopBarIcons();
  display.display();
}

void drawTvbgone() {
  display.clearDisplay();
  drawHeader("TV-B-Gone");
  display.setCursor(10, 20);
  display.println("Region:");
  display.setCursor(30, 30);
  if (tvbgone_region == 0) display.println("NA");
  else display.println("EU");
  display.setCursor(10, 45);
  display.println("Action: Power");
  display.setCursor(2, 56);
  display.println("UP/DOWN=Reg  OK=Send");
  drawTopBarIcons();
  display.display();
}

// ----------------------------------------------------------------------------
// КОНСОЛЬНЫЕ ФУНКЦИИ (ВВОД, ВЫВОД, КОМАНДЫ) – ЧАСТЬ 1
// ----------------------------------------------------------------------------


String inputStringWithKeyboard(bool allowExit, const char* title) {
  String result = "";
  int localSelected = 0;
  bool localCaps = false;
  bool okProcessed = false;
  unsigned long lastMoveTime = 0;
  bool leftHeld = false;
  bool rightHeld = false;
  int oldSelected = selectedKey;
  bool oldCaps = capsLock;
  String oldText = consoleText;
  selectedKey = localSelected;
  capsLock = localCaps;
  consoleText = result;
  while (true) {
    display.clearDisplay();
    display.drawRect(0, 0, 128, 10, WHITE);
    display.setCursor(2, 2);
    display.print(title);
    display.fillRect(0, 12, 128, 8, BLACK);
    display.setCursor(2, 13);
    String visible = result;
    if (visible.length() > 18) visible = visible.substring(visible.length() - 18);
    display.print(visible);
    display.display();
    for (int row = 0; row < rowsKeyboard; row++) {
      for (int col = 0; col < colsKeyboard; col++) {
        int index = row * colsKeyboard + col;
        if (index >= totalKeys) break;
        int x = col * 16;
        int y = 18 + row * 9;
        if (index == localSelected) {
          display.fillRect(x, y, 15, 8, WHITE);
          display.setTextColor(BLACK);
        } else {
          display.setTextColor(WHITE);
        }
        String key = localCaps ? upperKeys[index] : lowerKeys[index];
        if (key.length() > 0) {
          display.setCursor(x + 1, y + 1);
          display.print(key);
        }
      }
    }
    drawTopBarIcons();
    display.display();
    bool up = btnUp();
    bool down = btnDown();
    if (up) {
      if (!leftHeld) {
        leftHeld = true;
        lastMoveTime = millis();
        localSelected--;
        if (localSelected < 0) localSelected = totalKeys - 1;
      } else if (millis() - lastMoveTime > CONSOLE_AUTOREPEAT_DELAY) {
        lastMoveTime = millis();
        localSelected--;
        if (localSelected < 0) localSelected = totalKeys - 1;
      }
    } else {
      leftHeld = false;
    }
    if (down) {
      if (!rightHeld) {
        rightHeld = true;
        lastMoveTime = millis();
        localSelected++;
        if (localSelected >= totalKeys) localSelected = 0;
      } else if (millis() - lastMoveTime > CONSOLE_AUTOREPEAT_DELAY) {
        lastMoveTime = millis();
        localSelected++;
        if (localSelected >= totalKeys) localSelected = 0;
      }
    } else {
      rightHeld = false;
    }
    if (btnOk() && !okProcessed) {
      okProcessed = true;
      String key = localCaps ? upperKeys[localSelected] : lowerKeys[localSelected];
      if (key == "Caps") {
        localCaps = !localCaps;
      } else if (key == "Del") {
        if (result.length() > 0) result.remove(result.length() - 1);
      } else if (key == "OK") {
        selectedKey = oldSelected;
        capsLock = oldCaps;
        consoleText = oldText;
        drawKeyboard();
        return result;
      } else if (key == "Exit" && allowExit) {
        selectedKey = oldSelected;
        capsLock = oldCaps;
        consoleText = oldText;
        drawKeyboard();
        return "";
      } else if (key == "Space") {
        result += " ";
      } else if (key.length() > 0 && key != "Caps" && key != "Del" && key != "OK" && key != "Exit") {
        result += key;
      }
      consoleText = result;
      delay(150);
    }
    if (!btnOk()) okProcessed = false;
    delay(20);
  }
}

void showCommandOutput(String output, int timeoutSeconds) {
  commandOutput = output;
  commandStartTime = millis();
  commandTimeoutSec = timeoutSeconds;
  waitingForOk = true;
  appState = STATE_CONSOLE_COMMAND_OUTPUT;
  needRedraw = true;
}

void executeCommand(String cmd) {
  cmd.toLowerCase();
  if (cmd == CMD_INFO) {
    consoleMode = CONSOLE_INFO;
    infoPage = 0;
  } else if (cmd == CMD_DEV) {
    consoleMode = CONSOLE_DEV;
    codePos = 0;
  } else if (cmd == CMD_CLEAR) {
    consoleText = "";
  } else if (cmd == CMD_SFV) {
    showCommandOutput("Scanning WiFi...", 0);
    bool oldWifiSpectrum = wifiSpectrumActive;
    if (!oldWifiSpectrum) {
      wifiSpectrumActive = true;
      totalPackets = 0;
      memset(wifiPackets, 0, sizeof(wifiPackets));
      memset(wifiSmooth, 0, sizeof(wifiSmooth));
      esp_wifi_set_promiscuous(true);
      esp_wifi_set_promiscuous_rx_cb([](void* buf, wifi_promiscuous_pkt_type_t type) {
        wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
        int ch = pkt->rx_ctrl.channel;
        if (ch >= 1 && ch <= WIFI_CHANNELS) {
          wifiPackets[ch]++;
          totalPackets++;
        }
      });
      esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    }
    unsigned long start = millis();
    while (millis() - start < 2000) {
      static unsigned long lastSwitch = 0;
      if (millis() - lastSwitch > 100) {
        currentWifiChan = (currentWifiChan % WIFI_CHANNELS) + 1;
        esp_wifi_set_channel(currentWifiChan, WIFI_SECOND_CHAN_NONE);
        lastSwitch = millis();
      }
      yield();
    }
    int maxChan = 1;
    int maxPkts = 0;
    for (int ch = 1; ch <= WIFI_CHANNELS; ch++) {
      if (wifiPackets[ch] > maxPkts) {
        maxPkts = wifiPackets[ch];
        maxChan = ch;
      }
    }
    if (!oldWifiSpectrum) {
      wifiSpectrumActive = false;
      WiFi.mode(WIFI_STA);
      esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    }
    int freqMHz = 2407 + (maxChan - 1) * 5;
    String result = "Strongest WiFi: channel " + String(maxChan) + " (" + String(freqMHz) + " MHz) packets=" + String(maxPkts);
    showCommandOutput(result, COMMAND_TIMEOUT_SFV);
    return;
  } else if (cmd == CMD_CS) {
    if (!nrfOk) {
      showCommandOutput("nRF24 not found!", COMMAND_TIMEOUT_CS);
      return;
    }
    showCommandOutput("Scanning nRF24...", 0);
    bool oldJam = jamming;
    if (oldJam) stopJammer();
    radio.powerUp();
    radio.stopListening();
    radio.setPALevel(RF24_PA_MAX);
    int maxSignal = -100;
    int maxChannel = -1;
    for (int ch = 0; ch < NRF_CHANNELS; ch++) {
      radio.setChannel(ch);
      radio.startListening();
      delayMicroseconds(150);
      int rssi = radio.testCarrier() ? 40 : 0;
      radio.stopListening();
      if (nrfCalibrated) rssi -= nrfFloor[ch];
      if (rssi < 0) rssi = 0;
      if (rssi > maxSignal) {
        maxSignal = rssi;
        maxChannel = ch;
      }
      yield();
    }
    if (oldJam) startJammer();
    int freqMHz = 2400 + maxChannel;
    String result = "Max nRF24: channel " + String(maxChannel) + " (" + String(freqMHz) + " MHz) signal=" + String(maxSignal);
    showCommandOutput(result, COMMAND_TIMEOUT_CS);
    return;
  } else if (cmd == CMD_EXIT) {
    appState = STATE_MAIN_MENU;
    needRedraw = true;
    return;
  } else if (cmd == CMD_AP) {
    adminPanel();
    return;
  } else if (cmd == CMD_SD) {
    sdCardInfo();
    return;
  } else if (cmd == CMD_FS) {
    fsManager();
    return;
  } else if (cmd.length() > 0) {
    showCommandOutput("Unknown: " + cmd, 1);
    return;
  }
  drawKeyboard();
}

// ----------------------------------------------------------------------------
// АДМИНИСТРАТИВНАЯ ПАНЕЛЬ (БАЗОВЫЕ ФУНКЦИИ)
// ----------------------------------------------------------------------------
int editPinValue(const char* prompt) {
  consoleText = "";
  selectedKey = 0;
  capsLock = false;
  drawKeyboard();
  bool okProcessed = false;
  unsigned long lastMove = 0;
  bool leftHeld = false;
  bool rightHeld = false;
  while (true) {
    bool up = btnUp();
    bool down = btnDown();
    if (up) {
      if (!leftHeld) {
        selectedKey = (selectedKey - 1 + totalKeys) % totalKeys;
        drawKeyboard();
        leftHeld = true;
        lastMove = millis();
        delay(150);
      } else if (millis() - lastMove > 150) {
        selectedKey = (selectedKey - 1 + totalKeys) % totalKeys;
        drawKeyboard();
        lastMove = millis();
      }
    } else {
      leftHeld = false;
    }
    if (down) {
      if (!rightHeld) {
        selectedKey = (selectedKey + 1) % totalKeys;
        drawKeyboard();
        rightHeld = true;
        lastMove = millis();
        delay(150);
      } else if (millis() - lastMove > 150) {
        selectedKey = (selectedKey + 1) % totalKeys;
        drawKeyboard();
        lastMove = millis();
      }
    } else {
      rightHeld = false;
    }
    if (btnOk() && !okProcessed) {
      okProcessed = true;
      String key = capsLock ? upperKeys[selectedKey] : lowerKeys[selectedKey];
      if (key == "OK") {
        int val = consoleText.toInt();
        if ((val >= 0 && val <= 10) || val == 20 || val == 21) {
          return val;
        } else {
          display.clearDisplay();
          display.setCursor(10, 28);
          display.print("Invalid pin (0-10,20,21)");
          display.display();
          delay(1500);
          consoleText = "";
          drawKeyboard();
        }
      } else if (key == "Del") {
        if (consoleText.length() > 0) consoleText.remove(consoleText.length() - 1);
        drawKeyboard();
      } else if (key == "Exit") {
        return -1;
      } else if (key == "Space") {
        consoleText += " ";
        drawKeyboard();
      } else if (key == "Caps") {
        capsLock = !capsLock;
        drawKeyboard();
      } else {
        consoleText += key;
        drawKeyboard();
      }
      delay(150);
    }
    if (!btnOk()) okProcessed = false;
    delay(20);
  }
}

void changePins() {
  display.clearDisplay();
  drawHeader("Change pins");
  display.setCursor(10, 28);
  display.println("Not available");
  display.setCursor(10, 40);
  display.println("Analog buttons");
  drawTopBarIcons();
  display.display();
  delay(2000);
}

void setBrightness() {
  while (true) {
    display.clearDisplay();
    drawHeader("Brightness");
    display.setCursor(10, 20);
    display.print("Bright: ");
    display.print(displayBrightness);
    display.setCursor(10, 40);
    display.println("UP/DOWN adjust, OK save");
    drawTopBarIcons();
    display.display();
    if (btnUp()) {
      displayBrightness = constrain(displayBrightness + 5, 0, 255);
      delay(150);
    }
    if (btnDown()) {
      displayBrightness = constrain(displayBrightness - 5, 0, 255);
      delay(150);
    }
    if (btnOk()) {
      display.ssd1306_command(SSD1306_SETCONTRAST);
      display.ssd1306_command(displayBrightness);
      display.clearDisplay();
      display.setCursor(20, 28);
      display.print("Brightness saved");
      drawTopBarIcons();
      display.display();
      delay(1000);
      return;
    }
    delay(50);
  }
}

void setInvert() {
  displayInvert = !displayInvert;
  if (displayInvert) {
    display.ssd1306_command(SSD1306_INVERTDISPLAY);
  } else {
    display.ssd1306_command(SSD1306_NORMALDISPLAY);
  }
  display.clearDisplay();
  display.setCursor(20, 28);
  display.print(displayInvert ? "Display inverted" : "Display normal");
  drawTopBarIcons();
  display.display();
  delay(1000);
}

void showProcesses() {
  while (true) {
    display.clearDisplay();
    drawHeader("Processes");
    display.setCursor(2, 15);
    display.printf("Heap free: %d KB", ESP.getFreeHeap() / 1024);
    display.setCursor(2, 25);
    display.printf("Uptime: %d sec", millis() / 1000);
    display.setCursor(2, 35);
    display.printf("CPU freq: %d MHz", ESP.getCpuFreqMHz());
    display.setCursor(2, 45);
    display.printf("IR buffer: %d", irBufferSize);
    display.setCursor(2, 55);
    display.println("Press OK to exit");
    drawTopBarIcons();
    display.display();
    if (btnOk()) break;
    delay(50);
  }
}

void reinitHardware() {
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
}

void drawAdminMenuHighlight(int sel) {
  display.clearDisplay();
  drawHeader("Admin Panel");
  const char* items[] = {"Change pins", "Brightness", "Invert display", "Processes", "Exit admin"};
  for (int i = 0; i < 5; i++) {
    int y = 15 + i * 10;
    if (i == sel) {
      display.fillRect(0, y - 2, SCREEN_WIDTH, 12, WHITE);
      display.setTextColor(BLACK);
    } else {
      display.setTextColor(WHITE);
    }
    display.setCursor(5, y);
    display.print(i + 1);
    display.print(". ");
    display.print(items[i]);
  }
  drawTopBarIcons();
  display.display();
}

void adminPanel() {
  String pwd = inputStringWithKeyboard(true, "ENTER PASSWORD:");
  if (pwd == adminPassword && pwd != "") {
    adminMode = true;
    display.clearDisplay();
    display.setCursor(20, 28);
    display.println("Access granted");
    drawTopBarIcons();
    display.display();
    delay(1000);
  } else {
    if (pwd != "") showCommandOutput("Wrong password", 2);
    return;
  }
  while (adminMode) {
    int sel = 0;
    bool okPressed = false;
    drawAdminMenuHighlight(sel);
    while (true) {
      bool up = btnUp();
      bool down = btnDown();
      bool ok = btnOk();
      if (up) {
        sel = (sel - 1 + 5) % 5;
        drawAdminMenuHighlight(sel);
        delay(150);
      }
      if (down) {
        sel = (sel + 1) % 5;
        drawAdminMenuHighlight(sel);
        delay(150);
      }
      if (ok && !okPressed) {
        okPressed = true;
        switch (sel) {
          case 0: changePins(); break;
          case 1: setBrightness(); break;
          case 2: setInvert(); break;
          case 3: showProcesses(); break;
          case 4: adminMode = false; showMsg("Exited admin mode"); return;
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
// ФУНКЦИЯ ДЛЯ КОМАНДЫ "fs" (LittleFS)
// ----------------------------------------------------------------------------
void fsManager() {
  if (!LittleFS.begin(true)) {
    showCommandOutput("LittleFS mount failed", 2);
    return;
  }
  File root = LittleFS.open("/");
  if (!root) {
    showCommandOutput("Failed to open root", 2);
    return;
  }
  std::vector<String> files;
  File file = root.openNextFile();
  while (file) {
    files.push_back(String(file.name()));
    file = root.openNextFile();
  }
  root.close();
  if (files.size() == 0) {
    showCommandOutput("No files in LittleFS", 2);
    return;
  }
  int selected = 0;
  bool exit = false;
  while (!exit) {
    display.clearDisplay();
    drawHeader("LittleFS Manager");
    int visible = 4;
    int scroll = selected;
    if (scroll > visible - 1) scroll = selected - visible + 1;
    if (scroll < 0) scroll = 0;
    for (int i = 0; i < visible && i + scroll < (int)files.size(); i++) {
      int y = 14 + i * 10;
      if (selected == i + scroll) {
        display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(2, y);
        display.print(">");
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      display.setCursor(10, y);
      display.print(files[i + scroll]);
    }
    int yb = 14 + visible * 10;
    if (selected == (int)files.size()) {
      display.fillRect(0, yb - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, yb);
      display.print(">");
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(10, yb);
    display.println("Back");
    drawTopBarIcons();
    display.display();
    bool up = btnUp();
    bool down = btnDown();
    bool ok = btnOk();
    if (up) {
      selected--;
      if (selected < 0) selected = files.size();
      delay(150);
    }
    if (down) {
      selected++;
      if (selected > files.size()) selected = 0;
      delay(150);
    }
    if (ok) {
      if (selected == (int)files.size()) {
        exit = true;
        break;
      } else {
        String filename = files[selected];
        display.clearDisplay();
        drawHeader("Delete file");
        display.setCursor(10, 25);
        display.print("Delete ");
        display.print(filename);
        display.print("?");
        display.setCursor(30, 40);
        display.print("UP=Yes  DOWN=No");
        display.display();
        bool confirm = false;
        while (true) {
          if (btnUp()) {
            confirm = true;
            break;
          }
          if (btnDown()) {
            confirm = false;
            break;
          }
          delay(50);
        }
        if (confirm) {
          LittleFS.remove(filename);
          files.erase(files.begin() + selected);
          if (selected >= files.size()) selected = files.size();
          showMsg("Deleted");
        }
      }
    }
    delay(50);
  }
  LittleFS.end();
  appState = STATE_CONSOLE;
  consoleMode = CONSOLE_KEYBOARD;
  drawKeyboard();
  needRedraw = true;
}

// ----------------------------------------------------------------------------
// ОБРАБОТКА ДОЛГОГО НАЖАТИЯ КНОПОК
// ----------------------------------------------------------------------------
void handleButtons() {
  static bool okWasPressed = false;
  static unsigned long longPressStart = 0;
  bool okNow = btnOk();
  if (okNow && !okWasPressed) {
    longPressStart = millis();
    okWasPressed = true;
  }
  if (okNow && okWasPressed && (millis() - longPressStart) >= LONG_PRESS_MS && !longPressDetected) {
    longPressDetected = true;
    if (appState == STATE_NRF24_JAMMER && nrfOk) {
      if (jamming) {
        stopJammer();
        appState = STATE_NRF24_MENU;
        needRedraw = true;
        okWasPressed = false;
        longPressDetected = false;
        return;
      } else {
        jammerMode = (jammerMode == BLE_JAMMER_MODE) ? BLUETOOTH_JAMMER_MODE : BLE_JAMMER_MODE;
        if (jammerMode == BLE_JAMMER_MODE) showMsg("Mode: BLE");
        else showMsg("Mode: Bluetooth");
        needRedraw = true;
      }
    }
    delay(200);
    okWasPressed = false;
    longPressDetected = false;
    return;
  }
  if (!okNow && okWasPressed && !longPressDetected) {
    okWasPressed = false;
  }
  if (!okNow) {
    okWasPressed = false;
    longPressDetected = false;
  }
}

// ----------------------------------------------------------------------------
// WI-FI ЧАТ: ВЕБ-СТРАНИЦА И WEBSOCKET (HTML СТРАНИЦА)
// ----------------------------------------------------------------------------
const char CHAT_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Chat</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#1e1e2f;color:#f0f0f0;height:100vh;display:flex;justify-content:center;align-items:center}
.chat-container{width:100%;max-width:600px;height:90vh;background:#2d2d3a;border-radius:20px;box-shadow:0 10px 25px rgba(0,0,0,0.3);display:flex;flex-direction:column;overflow:hidden}
.header{background:#3a3a4e;padding:16px;text-align:center;font-size:1.4rem;font-weight:bold;border-bottom:1px solid #4a4a60}
.messages{flex:1;padding:16px;overflow-y:auto;display:flex;flex-direction:column;gap:12px}
.message{display:flex;align-items:flex-start;gap:10px;animation:fadeIn 0.2s ease}
.message.esp{flex-direction:row-reverse}
.avatar{width:36px;height:36px;border-radius:50%;background:#5a5a7a;display:flex;align-items:center;justify-content:center;font-weight:bold;font-size:14px}
.message.esp .avatar{background:#4c9aff}
.bubble{max-width:70%;padding:10px 14px;border-radius:18px;background:#3e3e55;color:#fff;word-wrap:break-word;font-size:14px;line-height:1.4}
.message.esp .bubble{background:#4c9aff;color:white}
.input-area{display:flex;padding:12px;background:#2a2a38;border-top:1px solid #4a4a60;gap:10px}
#msg{flex:1;padding:12px;border:none;border-radius:30px;background:#3a3a4e;color:white;font-size:14px;outline:none}
#msg::placeholder{color:#aaa}
button{padding:12px 24px;border:none;border-radius:30px;background:#4c9aff;color:white;font-weight:bold;cursor:pointer;transition:0.2s}
button:hover{background:#3a7fcc}
@keyframes fadeIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}
</style>
</head>
<body>
<div class="chat-container">
<div class="header">💬 ESP32 Chat</div>
<div class="messages" id="chat"></div>
<div class="input-area">
<input type="text" id="msg" placeholder="Напишите сообщение..." autocomplete="off">
<button onclick="sendMsg()">➤ Отправить</button>
</div>
</div>
<script>
let socket, reconnectTimer;
function addMessage(sender, text) {
const chatDiv=document.getElementById('chat');
const messageDiv=document.createElement('div');
messageDiv.className='message '+(sender==='esp'?'esp':'site');
const avatar=document.createElement('div');
avatar.className='avatar';
avatar.textContent=sender==='esp'?'🤖':'🌐';
const bubble=document.createElement('div');
bubble.className='bubble';
bubble.textContent=text;
messageDiv.appendChild(avatar);
messageDiv.appendChild(bubble);
chatDiv.appendChild(messageDiv);
chatDiv.scrollTop=chatDiv.scrollHeight;
}
function connect(){
socket=new WebSocket("ws://"+window.location.hostname+":81/");
socket.onopen=()=>{addMessage('site','✅ Соединение установлено');if(reconnectTimer)clearTimeout(reconnectTimer);};
socket.onmessage=(event)=>{addMessage('esp',event.data);};
socket.onclose=()=>{addMessage('site','❌ Соединение потеряно, переподключение...');reconnectTimer=setTimeout(connect,1000);};
socket.onerror=()=>{addMessage('site','⚠️ Ошибка соединения');};
}
function sendMsg(){
const input=document.getElementById('msg');
const text=input.value.trim();
if(text&&socket&&socket.readyState===WebSocket.OPEN){
socket.send(text);
addMessage('site',text);
input.value='';
}
}
document.getElementById('msg').addEventListener('keypress',(e)=>{if(e.key==='Enter')sendMsg();});
connect();
</script>
</body>
</html>
)rawliteral";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED:
      webSocketConnected = true;
      if (appState == STATE_WIFI_CHAT) {
        chatKeyboardActive = true;
        selectedKey = 0;
        capsLock = false;
        chatMessage = "";
        needRedraw = true;
      }
      Serial.println("WebSocket client connected");
      break;
case WStype_DISCONNECTED:
    webSocketConnected = false;
    Serial.println("WebSocket disconnected, reason unknown");
    if (appState == STATE_WIFI_CHAT) {
        chatKeyboardActive = false;
        needRedraw = true;
        // Показать сообщение пользователю, что связь потеряна
        showMsg("WebSocket lost");
    }
    break;
    case WStype_TEXT: {
      String msg = String((char*)payload);
      lastIncomingMsg = msg;
      addChatMessage("> " + msg);   // сохраняем в историю с префиксом
      needRedraw = true;
      Serial.print("Chat RX: ");
      Serial.println(msg);
      break;
    }
    default: break;
  }
}

void startWiFiChat() {
  if (!chatActive) {
    server.on("/", []() { server.send(200, "text/html", CHAT_PAGE); });
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    server.begin();
    chatActive = true;
    Serial.println("Chat server started");
  }
}

void stopWiFiChat() {
  if (chatActive) {
    server.stop();
    webSocket.close();
    chatActive = false;
    Serial.println("Chat server stopped");
  }
}

// =========================== КОНЕЦ ПЕРВОЙ ЧАСТИ (1200 строк) ===========================
// =========================== ВТОРАЯ ЧАСТЬ (1400 строк) ===========================
// Содержит все недостающие функции отрисовки, консольные режимы,
// updateDisplay, setup и loop. Полностью совместима с первой частью.

// ----------------------------------------------------------------------------
// ДОПОЛНИТЕЛЬНЫЕ ФУНКЦИИ ОТРИСОВКИ (не вошедшие в первую часть)
// ----------------------------------------------------------------------------

void drawWifiScan() {
  display.clearDisplay();
  drawHeader("WiFi Scan");

  if (wifiScanning) {
    display.setCursor(40, 30);
    display.println("Scanning...");
  } else if (wifiList.empty()) {
    display.setCursor(30, 30);
    display.println("No networks");
  } else {
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
      } else {
        display.setTextColor(SSD1306_WHITE);
      }

      String ssid = wifiList[idx].ssid;
      int rssi = wifiList[idx].rssi;
      String rssiStr = " " + String(rssi);
      
      // Упрощённый метод – используем символьную длину, а не пиксели (надёжнее)
      const int maxLen = 15; // символов
      display.setCursor(12, y);

      if (ssid.length() <= maxLen || wifiSelectedIdx != idx) {
        // Не выбранная или короткая – просто обрезаем
        if (ssid.length() > maxLen) ssid = ssid.substring(0, maxLen);
        display.print(ssid + rssiStr);
      } else {
        // Выбранная длинная сеть – бегущая строка
        if (millis() - lastScrollTime > 15) { // ускоренная прокрутка
          scrollOffset++;
          if (scrollOffset > ssid.length() + 3) scrollOffset = 0;
          lastScrollTime = millis();
        }
        String loopText = ssid.substring(scrollOffset) + " " + ssid.substring(0, scrollOffset);
        // Ограничиваем по длине
        if (loopText.length() > maxLen) loopText = loopText.substring(0, maxLen);
        display.print(loopText + rssiStr);
      }
    }

    int yb = 14 + visible * 12;
    if (wifiSelectedIdx == -1) {
      display.fillRect(0, yb - 1, SCREEN_WIDTH, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, yb);
      display.print(">");
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, yb);
    display.println("Back");
  }
  drawTopBarIcons();
  display.display();
}

// ----------------------------------------------------------------------------
// ОСТАЛЬНЫЕ ФУНКЦИИ ОТРИСОВКИ (nRF, IR, таймауты, подтверждения)
// ----------------------------------------------------------------------------
void drawNrfSpectrum() {
  display.clearDisplay();
  drawHeader("nRF Spectrum");
  if (!nrfOk) {
    display.setCursor(10, 30);
    display.println("nRF24 not found!");
    drawTopBarIcons();
    display.display();
    return;
  }
  int maxV = 1;
  for (int i = 0; i < NRF_CHANNELS; i++) if (nrfSmooth[i] > maxV) maxV = nrfSmooth[i];
  if (maxV < 3) maxV = 3;
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    int ch = map(x, 0, SCREEN_WIDTH - 1, 0, NRF_CHANNELS - 1);
    int sig = nrfSmooth[ch];
    if (sig < 3) sig = 0;
    int h = map(sig, 0, maxV, 0, 35);
    if (h > 0) display.drawLine(x, 48 - h, x, 48, SSD1306_WHITE);
  }
  display.drawLine(0, 48, SCREEN_WIDTH, 48, SSD1306_WHITE);
  display.setCursor(2, 52);
  display.printf("CH:%d", currentNrfChan);
  display.setCursor(70, 52);
  display.printf("%dMHz", 2400 + currentNrfChan);
  drawTopBarIcons();
  display.display();
}

void drawNrfJammer() {
  display.clearDisplay();
  if (!nrfOk) {
    drawHeader("nRF24 Error");
    display.setCursor(10, 30);
    display.println(nrfErrorMsg);
    drawTopBarIcons();
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
  drawTopBarIcons();
  display.display();
}

void drawIrCapture() {
  display.clearDisplay();
  drawHeader("IR Capture");
  if (irCapturing) {
    if (irTempReady) display.setCursor(20, 30);
    else display.setCursor(25, 30);
    display.println(irTempReady ? "Captured!" : "Listening...");
    display.setCursor(15, 42);
    display.printf("Timeout: %ds", (irTimeout - millis()) / 1000);
  } else {
    display.setCursor(30, 30);
    display.println("Press start");
  }
  display.setCursor(2, 56);
  display.println("Press=save");
  drawTopBarIcons();
  display.display();
}

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
  drawTopBarIcons();
  display.display();
}

void drawTimeoutMenu() {
  display.clearDisplay();
  drawHeader("Display Timeout");
  const char* opts[] = {"Off", "10s", "30s", "1min", "5min"};
  int vals[] = {0, 10, 30, 60, 300};
  int idx = 0;
  for (int i = 0; i < 5; i++) if (vals[i] == timeoutVal) idx = i;
  display.setCursor(30, 30);
  display.printf("Timeout: %s", opts[idx]);
  display.setCursor(2, 56);
  display.println("UP/DOWN=chg  OK=save");
  drawTopBarIcons();
  display.display();
}

void drawResetConfirm() {
  display.clearDisplay();
  drawHeader("Reset Device!");
  display.setCursor(15, 25);
  display.println("Reset ALL?");
  display.setCursor(15, 37);
  display.println("UP=YES  DOWN=NO");
  drawTopBarIcons();
  display.display();
}

void drawRebootConfirm() {
  display.clearDisplay();
  drawHeader("Reboot");
  display.setCursor(15, 30);
  display.println("Reboot?");
  display.setCursor(15, 42);
  display.println("UP=YES  DOWN=NO");
  drawTopBarIcons();
  display.display();
}

void drawChatKeyboard() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 10, WHITE);
  display.setCursor(2, 2);
  String visible = chatMessage;
  if (visible.length() > 18) visible = visible.substring(visible.length() - 18);
  display.print(visible);
  for (int row = 0; row < rowsKeyboard; row++) {
    for (int col = 0; col < colsKeyboard; col++) {
      int index = row * colsKeyboard + col;
      if (index >= totalKeys) break;
      int x = col * 16;
      int y = 13 + row * 9;
      if (index == selectedKey) {
        display.fillRect(x, y, 15, 8, WHITE);
        display.setTextColor(BLACK);
      } else {
        display.setTextColor(WHITE);
      }
      String key = capsLock ? upperKeys[index] : lowerKeys[index];
      if (key.length() > 0) {
display.setCursor(x + 1, y + 1);
if (key.length() == 1) {
    display.write(key[0]);  // выводим как символ
} else {
    display.print(key);      // для спецклавиш (Space, Caps...)
}
      }
    }
  }
  display.display();
}

void drawKeyboard() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 10, WHITE);
  display.setCursor(2, 2);
  String visible = consoleText;
  if (visible.length() > 18) visible = visible.substring(visible.length() - 18);
  display.print(visible);
  for (int row = 0; row < rowsKeyboard; row++) {
    for (int col = 0; col < colsKeyboard; col++) {
      int index = row * colsKeyboard + col;
      if (index >= totalKeys) break;
      int x = col * 16;
      int y = 13 + row * 9;
      if (index == selectedKey) {
        display.fillRect(x, y, 15, 8, WHITE);
        display.setTextColor(BLACK);
      } else {
        display.setTextColor(WHITE);
      }
      String key = capsLock ? upperKeys[index] : lowerKeys[index];
      if (key.length() > 0) {
        display.setCursor(x + 1, y + 1);
        display.print(key);
      }
    }
  }
  drawTopBarIcons();
  display.display();
}
void drawBatteryIcon(int x, int y) {
  int width = 16;
  int height = 8;
  int percent = (batteryRemaining / batteryCapacity) * 100;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  // Рисуем проценты слева от батарейки (отступ 2 пикселя)
  display.setTextSize(1);
  String percentStr = String(percent) + "%";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(percentStr, 0, 0, &x1, &y1, &w, &h);
  int percentX = x - w - 2;  // слева с отступом 2px
  int percentY = y + (height - h) / 2; // вертикальное центрирование
  display.setCursor(percentX, percentY);
  display.print(percentStr);

  // Рисуем батарейку
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  display.fillRect(x + width, y + 2, 2, height - 4, SSD1306_WHITE);
  int fillWidth = map(percent, 0, 100, 0, width - 2);
  if (fillWidth > 0) {
    display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
  }
}
void drawTopBarIcons() {
  // Позиция Wi-Fi (правый верхний угол)
  int x = SCREEN_WIDTH - 16;  // ширина иконки Wi-Fi 16px
  int y = 2;

  if (WiFi.status() == WL_CONNECTED) {
    drawWiFiIcon(x, y);
    // Отступ 20 пикселей влево от Wi-Fi
    x -= (16 + 20);   // 16px ширина иконки батареи + 20 отступ
  } else {
    // Если Wi-Fi не подключён, батарея всё равно рисуется, но со смещением
    x = SCREEN_WIDTH - 16 - 20 - 16; // отступ от правого края
  }
  drawBatteryIcon(x, y);
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
  display.setCursor(2, 52);
  display.printf("Ch:%d", currentWifiChan);
  display.setCursor(70, 52);
  display.printf("Pkt:%d", totalPackets);
  drawTopBarIcons();
  display.display();
}

void drawWifiInfo() {
  display.clearDisplay();
  drawHeader("Net Info");
  if (wifiSelectedIdx >= 0 && wifiSelectedIdx < (int)wifiList.size()) {
    WiFiNetworkInfo& n = wifiList[wifiSelectedIdx];
    display.setCursor(2, 14);
    display.printf("SSID: %s", n.ssid.substring(0, 13).c_str());
    display.setCursor(2, 24);
    display.printf("Ch: %d", n.channel);
    display.setCursor(2, 34);
    display.printf("RSSI: %d dBm", n.rssi);
    display.setCursor(2, 44);
    display.printf("Sec: %s", n.encryptionType.c_str());
  }
  display.setCursor(2, 56);
  display.print("Press=back");
  drawTopBarIcons();
  display.display();
}

void drawWifiConnecting() {
  display.clearDisplay();
  drawHeader("Connect WiFi");

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(connectSsid, 0, 0, &x1, &y1, &w, &h);
  const int maxPixels = 120;

  display.setCursor(5, 20);
  display.print("SSID: ");
  if (w <= maxPixels) {
    display.print(connectSsid);
  } else {
    if (millis() - lastScrollTime > 15) {
      scrollOffset++;
      if (scrollOffset > w + 25) scrollOffset = 0;
      lastScrollTime = millis();
    }
    String loopText = connectSsid + "   " + connectSsid;
    display.setCursor(5 + 40 - scrollOffset, 20);
    display.print(loopText);
  }

  display.setCursor(5, 35);
  display.print("Status: ");
  if (WiFi.status() == WL_CONNECTED) {
    display.print("Connected!");
    display.setCursor(5, 50);
    display.print("IP: ");
    display.print(WiFi.localIP());
  } else {
    display.print("Connecting...");
    int dots = (millis() / 500) % 4;
    for (int d = 0; d < dots; d++) display.print(".");
  }
  display.setCursor(5, 58);
  display.print("OK=back");
  drawTopBarIcons();
  display.display();
}
void drawFactoryResetConfirm() {
  display.clearDisplay();
  drawHeader("FACTORY RESET");
  display.setCursor(15, 25);
  display.println("Clear ALL settings?");
  display.setCursor(15, 37);
  display.println("UP=YES  DOWN=NO");
  drawTopBarIcons();
  display.display();
}



// ----------------------------------------------------------------------------
// ФУНКЦИИ ОБНОВЛЕНИЯ СПЕКТРОВ
// ----------------------------------------------------------------------------
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
// КОНСОЛЬНЫЕ РЕЖИМЫ info И def
// ----------------------------------------------------------------------------
void consoleInfoMode() {
  static bool btnProcessed = false;
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
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("SYSTEM INFO");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
  display.setCursor(0, 25);
  display.println(infoLines[infoPage]);
  display.setCursor(0, 56);
  display.print(String(infoPage + 1) + "/" + String(totalInfo));
  drawTopBarIcons();
  display.display();
}

void consoleDevMode() {
  static unsigned long lastDraw = 0;
  static unsigned long devStartTime = 0;
  if (DEV_MODE_TIMEOUT > 0 && devStartTime == 0) devStartTime = millis();
  if (DEV_MODE_TIMEOUT > 0 && (millis() - devStartTime >= DEV_MODE_TIMEOUT * 1000UL)) {
    consoleMode = CONSOLE_KEYBOARD;
    drawKeyboard();
    return;
  }
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
  if (millis() - lastDraw > 30) {
    display.clearDisplay();
    for (int i = 0; i < 200; i++) display.drawPixel(random(128), random(64), SSD1306_WHITE);
    for (int i = 0; i < 20; i++) {
      int x1 = random(128), y1 = random(64), x2 = random(128), y2 = random(64);
      display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }
    for (int i = 0; i < 10; i++) {
      int x = random(128), y = random(64), w = random(20), h = random(20);
      display.fillRect(x, y, w, h, SSD1306_WHITE);
    }
    drawTopBarIcons();
    display.display();
    lastDraw = millis();
  }
  static unsigned long lastSeqTime = 0;
  static bool upProcessed = false, downProcessed = false, okProcessedShort = false;
  if (btnUp() && !upProcessed) { enteredCode[codePos] = 1; codePos++; upProcessed = true; lastSeqTime = millis(); }
  else if (btnDown() && !downProcessed) { enteredCode[codePos] = 3; codePos++; downProcessed = true; lastSeqTime = millis(); }
  else if (btnOk() && !okProcessedShort && !longPressTriggered) { enteredCode[codePos] = 2; codePos++; okProcessedShort = true; lastSeqTime = millis(); }
  if (!btnUp()) upProcessed = false;
  if (!btnDown()) downProcessed = false;
  if (!btnOk()) okProcessedShort = false;
  if (codePos > 0 && (millis() - lastSeqTime > 1000)) codePos = 0;
  if (codePos >= 9) {
    bool correct = true;
    for (int i = 0; i < 9; i++) if (enteredCode[i] != secretCode[i]) correct = false;
    if (correct) {
      consoleMode = CONSOLE_KEYBOARD;
      drawKeyboard();
    }
    codePos = 0;
  }
}

// ----------------------------------------------------------------------------
// ОСНОВНОЙ ЦИКЛ КОНСОЛИ (УПРАВЛЕНИЕ ВИРТУАЛЬНОЙ КЛАВИАТУРОЙ)
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
  static unsigned long lastMoveTime = 0;
  static bool leftHeld = false, rightHeld = false;
  static bool okProcessed = false;
  if (btnUp()) {
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
  if (btnDown()) {
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
      if (consoleText.length() > 0) consoleText.remove(consoleText.length() - 1);
      drawKeyboard();
    } else if (key == "OK") {
      executeCommand(consoleText);
      consoleText = "";
      if (appState == STATE_CONSOLE && consoleMode == CONSOLE_KEYBOARD) drawKeyboard();
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
  if (!btnOk()) okProcessed = false;
}

// ----------------------------------------------------------------------------
// ОБНОВЛЕНИЕ ДИСПЛЕЯ (ГЛАВНЫЙ ДИСПЕТЧЕР)
// ----------------------------------------------------------------------------
void updateDisplay() {
  if (!displayOn) return;
if (statusMsg != "" && millis() - statusMsgTime < 1500) {
  display.clearDisplay();
  if (lastLongMsg != "") {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(lastLongMsg, 0, 0, &x1, &y1, &w, &h);
    if (millis() - lastMsgScrollTime > 15) {
      msgScrollOffset++;
      if (msgScrollOffset > w + 25) msgScrollOffset = 0;
      lastMsgScrollTime = millis();
    }
    String loopText = lastLongMsg + "   " + lastLongMsg;
    display.setCursor(20 - msgScrollOffset, 28);
    display.print(loopText);
  } else {
    display.setCursor(20, 28);
    display.println(statusMsg);
  }
  drawTopBarIcons();
  display.display();
  return;
}
  if (appState == STATE_CONSOLE_COMMAND_OUTPUT) {
    display.clearDisplay();
    display.setCursor(0, 10);
    display.println(commandOutput);
    display.setCursor(0, 50);
    display.println("Press OK to exit");
    drawTopBarIcons();
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
  switch (appState) {
    case STATE_MAIN_MENU: drawMainMenu(); break;
    case STATE_WIFI_MENU: drawWifiMenu(); break;
    case STATE_WIFI_SCAN: drawWifiScan(); break;
    case STATE_WIFI_SPECTRUM: drawWifiSpectrum(); break;
    case STATE_WIFI_ACTIONS: drawWifiActions(); break;
    case STATE_WIFI_INFO: drawWifiInfo(); break;
    case STATE_WIFI_CONNECTING: drawWifiConnecting(); break;
    case STATE_WIFI_CHAT: drawWifiChat(); break;
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
    case STATE_FACTORY_RESET_CONFIRM: drawFactoryResetConfirm(); break;
    case STATE_CONSOLE: consoleLoop(); break;
    default: break;
  }
  needRedraw = false;
}

// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------
void setup() {
  delay(500);
  deviceBootTime = millis();
  lastActivityTime = millis();
  Serial.begin(115200);
  Serial.println("\ncatZERO v3.0 Starting");
  debugPrint("Setup begin");

  // Дисплей
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) for (;;);
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);
  showBootLogo();
  debugPrint("Display initialized");

  // Кнопки
  pinMode(BTN_ANALOG_PIN, INPUT);
  analogSetPinAttenuation(BTN_ANALOG_PIN, ADC_11db);
  loadTimeout();
  debugPrint("Timeout loaded");

  // Кнопка сброса на GPIO10 (если не используется, можно закомментировать)
  pinMode(10, INPUT_PULLUP);

  // Wi-Fi
  WiFi.mode(WIFI_OFF);
  delay(100);
  debugPrint("WiFi mode set to OFF");

  // SPI и nRF24
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, RF_CSN_PIN);
  SPI.setFrequency(8000000);
  debugPrint("SPI started");
  if (radio.begin(RF_CE_PIN, RF_CSN_PIN) && radio.isChipConnected()) {
    nrfOk = true; nrfErrorMsg = "OK";
    radio.setChannel(42); radio.setPALevel(RF24_PA_MAX, true);
    radio.setAutoAck(false); radio.disableCRC(); radio.stopListening();
    Serial.println("nRF24 OK");
  } else {
    nrfOk = false; nrfErrorMsg = "not found";
    Serial.println("nRF24 FAIL");
  }

  // IR
  irRx = new IRrecv(IR_RX_PIN);
  irRx->enableIRIn();
  irRxOk = true;
  irTx = new IRsend(IR_TX_PIN);
  irTx->begin();
  for (int i = 0; i < IR_SLOTS_COUNT; i++) irSlots[i].isValid = false;
  loadIrSlots();
  debugPrint("IR initialized");

  // LittleFS
  LittleFS.begin(true);
  debugPrint("LittleFS mounted");

  // SD (закомментировано)
  // initSDCard();

  // Загрузка сохранённых Wi-Fi сетей
  loadWiFiCredentials();   // <-- ЭТО ВАЖНО

  // Добавление предустановленных сетей (если их ещё нет)
  const char* presetSSID[] = {"FRITZ!Box 6591 Cabel MK", "moto e13 ", "", "", ""};
  const char* presetPASS[] = {"43481208496765148415 ", "12345678", "", "", ""};
  for (int i = 0; i < 5; i++) {
    if (strlen(presetSSID[i]) == 0) continue;
    bool already = false;
    for (int j = 0; j < savedNetworksCount; j++) {
      if (savedSSID[j] == String(presetSSID[i])) {
        already = true;
        break;
      }
    }
    if (!already) {
      saveWiFiCredentials(String(presetSSID[i]), String(presetPASS[i]));
      Serial.print("Added preset: ");
      Serial.println(presetSSID[i]);
    }
  }
  // Перезагружаем список после добавления
  loadWiFiCredentials();

  // Инициализация массивов спектров и состояния
  memset(nrfSmooth, 0, sizeof(nrfSmooth));
  memset(wifiSmooth, 0, sizeof(wifiSmooth));
  appState = STATE_MAIN_MENU;
  needRedraw = true;

  // Автоподключение
  autoConnectScheduled = true;
  autoConnectTime = millis() + 500;
  debugPrint("Auto-connect scheduled");

  chatActive = false;

  Serial.println("Setup complete");
  debugPrintState();
}

// ----------------------------------------------------------------------------
// LOOP (ИСПРАВЛЕННЫЙ)
// ----------------------------------------------------------------------------
void loop() {
  handleButtons();
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  bool up = btnUp();
  bool down = btnDown();
  bool ok = btnOk() && !longPressDetected;

  static bool lastUp = false, lastDown = false, lastOk = false;
  if (up && !lastUp) debugButton("UP");
  if (down && !lastDown) debugButton("DOWN");
  if (ok && !lastOk) debugButton("OK");
  lastUp = up; lastDown = down; lastOk = ok;

  // Включение дисплея по нажатию
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

  // ========== АВТОПОДКЛЮЧЕНИЕ ==========
  if (autoConnectScheduled && millis() >= autoConnectTime) {
    autoConnectScheduled = false;
    Serial.println("Auto-connect timer triggered");
    loadWiFiCredentials();
    if (savedNetworksCount > 0) {
      bool connectedToSaved = false;
      if (WiFi.status() == WL_CONNECTED) {
        String currentSSID = WiFi.SSID();
        for (int i = 0; i < savedNetworksCount; i++) {
          if (currentSSID == savedSSID[i]) { connectedToSaved = true; break; }
        }
      }
      if (!connectedToSaved) attemptAutoConnect();
      else Serial.println("Already connected to a saved network");
    } else Serial.println("No saved networks");
  }

  // ========== РУЧНОЕ ПОДКЛЮЧЕНИЕ ==========
  if (appState == STATE_WIFI_CONNECTING) {
    if (connectInProgress) {
if (WiFi.status() == WL_CONNECTED) {
  connectInProgress = false;
  lastConnectedSSID = connectSsid;
  lastConnectedPassword = connectPassword;
  showMsg("Connected to Wi-Fi");
  delay(2000);
  appState = STATE_WIFI_ACTIONS;
  needRedraw = true;
}else if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
        connectInProgress = false; showMsg("Connection failed"); delay(2000);
        appState = STATE_WIFI_ACTIONS; needRedraw = true;
      } else if (needRedraw && displayOn) drawWifiConnecting();
    } else if (ok) { appState = STATE_WIFI_ACTIONS; needRedraw = true; delay(200); }
    if (needRedraw && displayOn) updateDisplay();
    delay(5);
    return;
  }

  // ========== ЗАПУСК/ОСТАНОВКА ЧАТА ==========
  if (appState == STATE_WIFI_CHAT && !chatActive && WiFi.status() == WL_CONNECTED) {
    startWiFiChat();
    chatKeyboardActive = false;
    webSocketConnected = false;
  }
  if (appState != STATE_WIFI_CHAT && chatActive) {
    stopWiFiChat();
    chatKeyboardActive = false;
    webSocketConnected = false;
  }

  // ========== ОБРАБОТКА ЧАТА (отдельный блок) ==========
  if (appState == STATE_WIFI_CHAT && chatActive) {
    server.handleClient();
    webSocket.loop();

    static unsigned long longPressStart = 0;
    static bool longPressFlag = false;

    // Подтверждение выхода
    if (confirmExit) {
      display.fillRect(20, 25, 88, 30, SSD1306_BLACK);
      display.drawRect(20, 25, 88, 30, SSD1306_WHITE);
      display.setCursor(25, 33);
      display.print("Exit chat?");
      display.setCursor(25, 43);
      display.print("UP=Yes  DOWN=No");
      display.display();

      if (btnUp()) {
        appState = STATE_WIFI_MENU;
        needRedraw = true;
        confirmExit = false;
        chatKeyboardActive = false;
        stopWiFiChat();
        delay(200);
      } else if (btnDown()) {
        confirmExit = false;
        needRedraw = true;
        delay(200);
      }
      return;
    }

    // Режим информации (клавиатура выключена)
    if (!chatKeyboardActive) {
      if (btnOk()) {
        if (!longPressFlag) {
          longPressStart = millis();
          longPressFlag = true;
        } else if (millis() - longPressStart >= LONG_PRESS_MS) {
          confirmExit = true;
          needRedraw = true;
          longPressFlag = false;
        }
      } else {
        if (longPressFlag && millis() - longPressStart < LONG_PRESS_MS) {
          chatKeyboardActive = true;
          selectedKey = 0;
          capsLock = false;
          chatMessage = "";
          needRedraw = true;
        }
        longPressFlag = false;
      }
    }
    // Режим клавиатуры
    else {
      static unsigned long lastMoveTime = 0;
      static bool leftHeld = false, rightHeld = false;
      static bool okProcessed = false;

      if (btnUp()) {
        if (!leftHeld) {
          leftHeld = true; lastMoveTime = millis();
          selectedKey = (selectedKey - 1 + totalKeys) % totalKeys;
          needRedraw = true;
        } else if (millis() - lastMoveTime > CONSOLE_AUTOREPEAT_DELAY) {
          lastMoveTime = millis();
          selectedKey = (selectedKey - 1 + totalKeys) % totalKeys;
          needRedraw = true;
        }
      } else leftHeld = false;

      if (btnDown()) {
        if (!rightHeld) {
          rightHeld = true; lastMoveTime = millis();
          selectedKey = (selectedKey + 1) % totalKeys;
          needRedraw = true;
        } else if (millis() - lastMoveTime > CONSOLE_AUTOREPEAT_DELAY) {
          lastMoveTime = millis();
          selectedKey = (selectedKey + 1) % totalKeys;
          needRedraw = true;
        }
      } else rightHeld = false;

      if (btnOk() && !okProcessed) {
        okProcessed = true;
        String key = capsLock ? upperKeys[selectedKey] : lowerKeys[selectedKey];
        if (key == "Caps") {
          capsLock = !capsLock;
          needRedraw = true;
        } else if (key == "Del") {
          if (chatMessage.length() > 0) chatMessage.remove(chatMessage.length() - 1);
          needRedraw = true;
        } else if (key == "OK") {
          if (chatMessage.length() > 0) {
            if (webSocket.connectedClients() > 0) {
              webSocket.broadcastTXT(chatMessage);
              Serial.print("Chat send: ");
              Serial.println(chatMessage);
              chatMessage = "";
              needRedraw = true;
            } else showMsg("No WebSocket client!");
          }
        } else if (key == "Exit") {
          confirmExit = true;
          needRedraw = true;
        } else if (key == "Space") {
          chatMessage += " ";
          needRedraw = true;
        } else if (key.length() > 0 && key != "Caps" && key != "Del" && key != "OK" && key != "Exit") {
          if (chatMessage.length() < 240) {
            chatMessage += key;
            needRedraw = true;
          } else showMsg("Msg too long");
        }
        delay(150);
      }
      if (!btnOk()) okProcessed = false;
    }

    if (needRedraw && displayOn) updateDisplay();
    delay(5);
    return;  // чат забрал весь цикл
  }
if (millis() - lastBatteryUpdate >= 1000) { // каждую секунду
  float used = estimatedCurrentMA / 3600.0; // мА·ч за секунду
  batteryRemaining -= used;
  if (batteryRemaining < 0) batteryRemaining = 0;
  lastBatteryUpdate = millis();
  needRedraw = true; // перерисовываем, чтобы обновить иконку
}
  // ========== ОСНОВНАЯ НАВИГАЦИЯ ПО МЕНЮ (для всех остальных состояний) ==========
  if (appState != STATE_CONSOLE && appState != STATE_CONSOLE_COMMAND_OUTPUT) {
    // Обработка UP
    if (up) {
      switch (appState) {
        case STATE_MAIN_MENU: mainIdx = (mainIdx - 1 + MAIN_SIZE) % MAIN_SIZE; break;
        case STATE_WIFI_MENU: wifiMenuIdx = (wifiMenuIdx - 1 + WIFI_MENU_SIZE) % WIFI_MENU_SIZE; break;
        case STATE_WIFI_SCAN:
          if (!wifiScanning) {
            int sz = wifiList.size();
            scrollOffset = 0;
            lastScrollTime = 0;
            if (wifiSelectedIdx == -1) { if (sz > 0) wifiSelectedIdx = sz - 1; }
            else { wifiSelectedIdx--; if (wifiSelectedIdx < -1) wifiSelectedIdx = -1; }

          }
          break;
          case STATE_FACTORY_RESET_CONFIRM:
  if (up) {
    clearAllPreferences();
    delay(500);
    ESP.restart();
  } else if (down) {
    appState = STATE_SETTINGS_MENU;
    needRedraw = true;
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
          idx = (idx - 1 + 5) % 5; timeoutVal = opts[idx]; needRedraw = true;
          break;
        }
        case STATE_TVBGONE: tvbgone_region = !tvbgone_region; delay(200); break;
        case STATE_RESET_CONFIRM: clearAllPreferences(); delay(500); ESP.restart(); break;
        case STATE_REBOOT_CONFIRM: ESP.restart(); break;
        default: break;
      }
      delay(150);
    }
    // Обработка DOWN
    if (down) {
      switch (appState) {
        case STATE_MAIN_MENU: mainIdx = (mainIdx + 1) % MAIN_SIZE; break;
        case STATE_WIFI_MENU: wifiMenuIdx = (wifiMenuIdx + 1) % WIFI_MENU_SIZE; break;
        case STATE_WIFI_SCAN:
          if (!wifiScanning) {
            int sz = wifiList.size();
            scrollOffset = 0;
            lastScrollTime = 0;
            if (wifiSelectedIdx == -1) { if (sz > 0) wifiSelectedIdx = 0; }
            else { wifiSelectedIdx++; if (wifiSelectedIdx >= sz) wifiSelectedIdx = -1; }
            scrollOffset = 0;
            lastScrollTime = 0;
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
          idx = (idx + 1) % 5; timeoutVal = opts[idx]; needRedraw = true;
          break;
        }
        case STATE_TVBGONE: tvbgone_region = !tvbgone_region; delay(200); break;
        case STATE_RESET_CONFIRM:
        case STATE_REBOOT_CONFIRM: appState = STATE_SETTINGS_MENU; needRedraw = true; break;
        default: break;
      }
      delay(150);
    }
    // Обработка OK
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
            case 2:
              if (WiFi.status() == WL_CONNECTED) { appState = STATE_WIFI_CHAT; needRedraw = true; }
              else showMsg("No WiFi connection");
              break;
            case 3: appState = STATE_MAIN_MENU; break;
          }
          break;
        case STATE_WIFI_SCAN:
          if (!wifiScanning) {
            if (wifiSelectedIdx == -1) appState = STATE_WIFI_MENU;
            else if (wifiSelectedIdx >=0 && wifiSelectedIdx<(int)wifiList.size()) appState = STATE_WIFI_ACTIONS;
          }
          break;
        case STATE_WIFI_ACTIONS:
          switch(wifiActionIdx) {
            case 0: appState = STATE_WIFI_INFO; break;
            case 1:
              deauthActive=true; memcpy(targetBSSID,wifiList[wifiSelectedIdx].bssid,6);
              targetChan = wifiList[wifiSelectedIdx].channel; showMsg("Deauth started");
              break;
            case 2: deauthActive=false; targetChan=0; showMsg("Deauth stopped"); appState = STATE_WIFI_SCAN; break;
            case 3: {
              if (wifiSelectedIdx >=0 && wifiSelectedIdx<(int)wifiList.size()) {
                String ssid = wifiList[wifiSelectedIdx].ssid;
                String password = inputStringWithKeyboard(true, "Enter password:");
                if (password.length() > 0) {
                  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) showMsg("Already connected");
                  else {
                    connectSsid = ssid; connectPassword = password;
                    connectInProgress = true; connectStartTime = millis();
                    WiFi.disconnect(true); delay(100); WiFi.mode(WIFI_STA);
                    WiFi.begin(connectSsid.c_str(), connectPassword.c_str());
                    appState = STATE_WIFI_CONNECTING; needRedraw = true;
                  }
                } else showMsg("No password");
              } else showMsg("No network selected");
              break;
            }
case 4: { // Save
  if (wifiSelectedIdx >= 0 && wifiSelectedIdx < (int)wifiList.size()) {
    String ssid = wifiList[wifiSelectedIdx].ssid;
    String password = "";
    
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) {
      password = lastConnectedPassword;
      Serial.println("Using password from current connection");
    } else {
      if (wifiList[wifiSelectedIdx].encryptionType != "Open") {
        password = inputStringWithKeyboard(true, "Enter password:");
        if (password.length() == 0) {
          showMsg("No password, save cancelled");
          break;
        }
      }
    }
    saveWiFiCredentials(ssid, password);
    showMsg("Network saved");
  } else {
    showMsg("No network selected");
  }
  appState = STATE_WIFI_SCAN;
  needRedraw = true;
  break;
}
            case 5:
              if (WiFi.status() == WL_CONNECTED) disconnectWiFi();
              else showMsg("Not connected");
              appState = STATE_WIFI_SCAN; needRedraw = true;
              break;
            case 6: appState = STATE_WIFI_SCAN; break;
          }
          break;
        case STATE_WIFI_INFO: appState = STATE_WIFI_ACTIONS; break;
        case STATE_WIFI_SPECTRUM: wifiSpectrumActive=false; esp_wifi_set_promiscuous(false); WiFi.mode(WIFI_STA); appState = STATE_WIFI_MENU; break;
        case STATE_WIFI_CHAT: appState = STATE_WIFI_MENU; needRedraw = true; break;
        case STATE_NRF24_MENU:
          switch(nrfMenuIdx) {
            case 0: nrfCalibrated=false; appState = STATE_NRF24_SPECTRUM; break;
            case 1: if(nrfOk) appState = STATE_NRF24_JAMMER; else showMsg(nrfErrorMsg.c_str()); break;
            case 2: recheckNRF24(); break;
            case 3: appState = STATE_MAIN_MENU; break;
          }
          break;
        case STATE_NRF24_SPECTRUM: appState = STATE_NRF24_MENU; break;
        case STATE_NRF24_JAMMER:
          if(!jamming && nrfOk) startJammer(); else if(jamming) { stopJammer(); appState = STATE_NRF24_MENU; }
          break;
        case STATE_IR_MENU:
          switch(irMenuIdx) {
            case 0: irCapturing=true; irTempReady=false; irTimeout=millis()+15000; if(irRx) irRx->resume(); appState = STATE_IR_CAPTURE; break;
            case 1: irTxScroll=0; appState = STATE_IR_TRANSMIT; break;
            case 2: eraseAllIrSlots(); appState = STATE_IR_MENU; break;
            case 3: appState = STATE_TVBGONE; break;
            case 4: appState = STATE_MAIN_MENU; break;
          }
          break;
        case STATE_IR_CAPTURE:
          if(irTempReady) {
            int freeSlot=-1; for(int i=0;i<IR_SLOTS_COUNT;i++) if(!irSlots[i].isValid) { freeSlot=i; break; }
            if(freeSlot==-1) freeSlot=curIrSlot; curIrSlot=freeSlot;
            irSlots[curIrSlot].rawLength=tempRawLen; memcpy(irSlots[curIrSlot].rawBuffer,tempRaw,tempRawLen*2);
            irSlots[curIrSlot].protocolName=tempProto; irSlots[curIrSlot].isValid=true; saveIrSlots(); irTempReady=false;
          }
          appState = STATE_IR_MENU; break;
        case STATE_IR_TRANSMIT:
          if(irTxScroll==IR_SLOTS_COUNT) appState = STATE_IR_MENU; else if(irSlots[irTxScroll].isValid) sendIr(irTxScroll);
          break;
        case STATE_TVBGONE: tvbgone_menu(); break;
        case STATE_SETTINGS_MENU:
          switch(settingsIdx) {
            case 0: appState = STATE_SYSTEM_INFO; break;
            case 1: timeoutVal=displayTimeoutSec; appState = STATE_TIMEOUT; break;
            case 2: appState = STATE_RESET_CONFIRM; break;
            case 3: appState = STATE_REBOOT_CONFIRM; break;
            case 4: appState = STATE_MAIN_MENU; break;
          }
          break;
        case STATE_SYSTEM_INFO: appState = STATE_SETTINGS_MENU; break;
        case STATE_TIMEOUT: displayTimeoutSec=timeoutVal; saveTimeout(); appState = STATE_SETTINGS_MENU; break;
        default: break;
      }
      delay(200);
    }
  }

  // ========== ОБНОВЛЕНИЕ СПЕКТРОВ, ДЖАММЕРА, ДЕАУТА ==========
  if (displayOn && displayTimeoutSec > 0 && (millis() - lastActivityTime) > (displayTimeoutSec * 1000UL)) setPower(false);
  if (appState == STATE_WIFI_SPECTRUM && wifiSpectrumActive) updateWifiSpectrum();
  if (appState == STATE_NRF24_SPECTRUM && nrfOk) updateNrfSpectrum();
  if (appState == STATE_NRF24_JAMMER && jamming && nrfOk && millis() - lastJamTime > JAM_INTERVAL_MS) updateJammer();
  if (deauthActive && targetChan != 0 && millis() - lastDeauthTime > 100) { sendDeauth(); lastDeauthTime = millis(); }
  if (appState == STATE_IR_CAPTURE && irCapturing) processIrCapture();

  // ========== ОБНОВЛЕНИЕ ДИСПЛЕЯ ==========
  if (needRedraw && displayOn) updateDisplay();
  delay(5);
}
