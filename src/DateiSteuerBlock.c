#include "params.h"
#include "block.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "knoten.h"
#include "dateisteuerblock.h"
#include "log.h"

extern fileControlBlock inodes[100];
extern char *diskFile;
extern superblock sBlockData;
extern superblock* sBlock;
extern int diskSize, diskfile;

/**
 * Input: /home/bob/file.txt, /hi.txt
 * Output: bob, /
 */
char *getRelativeParentName(const char *filePath)
{
	if (filePath != NULL) {
		char *fromLastSlash = strrchr(filePath, '/');
		if (fromLastSlash != NULL) {
			char *absoluteParentName = malloc(sizeof(char) * strlen(filePath));
			if (absoluteParentName != NULL) {

				int lenAbsParent = strlen(filePath) - strlen(fromLastSlash);
				if (lenAbsParent == 0) {
					char* r00t = malloc(sizeof(char)*2);
					if (r00t != NULL) { 
						strncpy(r00t, "/", 2);
						log_msg(" \n [getRelativeParentName] called on %s, returned %s \n", filePath, r00t);
						return r00t;
					} else {
						log_msg("\n [getRelativeParentName] 2 byte malloc failed, what is life \n");
					}
				} else {

					strncpy(absoluteParentName, filePath, lenAbsParent);
					char *fromLastSlashInAP = strrchr(absoluteParentName, '/');
					if (fromLastSlashInAP != NULL) {
						char* relativeParentName = malloc(sizeof(char) * strlen(fromLastSlashInAP));
						if (relativeParentName != NULL) {
							char* RelativeParentName = strcpy(relativeParentName, fromLastSlashInAP);
							log_msg(" \n [getRelativeParentName] called on %s, returned %s \n", filePath, RelativeParentName);
							return RelativeParentName;
						} else {
							log_msg(" \n [getRelativeParentName] could not malloc for relativeParentName\n");
						}
					} else {
						char* r00t = malloc(sizeof(char)*2);
						if (r00t != NULL) {
							strncpy(r00t, "/", 2);
							log_msg(" \n [getRelativeParentName] called on %s, returned %s \n", filePath, r00t);
							return r00t;
						} else {
							log_msg("\n [getRelativeParentName] (version 2) 2 byte malloc failed\n");
						}
					}
				}
			} else {
				log_msg("\n [getRelativeParentName] could not malloc for absoluteParentName \n");
			}
		} else {
			log_msg("\n [getRelativeParentName] Wrong file name, path always needs slash\n");
		} 
	} else {
		log_msg("\n [getRelativeParentName] filePath give nto getRelativeParentName is NULL\n");
	}

	log_msg("\n [getRelativeParentName] returning NULL \n");
	return NULL;
}

int getFreeInodeNum(superblock *sBlock)
{
	int i = 0;
	for(; i < sBlock->numInodes; i++){
		if(sBlock->ibmap[i] == NOT_USED) {
			log_msg("\n[getFreeInodeNum] returning inode number %d\n", i);
			return i;
		}
	}

	//no more free inodes
	return -1;
}

fileControlBlock *getParentFcb(fileControlBlock *child)
{
	if (child != NULL) {
		return &inodes[child->parent_inumber];
	}

	return NULL;
}

BOOL add_to_direntry(fileControlBlock* parent, fileControlBlock *child)
{
	if ((parent != NULL) && (child != NULL))
	{
		if (parent->dirContents == NULL) {
			log_msg("\n [add_to_direntry] adding new inode to head of parent directory contents \n");
			parent->dirContents[0] = child;
		} else {
			int i;
			for(i = 0; i < MAX_FILES_IN_DIR; i++) {
				if (parent->dirContents[i] == NULL) {
					parent->dirContents[i] = &inodes[i];
					break;
				}		
			}

			if (i > MAX_FILES_IN_DIR) {
				log_msg("\n [add_to_direntry] more than the maximum files allowed in this entry! \n");
				return FALSE;
			} else {
				log_msg("\n [add_to_direntry] replaced parent directory entry %d with new inode \n", i);
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL indexed_remove_from_direntry(fileControlBlock* parent, int dirContentIndex)
{
	int lastEntryInDirentry = -1;

	if (parent != NULL) {
		int i;
		for(i = 0; i < MAX_FILES_IN_DIR; i++) {
			if (parent->dirContents[i] != NULL) {
				i++;
			}
		}
		lastEntryInDirentry = i;

		if(lastEntryInDirentry != 0) {
			if (lastEntryInDirentry == dirContentIndex) {
				parent->dirContents[lastEntryInDirentry] = NULL;
				return TRUE;
			} else {
				parent->dirContents[dirContentIndex] = parent->dirContents[lastEntryInDirentry];
				parent->dirContents[lastEntryInDirentry] = NULL;
				return TRUE;
			}
		} else {
			log_msg("\n [indexed_remove_from_direntry] parent appears to be empty \n");
		}
	}

	return FALSE;
} 

BOOL remove_from_direntry(fileControlBlock* parent, fileControlBlock *child)
{
	if ((parent != NULL) && (child != NULL))
	{
		if (parent->dirContents != NULL) {
			int i;
			for(i = 0; i < MAX_FILES_IN_DIR; i++) {
				if (parent->dirContents[i] != NULL) {
					if (parent->dirContents[i]->inumber == child->inumber) {
						if (indexed_remove_from_direntry(parent, i)) {
							log_msg("\n [remove_from_direntry] removed %d from direntry \n", i);
							return TRUE;
						} else {
							log_msg("\n [remove_from_direntry] could not remove index number %d from direntry \n", i);
							return FALSE;
						}
					}
				}
			}

			if (i > MAX_FILES_IN_DIR) {
				log_msg("\n [remove_from_direntry] could not find child in parent direntry \n");
				return FALSE;
			} else {
				log_msg("\n [remove_from_direntry] removed child entry %d from parent array \n", i);
				return TRUE;
			}
		} else {
			log_msg("\n [remove_from_direntry] parent has no directory contents! cannot remove! \n");
			return FALSE;
		}
	}

	return FALSE;
}

// create_inode assumes it is given an absolute path, which it converts to relative
fileControlBlock *create_inode(fileType ftype, const char * path, mode_t mode)
{
	int i =1;
	fileControlBlock *parent = NULL;
	while(i < sBlock->numInodes)
	{
		//TODO ENSURE BMAPS ARE SET PROPERLY
		if(sBlock->ibmap[i]!=USED)
		{
			log_msg("\n [create_inode] free inode space found in index %d\n", i);

			char *pLastSlash = strrchr(path, '/');
			const char *relativeName = pLastSlash ? pLastSlash : path;

			memcpy(&inodes[i].fileName, relativeName, strlen(relativeName));
			inodes[i].fileSize = 0; //no data yet, still unknown

			char *parentName = getRelativeParentName(path);
			if (parentName != NULL) {
				memcpy(&inodes[i].parentDir, parentName, strlen(parentName));
				//find parent
				parent = findFileOrDir(parentName, TRUE);
				free(parentName);
			} else {
				log_msg("\n [create_inode] could not get relative parent name for %s\n", path);
			}	


			if(parent == NULL){
				log_msg("\n [create_inode] Could not find the parent file control block \n");
				return NULL;		
			} else {
				if (!add_to_direntry(parent, &inodes[i])) {
					log_msg("\n [create_inode] Could not add new inode to parent directory entry \n");
					return NULL;
				}
			}

			inodes[i].fileType = ftype;
			inodes[i].mode = mode;
			inodes[i].uid = getuid();
			inodes[i].time = time(NULL);
			inodes[i].inumber = i;
			inodes[i].parent_inumber = parent->inumber;
			sBlock->ibmap[i]=USED;
			return &inodes[i];
		}
		i++;
	}

	log_msg("\n [create_inode] could not find a free inode, searched %d out of %d \n", i, sBlock->numDataBlocks);
	return NULL;
}


BOOL remove_inode(fileType type, const char *filePath)
{
	log_msg("\n [remove_inode] on %s, currently doing nothing \n", filePath);

	fileControlBlock *f = findFileOrDir(filePath, type);
	if (f != NULL) {
		fileControlBlock *p = getParentFcb(f);
		if (p != NULL) {
			if (remove_from_direntry(p, f)) {
				sBlock->ibmap[f->inumber] = NOT_USED;
				return TRUE;
			} else {
				log_msg("\n [remove_inode] could not remove child %s from parent direntry \n", filePath);
			}
		} else {
			log_msg("\n [remove_inode] could not find parent inode for %s so cannot remove \n", filePath);
		}
	} else {
		log_msg("\n [remove_inode] could not find %s \n", filePath);
	}

	return FALSE;
}

void showInodeNames()
{
	int i = 0;
	for(i; i < 64; i++){

		if(inodes[i].fileName[0] == '\0'){
			log_msg("[showInodeNames] Inode Name [%d]: NULL \n", i );
		}
		else {
			log_msg("[showInodeNames] Inode Name [%d]: %s\n", i, inodes[i].fileName);
			log_msg("[showInodeNames] Inode Name [%d]: NULL\n", i );
		}
	}
}

fileControlBlock* findFileOrDir(const char *filePath, BOOL isDir)
{
	fileControlBlock* root = findRootOrDieTrying();
	if (root != NULL) {
		return findFileOrDirInternal(filePath, root, isDir);
	}

	log_msg("\n[findFileOrDir] could not find parent directory root\n");
	return NULL;
}

/*
 *  findFileOrDir ASSUMES YOU ARE GIVEN AN ABSOLUTE PATH NAME AND STRIPS IT DOWN TO RELATIVE NAME
 */

fileControlBlock *findFileOrDirInternal(const char *filePath, fileControlBlock *curr, BOOL isDir)
{
	if (strcmp(filePath, "/") == 0) {
		return findRootOrDieTrying();
	}

	int x = 0;
	while(x < sBlock->numInodes){
		fileControlBlock *currFCB = &inodes[x];
		char *pLastSlash = strrchr(filePath, '/');
		const char *relativeName = pLastSlash ? pLastSlash : filePath;

		if(strcmp(currFCB->fileName, relativeName) == 0){

			char* RelativeParentName = getRelativeParentName(filePath);
			if (RelativeParentName != NULL) {

				char* currInodeParentName = &currFCB->parentDir[0];
				if (currInodeParentName != NULL) {
					if (strcmp(currInodeParentName, RelativeParentName) == 0) {
						log_msg("\n [findFileOrDir] relativeName (%s) and RelativeParentName (%s) for inode %d match \n", currInodeParentName, RelativeParentName, x);
					} else {
						log_msg("\n [findFileOrDir] given relativeName (%s) and found relativeName (%s) do not match but returning finding anyway. \n", relativeName, RelativeParentName);
					}
				}

				free(RelativeParentName);
			} else {
				log_msg("\n [findFileOrDir] could not find relative parent name for %s \n", filePath);
			}

			return currFCB;
		}

		x++;
	}

	return NULL;

}

fileControlBlock *findRootOrDieTrying()
{
	fileControlBlock *root = &inodes[0];

	if (root == NULL) {
		log_msg("\n [findRootOrDieTrying] ROOT is NULL\n");
		return NULL;
	}

	return root;
}

void initDirContents(fileControlBlock *fcb) 
{
	int i = 0;
	while (i < MAX_FILES_IN_DIR) {
		fcb->dirContents[i] = NULL;
		i++;
	}
}

/* Initializes the disk to the default
 *  and appropriate values
 *
 */
int formatDisk(superblock *sBlock)
{
	//TODO: CALCULATE NUM INODES AT RUNTIME
	int remainingSpace = diskSize;
	log_msg("\n [formatDisk] remainingSpace == diskSize == %d \n", diskSize);

	//account for sblock space
	remainingSpace -= BLOCK_SIZE;
	int spaceUsed = 0;
	int currAllocation = 0;
	while(spaceUsed < remainingSpace){
		currAllocation += (BLOCK_SIZE * 15 + (BLOCK_SIZE / sizeof(fileControlBlock))	);

		if(currAllocation < remainingSpace){
			spaceUsed += currAllocation;
			sBlock->numInodes++;
			sBlock->numDataBlocks += 15;
		}
		else{
			break;
		}
	}

	// set up sBlock located in block 0
	sBlock->magicNum = 666;
	sBlock->inodeStartIndex = 1; // index of the first inode struct
	sBlock->ibmap[0] = USED;
	// set up root dir

	inodes[0].fileName[0] = '/';
	inodes[0].fileName[1] = '\0';
	inodes[0].fileSize = 0;
	inodes[0].parentDir[0] = '\0';
	inodes[0].fileType = IS_DIR;
	inodes[0].mode = S_IFDIR | 0755;

	inodes[0].uid = getuid();
	inodes[0].time = time(NULL);
	inodes[0].inumber = 0;
	inodes[0].parent_inumber = -1;
	initDirContents(&inodes[0]);

	//write to disk
	block_write_padded(0, sBlock, sizeof(superblock), 0);

	//no offset for the first fcb (aka root)
	block_write_padded(1, &inodes[0], sizeof(fileControlBlock), 0);
	// start at 1 to account for the root node being written!
	int currBlockFillNum = 1;
	int currBlock = 1;

	//TODO: FORMAT ALL INODES HERE!! THE ENTIRE ARRAY
	int i = 1;
	int endCond = sBlock->numInodes;
	for(; i < (endCond - 1); i++){
		fileControlBlock curr = inodes[i];
		//TODO ENSURE THESE FIELDS ARE CORRECT
		curr.fileName[0] = '\0';
		curr.fileSize = 0;
		curr.parentDir[0] = '\0';
		curr.inumber = i;
		curr.parent_inumber = -1;
		curr.fileType = IS_DIR;
		curr.mode = S_IFDIR;

		curr.uid = getuid();
		curr.time = time(NULL);
		initDirContents(&curr);

		sBlock->ibmap[i] = NOT_USED;

		block_write_padded( currBlock, &inodes[i], 
				sizeof(fileControlBlock), currBlockFillNum * sizeof(fileControlBlock));

		//NEED BLOCK WRITE HERE FOR THE INODES
		currBlockFillNum++;
		if(currBlockFillNum >= 6){
			currBlockFillNum = 0;
			currBlock++;
		} 

	}
}

