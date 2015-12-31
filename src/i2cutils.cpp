#include "i2cutils.h"

// ---------------------------------------------------------------------------------------------

void i2c_set_address(int deviceaddress, unsigned int eeaddress, bool close)
{
  Wire.beginTransmission(deviceaddress);
  Wire.write((int)((eeaddress) >> 8));   // MSB
  Wire.write((int)((eeaddress) & 0xFF)); // LSB
  if(close) Wire.endTransmission();
}

// ---------------------------------------------------------------------------------------------

void i2c_write_buffer(int deviceaddress, unsigned int eeaddress, uint8_t* data, int data_len) 
{
  // Uses Page Write for 24LC256
  // Allows for 64 byte page boundary
  // Splits string into max 16 byte writes

  unsigned int  next_page;
  while(data_len)  {

     next_page  = ((eeaddress >> 4) + 1) << 4;
     i2c_set_address(deviceaddress, eeaddress, false);
     while(data_len && eeaddress < next_page) {
        Wire.write((uint8_t) *data++);
        eeaddress++;
        data_len--;
     }
     Wire.endTransmission();
     delay(6);  // needs 5ms for page write
  }
}
 
// ---------------------------------------------------------------------------------------------

void i2c_read_buffer(int deviceaddress, unsigned int eeaddress, uint8_t* data, int data_len) 
{
  // Uses Page Write for 24LC256
  // Allows for 64 byte page boundary
  // Splits string into max 16 byte reads

  unsigned int  next_page;
  unsigned int  bytes_read;
  unsigned int  len = data_len;
  char          c = 1;

  while(len)  {

     next_page  = ((eeaddress >> 4) + 1) << 4;
     bytes_read = min(len, (next_page - eeaddress));
     
     i2c_set_address(deviceaddress, eeaddress, true);
     Wire.requestFrom(deviceaddress, bytes_read);

     while(Wire.available()) { 

        c = Wire.read();
        *data++ = c;
        eeaddress++;
        len--;

     }
  }

}
 
