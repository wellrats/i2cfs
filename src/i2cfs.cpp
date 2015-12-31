
#include "i2cfs.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "i2cutils.h"

I2CFS::I2CFS():last_acessed(0)
{
}

void I2CFS::begin(uint8_t addr) {
	i2c_addr = addr;
    read_master_block();
}
/*
 * Format 
 */

void I2CFS::clear_temp_block() {
	memset(block.raw, 0, BLOCK_SIZE);
}

bool I2CFS::read_master_block() { 
	i2c_read_buffer(i2c_addr, 0, (uint8_t*) &master_block, sizeof(MasterBlock));
	IF_SERIAL_DEBUG(master_block.print('R'));
}

bool I2CFS::save_master_block() { 
	i2c_write_buffer(i2c_addr, 0, (uint8_t*) &master_block, sizeof(MasterBlock));
	IF_SERIAL_DEBUG(master_block.print('W'));
}

bool I2CFS::read_block(uint16_t block_num, uint16_t size) { 
	uint32_t block_addr = block_num * 64;
	i2c_read_buffer(i2c_addr, block_addr, (uint8_t*) &block.raw, size);	
}

bool I2CFS::read_block_ex(uint16_t block_num, uint8_t offset, void* buffer, uint16_t size) { 
	uint32_t block_addr = block_num * 64 + offset;
	i2c_read_buffer(i2c_addr, block_addr, (uint8_t*) buffer, size);	
}

bool I2CFS::write_block(uint16_t block_num, uint16_t size) { 
	uint32_t block_addr = block_num * 64;
	i2c_write_buffer(i2c_addr, block_addr, (uint8_t*) &block.raw, size);
}

bool I2CFS::write_block_ex(uint16_t block_num, uint8_t offset, void* buffer, uint16_t size) { 
	uint32_t block_addr = block_num * 64 + offset;
	i2c_write_buffer(i2c_addr, block_addr, (uint8_t*) buffer, size);
}

bool I2CFS::read_block_type_free(uint16_t block_num) { 
	read_block(block_num, sizeof(FreeBlock));
	IF_SERIAL_DEBUG(block.free.print('R', block_num));
}

bool I2CFS::write_block_type_free(uint16_t block_num) { 
	write_block(block_num, sizeof(FreeBlock));
	IF_SERIAL_DEBUG(block.free.print('W', block_num));
}

bool I2CFS::read_block_type_file(uint16_t block_num) { 
	read_block(block_num, sizeof(FileBlock));
	IF_SERIAL_DEBUG(block.file.print('R'));
}

bool I2CFS::write_block_type_file(uint16_t block_num) { 
	block.file.this_block = block_num;
	write_block(block_num, sizeof(FileBlock));
	IF_SERIAL_DEBUG(block.file.print('W'));
}

bool I2CFS::read_block_type_dir(uint16_t block_num) { 
	read_block(block_num, sizeof(DirectoryBlock));
	IF_SERIAL_DEBUG(block.directory.print('R'));
}

bool I2CFS::write_block_type_dir(uint16_t block_num) { 
	block.directory.this_block = block_num;
	write_block(block_num, sizeof(DirectoryBlock));
	IF_SERIAL_DEBUG(block.directory.print('W'));
}

bool I2CFS::read_block_type_data(uint16_t block_num) { 
	read_block(block_num, sizeof(DataBlock));
	IF_SERIAL_DEBUG(block.data.print('R', block_num));
}

bool I2CFS::write_block_type_data(uint16_t block_num) { 
	write_block(block_num, sizeof(DataBlock));
	IF_SERIAL_DEBUG(block.data.print('W', block_num));
}

BLOCK I2CFS::get_one_free_block() { 

	BLOCK free_block_num = master_block.first_free_block;

	if (free_block_num) {

		read_block_type_free(free_block_num);

		BLOCK next_free_block_num = block.free.next_free_block;
		master_block.first_free_block = next_free_block_num;
		master_block.used_blocks++;

		save_master_block();
		return free_block_num;

	}
}

void I2CFS::release_one_used_block(BLOCK used_block) { 

    #ifndef READ_ONLY

	if(!used_block) return;

    block.free.next_free_block = master_block.first_free_block;
    write_block_type_free(used_block);

    master_block.first_free_block = used_block;
    master_block.used_blocks--;
    save_master_block();

    #endif

}

void I2CFS::release_data_blocks(BLOCK first_block) { 

    #ifndef READ_ONLY

	if(!first_block) return;

    BLOCK next_block = first_block;
    BLOCK last_block = 0;
    uint16_t released_blocks=0;

    while (next_block) {

    	last_block = next_block;
    	released_blocks++;
    	read_block_type_free(next_block);
    	next_block = block.free.next_free_block;

    }

    if(first_block == last_block) release_one_used_block(first_block);
    else {

       // read_block_type_free(last_block); // It's in buffer
       block.free.next_free_block = master_block.first_free_block;
       write_block_type_free(last_block);

       master_block.first_free_block = first_block;
       master_block.used_blocks      -= released_blocks;

       save_master_block();
    }

    #endif
    
}

BLOCK I2CFS::find_dir_block_by_name(const char* name) {
    
	BLOCK next_block = master_block.first_directory_block;

	while (next_block) {
		read_block_type_dir(next_block);
		if(strcmp(name, block.directory.name) == 0) return next_block;
		next_block = block.directory.next_dir_block;
	}
    
	return 0;

}

FS_STATUS I2CFS::directory_exists(const char* name) {

	if(!find_dir_block_by_name(name)) return FS_STATUS_OK;
    return FS_STATUS_NOT_FOUND;

}

bool I2CFS::is_valid_dir_name(const char * name) {

	if((!strlen(name)) || 
       (*name != '/')  || 
	   (strlen(name) == 1)) return false;

	return true;
}

FS_STATUS I2CFS::create_directory(const char* name) {

    #ifdef READ_ONLY

    return FS_STATUS_ACESS_DENIED;

    #else

    if(!is_valid_dir_name(name))               return FS_STATUS_INVALID_FILE_NAME;
    if(directory_exists(name) != FS_STATUS_OK) return FS_STATUS_DUPLICATED_FILE_NAME;


    BLOCK new_block_num = get_one_free_block();

    if(!new_block_num) return FS_STATUS_DISK_FULL;

    BLOCK previous_dir_block_num = master_block.first_directory_block;

    if(previous_dir_block_num) {

        // Update Old First Block of Chain to point to
        // New first block of chain

    	read_block_type_dir(previous_dir_block_num);
    	block.directory.previous_dir_block = new_block_num;
    	write_block_type_dir(previous_dir_block_num);

    }

    // Set new first Block of chain

    master_block.first_directory_block = new_block_num;

    save_master_block();

    // Save new Block of chain

    block.directory.next_dir_block     = previous_dir_block_num;
    block.directory.previous_dir_block = 0;
    memcpy(block.directory.name, name, strlen(name) + 1);

    write_block_type_dir(new_block_num);

    return FS_STATUS_OK;

    #endif

}

FS_STATUS I2CFS::open_directory(const char* name, DIR_HANDLE& dir_handle) {
   
	if(!strlen(name))  return FS_STATUS_INVALID_FILE_NAME;

	if((strlen(name) == 1) && (*name == '/')) // "/"
	   dir_handle.block_num       = 0; // ROOT
	else {
       BLOCK dir_block = find_dir_block_by_name(name);

	   if(!dir_block) 
	   	 return FS_STATUS_NOT_FOUND;

       dir_handle.block_num = dir_block;
    }

    dir_handle.next_file_block = master_block.first_file_block;

    return FS_STATUS_OK;

}

FS_STATUS I2CFS::close_directory(DIR_HANDLE& dir_handle) {
    dir_handle.block_num = 0;
    return FS_STATUS_OK;
}

FS_STATUS I2CFS::rename_directory(DIR_HANDLE&  dir_handle, const char* new_name) {

    #ifdef READ_ONLY

    return FS_STATUS_ACESS_DENIED;

    #else

	if(!dir_handle.block_num)          return FS_STATUS_ACESS_DENIED;

	if(!is_valid_dir_name(new_name))   return FS_STATUS_INVALID_FILE_NAME;
	if(directory_exists(new_name))     return FS_STATUS_DUPLICATED_FILE_NAME;

	read_block_type_dir(dir_handle.block_num);
	strncpy(block.directory.name, new_name, 32);
	write_block_type_dir(dir_handle.block_num);

	return FS_STATUS_OK;

	#endif
}

FS_STATUS I2CFS::delete_directory(const char* name) {

    #ifdef READ_ONLY

    return FS_STATUS_ACESS_DENIED;

    #else

    DIR_HANDLE dir_handle;
    if(open_directory(name, dir_handle) == FS_STATUS_OK)
        return delete_directory(dir_handle);

    #endif
}

FS_STATUS I2CFS::delete_directory(DIR_HANDLE&  dir_handle) {

    #ifdef READ_ONLY

    return FS_STATUS_ACESS_DENIED;

    #else

	if(!dir_handle.block_num) return FS_STATUS_ACESS_DENIED;
    
    FILE_ENTRY file_entry;
    find_first_file(dir_handle);

    while(find_next_file(dir_handle, file_entry) == FS_STATUS_OK) {
        erase(dir_handle, file_entry.name);
    }
    
	read_block_type_dir(dir_handle.block_num);
	uint16_t next     = block.directory.next_dir_block;
	uint16_t previous = block.directory.previous_dir_block;

    if(previous) {
		read_block_type_dir(previous);
		block.directory.next_dir_block = next;
		write_block_type_dir(previous);
	} else {
		master_block.first_directory_block = next;
		save_master_block();
	}

	if(next) {
		read_block_type_dir(next);
		block.directory.previous_dir_block = previous;
		write_block_type_dir(next);
	}

	release_one_used_block(dir_handle.block_num);
	dir_handle.block_num = 0;

	return FS_STATUS_OK;

	#endif

}


FS_STATUS I2CFS::erase(DIR_HANDLE& dir_handle, const char* name) {

    #ifdef READ_ONLY

    return FS_STATUS_ACESS_DENIED;

    #else

    FILE_ENTRY file_entry;
    FS_STATUS  find_status = find_file(dir_handle, name, file_entry);

    if(find_status != FS_STATUS_OK) return FS_STATUS_NOT_FOUND;

    truncate_file_entry(file_entry);

    uint16_t next     = file_entry.next_file_block;
    uint16_t previous = file_entry.previous_file_block;

    if(previous) {
        read_block_type_file(previous);
        block.file.next_file_block = next;
        write_block_type_file(previous);
    } else {
        master_block.first_file_block = next;
        save_master_block();
    }

    if(next) {
        read_block_type_file(next);
        block.file.previous_file_block = previous;
        write_block_type_file(next);
    }

    release_one_used_block(file_entry.this_block);

    return FS_STATUS_OK;

    #endif

}

FS_STATUS I2CFS::find_first_dir() {

    next_dir_block = master_block.first_directory_block;
    return FS_STATUS_OK;
}

FS_STATUS I2CFS::find_next_dir(DIR_ENTRY& dir_entry) {

    if(!next_dir_block) return FS_STATUS_NOT_FOUND;
    read_block_type_dir(next_dir_block);
    memcpy(&dir_entry, &block, sizeof(DIR_ENTRY));
    next_dir_block = block.directory.next_dir_block;
    return FS_STATUS_OK;
}

FS_STATUS I2CFS::find_first_file(DIR_HANDLE&  dir_handle) {

    dir_handle.next_file_block = master_block.first_file_block;
	return FS_STATUS_OK;
}

FS_STATUS I2CFS::find_next_file(DIR_HANDLE&  dir_handle, FILE_ENTRY& file_entry) {

	while (dir_handle.next_file_block) {

		read_block_type_file(dir_handle.next_file_block);
		dir_handle.next_file_block = block.file.next_file_block;

		if (block.file.parent_directory == dir_handle.block_num) {
			memcpy(&file_entry, &block, sizeof(FILE_ENTRY));
			return FS_STATUS_OK;
		}
	}

	return FS_STATUS_NOT_FOUND;
}

FS_STATUS I2CFS::find_file(DIR_HANDLE& dir_handle, const char* name, FILE_ENTRY& file_entry) {

	find_first_file(dir_handle);

	while(find_next_file(dir_handle, file_entry) == FS_STATUS_OK) {
		if(strcmp(file_entry.name, name) == 0) return FS_STATUS_OK;
	}

	return FS_STATUS_NOT_FOUND;
}

FS_STATUS I2CFS::create_file_entry(DIR_HANDLE& dir_handle, const char* name, FILE_ENTRY& file_entry) {

	if(!strlen(name)) return FS_STATUS_INVALID_FILE_NAME;

    BLOCK new_block_num = get_one_free_block();
    if(!new_block_num) return FS_STATUS_DISK_FULL;

    BLOCK previous_file_block_num = master_block.first_file_block;

    if(previous_file_block_num) {

        // Update Old First Block of Chain to point to
        // New first block of chain

    	read_block_type_file(previous_file_block_num);
    	block.file.previous_file_block = new_block_num;
    	write_block_type_file(previous_file_block_num);

    }

    // Set new first Block of chain

    master_block.first_file_block = new_block_num;

    save_master_block();

    // Save new Block of chain

    clear_temp_block();
    block.file.next_file_block     = previous_file_block_num;
    block.file.parent_directory    = dir_handle.block_num;

    //block.file.previous_file_block = 0;
    //block.file.size                = 0;
    //block.file.num_data_blocks     = 0;
    //block.file.first_data_block    = 0;
    //block.file.last_data_block     = 0;

    memcpy(block.file.name, name, strlen(name) + 1);

    write_block_type_file(new_block_num);
    read_block_type_file(new_block_num);
    memcpy(&file_entry, &block.file, sizeof(FILE_ENTRY));

    return FS_STATUS_OK;

}

FS_STATUS I2CFS::truncate_file_entry(FILE_ENTRY& file_entry) {

	release_data_blocks(file_entry.first_data_block);

    block.file.size                = 0;
    block.file.num_data_blocks     = 0;
    block.file.first_data_block    = 0;
    block.file.last_data_block     = 0;

	write_block_type_file(file_entry.this_block);

	memcpy(&file_entry, &block.file, sizeof(FILE_ENTRY));

	return FS_STATUS_OK;

}

FS_STATUS I2CFS::seek(FILE_HANDLE& file_handle, uint32_t pos) {

    if(!file_handle.block_num) return FS_STATUS_INVALID_HANDLE;

    if (pos > file_handle.size) {
    	return FS_STATUS_INVALID_SEEK;
    	IF_SERIAL_DEBUG(pdebug_P(PSTR("seek: invalid\n")))
    }

    IF_SERIAL_DEBUG(pdebug_P(PSTR("seek: begin\n")))
    IF_SERIAL_DEBUG(file_handle.print())

    file_handle.position           = 0;
    file_handle.position_in_block  = 0;

    BLOCK next_data_block  = file_handle.first_data_block;

	while (true) {

		if(next_data_block) { 
			read_block_type_data(next_data_block);
		}

     	file_handle.next_data_block = next_data_block;

		if(pos >= DATA_SIZE) {

			file_handle.position += DATA_SIZE;
			pos -= DATA_SIZE;
			next_data_block = block.data.next_data_block;

			if(!next_data_block && pos) return FS_STATUS_ACESS_DENIED;

		} else {

			file_handle.position += pos;
			file_handle.position_in_block = (uint8_t) pos;
			IF_SERIAL_DEBUG(file_handle.print())
		    IF_SERIAL_DEBUG(pdebug_P(PSTR("seek: end\n")))
			return FS_STATUS_OK;

		}
	}

    IF_SERIAL_DEBUG(pdebug_P(PSTR("seek: end\n")))
	return FS_STATUS_OK;

}

void  I2CFS::file_handle_from_file_entry(FILE_HANDLE& file_handle, 
                                  FILE_ENTRY&  file_entry, 
                                  uint32_t seek_pos) {


    file_handle.block_num         = file_entry.this_block;
    file_handle.size              = file_entry.size;
    file_handle.first_data_block  = file_entry.first_data_block;
    seek(file_handle, seek_pos);

}

FS_STATUS I2CFS::open(const char* name, FILE_MODE mode, DIR_HANDLE& dir_handle, FILE_HANDLE& file_handle) {

    IF_SERIAL_DEBUG(pdebug_P(PSTR("open: begin\n")))

    FILE_ENTRY file_entry;
    FS_STATUS  find_status = find_file(dir_handle, name, file_entry);

    if(mode == MODE_READ) {

    	if(find_status != FS_STATUS_OK) {
            file_handle.block_num = 0;
    		return FS_STATUS_NOT_FOUND;
    		IF_SERIAL_DEBUG(pdebug_P(PSTR("open: end not found\n")))
        }
    } 

    #ifndef READ_ONLY

    else {

	    if(find_status == FS_STATUS_OK) {

	    	if(mode == MODE_WRITE) truncate_file_entry(file_entry);

	    } else {

	      FS_STATUS create_status = create_file_entry(dir_handle, name, file_entry); 

	      if(create_status != FS_STATUS_OK) {
	      	return create_status;
	      }

	    }

	}
	#else

	return FS_STATUS_ACESS_DENIED;

	#endif


    file_handle_from_file_entry(file_handle, 
                                file_entry, 
                                mode == MODE_APPEND ? file_entry.size : 0);
    file_handle.mode = mode;
    /*
    file_handle.block_num         = file_entry.this_block;
    file_handle.size              = file_entry.size;
    file_handle.first_data_block  = file_entry.first_data_block;

	IF_SERIAL_DEBUG(file_entry.print('D'))
	IF_SERIAL_DEBUG(file_handle.print())

	if(mode == MODE_APPEND) seek(file_handle, file_entry.size);
	else                    seek(file_handle, 0);
    */
    IF_SERIAL_DEBUG(file_handle.print())
    IF_SERIAL_DEBUG(pdebug_P(PSTR("open: end\n")))

	return FS_STATUS_OK;
    
}

FS_STATUS I2CFS::close(FILE_HANDLE& file_handle) {
   file_handle.block_num = 0;
   return FS_STATUS_OK;
}

BLOCK I2CFS::append_new_data_block(BLOCK file_block, BLOCK last_data_block) {

    BLOCK new_block_num = get_one_free_block();

    if(!new_block_num) return 0;
    read_block_type_file(file_block);

    if(!last_data_block) {
    	block.file.first_data_block = new_block_num;
    }

    block.file.last_data_block = new_block_num;
    write_block_type_file(file_block);

    if(last_data_block) {
      read_block_type_data(last_data_block);
      block.data.next_data_block = new_block_num;
      write_block_type_data(last_data_block);
    }

    read_block_type_data(new_block_num);
    block.data.next_data_block = 0;
    write_block_type_data(new_block_num);

    return new_block_num;


}	


FS_STATUS I2CFS::read(FILE_HANDLE& file_handle, 
                      void*        buffer, 
                      uint16_t     size, 
                      uint16_t*    really_read) {
    return read(file_handle, buffer, size, really_read, false, 0);
}

FS_STATUS I2CFS::read(FILE_HANDLE& file_handle, 
                      void*        buffer, 
                      uint16_t     size, 
                      uint16_t*    really_read,
                      char         delimiter) {
    return read(file_handle, buffer, size, really_read, true, delimiter);
}

FS_STATUS I2CFS::read(FILE_HANDLE& file_handle, 
                      void*        buffer, 
                      uint16_t     size, 
                      uint16_t*    really_read,
                      bool         has_delimiter,
                      char         delimiter) {

    uint8_t* pointer   = (uint8_t*) buffer;
    bool     eof       = false;

    *really_read = 0;

    if(!file_handle.block_num) return FS_STATUS_INVALID_HANDLE;

    IF_SERIAL_DEBUG(pdebug_P(PSTR("read: begin (%i)\n"), size))

    while(size) {

    	IF_SERIAL_DEBUG(file_handle.print())

    	if((eof) || (!file_handle.next_data_block)) {
    		return FS_STATUS_END_OF_FILE;
            IF_SERIAL_DEBUG(pdebug_P(PSTR("read: end end_of_file\n")))
    	}


    	uint8_t bytes_read = min(size, 
    		                     (DATA_SIZE - file_handle.position_in_block));

    	if ((file_handle.position + bytes_read ) > file_handle.size) {
    		bytes_read = file_handle.size - file_handle.position;
    		eof = true;
    	}

        read_block_ex(file_handle.next_data_block, 
        	          file_handle.position_in_block + sizeof(BLOCK), 
        	          &block.raw, 
        	          bytes_read);

        if(has_delimiter) {

            uint8_t j=0;
            while(j < bytes_read) {

                char c = block.raw[j++];
                *pointer++ = c;
                if(c == delimiter) {
                    bytes_read = j;
                    size = 0;
                }
            }

        } else {

            memcpy(pointer, &block.raw, bytes_read);
            pointer += bytes_read;
        }

        #ifdef SERIAL_DEBUG
        pdebug_P(PSTR("read: next_data: %u, offset: %u, bytes_read: %i\n"),
        	     file_handle.next_data_block, file_handle.position_in_block, bytes_read);
        #endif
        
        //pointer                       += bytes_read;

        if(size) size 				  -= bytes_read;
    	file_handle.position          += bytes_read;
    	file_handle.position_in_block += bytes_read;
    	*really_read                  += bytes_read;

        if((file_handle.position_in_block == DATA_SIZE)) {

        	read_block_type_data(file_handle.next_data_block);
	 	    file_handle.next_data_block   = block.data.next_data_block;
	 	    file_handle.position_in_block = 0;
	 	}

    }

    IF_SERIAL_DEBUG(file_handle.print())
    IF_SERIAL_DEBUG(pdebug_P(PSTR("read: end\n")))
    return FS_STATUS_OK;

}

FS_STATUS I2CFS::write(FILE_HANDLE& file_handle, void* buffer, uint16_t size, uint16_t* really_write) {

    #ifdef READ_ONLY

    return FS_STATUS_ACESS_DENIED;

    #else

    uint8_t* pointer = (uint8_t*) buffer;
    *really_write   = 0;

    if(!file_handle.block_num)        return FS_STATUS_INVALID_HANDLE;   
    if(file_handle.mode == MODE_READ) return FS_STATUS_ACESS_DENIED;

    IF_SERIAL_DEBUG(pdebug_P(PSTR("write: (%i) begin\n"), size))

    while(size) {

        IF_SERIAL_DEBUG(file_handle.print())


    	if(!file_handle.next_data_block) {

    		read_block_type_file(file_handle.block_num);
    		file_handle.next_data_block = append_new_data_block(file_handle.block_num, block.file.last_data_block);
            if(!file_handle.first_data_block) {
                file_handle.first_data_block = file_handle.next_data_block;
            }

    		if(!file_handle.next_data_block) {
                IF_SERIAL_DEBUG(pdebug_P(PSTR("write: end disk full\n")))
    			return FS_STATUS_DISK_FULL;
    		}

    		file_handle.position_in_block = 0;
        };

        uint16_t pib        = file_handle.position_in_block;
    	uint8_t bytes_write = min(size, 
    		                     (DATA_SIZE - pib));

        write_block_ex(file_handle.next_data_block, 
        	           pib + sizeof(BLOCK), 
        	           pointer, 
        	           bytes_write);

        #ifdef SERIAL_DEBUG
        pdebug_P(PSTR("write: next_data: %u, offset: %u, bytes_write: %i\n"),
        	     file_handle.next_data_block, pib, bytes_write);
        #endif

       	pointer          			  += bytes_write;
       	size                          -= bytes_write;
    	file_handle.position          += bytes_write;
    	file_handle.position_in_block += bytes_write;
        *really_write                 += bytes_write;

        if(file_handle.position_in_block == DATA_SIZE) {
          //file_handle.next_data_block    = next;
          read_block_type_data(file_handle.next_data_block);
          file_handle.next_data_block   = block.data.next_data_block;
          file_handle.position_in_block = 0;
        }

    }

   	if(file_handle.position > file_handle.size) {
   		read_block_type_file(file_handle.block_num);
   		block.file.size = file_handle.position;
        file_handle.size = file_handle.position;
   		write_block_type_file(file_handle.block_num);
   	}


    IF_SERIAL_DEBUG(file_handle.print())
    IF_SERIAL_DEBUG(pdebug_P(PSTR("write: end\n")))

    return FS_STATUS_OK;

    #endif

}

FS_STATUS I2CFS::truncate(FILE_HANDLE& file_handle) {

    #ifdef READ_ONLY

    return FS_STATUS_ACESS_DENIED;

    #else

    if(!file_handle.block_num) return FS_STATUS_INVALID_HANDLE;

    FILE_ENTRY file_entry;

    read_block_type_file(file_handle.block_num);
    memcpy(&file_entry, &block.file, sizeof(FILE_ENTRY));
    truncate_file_entry(file_entry);
    file_handle_from_file_entry(file_handle, file_entry, 0);

    return FS_STATUS_OK;

    #endif
}

FS_STATUS I2CFS::format(uint16_t size_in_KB) { // 204 bytes

    #ifdef READ_ONLY

    return FS_STATUS_ACESS_DENIED;

    #else

    uint16_t total_blocks = size_in_KB << 4; // size_in_KB * 1024 / 64

	master_block.total_blocks     		= total_blocks; 
	master_block.used_blocks      		= 1;
	master_block.first_free_block 		= 1;
	master_block.first_file_block 		= 0;
	master_block.first_directory_block  = 0;

    save_master_block();

    uint16_t last_block=total_blocks - 1; 

	for (uint16_t i=1; i<=last_block; i++)
	{
		 block.free.next_free_block = (i == last_block ? 0 : i + 1);
		 write_block_type_free(i);
	}

	#endif

}

#ifdef SERIAL_DEBUG

const void MasterBlock::print(char op) const {
	char buffer[90];
	toString(buffer, sizeof(buffer));
	pdebug_P(PSTR("MasterBlock(%c): %s\n"), op, buffer);
}

const void MasterBlock::toString(char* buffer, uint8_t size_buf) const {

	snprintf_P(buffer, size_buf, 
		       PSTR("total: %u, used: %u, first_free: %u, first_file: %u, first_dir: %u"), 
		       total_blocks, used_blocks, 
		       first_free_block,
		       first_file_block, first_directory_block);
}

const void DataBlock::print(char op, BLOCK this_block) const {
	char buffer[32];
	toString(buffer, sizeof(buffer));
	pdebug_P(PSTR("DataBlock(%c): this: %u, %s\n"), op, this_block, buffer);
}

const void DataBlock::toString(char* buffer, uint8_t size_buf) const {

	snprintf_P(buffer, size_buf, 
		       PSTR("next_data_block: %u"), 
		       next_data_block);

}

const void DirectoryBlock::print(char op) const {
	char buffer[64];
	toString(buffer, sizeof(buffer));
	pdebug_P(PSTR("DirectoryBlock(%c): %s\n"), op, buffer);
}

const void DirectoryBlock::toString(char* buffer, uint8_t size_buf) const {

	snprintf_P(buffer, size_buf, 
		       PSTR("this: %u, previous: %u, next: %u, name: %s"), 
		       this_block, previous_dir_block, next_dir_block, name);

}

const void FileBlock::print(char op) const {
	char buffer[160];
	toString(buffer, sizeof(buffer));
	pdebug_P(PSTR("FileBlock(%c): %s\n"), op, buffer);
}

const void FileBlock::toString(char* buffer, uint8_t size_buf) const {

    
	snprintf_P(buffer, size_buf, 
		       PSTR("name: %s, attr: %i, this: %u, prev_file: %u, next_file: %u, parent_dir: %u, size: %lu, num_blocks: %lu, first_data: %u, last_data: %u"), 
		       name, attributes,
               this_block ,previous_file_block, next_file_block,
               parent_directory, size, num_data_blocks,
               first_data_block, last_data_block);
}

const void FreeBlock::print(char op, BLOCK this_block) const {
	char buffer[16];
	toString(buffer, sizeof(buffer));
	pdebug_P(PSTR("FreeBlock(%c): this: %u, %s\n"), op, this_block, buffer);
}

const void FreeBlock::toString(char* buffer, uint8_t size_buf) const {

	snprintf_P(buffer, size_buf, 
		       PSTR("next: %u"), 
		       next_free_block);

}

const void FILE_HANDLE::print() const {
	char buffer[160];
	toString(buffer, sizeof(buffer));
	pdebug_P(PSTR("FILE_HANDLE: %s\n"), buffer);
}

const void FILE_HANDLE::toString(char* buffer, uint8_t size_buf) const {

    
	snprintf_P(buffer, size_buf, 
		       PSTR("file_block: %i, position: %lu, next_data: %u, pos_in_block: %u, size: %lu, first_data: %u"), 
               block_num, position, next_data_block, position_in_block,
               size, first_data_block);
}

const void DIR_HANDLE::print() const {
	char buffer[160];
	toString(buffer, sizeof(buffer));
	pdebug_P(PSTR("DIR_HANDLE: %s\n"), buffer);
}

const void DIR_HANDLE::toString(char* buffer, uint8_t size_buf) const {

    
	snprintf_P(buffer, size_buf, 
		       PSTR("dir_block: %i, next_file: %u"), 
	           block_num, next_file_block);
}

#endif