#ifndef __KNOTEN_H__
#define __KNOTEN_H__


#define MAX_BLOCKS 512
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
	int parentDir;
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


typedef struct {

	long size; //how many bytes are in this file?
	long ctime; //what time was this file created?
	long mtime; //what time was this file last modified?
	long dtime; //what time was this inode deleted?
	short gid; //what group does this file belong to?
	short links_count; //how many hard links are there to this file?
	long blocks; //how many blocks have been allocated to this file?
	long flags; //how should ext2 use this inode?
	long osd1; //an OS-dependent field

	long generation; //file version (used by NFS)
	long file_acl; //a new permissions model beyond mode bits
	long dir_acl; //called access control lists
} iknoten;


fileControlBlock *findFileOrDir(const char *filePath, fileControlBlock *curr, BOOL isDir);
int formatDisk(superblock *sBlock);


#endif
