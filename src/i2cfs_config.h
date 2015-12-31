#include <Arduino.h>

#ifndef I2CFS_CONFIG_H
#define I2CFS_CONFIG_H

#define SERIAL_DEBUG
#undef  READ_ONLY      // Do no compile the code of all methods that write data
                       // This do the code smaller if you want just read the 
                       // File System

#define DRIVER_I2C     
#undef  DRIVER_EEPROM
#undef  DRIVER_SPI

#ifdef DRIVER_I2C
    #define driver_write i2c_write_buffer
    #define driver_read  i2c_read_buffer
#endif

#ifdef ESP8266
    #undef  PSTR
    #define pdebug_P Serial.printf
    #define PSTR
#else
    #define pdebug_P printf_P
#endif

#ifdef SERIAL_DEBUG
  	#define IF_SERIAL_DEBUG(x) (x);
#else
  	#define IF_SERIAL_DEBUG(x)
#endif

#endif