#include <Arduino.h>
#include <stddef.h>
#include <inttypes.h>
#include <Wire.h>

void i2c_set_address(int deviceaddress, unsigned int eeaddress, bool close);
void i2c_write_buffer(int deviceaddress, unsigned int eeaddress, uint8_t* data, int data_len);
void i2c_read_buffer(int deviceaddress, unsigned int eeaddress, uint8_t* data, int data_len);
