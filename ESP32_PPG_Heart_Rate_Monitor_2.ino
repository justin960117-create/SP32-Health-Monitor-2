#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

MAX30105 particleSensor;
BLECharacteristic *pCharacteristic;

// --- 記憶體與變數宣告 ---
#define BUFFER_SIZE 100
static uint32_t irBuffer[BUFFER_SIZE]; 
static uint32_t redBuffer[BUFFER_SIZE];

int32_t spo2 = 0;            
int8_t validSPO2 = 0;        
int32_t heartRate = 0;       
int8_t validHR = 0;

float smoothBPM = 0;   
float smoothSpO2 = 0;  
bool firstRun = true;  

void setup() {
  Serial.begin(115200);
  
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("找不到 MAX30102！");
    while (1);
  }

  // 1. 初始化預填緩衝區，防止演算法讀取到 0 導致崩潰
  for (int i = 0; i < BUFFER_SIZE; i++) {
    irBuffer[i] = 60000; 
    redBuffer[i] = 60000;
  }
  
  /** 
   * 2. 硬體參數優化 (解決 150 BPM 的關鍵)
   * 亮度: 60, 平均採樣: 8 (大幅減少高頻雜訊), 模式: 2 (Red+IR), 
   * 採樣率: 100Hz, 脈衝寬度: 411, ADC範圍: 4096 
   */
  particleSensor.setup(60, 8, 2, 100, 411, 4096); 
  
  // 3. 藍牙初始化 (TZUCHI 專案專屬 UUID)
  BLEDevice::init("ESP32_HEALTH_MONITOR");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
  pCharacteristic = pService->createCharacteristic(
                      "beb5483e-36e1-4688-b7f5-ea07361b26a8",
                      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  pServer->getAdvertising()->start();
  
  Serial.println("系統就緒，請「輕輕」覆蓋手指...");
}

void loop() {
  // --- A. 滑動窗口更新數據 ---
  for (int i = 1; i < BUFFER_SIZE; i++) {
    redBuffer[i - 1] = redBuffer[i];
    irBuffer[i - 1] = irBuffer[i];
  }
  
  while (!particleSensor.available()) particleSensor.check();
  redBuffer[BUFFER_SIZE - 1] = particleSensor.getRed();
  irBuffer[BUFFER_SIZE - 1] = particleSensor.getIR();
  particleSensor.nextSample();

  // --- B. 偵測手指與執行演算法 ---
  if (irBuffer[BUFFER_SIZE - 1] > 50000) {
    
    static int calcCounter = 0;
    if (calcCounter++ > 30) { // 每 30 筆新樣本算一次，約 0.3 秒更新一次底層數值
      maxim_heart_rate_and_oxygen_saturation(irBuffer, BUFFER_SIZE, redBuffer, &spo2, &validSPO2, &heartRate, &validHR);
      calcCounter = 0;
    }

    // --- C. 異常值剔除與平滑過濾 (核心邏輯) ---
    // 排除過高(150+)或過低的心率雜訊
    if (validHR && heartRate >= 45 && heartRate <= 135) { 
      if (firstRun) {
        smoothBPM = heartRate;
        smoothSpO2 = (validSPO2 && spo2 > 90) ? spo2 : 98;
        firstRun = false;
      } else {
        // 使用極高比例的舊值權重 (0.95)，讓數值極端穩定
        smoothBPM = (heartRate * 0.05) + (smoothBPM * 0.95);
        
        if (validSPO2 && spo2 >= 90 && spo2 <= 100) {
          smoothSpO2 = (spo2 * 0.02) + (smoothSpO2 * 0.98);
        }
      }
    }

    // --- D. 定時藍牙通知與串口輸出 ---
    static long lastNotify = 0;
    if (millis() - lastNotify > 1000) { // 每秒發送一次最終結果
      int finalBPM = (int)(smoothBPM + 0.5);
      int finalSpO2 = (int)(smoothSpO2 + 0.5);
      
      // 構建顯示字串
      String bpmText = (finalBPM > 40) ? String(finalBPM) : "--";
      String spo2Text = (finalSpO2 > 80) ? String(finalSpO2) : "--";
      
      String output = "BPM:" + bpmText + " SpO2:" + spo2Text + "%";
      Serial.println(output);
      
      pCharacteristic->setValue(output.c_str());
      pCharacteristic->notify();
      lastNotify = millis();
    }
  } else {
    // 手指離開時重置過濾器
    firstRun = true;
    if (millis() % 3000 == 0) Serial.println("等待手指穩定放置...");
  }
}