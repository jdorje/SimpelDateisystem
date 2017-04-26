#ifndef __KNOTEN_H__
#define __KNOTEN_H__


#define MAX_BLOCKS 1024
#define NAME_SIZE 256

typedef enum {
	

	IS_FILE,
	IS_DIR

} fileType;

typedef enum {
	
	USED,
	NOT_USED

} status;

typedef enum {
	
	TRUE,
	FALSE

} BOOL;


typedef struct fileControlBlock fileControlBlock;

typedef struct fcbNode fcbNode;

typedef struct {

	int magicNum; //fat32, ext2 etc.
	int numInodes; //total number of inodes
	int numDataBlocks; // total number of data blocks
	int inodeStartIndex; //the index of the start of the first inode
	
	status ibmap[MAX_BLOCKS];
	status dbmap[MAX_BLOCKS];

} superblock;


struct fileControlBlock {

	char fileName[NAME_SIZE];
	long fileSize; // number of files in directory or size of actual file
	char parentDir[NAME_SIZE];
	fileType fileType;
	short mode; //can this file be read/written/executed?
	short uid; //who owns this file?
	char block[60]; //a set of disk pointers (15 total)
	long time; //what time was this file last accessed?
	fcbNode *dirContents; //the file or directory linked list structure
	
} ;

struct fcbNode{

	fileControlBlock *fileOrDir; //current fcb in list
	fcbNode *head;
	fcbNode *next;

} ;


fileControlBlock *findFileOrDir(const char *filePath, fileControlBlock *curr, BOOL isDir);
fileControlBlock *findRootOrDieTrying();

int formatDisk(superblock *sBlock);
void showInodeNames();

int getFreeBlockNum();
int getFreeInodeNum();
int createAllInodes();

#endif
