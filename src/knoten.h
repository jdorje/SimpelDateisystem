#pragma once
#define MAX_BLOCKS 5859
#define NAME_SIZE 256
#define MAX_FILES_IN_DIR 30

typedef enum {
	FALSE,
	TRUE
} BOOL;


typedef struct fileControlBlock fileControlBlock;


typedef struct {

	int magicNum; //fat32, ext2 etc.
	int numInodes; //total number of inodes
	int numDataBlocks; // total number of data blocks
	int inodeStartIndex; //the index of the start of the first inode
	int datablockStartIndex; //the index of the start of the first data block
	int numInodesPerBlock;

	unsigned char ibmap[MAX_BLOCKS];
	unsigned char dbmap[MAX_BLOCKS];

} superblock;


struct fileControlBlock {

	char fileName[NAME_SIZE];
	long fileSize; // number of files in directory or size of actual file
	short numBlocks; //number of blocks used for data
	short inumber; //its own index number in the array of inodes
	short parent_inumber; //its parent's inumber
	char parentDir[NAME_SIZE]; //path of parent directory
	short mode; //can this file be read/written/executed?
	short uid; //who owns this file?
	char block[60]; //a set of disk pointers (15 total)
	long time; //what time was this file last accessed?


	fileControlBlock *dirContents[MAX_FILES_IN_DIR];
} ;



