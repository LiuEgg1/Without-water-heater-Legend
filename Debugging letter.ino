// 内存使用情况
void checkMemory() {
  Serial.print(F("免费内存: "));
  Serial.println(freeMemory());
}

// 引脚状态检查
void checkPinStatus() {
  for(int i=2; i<=13; i++) {
    Serial.print("Pin ");
    Serial.print(i);
    Serial.print(": ");
    Serial.println(digitalRead(i));
  }
}
