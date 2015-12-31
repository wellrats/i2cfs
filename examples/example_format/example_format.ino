#include <Arduino.h>
#include <i2cfs.h>
#include <Wire.h>
#include <printf.h>

I2CFS fs;

void setup() {
  
  printf_begin();
  Serial.begin(57600);
  Wire.begin();
  fs.begin(0x50);
}

void loop() {

  delay(5000);
  printf_P(PSTR("Formatting ...\n"));
  fs.format(32); // 24LC256 // 32 Kb
  printf_P(PSTR("Format Complete\n"));
  #ifdef SERIAL_DEBUG
  fs.master_block.print();
  #endif
  while(1);

}

