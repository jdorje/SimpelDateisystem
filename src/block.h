/*
  Copyright (C) 2015 CS416/CS516

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef _BLOCK_H_
#define _BLOCK_H_

#define BLOCK_SIZE 4096

void disk_open(const char* diskfile_path);
void disk_close();
int block_read(const int block_num, void *buf);
int block_write(const int block_num, const void *buf);
int inode_read(const int block_num, void* buf, int offset);
int sBlock_write_padded(const int block_num, const void *buf, int size);
int block_write_padded(const int block_num, const void *buf, int size, int offset);
int sBlock_read(const int block_num, void *buf, int size);



#endif
