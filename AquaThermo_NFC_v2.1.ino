#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PN532.h>
#include <PN532_I2C.h>

// OLED屏幕设置
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// 温度传感器设置
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// NFC设置
PN532_I2C pn532_i2c(Wire);
PN532 nfc(pn532_i2c);

// 引脚定义
const int heaterPin = 3;
const int buttonTooHot = 4;
const int buttonTooCold = 5;
const int buttonJustRight = 6;

// 变量
float currentTemp = 0;
float targetTemp = 37.0; // 默认目标温度
bool heaterState = false;
unsigned long lastTempRead = 0;
const unsigned long tempReadInterval = 1000;

// 用户温度偏好结构
struct UserPreference {
  uint32_t uid;
  float preferredTemp;
};

UserPreference userPrefs[10]; // 最多存储10个用户
int userCount = 0;

void setup() {
  Serial.begin(9600);
  
  // 初始化OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306分配失败"));
    for(;;);
  }
  
  // 初始化温度传感器
  sensors.begin();
  
  // 初始化NFC
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("未找到PN53x板卡");
    while (1);
  }
  nfc.SAMConfig();
  
  // 设置引脚模式
  pinMode(heaterPin, OUTPUT);
  pinMode(buttonTooHot, INPUT_PULLUP);
  pinMode(buttonTooCold, INPUT_PULLUP);
  pinMode(buttonJustRight, INPUT_PULLUP);
  
  // 初始显示
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("智能恒温系统");
  display.println("等待NFC卡片...");
  display.display();
  
  Serial.println("系统启动完成");
}

void loop() {
  // 读取温度
  if (millis() - lastTempRead > tempReadInterval) {
    readTemperature();
    controlHeater();
    updateDisplay();
    lastTempRead = millis();
  }
  
  // 检查按钮
  checkButtons();
  
  // 检查NFC
  checkNFC();
  
  delay(100);
}

void readTemperature() {
  sensors.requestTemperatures();
  currentTemp = sensors.getTempCByIndex(0);
}

void controlHeater() {
  if (currentTemp < targetTemp - 0.5) {
    digitalWrite(heaterPin, HIGH);
    heaterState = true;
  } else if (currentTemp > targetTemp + 0.5) {
    digitalWrite(heaterPin, LOW);
    heaterState = false;
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0,0);
  
  display.print("当前温度: ");
  display.print(currentTemp);
  display.println(" C");
  
  display.print("目标温度: ");
  display.print(targetTemp);
  display.println(" C");
  
  display.print("加热器: ");
  display.println(heaterState ? "开启" : "关闭");
  
  display.print("用户数: ");
  display.println(userCount);
  
  display.println("A:太热 B:太冷 C:合适");
  
  display.display();
}

void checkButtons() {
  // 太热按钮
  if (digitalRead(buttonTooHot) == LOW) {
    delay(50); // 防抖
    if (digitalRead(buttonTooHot) == LOW) {
      adjustTemperature(-1); // 降温
      while(digitalRead(buttonTooHot) == LOW); // 等待释放
    }
  }
  
  // 太冷按钮
  if (digitalRead(buttonTooCold) == LOW) {
    delay(50);
    if (digitalRead(buttonTooCold) == LOW) {
      adjustTemperature(1); // 升温
      while(digitalRead(buttonTooCold) == LOW);
    }
  }
  
  // 刚刚好按钮
  if (digitalRead(buttonJustRight) == LOW) {
    delay(50);
    if (digitalRead(buttonJustRight) == LOW) {
      saveCurrentTemperature();
      while(digitalRead(buttonJustRight) == LOW);
    }
  }
}

void adjustTemperature(int direction) {
  // 生成-5到+5度的随机调整（不超过±10度限制）
  float adjustment = random(1, 6) * direction;
  float newTemp = targetTemp + adjustment;
  
  // 确保在合理范围内
  if (newTemp >= 20 && newTemp <= 50) {
    targetTemp = newTemp;
    
    // 暂时关闭加热器
    digitalWrite(heaterPin, LOW);
    delay(2000);
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("调整温度中...");
    display.print("新目标: ");
    display.print(targetTemp);
    display.println(" C");
    display.display();
    
    delay(2000);
  }
}

void saveCurrentTemperature() {
  // 在实际应用中，这里应该与NFC卡片关联
  // 现在我们先保存到内存中
  if (userCount < 10) {
    userPrefs[userCount].uid = userCount + 1; // 模拟UID
    userPrefs[userCount].preferredTemp = targetTemp;
    userCount++;
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("温度偏好已保存!");
    display.print("用户ID: ");
    display.println(userCount);
    display.print("偏好温度: ");
    display.print(targetTemp);
    display.println(" C");
    display.display();
    
    delay(2000);
  }
}

void checkNFC() {
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;
  
  // 检查NFC标签
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  if (success) {
    uint32_t cardUID = 0;
    for (uint8_t i = 0; i < 4; i++) {
      cardUID <<= 8;
      cardUID |= uid[i];
    }
    
    // 查找用户偏好
    float userTemp = findUserPreference(cardUID);
    
    if (userTemp > 0) {
      // 老用户：使用保存的温度偏好
      targetTemp = userTemp;
      
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("NFC卡片已识别!");
      display.print("用户ID: ");
      display.println(cardUID, HEX);
      display.print("设定温度: ");
      display.print(targetTemp);
      display.println(" C");
      display.display();
      
      delay(3000);
    } else {
      // 新用户：先升温至40度，然后等待用户指令
      targetTemp = 40.0; // 设置目标温度为40度
      
      display.clearDisplay();
      display.setCursor(0,0);
      display.println("新用户识别!");
      display.println("正在升温至40度...");
      display.println("请使用按钮调整");
      display.println("然后按'合适'保存");
      display.display();
      
      // 不立即保存用户偏好，等待用户确认
      // 用户可以通过按钮调整温度，然后按"刚刚好"按钮保存偏好
      
      delay(3000);
    }
    
    // 等待卡片移开
    while(nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
      delay(500);
    }
  }
}

float findUserPreference(uint32_t uid) {
  for (int i = 0; i < userCount; i++) {
    if (userPrefs[i].uid == uid) {
      return userPrefs[i].preferredTemp;
    }
  }
  return -1; // 未找到
}
