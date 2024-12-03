void BluePix() {
  pixels.setPixelColor(0, pixels.Color(0, 0, 20));
  pixels.show();
}

void DimBluePix() {
  pixels.setPixelColor(0, pixels.Color(0, 0, 1));
  pixels.show();
}

void GreenPix() {
  pixels.setPixelColor(0, pixels.Color(20, 0, 0));
  pixels.show();
}

void RedPix() {
  pixels.setPixelColor(0, pixels.Color(0, 20, 0));
  pixels.show();
}

void PurplePix() {
  pixels.setPixelColor(0, pixels.Color(0, 50, 50));
  pixels.show();
}

void NoPix() {
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.show();
  digitalWrite(PIN, LOW);
}

void Vibrate(unsigned long wait) {
  mcp.digitalWrite(haptic, HIGH);
  delay(wait);
  mcp.digitalWrite(haptic, LOW);
}
