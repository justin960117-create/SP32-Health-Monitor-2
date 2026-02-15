# SP32-Health-Monitor-2
使用 ESP32 與 MAX30102 的藍牙心律血氧監測器
markdown
# ESP32 藍牙心律血氧監測器 (Vital Sense Monitor)

這是一個利用 ESP32 與 MAX30102 感測器的穿戴式健康監測專案。透過 BLE (Bluetooth Low Energy) 將即時數據傳輸至手機 App。

## 主要功能
- **即時監測**：測量心率 (BPM) 與血氧飽和度 (SpO2)。
- **訊號穩定優化**：內建滑動窗口 (Sliding Window) 與權重移動平均濾波 (EMA)，有效解決數值跳動問題。
- **硬體抗噪**：啟動 8 倍硬體採樣平均，解決常見的 150 BPM 倍頻錯誤。
- **藍牙連接**：使用 BLE 協議，低耗電且相容大多數手機測試軟體（如 nRF Connect）。

## 硬體接線 (ESP32)
| MAX30102 Pin | ESP32 GPIO |
| :--- | :--- |
| VIN | 3.3V  |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

## 依賴程式庫
- [SparkFun MAX3010x Pulse and Proximity Sensor Library](https://github.com)
- 內建 ESP32 BLE Libraries

## 注意事項
- 測量時手指請「輕輕」覆蓋感測器，過度按壓會影響血液流動導致數據不準。
- 數值僅供實驗參考，不可作為醫療診斷用途。
