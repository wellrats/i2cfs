/*

+--------------+--------------+-------------+-------+------------+--------------+--------------+-----+--------+ 
| Master Block | File Dir  1  | File Dir  2 | ....  | File Dir N | Data Block 1 | Data Block 2 | ... | Data N |
+--------------+--------------+-------------+-------+------------+--------------+--------------+-----+--------+ 

+--------------+-----------+----------+---------+-------
| Master Block | File  1   | File 2   | File N  | Data 1 
+--------------+-----------+----------+---------+

*/

/**
 * Structure that holds the Master Block of the file system
 */

#ifndef I2CFS_H

#define I2CFS_H

#include <stdint.h>
#include "i2cfs_config.h"

typedef char     FILENAME[32];
typedef uint16_t BLOCK;

struct MasterBlock {

  uint16_t total_blocks;              // Total Blocks of 64bytes this EEPROM has
  uint16_t used_blocks;               // Total of used blocks
  BLOCK    first_used_block;          // Number of first used block (ZERO if none)
  BLOCK    last_used_block;
  BLOCK    first_free_block;          // Number of first free block (ZERO if none)
  BLOCK    last_free_block;
  BLOCK    first_file_block;          // Number of first block that is a file 
  BLOCK    first_directory_block;     // Number of first block that is a directory

  #ifdef SERIAL_DEBUG
  const void print(char op) const;
  const void toString(char* buffer, uint8_t size_buf) const;
  #endif

}  __attribute__((__packed__));

/*
 * Structure that holds a File Entry Block
 */

struct DirectoryBlock {

  BLOCK    this_block;
  BLOCK    next_dir_block;           // Number of the next used block
  BLOCK    previous_dir_block;       // Number of the previous used block
  FILENAME name;                      // Name of the file

  #ifdef SERIAL_DEBUG
  const void print(char op) const;
  const void toString(char* buffer, uint8_t size_buf) const;
  #endif

} __attribute__((__packed__));

struct FileBlock {

  BLOCK    this_block;
  BLOCK    next_file_block;
  BLOCK    previous_file_block;
  BLOCK    parent_directory;          // Number of parent directory
  uint32_t size;                      // Size in bytes of file
  uint32_t num_data_blocks;           // Number of data blocks
  BLOCK    first_data_block;          // Number of the first data block of this file
  BLOCK    last_data_block;
  uint8_t  attributes;
  FILENAME name;                      // Name of the file

  #ifdef SERIAL_DEBUG
  const void print(char op) const;
  const void toString(char* buffer, uint8_t size_buf) const;
  #endif

}  __attribute__((__packed__));

/*
 * Structure that holds a DataBlock
 */

struct DataBlock {
  
  BLOCK    next_data_block;           // Number of the next data block (ZERO if none)
  uint8_t  data[62];

  #ifdef SERIAL_DEBUG
  const void print(char op, BLOCK this_block) const;
  const void toString(char* buffer, uint8_t size_buf) const;
  #endif
}  __attribute__((__packed__));

/*
 * Structure that holds a DataBlock
 */

struct FreeBlock {
  
  BLOCK    next_free_block;           // Number of the next data block (ZERO if none)
  
  #ifdef SERIAL_DEBUG
  const void print(char op, BLOCK this_block) const;
  const void toString(char* buffer, uint8_t size_buf) const;
  #endif

} __attribute__((__packed__));

typedef enum { MODE_READ = 0, MODE_WRITE, MODE_APPEND } FILE_MODE ;

struct FILE_HANDLE {
   
  BLOCK      block_num;
  FILE_MODE  mode;
  BLOCK      next_data_block;
  uint8_t    position_in_block;
  uint32_t   size;
  uint32_t   position;
  BLOCK      first_data_block;

  #ifdef SERIAL_DEBUG
  const void print() const;
  const void toString(char* buffer, uint8_t size_buf) const;
  #endif    
    
}  __attribute__((__packed__));
   
struct DIR_HANDLE  {
   
  BLOCK    block_num;
  BLOCK    next_file_block;
  uint32_t pos;

  #ifdef SERIAL_DEBUG
  const void print() const;
  const void toString(char* buffer, uint8_t size_buf) const;
  #endif

    
} __attribute__((__packed__));

typedef uint8_t   FS_STATUS;
typedef FileBlock FILE_ENTRY;
typedef DirectoryBlock DIR_ENTRY;

#define BLOCK_SIZE 64
#define DATA_SIZE  (BLOCK_SIZE - sizeof(BLOCK))

#define FS_STATUS_OK                    0
#define FS_STATUS_INVALID_FILE_NAME     1
#define FS_STATUS_DUPLICATED_FILE_NAME  2
#define FS_STATUS_NOT_FOUND             3
#define FS_STATUS_DISK_FULL             4
#define FS_STATUS_ACESS_DENIED          5
#define FS_STATUS_INVALID_SEEK          6
#define FS_STATUS_END_OF_FILE           7
#define FS_STATUS_INVALID_HANDLE        8

class I2CFS
{

  public:

  struct {

    union {
      uint8_t         raw[BLOCK_SIZE];
      DirectoryBlock  directory;
      FileBlock       file;
      DataBlock       data;
      FreeBlock       free;
    };

  } block;

  //uint8_t         block[BLOCK_SIZE];
  uint8_t         i2c_addr;
  BLOCK           last_acessed;
  MasterBlock     master_block;
  BLOCK           next_dir_block;
  
  bool       read_block(uint16_t block_num, uint16_t size);
  bool       write_block(uint16_t block_num, uint16_t size);

  bool       read_block_ex(uint16_t block_num, uint8_t offset, void* buffer, uint16_t size);
  bool       write_block_ex(uint16_t block_num, uint8_t offset, void* buffer, uint16_t size);

  bool       read_block_type_free(uint16_t block_num);
  bool       write_block_type_free(uint16_t block_num);

  bool       read_block_type_file(uint16_t block_num);
  bool       write_block_type_file(uint16_t block_num);

  bool       read_block_type_dir(uint16_t block_num);
  bool       write_block_type_dir(uint16_t block_num);

  bool       read_block_type_data(uint16_t block_num);
  bool       write_block_type_data(uint16_t block_num);

  BLOCK      get_one_free_block();
  void       release_one_used_block(BLOCK used_block);
  void       release_data_blocks(BLOCK first_block);

  BLOCK      find_dir_block_by_name(const char *name);

  FS_STATUS truncate_file_entry(FILE_ENTRY& file_entry);
  FS_STATUS create_file_entry(DIR_HANDLE& dir_handle,
                              const char *name,
                              FILE_ENTRY& file_entry);
  
  bool       read_master_block();
  bool       save_master_block();

  void       clear_temp_block();
  BLOCK      append_new_data_block(BLOCK file_block, BLOCK last_data_block);
  void       file_handle_from_file_entry(FILE_HANDLE& file_handle, FILE_ENTRY& file_entry, uint32_t seek_pos);

  /*
  bool write_block(uint16_t block, void* data);
  bool get_next_free_block();
  bool release_used_block();
  bool write_file_block();
  bool write_data_block();
  */

  //public:

  
  /**
   * Format the FileSystem for first time use
   *
   * Before using a I2CFS you must format it so the free blocks list and
   * used blocks list can be initiated to default values
   *
   * @param bits_size Size in bits for the I2C memory this file system will
   * handle
   */

   I2CFS();

   void      begin (uint8_t addr);
   FS_STATUS format(uint16_t size_in_KB);

   FS_STATUS directory_exists(const char *name);
   FS_STATUS create_directory(const char *name);
   bool      is_valid_dir_name(const char *name);
   FS_STATUS open_directory(const char *name, DIR_HANDLE& dir_handle);
   FS_STATUS rename_directory(DIR_HANDLE&  dir_handle, const char* new_name);
   FS_STATUS delete_directory(DIR_HANDLE&  dir_handle);
   FS_STATUS delete_directory(const char *name);
   FS_STATUS close_directory(DIR_HANDLE& dir_handle);
   
   FS_STATUS find_first_file(DIR_HANDLE&  dir_handle);
   FS_STATUS find_next_file(DIR_HANDLE&  dir_handle, FILE_ENTRY& file);

   FS_STATUS find_first_dir();
   FS_STATUS find_next_dir(DIR_ENTRY&  dir_entry);

   FS_STATUS find_file(DIR_HANDLE& dir_handle, const char* name, FILE_ENTRY& file_entry);

   FS_STATUS seek(FILE_HANDLE& file_handle, uint32_t pos);

   FS_STATUS open(const char* name, 
                   FILE_MODE mode, 
                   DIR_HANDLE& dir_handle, 
                   FILE_HANDLE& file_handle);

   FS_STATUS read(FILE_HANDLE& file_handle, void* buffer, uint16_t size, uint16_t* really_read, bool has_delimiter, char delimiter);
   FS_STATUS read(FILE_HANDLE& file_handle, void* buffer, uint16_t size, uint16_t* really_read);
   FS_STATUS read(FILE_HANDLE& file_handle, void* buffer, uint16_t size, uint16_t* really_read, char delimiter);
   FS_STATUS write(FILE_HANDLE& file_handle, void* buffer, uint16_t size, uint16_t* really_write);

   FS_STATUS close(FILE_HANDLE& file_handle);
   FS_STATUS truncate(FILE_HANDLE& file_handle);
   FS_STATUS erase(DIR_HANDLE& dir_handle, const char* name);
  /**
   * Creates a directory
   *
   * Creates a new directory at the root level. It's not possible create
   * directories inside directories
   *
   * @param name Name of directory to be created
   *
   */

   /*

   FS_STATUS find_first(DIR_HANDLE* dir_handle, FileBlock *file_block);

   FS_STATUS rename_directory(DIR_HANDLE*  dir_handle, char* new_name);

   FS_STATUS delete_directory(DIR_HANDLE*  handle);

   FS_STATUS open(FILE_HANDLE* handle, char *name, char *attributes);

   FS_STATUS seek(FILE_HANDLE* handle, uint32_t position);

   FS_STATUS write(FILE_HANDLE* handle, void *buffer, uint32_t size);

   FS_STATUS write(FILE_HANDLE* handle, char* str);

   FS_STATUS writeln(FILE_HANDLE* handle, char* str);

   FS_STATUS delete(FILE_HANDLE* handle, char* str);
   */
   
};

#endif
   
