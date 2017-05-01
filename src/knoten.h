#pragma once
#define MAX_BLOCKS 1024
#define NAME_SIZE 256
#define MAX_FILES_IN_DIR 30

typedef enum {
	

	IS_FILE,
	IS_DIR

} fileType;

typedef enum {
	
	NOT_USED,
	USED

} status;

typedef enum {
	
	TRUE,
	FALSE

} BOOL;


typedef struct fileControlBlock fileControlBlock;


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
	char parentDir[NAME_SIZE]; //name of parent directory
	fileType fileType;
	short mode; //can this file be read/written/executed?
	short uid; //who owns this file?
	char block[60]; //a set of disk pointers (15 total)
	long time; //what time was this file last accessed?

	//array of files/directories in the current directory
	// null if no files
	//TODO: CHANGE DIRCONTENTS TO FIXED ARRAY
	fileControlBlock *dirContents[MAX_FILES_IN_DIR];
	
} ;



fileControlBlock *findFileOrDir(const char *filePath, fileControlBlock *curr, BOOL isDir);
fileControlBlock *findRootOrDieTrying();
fileControlBlock *getParentFcb(fileControlBlock *child);

fileControlBlock *create_inode(fileType type, char *filePath);
int formatDisk(superblock *sBlock);
char *getRelativeParentName(char *filePath);
BOOL remove_inode(fileType type, char *filePath);
void initDirContents(fileControlBlock *fcb);

void showInodeNames();

int getFreeDataBlockNum();
int getFreeInodeNum();
int createAllInodes();