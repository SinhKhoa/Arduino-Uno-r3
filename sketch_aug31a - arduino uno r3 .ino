#include <SoftwareSerial.h>
#include <Servo.h>
#include <BTS7960.h>

//=====================================
// Định nghĩa chân kết nối
//=====================================
#define L_EN 2
#define R_EN 4
#define L_PWM 5
#define R_PWM 6

//=====================================
// Định nghĩa lệnh điều khiển
//=====================================
#define FORWARD 'F'
#define BACKWARD 'B'
#define LEFT 'L'
#define RIGHT 'R'
#define INCREASE_SPEED 'T'
#define DECREASE_SPEED 'X'
#define STOP '0'
#define STOP_START 'S'   // nút Start
#define RESET 'Z'  // nút Select

// Bluetooth (RX=8, TX=9)
SoftwareSerial BT(8, 9); 
BTS7960 motor(L_EN, R_EN, L_PWM, R_PWM);

// buffer nhận dữ liệu từ gamepad
byte buffer[20];
int index = 0;

//=====================================
// Servo
//=====================================
Servo wheel_servo; // servo bánh xe
Servo IR_Servo;    // servo IR

int pos_wheel = 90; // góc mặc định servo bánh xe
int pos_ir = 90;    // góc mặc định servo IR
int speed = 40;     // tốc độ mặc định

#define MAX_SPEED 200
#define MIN_SPEED 10
#define MIN_WHEEL_ANGLE 65
#define MAX_WHEEL_ANGLE 118

//=====================================
// Trạng thái chạy
//=====================================
bool isForward = false;
bool isBackward = false;
bool isLeft = false;
bool isRight = false;
bool isOn = true;
bool isIncrease = false; // giữ nút tăng tốc
bool isDecrease = false; // giữ nút giảm tốc

//=====================================
// SETUP
//=====================================
void setup() {
  Serial.begin(9600);
  BT.begin(9600);
  Serial.println("=== KHỞI ĐỘNG MODULES CHÍNH ===");

  wheel_servo.attach(11, 500, 2500);
  IR_Servo.attach(10, 50, 2500);

  wheel_servo.write(pos_wheel);
  IR_Servo.write(pos_ir);

  motor.begin();
  motor.enable();
}

//=====================================
// LOOP
//=====================================
void loop() {
  // Đọc dữ liệu từ Bluetooth
  while (BT.available()) {
    byte c = BT.read();

    if (c == 0xFF) { // start byte
      index = 0;
      buffer[index++] = c;
    } else if (index > 0) {
      buffer[index++] = c;
    }

    if (index >= 8) {
      displayButton(buffer);
      index = 0;
    }
  }

  // --- Điều khiển motor theo trạng thái nhấn giữ ---
  if (isOn) {
    if (isForward) {
      motor.pwm = speed;
      motor.front();
    } else if (isBackward) {
      motor.pwm = speed;
      motor.back();
    } else {
      motor.stop();
    }
  } else {
    motor.stop();
  }

  // --- Xử lý rẽ liên tục ---
  if (isLeft) {
    pos_wheel = max(pos_wheel - 1, MIN_WHEEL_ANGLE); 
    wheel_servo.write(pos_wheel);
    IR_Servo.write(180 - pos_wheel);
    delay(8);
  }

  if (isRight) {
    pos_wheel = min(pos_wheel + 1, MAX_WHEEL_ANGLE);
    wheel_servo.write(pos_wheel);
    IR_Servo.write(180 - pos_wheel);
    delay(8);
  }

  // --- Xử lý giữ nút tăng/giảm tốc ---
  if (isIncrease) {
    speed = min(speed + 1, MAX_SPEED);
    Serial.print("[CMD] Giữ Tam giác (+) Speed = ");
    Serial.println(speed);
    delay(50);
  }
  if (isDecrease) {
    speed = max(speed - 1, MIN_SPEED);
    Serial.print("[CMD] Giữ X (-) Speed = ");
    Serial.println(speed);
    delay(50);
  }
}

//=====================================
// MAPPING NÚT ĐA PHÍM
//=====================================
// Hàm này cập nhật trạng thái từng phím dựa trên từng bit b5, b6
void displayButton(byte* buf) {
  byte b5 = buf[5];
  byte b6 = buf[6];

  // --- D-pad ---
  isForward  = (b6 & 0x01) > 0; // giữ lên
  isBackward = (b6 & 0x02) > 0; // giữ xuống
  isLeft     = (b6 & 0x04) > 0; // giữ trái
  isRight    = (b6 & 0x08) > 0; // giữ phải
  // --- Nút hình học ---
  isIncrease  = (b5 & 0x04) > 0; // Tam giác giữ
  isDecrease  = (b5 & 0x10) > 0; // X giữ
  isRight    |= (b5 & 0x08) > 0; // Tròn giữ (cộng dồn với D-pad phải)
  isLeft     |= (b5 & 0x20) > 0; // Vuông giữ (cộng dồn với D-pad trái)

  // --- Start / Select ---
  if ((b5 & 0x01) > 0 && b6 == 0x00) {
    handleCommand(STOP_START);
  }
  if ((b5 & 0x02) > 0 && b6 == 0x00) {
    handleCommand(RESET);
  }

  // Nếu không giữ nút nào (b6 == 0x00 và b5 == 0x00) thì dừng
  if ((b6 == 0x00) && (b5 == 0x00)) {
    handleCommand(STOP);
  }
}

//=====================================
// XỬ LÝ LỆNH
//=====================================
void handleCommand(char cmd) {
  switch (cmd) {
    case STOP:
      // Dừng hết mọi trạng thái
      isForward = isBackward = isLeft = isRight = isIncrease = isDecrease = false;
      Serial.println("[CMD] DỪNG (thả nút) - dừng toàn bộ ");
      break;

    case STOP_START: 
      if (isOn) {
        motor.pwm = 0;
        motor.front();
        motor.disable();
        Serial.println("[CMD] TẠM DỪNG");
        isOn = false;
      } else {
        isOn = true;
        motor.enable();
        Serial.println("[CMD] TIẾP TỤC");
      }
      break;

    case RESET:
      motor.pwm = 0;
      motor.disable();
      pos_wheel = 90;
      wheel_servo.write(pos_wheel);
      pos_ir = 180 - pos_wheel;
      IR_Servo.write(pos_ir);
      speed = 40;
      isForward = isBackward = isLeft = isRight  = isIncrease = isDecrease = false;
      isOn = true;
      motor.enable();
      Serial.println("[CMD] RESET HOÀN TẤT");
      break;

    default:
      Serial.print("[CMD] Unknown: ");
      Serial.println(cmd);
      break;
  }
}