#include <Wire.h>

void setup() {
  Serial.begin(115200);

  while (!Serial)
    delay(10);

  Wire.begin();
  Wire.setClock(400000);
  Wire.setWireTimeout(200000, false);
}

void loop() {
  Serial.println("\nI2C write -> repeated START -> write test");

  // First write, without STOP
  Wire.beginTransmission(0x70);
  Wire.write(0x01);
  Wire.write(0x02);

  uint8_t err1 = Wire.endTransmission(false);

  Serial.print("Write 1 result: ");
  Serial.println(err1);

  // Second write: starts with repeated START
  Wire.beginTransmission(0x70);
  Wire.write(0x03);
  Wire.write(0x04);

  uint8_t err2 = Wire.endTransmission(true);

  Serial.print("Write 2 result: ");
  Serial.println(err2);

  delay(100);
}