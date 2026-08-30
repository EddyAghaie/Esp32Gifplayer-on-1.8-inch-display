/*
  ====================================================================
  GifFrame - ESP32 + ST7735 1.8" (160x128 Landscape) WiFi Image/GIF Frame
  ====================================================================
  پین‌بندی نهایی:
    SDA (MOSI) = GPIO23
    SCK        = GPIO18
    CS         = GPIO5
    LED (BL)   = GPIO4
    RESET      = GPIO2
    DC         = GPIO21
    VCC -> 3.3V , GND -> GND

  کتابخانه‌های لازم (از Library Manager نصب کنید):
    - Adafruit GFX Library
    - Adafruit ST7735 and ST7789 Library (+ Adafruit BusIO)
    - ESPAsyncWebServer + AsyncTCP  (فورک ESP32Async از گیت‌هاب - ZIP)
    - LittleFS (داخل هسته ESP32)
    - Preferences (داخل هسته ESP32)
    - QRCodeGen.h / QRCodeGen.c (کنار فایل .ino - فایل‌های ricmoo/QRCode با اصلاح bool)

  نکته: AnimatedGIF دیگر لازم نیست. پردازش/ریسایز کاملاً سمت گوشی
        با جاوااسکریپت خالص (بدون نیاز به اینترنت/CDN) انجام می‌شود
        و یک فرمت باینری سبک سفارشی به نام "GFA1" به دستگاه ارسال می‌شود.

  منطق کلی:
    1) بوت اول (هیچ فایلی ذخیره نشده): صفحه QR1 با پس‌زمینه سفید نشان داده
       می‌شود - برای اتصال خودکار گوشی به وای‌فای دستگاه (بدون پسورد).
    2) به محض اتصال یک کلاینت به AP (رویداد WiFi)، صفحه به QR2 با پس‌زمینه
       قرمز تغییر می‌کند که آدرس "http://192.168.4.1/" را کد کرده -
       با اسکن، صفحه تنظیمات مستقیماً در مرورگر گوشی باز می‌شود (بدون
       نیاز به Captive Portal، که در iOS محدودیت‌های زیادی دارد).
    3) صفحه‌ی وب: آپلود هر عکس/گیف از گالری گوشی + راهنمای انگلیسی برای
       افزودن به Home Screen یا ذخیره آدرس 192.168.4.1.
       مرورگر خودش عکس/گیف رو دیکود، cover (پر کردن کامل صفحه بدون فضای
       خالی) به 160x128 ریسایز، و به فرمت "GFA1" (هدر + فریم‌های RGB565
       خام) تبدیل و آپلود می‌کند.
    4) ESP32 فایل "/current.anim" را ذخیره و به‌صورت لوپ دائم پخش می‌کند؛
       از این لحظه صفحات QR دیگر نمایش داده نمی‌شوند.
    5) فکتوری ریست: شمارش 5 بار روشن/خاموش در بازه 10 ثانیه با Preferences (NVS)
       -> پاک کردن LittleFS و ریست تنظیمات (برگشت به صفحه QR1).

  فرمت فایل "/current.anim" (GFA1):
    بایت 0-3   : "GFA1"
    بایت 4-5   : width  (uint16, little-endian) -> باید برابر SCREEN_W باشد
    بایت 6-7   : height (uint16, little-endian) -> باید برابر SCREEN_H باشد
    بایت 8-9   : frameCount (uint16, little-endian)
    سپس به ازای هر فریم:
      2 بایت delay_ms (uint16, little-endian)
      width*height*2 بایت پیکسل RGB565 (هر پیکسل 2 بایت big-endian/wire order)

  ====================================================================
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <FS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "QRCodeGen.h" // کتابخانه QRCode (ricmoo) - فایل‌های QRCodeGen.h/.c باید کنار .ino باشن

// ---------------- پین‌بندی ----------------
#define TFT_CS   5
#define TFT_DC   21
#define TFT_RST  2
#define TFT_LED  4
// MOSI=23 (SDA), SCK=18 -> پیش‌فرض HSPI/VSPI روی ESP32 همین‌ها هستند

#define SCREEN_W 160
#define SCREEN_H 128

// اگر تصویر روی نمایشگر شیفت/کراپ/wrap شده دیده شد، این مقادیر را تغییر بده.
// 2,1 رایج‌ترین مقدار برای پنل‌های 1.8" ST7735 (132x162 GRAM روی پنل 128x160) است.
// مقادیر دیگری که می‌توان امتحان کرد: (0,0), (2,3), (1,2), (3,2), (4,0)
// با کد اولیه (offset=0,0) تصاویر کامل و بدون موج/کجی نمایش داده می‌شدن.
// تغییر این مقادیر باعث موج‌دار/کج‌شدن تصویر می‌شود - دست نزنید مگر مطمئن باشید.
#define COL_START 0
#define ROW_START 0

// X_OFFSET/Y_OFFSET برای تنظیم ریز (fine-tuning) اضافه، معمولاً 0 بماند.
#define X_OFFSET 0
#define Y_OFFSET 0

// کلاس کوچک مشتق‌شده فقط برای دسترسی به setColRowStart که در کتابخونه protected است
class Adafruit_ST7735_Custom : public Adafruit_ST7735 {
public:
  Adafruit_ST7735_Custom(int8_t cs, int8_t dc, int8_t rst) : Adafruit_ST7735(cs, dc, rst) {}
  void setOffset(int8_t col, int8_t row) { setColRowStart(col, row); }
};

Adafruit_ST7735_Custom tft = Adafruit_ST7735_Custom(TFT_CS, TFT_DC, TFT_RST);
AsyncWebServer server(80);
Preferences prefs;

String apSSID;

// از بخش پخش (پایین‌تر تعریف می‌شود) - برای جلوگیری از دست‌کاری صفحه در حین پخش فریم
extern volatile bool animPlaying;

// ====================================================================
//                     ۱) FACTORY RESET LOGIC
// ====================================================================
// با 5 بار روشن/خاموش در بازه 10 ثانیه از حافظه NVS، ریست فکتوری انجام می‌شود.
void checkFactoryReset() {
  prefs.begin("sysinfo", false);
  uint8_t bootCount = prefs.getUChar("bootcnt", 0);

  bootCount++;
  if (bootCount >= 5) {
    Serial.println("== FACTORY RESET TRIGGERED ==");
    prefs.putUChar("bootcnt", 0);
    prefs.end();

    // پاک کردن کامل LittleFS (شامل فریم فعال و تنظیمات)
    LittleFS.format();

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);
    tft.setCursor(10, 60);
    tft.println("Factory Reset...");
    delay(1500);
    ESP.restart();
    return;
  }

  prefs.putUChar("bootcnt", bootCount);
  prefs.end();
}

void resetBootCounterAfterTimeout() {
  static unsigned long startTime = millis();
  static bool done = false;
  if (!done && millis() - startTime > 10000) {
    prefs.begin("sysinfo", false);
    prefs.putUChar("bootcnt", 0);
    prefs.end();
    done = true;
  }
}

// ====================================================================
//                     ۲) صفحات QR (Landscape) - دو مرحله‌ای
// ====================================================================
// QR_VERSION=4 برای هر دو متن (WiFi config و URL) کافی است.
// سایز بافر = ((4*version+17)^2 + 7) / 8  ->  برای version=4: ((33*33)+7)/8 = 137
#define QR_VERSION 4
#define QR_BUFFER_SIZE 137
uint8_t qrcodeBuffer[QR_BUFFER_SIZE];

// رسم ماژول‌های QR در یک مستطیل مشخص
void drawQRModules(QRCode &qrcode, int offsetX, int offsetY, int pixelSize, uint16_t fg, uint16_t bg) {
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      uint16_t color = qrcode_getModule(&qrcode, x, y) ? fg : bg;
      tft.fillRect(offsetX + x * pixelSize, offsetY + y * pixelSize, pixelSize, pixelSize, color);
    }
  }
}

// محاسبه‌ی اندازه و موقعیت QR وسط‌چین (هم افقی هم عمودی) با کمی فضا برای یک برچسب بالای آن
void computeQRLayoutCentered(QRCode &qrcode, int &offsetX, int &offsetY, int &pixelSize, int &totalSize, int topReserved) {
  const int sideMargin = 6;
  const int bottomMargin = 4;
  int availW = SCREEN_W - sideMargin * 2;
  int availH = SCREEN_H - topReserved - bottomMargin;

  pixelSize = min(availW / qrcode.size, availH / qrcode.size);
  if (pixelSize < 1) pixelSize = 1;
  totalSize = pixelSize * qrcode.size;

  offsetX = (SCREEN_W - totalSize) / 2 + X_OFFSET;
  offsetY = topReserved + (SCREEN_H - topReserved - totalSize) / 2 + Y_OFFSET;
}

// رسم یک برچسب کوچک وسط‌چین در بالای صفحه
void drawTopLabel(const char* text, uint16_t fg, uint16_t bg) {
  tft.setTextColor(fg, bg);
  tft.setTextSize(1);
  int textW = strlen(text) * 6; // هر کاراکتر در سایز 1 حدوداً 6px
  int x = (SCREEN_W - textW) / 2;
  if (x < 0) x = 0;
  tft.setCursor(x, 4);
  tft.println(text);
}

// -------- مرحله ۱: صفحه اتصال به وای‌فای (پس‌زمینه سفید) --------
// -------- مرحله ۱: صفحه اتصال به وای‌فای (پس‌زمینه سفید) --------
void drawConnectScreen() {
  tft.fillScreen(ST77XX_WHITE);

  String qrText = "WIFI:T:nopass;S:" + apSSID + ";;";
  QRCode qrcode;
  qrcode_initText(&qrcode, qrcodeBuffer, QR_VERSION, ECC_MEDIUM, qrText.c_str());

  const int topReserved = 16;
  int offsetX, offsetY, pixelSize, totalSize;
  computeQRLayoutCentered(qrcode, offsetX, offsetY, pixelSize, totalSize, topReserved);

  drawTopLabel("WiFi Connect", ST77XX_BLACK, ST77XX_WHITE);
  drawQRModules(qrcode, offsetX, offsetY, pixelSize, ST77XX_BLACK, ST77XX_WHITE);
}

// -------- مرحله ۲: صفحه باز کردن صفحه تنظیمات (پس‌زمینه قرمز) --------
void drawOpenPageScreen() {
  tft.fillScreen(ST77XX_RED);

  String url = "http://192.168.4.1/";
  QRCode qrcode;
  qrcode_initText(&qrcode, qrcodeBuffer, QR_VERSION, ECC_MEDIUM, url.c_str());

  const int topReserved = 16;
  int offsetX, offsetY, pixelSize, totalSize;
  computeQRLayoutCentered(qrcode, offsetX, offsetY, pixelSize, totalSize, topReserved);

  // کادر سفید پشت QR برای کنتراست بهتر روی پس‌زمینه قرمز
  tft.fillRect(offsetX - 4, offsetY - 4, totalSize + 8, totalSize + 8, ST77XX_WHITE);

  drawTopLabel("Upload Page", ST77XX_WHITE, ST77XX_RED);
  drawQRModules(qrcode, offsetX, offsetY, pixelSize, ST77XX_BLACK, ST77XX_WHITE);
}

// ====================================================================
//                     ۳) راه‌اندازی WiFi AP (بدون Captive Portal)
// ====================================================================

// وقتی هیچ فایل anim پخش نمی‌شود (هنوز در مرحله تنظیمات هستیم)، با اتصال/قطع
// کلاینت‌ها بین صفحه QR1 (اتصال) و QR2 (باز کردن صفحه تنظیمات) سوییچ می‌کنیم.
void onWiFiAPEvent(WiFiEvent_t event) {
  if (animPlaying) return; // اگه قاب در حال پخشه، صفحه رو دست نزن

  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    drawOpenPageScreen();
  } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    if (WiFi.softAPgetStationNum() == 0) {
      drawConnectScreen();
    }
  }
}

void setupWiFiAP() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[5];
  sprintf(suffix, "%02X%02X", mac[4], mac[5]);
  apSSID = "TABLO" + String(suffix);

  WiFi.onEvent(onWiFiAPEvent);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str()); // بدون پسورد -> open network

  IPAddress apIP = WiFi.softAPIP(); // معمولاً 192.168.4.1

  drawConnectScreen();
  Serial.println("AP started: " + apSSID);
  Serial.println("IP: " + apIP.toString());
}

// ====================================================================
//                     ۴) پخش فایل GFA1 (لوپ دائم)
// ====================================================================
File animFile;
uint16_t animW = 0, animH = 0, animFrameCount = 0;
const uint32_t ANIM_DATA_START = 10;
uint16_t animFrameIndex = 0;
unsigned long animLastFrameTime = 0;
uint16_t animCurrentDelay = 0;
volatile bool newAnimAvailable = false;
volatile bool animPlaying = false;

bool openCurrentAnim() {
  if (animFile) animFile.close();
  if (!LittleFS.exists("/current.anim")) return false;

  animFile = LittleFS.open("/current.anim", "r");
  if (!animFile) return false;

  uint8_t header[10];
  if (animFile.read(header, 10) != 10) { animFile.close(); return false; }
  if (header[0] != 'G' || header[1] != 'F' || header[2] != 'A' || header[3] != '1') {
    Serial.println("Invalid anim file header");
    animFile.close();
    return false;
  }

  animW = header[4] | (header[5] << 8);
  animH = header[6] | (header[7] << 8);
  animFrameCount = header[8] | (header[9] << 8);

  if (animW != SCREEN_W || animH != SCREEN_H || animFrameCount == 0) {
    Serial.println("Anim size mismatch or empty");
    animFile.close();
    return false;
  }

  animFrameIndex = 0;
  animFile.seek(ANIM_DATA_START);
  tft.fillScreen(ST77XX_BLACK);
  animLastFrameTime = 0;
  animCurrentDelay = 0; // فریم اول فوری نمایش داده شود
  return true;
}

void playAnimTick() {
  if (!animPlaying || !animFile) return;
  if (millis() - animLastFrameTime < animCurrentDelay) return;

  uint8_t db[2];
  if (animFile.read(db, 2) != 2) {
    // پایان فایل -> برگرد به فریم اول (لوپ دائم)
    animFile.seek(ANIM_DATA_START);
    animFrameIndex = 0;
    if (animFile.read(db, 2) != 2) return; // فایل خراب
  }
  uint16_t delayMs = db[0] | (db[1] << 8);
  if (delayMs < 20) delayMs = 100;

  static uint16_t lineBuf[SCREEN_W];

  tft.startWrite();
  tft.setAddrWindow(X_OFFSET, Y_OFFSET, animW, animH);
  for (uint16_t row = 0; row < animH; row++) {
    size_t need = (size_t)animW * 2;
    size_t got = animFile.read((uint8_t*)lineBuf, need);
    if (got != need) {
      memset(lineBuf, 0, sizeof(lineBuf));
    }
    tft.writePixels(lineBuf, animW, false, true); // bigEndian=true: بایت‌ها همان‌طور که نوشته شده‌اند ارسال می‌شوند
  }
  tft.endWrite();

  animFrameIndex++;
  if (animFrameIndex >= animFrameCount) {
    animFrameIndex = 0;
    animFile.seek(ANIM_DATA_START);
  }

  animLastFrameTime = millis();
  animCurrentDelay = delayMs;
}

// ====================================================================
//                     ۵) صفحه وب (HTML/CSS/JS کاملاً آفلاین)
// ====================================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
<title>GifFrame</title>
<link rel="manifest" href="/manifest.json">
<link rel="apple-touch-icon" href="/icon.png">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="theme-color" content="#111111">
<style>
  * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
  body {
    margin:0; font-family: -apple-system, 'Segoe UI', Tahoma, sans-serif;
    background: linear-gradient(180deg,#1a1a2e,#0f0f1a); color:#fff; min-height:100vh;
  }
  header {
    padding: 18px 16px; text-align:center; font-size:20px; font-weight:700;
    background: rgba(255,255,255,0.05); backdrop-filter: blur(10px);
    position: sticky; top:0; z-index:10;
  }
  .container { padding: 16px; max-width: 600px; margin: 0 auto; }
  .upload-box {
    border: 2px dashed #555; border-radius: 16px; padding: 30px 16px;
    text-align: center; margin-bottom: 20px; transition: 0.2s;
    background: rgba(255,255,255,0.03);
  }
  .upload-box.dragover { border-color:#6c5ce7; background: rgba(108,92,231,0.1); }
  .upload-box input { display:none; }
  .upload-box label {
    display:inline-block; padding: 12px 24px; border-radius: 12px;
    background: linear-gradient(135deg,#6c5ce7,#a29bfe); color:#fff;
    font-weight:600; cursor:pointer; margin-top:10px;
  }
  .progress-wrap {
    width:100%; height:10px; background:rgba(255,255,255,0.08);
    border-radius:6px; overflow:hidden; margin-top:14px; display:none;
  }
  .progress-bar {
    height:100%; width:0%; background:linear-gradient(135deg,#6c5ce7,#a29bfe);
    transition: width 0.15s;
  }
  .status {
    text-align:center; padding:10px; border-radius:10px; margin-bottom:14px;
    background: rgba(0,255,150,0.1); color:#7fffd4; font-size:14px; display:none;
  }
  .status.show { display:block; }
  .info-banner {
    background: rgba(108,92,231,0.12); border: 1px solid rgba(108,92,231,0.35);
    border-radius: 12px; padding: 12px 14px; margin-bottom: 14px;
    font-size: 13px; line-height: 1.7; text-align: left; direction: ltr;
    position: relative;
  }
  .info-banner b { color: #c2b8ff; }
  .info-banner .close-btn {
    position: absolute; top: 6px; right: 8px; background: none; border: none;
    color: #aaa; font-size: 16px; cursor: pointer; line-height: 1;
  }
  .preview { margin-top:14px; text-align:center; }
  .preview canvas {
    max-width:100%; border-radius:10px; image-rendering: pixelated;
    background:#000; border:1px solid rgba(255,255,255,0.1);
  }
</style>
</head>
<body>
<header>📺 GifFrame</header>
<div class="container">

  <div class="info-banner" id="infoBanner">
    <button class="close-btn" onclick="document.getElementById('infoBanner').style.display='none'">✕</button>
    📌 <b>Tip:</b> Add this page to your Home Screen, or save this address for next time:<br>
    <b>http://192.168.4.1</b>
  </div>

  <div class="status" id="status"></div>

  <div class="upload-box" id="dropZone">
    <div>یک عکس یا GIF بکش اینجا یا انتخاب کن</div>
    <label for="fileInput">انتخاب از گالری</label>
    <input type="file" id="fileInput" accept="image/*">
    <div class="progress-wrap" id="progressWrap">
      <div class="progress-bar" id="progressBar"></div>
    </div>
    <div class="preview" id="preview"></div>
  </div>

</div>

<script>
// ابعاد نمایشگر (Landscape)
const SCREEN_W = 160;
const SCREEN_H = 128;

// بِزِل فیزیکی پنل کمی از ارتفاع بالا/پایین رو می‌پوشونه. برای جلوگیری از
// کراپ‌شدن محتوای مهم تصویر، کاور رو بر اساس یک "ارتفاع ایمن" کوچک‌تر
// محاسبه می‌کنیم (با حاشیه‌ی سیاه ناچیز که زیر بزل پنهان می‌شه).
// اگه هنوز کمی بالا/پایین کراپ بود، این عدد رو کمتر کن (مثلاً 112).
// اگه حاشیه سیاه قابل‌توجه دیدی، بیشترش کن (مثلاً 124).
const VISIBLE_H = 118;

const MAX_FRAMES = 30; // حداکثر فریم خروجی برای جلوگیری از حجم زیاد

function showStatus(msg, ok=true) {
  const s = document.getElementById('status');
  s.textContent = msg;
  s.style.background = ok ? 'rgba(0,255,150,0.1)' : 'rgba(255,80,80,0.15)';
  s.style.color = ok ? '#7fffd4' : '#ff7f7f';
  s.classList.add('show');
}

function setProgress(pct) {
  const wrap = document.getElementById('progressWrap');
  const bar = document.getElementById('progressBar');
  if (pct === null) { wrap.style.display = 'none'; return; }
  wrap.style.display = 'block';
  bar.style.width = pct + '%';
}

// محاسبه سایز "cover": عرض کامل صفحه + ارتفاع "ایمن" (VISIBLE_H) رو پر می‌کند
function fitSize(srcW, srcH) {
  const scale = Math.max(SCREEN_W / srcW, VISIBLE_H / srcH);
  const w = Math.round(srcW * scale);
  const h = Math.round(srcH * scale);
  const x = Math.round((SCREEN_W - w) / 2); // معمولاً <= 0
  const y = Math.round((SCREEN_H - h) / 2); // ممکنه حاشیه کوچک ایجاد کند
  return { w, h, x, y };
}

// ============================================================
// ====== رمزگشای GIF خالص جاوااسکریپت (بدون کتابخونه خارجی) ======
// ============================================================
function parseGifFile(buffer) {
  const data = new Uint8Array(buffer);
  let p = 0;
  function readByte(){ return data[p++]; }
  function readU16(){ const v = data[p] | (data[p+1]<<8); p+=2; return v; }

  if (!(data[0]===0x47 && data[1]===0x49 && data[2]===0x46)) {
    throw new Error('فایل GIF معتبر نیست');
  }
  p = 6;
  const screenW = readU16();
  const screenH = readU16();
  const packed = readByte();
  const gctFlag = (packed & 0x80) !== 0;
  const gctSize = 2 << (packed & 0x07);
  readByte(); // background color index
  readByte(); // aspect ratio
  let gct = null;
  if (gctFlag) {
    gct = [];
    for (let i=0;i<gctSize;i++) gct.push([data[p++],data[p++],data[p++]]);
  }

  const frames = [];
  let gceDelay = 100, gceTransparent = -1, gceDisposal = 0;

  while (p < data.length) {
    const block = readByte();
    if (block === 0x21) { // Extension
      const label = readByte();
      if (label === 0xF9) { // Graphic Control Extension
        readByte(); // block size = 4
        const packed2 = readByte();
        gceDisposal = (packed2 >> 2) & 0x07;
        const hasTransparency = (packed2 & 0x01) !== 0;
        gceDelay = readU16() * 10;
        if (gceDelay === 0) gceDelay = 100;
        const transIdx = readByte();
        gceTransparent = hasTransparency ? transIdx : -1;
        readByte(); // terminator
      } else {
        let size;
        while ((size = readByte()) !== 0) p += size;
      }
    } else if (block === 0x2C) { // Image Descriptor
      const left = readU16();
      const top = readU16();
      const width = readU16();
      const height = readU16();
      const packed3 = readByte();
      const lctFlag = (packed3 & 0x80) !== 0;
      const interlace = (packed3 & 0x40) !== 0;
      const lctSize = 2 << (packed3 & 0x07);
      let lct = null;
      if (lctFlag) {
        lct = [];
        for (let i=0;i<lctSize;i++) lct.push([data[p++],data[p++],data[p++]]);
      }
      const minCodeSize = readByte();
      const bytes = [];
      let size;
      while ((size = readByte()) !== 0) {
        for (let i=0;i<size;i++) bytes.push(data[p++]);
      }
      let indices = lzwDecode(bytes, minCodeSize, width*height);
      if (interlace) indices = deinterlace(indices, width, height);

      frames.push({
        left, top, width, height,
        colorTable: lct || gct || [[0,0,0]],
        transparent: gceTransparent,
        disposal: gceDisposal,
        delay: gceDelay,
        indices
      });
      gceDelay = 100; gceTransparent = -1; gceDisposal = 0;
    } else if (block === 0x3B) {
      break; // Trailer
    } else {
      break; // ناشناخته - برای جلوگیری از حلقه بی‌پایان متوقف شو
    }
  }
  return { width: screenW, height: screenH, frames };
}

function lzwDecode(bytes, minCodeSize, pixelCount) {
  const clearCode = 1 << minCodeSize;
  const eoiCode = clearCode + 1;
  let codeSize, dict;

  function resetDict() {
    dict = [];
    for (let i=0;i<clearCode;i++) dict[i] = [i];
    dict[clearCode] = [];
    dict[eoiCode] = [];
    codeSize = minCodeSize + 1;
  }
  resetDict();

  const output = new Uint8Array(pixelCount);
  let outPos = 0;
  let bitPos = 0;
  const totalBits = bytes.length * 8;

  function readCode() {
    let code = 0;
    for (let i=0;i<codeSize;i++) {
      if (bitPos >= totalBits) return eoiCode;
      const byteIndex = bitPos >> 3;
      const bitIndex = bitPos & 7;
      if ((bytes[byteIndex] >> bitIndex) & 1) code |= (1 << i);
      bitPos++;
    }
    return code;
  }

  let prevCode = -1;
  while (outPos < pixelCount) {
    const code = readCode();
    if (code === clearCode) { resetDict(); prevCode = -1; continue; }
    if (code === eoiCode) break;

    let entry;
    if (dict[code] !== undefined) {
      entry = dict[code];
    } else if (code === dict.length && prevCode !== -1) {
      entry = dict[prevCode].concat([dict[prevCode][0]]);
    } else {
      break; // داده خراب
    }

    for (let i=0;i<entry.length && outPos<pixelCount;i++) output[outPos++] = entry[i];

    if (prevCode !== -1 && dict.length < 4096) {
      dict[dict.length] = dict[prevCode].concat([entry[0]]);
      if (dict.length === (1<<codeSize) && codeSize < 12) codeSize++;
    }
    prevCode = code;
  }
  return output;
}

function deinterlace(indices, width, height) {
  const out = new Uint8Array(width*height);
  const passes = [ {start:0,step:8}, {start:4,step:8}, {start:2,step:4}, {start:1,step:2} ];
  let srcRow = 0;
  for (const pass of passes) {
    for (let y = pass.start; y < height; y += pass.step) {
      for (let x=0;x<width;x++) out[y*width+x] = indices[srcRow*width+x];
      srcRow++;
    }
  }
  return out;
}

// ============================================================
// ====== ساخت فایل باینری GFA1 ======
// ============================================================
function buildAnimBuffer(framesData, w, h, delays) {
  const frameCount = framesData.length;
  const frameSize = w*h*2;
  const totalSize = 10 + frameCount*(2+frameSize);
  const buf = new Uint8Array(totalSize);
  let off=0;
  buf[off++]=0x47; buf[off++]=0x46; buf[off++]=0x41; buf[off++]=0x31; // 'GFA1'
  buf[off++]=w & 0xFF; buf[off++]=(w>>8)&0xFF;
  buf[off++]=h & 0xFF; buf[off++]=(h>>8)&0xFF;
  buf[off++]=frameCount & 0xFF; buf[off++]=(frameCount>>8)&0xFF;

  for (let f=0; f<frameCount; f++) {
    const delay = Math.min(65000, Math.max(20, delays[f]));
    buf[off++]=delay & 0xFF; buf[off++]=(delay>>8)&0xFF;
    const data = framesData[f];
    for (let px=0; px<w*h; px++) {
      const r=data[px*4], g=data[px*4+1], b=data[px*4+2];
      const c = ((r&0xF8)<<8) | ((g&0xFC)<<3) | (b>>3);
      buf[off++] = (c>>8)&0xFF; // بایت بالا اول (wire order برای ST7735)
      buf[off++] = c & 0xFF;
    }
  }
  return new Blob([buf], {type:'application/octet-stream'});
}

// ============================================================
// ====== ریسایز عکس ساده (jpg/png/webp/bmp/...) -> تک‌فریم letterbox ======
// ============================================================
function resizeStaticImage(file) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => {
      const { w, h, x, y } = fitSize(img.naturalWidth, img.naturalHeight);
      const canvas = document.createElement('canvas');
      canvas.width = SCREEN_W; canvas.height = SCREEN_H;
      const ctx = canvas.getContext('2d');
      ctx.fillStyle = '#000000';
      ctx.fillRect(0, 0, SCREEN_W, SCREEN_H);
      ctx.drawImage(img, x, y, w, h);
      const data = ctx.getImageData(0, 0, SCREEN_W, SCREEN_H).data;
      showPreviewCanvas(canvas);
      resolve(buildAnimBuffer([data], SCREEN_W, SCREEN_H, [1000]));
    };
    img.onerror = () => reject(new Error('فایل عکس قابل خواندن نیست'));
    img.src = URL.createObjectURL(file);
  });
}

// ============================================================
// ====== ریسایز گیف متحرک -> فریم‌های letterbox به ابعاد صفحه ======
// ============================================================
async function resizeAnimatedGif(file) {
  const buffer = await file.arrayBuffer();
  const gifData = parseGifFile(buffer);
  if (!gifData.frames.length) throw new Error('گیف خوانده نشد یا خالی است');

  const srcW = gifData.width, srcH = gifData.height;
  const { w, h, x, y } = fitSize(srcW, srcH);

  const totalFrames = gifData.frames.length;
  const step = Math.max(1, Math.ceil(totalFrames / MAX_FRAMES));

  const fullCanvas = document.createElement('canvas');
  fullCanvas.width = srcW; fullCanvas.height = srcH;
  const fctx = fullCanvas.getContext('2d', { willReadFrequently: true });
  fctx.fillStyle = '#000000';
  fctx.fillRect(0,0,srcW,srcH);

  const outCanvas = document.createElement('canvas');
  outCanvas.width = SCREEN_W; outCanvas.height = SCREEN_H;
  const octx = outCanvas.getContext('2d', { willReadFrequently: true });

  const framesData = [];
  const delays = [];

  for (let i=0;i<totalFrames;i++) {
    const frame = gifData.frames[i];

    let fImageData;
    try {
      fImageData = fctx.getImageData(frame.left, frame.top, frame.width, frame.height);
    } catch (e) {
      fImageData = new ImageData(frame.width, frame.height);
    }
    const ct = frame.colorTable;
    for (let px=0; px<frame.width*frame.height; px++) {
      const idx = frame.indices[px];
      if (idx === frame.transparent) continue;
      const c = ct[idx] || [0,0,0];
      fImageData.data[px*4]   = c[0];
      fImageData.data[px*4+1] = c[1];
      fImageData.data[px*4+2] = c[2];
      fImageData.data[px*4+3] = 255;
    }
    fctx.putImageData(fImageData, frame.left, frame.top);

    if (i % step === 0) {
      octx.fillStyle = '#000000';
      octx.fillRect(0,0,SCREEN_W,SCREEN_H);
      octx.drawImage(fullCanvas, x, y, w, h);
      const outData = octx.getImageData(0,0,SCREEN_W,SCREEN_H).data;
      framesData.push(outData);

      let accDelay = frame.delay;
      for (let k=1;k<step && (i+k)<totalFrames;k++) accDelay += gifData.frames[i+k].delay;
      delays.push(accDelay);

      if (i === 0) showPreviewCanvas(outCanvas);
    }

    if (frame.disposal === 2) {
      fctx.fillStyle = '#000000';
      fctx.fillRect(frame.left, frame.top, frame.width, frame.height);
    }

    // هر چند فریم یک‌بار به مرورگر فرصت نفس کشیدن بده تا قفل نشه
    if (i % 5 === 0) {
      showStatus(`در حال پردازش فریم ${i+1} از ${totalFrames}...`);
      await new Promise(r => setTimeout(r, 0));
    }
  }

  if (!framesData.length) throw new Error('هیچ فریمی پردازش نشد');
  return buildAnimBuffer(framesData, SCREEN_W, SCREEN_H, delays);
}

function showPreviewCanvas(srcCanvas) {
  const preview = document.getElementById('preview');
  preview.innerHTML = '';
  const c = document.createElement('canvas');
  c.width = SCREEN_W; c.height = SCREEN_H;
  c.getContext('2d').drawImage(srcCanvas, 0, 0);
  preview.appendChild(c);
}

// ============================================================
// ====== مدیریت آپلود فایل ======
// ============================================================
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');

fileInput.addEventListener('change', e => {
  if (e.target.files.length) handleFile(e.target.files[0]);
});
dropZone.addEventListener('dragover', e => { e.preventDefault(); dropZone.classList.add('dragover'); });
dropZone.addEventListener('dragleave', () => dropZone.classList.remove('dragover'));
dropZone.addEventListener('drop', e => {
  e.preventDefault(); dropZone.classList.remove('dragover');
  if (e.dataTransfer.files.length) handleFile(e.dataTransfer.files[0]);
});

async function handleFile(file) {
  if (!file.type.startsWith('image/')) {
    showStatus('فقط فایل عکس یا GIF قابل قبول است!', false);
    return;
  }
  setProgress(0);
  document.getElementById('preview').innerHTML = '';
  showStatus('در حال پردازش و ریسایز...');

  try {
    let outBlob;
    if (file.type === 'image/gif') {
      try {
        outBlob = await resizeAnimatedGif(file);
      } catch (gifErr) {
        console.warn('GIF decode failed, fallback to static image:', gifErr);
        showStatus('گیف پشتیبانی‌نشده - به عکس ساده تبدیل می‌شود...');
        outBlob = await resizeStaticImage(file);
      }
    } else {
      outBlob = await resizeStaticImage(file);
    }
    uploadBlob(outBlob);
  } catch (err) {
    console.error(err);
    showStatus('خطا در پردازش فایل: ' + err.message, false);
    setProgress(null);
  }
}

// ====== ارسال به ESP32 با Progress Bar ======
function uploadBlob(blob) {
  showStatus('در حال ارسال به دستگاه... (' + Math.round(blob.size/1024) + ' KB)');
  const formData = new FormData();
  formData.append('anim', blob, 'current.anim');

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/upload', true);

  xhr.upload.onprogress = e => {
    if (e.lengthComputable) {
      const pct = Math.round((e.loaded / e.total) * 100);
      setProgress(pct);
    }
  };
  xhr.onload = () => {
    setProgress(100);
    if (xhr.status >= 200 && xhr.status < 300) {
      showStatus('با موفقیت روی دستگاه نمایش داده می‌شود ✅');
    } else if (xhr.status === 413) {
      showStatus('فایل برای حافظه دستگاه بزرگ است، فایل کوچک‌تری انتخاب کنید', false);
    } else {
      showStatus('خطا در آپلود (کد ' + xhr.status + ')', false);
    }
    setTimeout(() => setProgress(null), 1500);
  };
  xhr.onerror = () => {
    showStatus('خطا در ارتباط با دستگاه!', false);
    setProgress(null);
  };
  xhr.send(formData);
}
</script>
</body>
</html>
)rawliteral";

const char manifest_json[] PROGMEM = R"rawliteral(
{
  "name": "GifFrame",
  "short_name": "GifFrame",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#0f0f1a",
  "theme_color": "#111111",
  "icons": [
    { "src": "/icon.png", "sizes": "192x192", "type": "image/png" },
    { "src": "/icon.png", "sizes": "512x512", "type": "image/png" }
  ]
}
)rawliteral";

// ====================================================================
//                     ۶) Web Server Routes
// ====================================================================
File uploadFile;
bool uploadTooBig = false;

void setupWebServer() {
  // صفحه اصلی
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // PWA manifest
  server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "application/json", manifest_json);
  });

  // آیکون اپ - باید فایل icon.png را در LittleFS آپلود کنید (مرحله جداگانه)
  server.on("/icon.png", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (LittleFS.exists("/icon.png")) {
      request->send(LittleFS, "/icon.png", "image/png");
    } else {
      request->send(404, "text/plain", "icon not found");
    }
  });

  // آپلود فایل GFA1 (تصویر/گیف پردازش‌شده توسط مرورگر)
  server.on("/upload", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (uploadTooBig) {
        request->send(413, "text/plain", "Not enough space on device");
      } else {
        request->send(200, "text/plain", "OK");
      }
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (index == 0) {
        uploadTooBig = false;
        animPlaying = false;

        // بستن فایل قبلی قبل از حذف تا LittleFS فضا را کامل آزاد کند
        if (animFile) animFile.close();
        if (LittleFS.exists("/current.anim")) LittleFS.remove("/current.anim");

        uploadFile = LittleFS.open("/current.anim", "w");
        if (!uploadFile) {
          uploadTooBig = true;
          Serial.println("Failed to open file for writing");
        }
      }

      if (!uploadTooBig && uploadFile) {
        size_t written = uploadFile.write(data, len);
        if (written != len) {
          // دیسک پر شد در حین نوشتن
          uploadTooBig = true;
          uploadFile.close();
          LittleFS.remove("/current.anim");
          Serial.println("Disk full during write!");
        }
      }

      if (final) {
        if (uploadFile) uploadFile.close();
        if (!uploadTooBig) {
          newAnimAvailable = true;
          Serial.printf("Upload OK - LittleFS: %u / %u bytes used\n", LittleFS.usedBytes(), LittleFS.totalBytes());
        }
      }
    }
  );

  // درخواست‌های ناشناخته -> صفحه اصلی (کاربر URL را با QR2 یا دستی باز می‌کند)
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });

  server.begin();
}

// ====================================================================
//                              SETUP
// ====================================================================
void setup() {
  Serial.begin(115200);

  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH); // روشن کردن بک‌لایت

  tft.initR(INITR_BLACKTAB); // بسته به ماژول ممکن است INITR_GREENTAB یا 144GREENTAB لازم باشد
  tft.setOffset(COL_START, ROW_START); // اصلاح offset پنل - باید قبل از setRotation باشد
  tft.setRotation(1); // Landscape - اگر معکوس بود به 3 تغییر بده
  tft.fillScreen(ST77XX_BLACK);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }
  Serial.printf("LittleFS: %u / %u bytes used\n", LittleFS.usedBytes(), LittleFS.totalBytes());

  // بررسی فکتوری ریست (5 بار روشن/خاموش طی 10 ثانیه)
  checkFactoryReset();

  // راه‌اندازی AP + QR + Captive Portal
  setupWiFiAP();

  // وب سرور
  setupWebServer();

  // اگر از قبل فایلی ذخیره شده، آن را پخش کن
  if (LittleFS.exists("/current.anim")) {
    newAnimAvailable = true;
  }
}

// ====================================================================
//                              LOOP
// ====================================================================
void loop() {
  resetBootCounterAfterTimeout();

  if (newAnimAvailable) {
    newAnimAvailable = false;
    if (openCurrentAnim()) {
      animPlaying = true;
    }
  }

  playAnimTick();

  delay(1);
}
