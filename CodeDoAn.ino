// Khai báo các thông tin cấu hình Blynk
#define BLYNK_TEMPLATE_ID "TMPL6Mo5yu5Tc"                   // Acc số 2
#define BLYNK_TEMPLATE_NAME "DA1"
#define BLYNK_AUTH_TOKEN "OdtkQmi-9SbVU7paNmk5yQbboS-tMvLq"

// Khai báo thư viện
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Adafruit_SSD1306.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "ng.hoangphat";
char pass[] = "1234567890";

// Định nghĩa chân và hằng số
#define TANK_LED 14      // Led bồn
#define WIFI_LED 2       // Led wifi
#define PUMP_LED 17      // Led của việc bơm thủ công
#define TRIG_PIN 27      // Chân Trig của HY-SRF05
#define ECHO_PIN 25      // Chân Echo của HY-SRF05
#define PUMP_PIN 15      // Chân In của Relay    
#define BUTTON_PIN 16    // Nút nhấn máy bơm thủ công

// Khai báo thông số hiển thị OLED
#define SCREEN_WIDTH 128      // Kích thước màn hình OLED: 128x64 pixel
#define SCREEN_HEIGHT 64
#define OLED_RESET -1         // Không dùng chân reset riêng cho màn hình

// Khai báo ngưỡng của mực nước
#define emptyTankDistance 24  // Khoảng cách từ cảm biến đến mặt nước khi bồn rỗng (mực nước thấp nhất)
#define fullTankDistance 5    // Khoảng cách từ cảm biến đến mặt nước khi bồn đầy  (mực nước cao nhất)                                    
#define TANK_AREA 240         // Diện tích đáy bồn

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Biến toàn cục
float duration, distance, waterVolume, waterHeight;   // Thời gian, khoảng cách, thể tích, mực nước của tín hiệu siêu âm
int waterLevelPer;                                    // % mực nước trong bồn
bool pumpState = false;                               // Biến trạng thái máy bơm
bool ledState = false;                                // Biến trạng thái led báo hiệu mực nước
bool manualPumpControl = false;                       // Biến điều khiển bơm thủ công
bool previousWiFiStatus = false;                      // Lưu trạng thái WiFi ở lần kiểm tra trước (false = chưa kết nối, true = đã kết nối)

int timecho = 500;
unsigned long hientai = 0;
unsigned long thoigian;

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(WIFI_LED, OUTPUT);
  pinMode(TANK_LED, OUTPUT);
  pinMode(PUMP_LED, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);      // Dùng điện trở kéo lên nội bộ

  digitalWrite(WIFI_LED, LOW);
  digitalWrite(TANK_LED, LOW);
  digitalWrite(PUMP_LED, LOW);
  digitalWrite(PUMP_PIN, LOW);

  Blynk.begin(auth, ssid, pass);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("❌ Khởi động màn hình SSD1306 không thành công"));
    for(;;);
  }

  display.setTextSize(1);       // Kích thước chữ: 1 (nhỏ nhất)
  display.setTextColor(WHITE);  // Màu chữ: Trắng
  display.clearDisplay();       // Xóa toàn bộ nội dung đang hiển thị
}

void loop() {
  Blynk.run();
  checkWiFiConnection();  // Kiểm tra kết nối Wi-Fi
  buttonCheck();          // Hàm kiểm tra nút nhấn thủ công

  thoigian = millis();
  if(thoigian - hientai >= timecho) {
    hientai = millis();
    measureDistance();      // Hàm đo khoảng cách
  }

  handleDistance();

  // Gửi dữ liệu đến các chân ảo (Virtual Pins) trên ứng dụng Blynk
  Blynk.virtualWrite(V1, waterLevelPer);
  Blynk.virtualWrite(V2, String(waterHeight) + " cm");
  Blynk.virtualWrite(V3, String(waterVolume) + " L");
  Blynk.virtualWrite(V4, pumpState);
  Blynk.virtualWrite(V5, ledState);
  Blynk.virtualWrite(V6, waterHeight);
  Blynk.virtualWrite(V7, manualPumpControl);

  display.clearDisplay();   // Xóa toàn bộ nội dung trên màn hình OLED
  drawWaterLevelScreen();   // Hàm vẽ thông tin mới lên màn hình OLED
  display.display();        // Hiển thị nội dung đã vẽ
}

void checkWiFiConnection() {
  bool currentWiFiStatus = (WiFi.status() == WL_CONNECTED);

  if (currentWiFiStatus != previousWiFiStatus) {
    if (currentWiFiStatus) {
      digitalWrite(WIFI_LED, HIGH);
      Serial.println("✅ Đã kết nối WiFi!");
    } else {
      digitalWrite(WIFI_LED, LOW);
      Serial.println("❌ Mất kết nối WiFi!");
    }
    previousWiFiStatus = currentWiFiStatus;
  }
}

// Hàm xử lý sự kiện nút nhấn (bật/tắt máy bơm thủ công)
void buttonCheck() {
  int buttonState = digitalRead(BUTTON_PIN); // Đọc trạng thái nút

  if (buttonState == LOW) { // Nút được nhấn (vì INPUT_PULLUP → LOW khi nhấn)
    if (waterLevelPer > 10 && waterLevelPer < 100) {
      manualPumpControl = true;
      pumpState = !pumpState;
    }

  delay(10); // Tránh nhiễu nút nhấn
  }
}

void measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH);
  distance = ((duration / 2) * 0.343) / 10;  // Khoảng cách từ cảm biến đến vật cản
}

void handleDistance() {
  if (duration > 0 && distance >= fullTankDistance && distance <= emptyTankDistance) {
    waterLevelPer = map((int)distance, emptyTankDistance, fullTankDistance, 0, 100);
    waterLevelPer = constrain(waterLevelPer, 0, 100);
    waterHeight = constrain(emptyTankDistance - distance, 0, 19);
    waterVolume = constrain((TANK_AREA * waterHeight) / 1000.0, 0, 4.56);

    if (waterLevelPer < 10 || waterLevelPer == 100) {
      digitalWrite(TANK_LED, HIGH);
      ledState = true;
    }
    else {
      digitalWrite(TANK_LED, LOW);
      ledState = false;
    }

    if (!manualPumpControl) {                   // Kiểm tra trạng thái nút nhấn thủ công (nếu không có nhấn thì thực hiện công việc phía dưới)
      if (waterLevelPer < 10) {
        digitalWrite(PUMP_PIN, HIGH);
        pumpState = true;
        digitalWrite(PUMP_LED, HIGH);
        Serial.println("🔴 Mực nước xuống thấp!");
        Serial.println("Bơm: ON (⚙️ Tự động)");
      }
      else if (waterLevelPer == 100) {
        digitalWrite(PUMP_PIN, LOW);
        pumpState = false;
        digitalWrite(PUMP_LED, LOW);
        Serial.println("🟢 Mực nước đạt mức cao nhất!");
        Serial.println("Bơm: OFF (⚙️ Tự động)");
      } 
      manualPumpControl = false;              // Gán biến nút nhấn bơm thủ công trở về mức 0
    }
    else {
      digitalWrite(PUMP_PIN, pumpState ? HIGH : LOW);
      if (pumpState == true) {
        digitalWrite(PUMP_LED, HIGH);
        Serial.println("Bơm: ON (🛠️ Thủ công)");
      }
      else {
        digitalWrite(PUMP_LED, LOW);
        Serial.println("Bơm: OFF (🛠️ Thủ công)");
      }
    }
  }
  else {
    /*waterLevelPer = 0;
    waterHeight = 0.0;
    waterVolume = 0.0;*/
    digitalWrite(PUMP_PIN, LOW);
    digitalWrite(TANK_LED, LOW);
    digitalWrite(PUMP_LED, LOW);
    pumpState = false;
    ledState = false;
    manualPumpControl = false;
  }
}

void drawWaterLevelScreen() {
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Muc nuoc:");
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.print(waterHeight, 1);
  display.print(" cm");

  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("The tich:");
  display.setTextSize(2);
  display.setCursor(0, 40);
  display.print(waterVolume, 1);
  display.print(" L");

  int rectX = 85, rectY = 16, rectWidth = 40, rectHeight = 40;
  display.drawRect(rectX, rectY, rectWidth, rectHeight, WHITE);

  int waterHeightRect = map(waterLevelPer, 0, 100, 0, rectHeight - 4);
  int waterTopY = rectY + rectHeight - 2 - waterHeightRect;
  display.fillRect(rectX + 2, waterTopY, rectWidth - 4, waterHeightRect, WHITE);
}
