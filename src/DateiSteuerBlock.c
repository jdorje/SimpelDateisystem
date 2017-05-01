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
			char *absoluteParentName = calloc(sizeof(char) * strlen(filePath), sizeof(char));
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
						char* relativeParentName = calloc(sizeof(char)*strlen(fromLastSlashInAP)+1, sizeof(char));
						if (relativeParentName != NULL) {
							char* RelativeParentName = strcpy(relativeParentName, fromLastSlashInAP+1);
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
			log_msg("\n [getRelativeParentName] Wrong file name (%s), path always needs slash\n", filePath);
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
		if(sBlock->ibmap[i] == 0) {
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
					parent->dirContents[i] = child;
					break;
				}		
			}

			if (i > MAX_FILES_IN_DIR) {
				log_msg("\n [add_to_direntry] more than the maximum files allowed in this entry! \n");
				return FALSE;
			} else {
				log_msg("\n [add_to_direntry] replaced parent (%s)'s directory entry %d with new inode \n", parent->fileName, i);
				return TRUE;
			}
		}
	}

	return TRUE;
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
		lastEntryInDirentry = i-1;

		if(lastEntryInDirentry != -1) {
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
fileControlBlock *create_inode(const char * path, mode_t mode)
{
	int i =1;
	while(i < sBlock->numInodes)
	{

		if(sBlock->ibmap[i]!= 1)
		{
			log_msg("\n [create_inode] free inode space found in index %d\n", i);

			char *pLastSlash = strrchr(path, '/');
			const char *relativeName = pLastSlash ? pLastSlash+1 : path;
			fileControlBlock *parent = NULL;

			const char* parentDir = getRelativeParentName(path);
			if (parentDir != NULL) {
				if (strcmp(parentDir, "/") == 0) {
					parent = findRootOrDieTrying();
				} else {
					char* absolutePathToParent = calloc(sizeof(char)*strlen(path), sizeof(char));
					if (absolutePathToParent != NULL) {
						strncpy(absolutePathToParent, path, strlen(path)-strlen(relativeName)-1);
						parent = findFileOrDir(absolutePathToParent);
					} else {
						log_msg("\n [create_inode] calloc fail \n");
					}
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
			} else {
				log_msg(" \n [create_inode] could not find parent name \n");
				return NULL;
			}

			log_msg("\n [create_inode] creating inode of name %s, parent dir %s \n", relativeName, parentDir);
			strcpy(inodes[i].fileName, relativeName);
			strcpy(inodes[i].parentDir, parentDir);
			inodes[i].parent_inumber = parent->inumber;
			inodes[i].mode = mode;
			inodes[i].uid = getuid();
			inodes[i].time = time(NULL);
			inodes[i].inumber = i;
			inodes[i].parent_inumber = parent->inumber;
			sBlock->ibmap[i] = 1;

			//write to disk
			flushAllInodesTodisk(FALSE);

			return &inodes[i];

		}

		i++;
	}

	log_msg("\n [create_inode] could not find a free inode, searched %d out of %d \n", i, sBlock->numDataBlocks);
	return NULL;
}


BOOL remove_inode(const char *filePath)
{
	fileControlBlock *f = findFileOrDir(filePath);
	if (f != NULL) {
		fileControlBlock *p = getParentFcb(f);
		if (p != NULL) {
			if (remove_from_direntry(p, f)) {
				sBlock->ibmap[f->inumber] = 0;
				//write to disk
				flushAllInodesTodisk(FALSE);

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

/*
 *  findFileOrDir ASSUMES YOU ARE GIVEN AN ABSOLUTE PATH NAME AND STRIPS IT DOWN TO RELATIVE NAME
 */

fileControlBlock *findFileOrDir(const char *filePath)
{
	if (strcmp(filePath, "/") == 0) {
		return findRootOrDieTrying();
	} else {
		char *pLastSlash = strrchr(filePath, '/');
		const char *relativeName = pLastSlash ? pLastSlash+1 : filePath;

		int x;
		for(x = 0; x < sBlock->numInodes; x++){
			fileControlBlock *currFCB = &inodes[x];
			if(strcmp(currFCB->fileName, relativeName) == 0){
				char* RelativeParentName = getRelativeParentName(filePath);
				if (RelativeParentName != NULL) {
					char* currInodeParentName = &currFCB->parentDir[0];
					if (currInodeParentName != NULL) {
						if (strcmp(currInodeParentName, RelativeParentName) == 0) {
							log_msg("\n [findFileOrDir] given and found parent relative names (%s, %s) for inode %d match \n", RelativeParentName, currInodeParentName, x);
						} else {
							log_msg("\n [findFileOrDir] given and found parent relative names (%s, %s) do not match. \n", RelativeParentName, currInodeParentName);
							free(RelativeParentName);
							continue;
						}
					}

					free(RelativeParentName);
				} else {
					log_msg("\n [findFileOrDir] could not find relative parent name for %s \n", filePath);
				}

				return currFCB;
			}
		}
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
	int remainingSpace = diskSize;
	log_msg("\n [formatDisk] remainingSpace == diskSize == %d \n", diskSize);

	//account for sblock space
	remainingSpace -= BLOCK_SIZE;
	int spaceUsed = 0;
	int currAllocation = 0;
	sBlock->numInodesPerBlock = (BLOCK_SIZE / sizeof(fileControlBlock));
	sBlock->inodeStartIndex = (int) (1 + ( sizeof(superblock) / BLOCK_SIZE) );

	// dynamically allocate to account for 15 blocks per inode
	// set the rest of the sblock values accordingly
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
	sBlock->ibmap[0] = 1;
	// set up root dir

	inodes[0].fileName[0] = '/';
	inodes[0].fileName[1] = '\0';
	inodes[0].fileSize = 0;
	inodes[0].parentDir[0] = '\0';
	inodes[0].mode = S_IFDIR | 0755;

	inodes[0].uid = getuid();
	inodes[0].time = time(NULL);
	inodes[0].inumber = 0;
	inodes[0].parent_inumber = -1;
	initDirContents(&inodes[0]);

	//write sblock to disk
	sBlock_write_padded(0, sBlock, sizeof(superblock));
	
	//TODO ENSURE ALL BLOCK_WRITES FOR INODES START AT INODESTARTINDEX
	//no offset for the first fcb (aka root)
	block_write_padded(sBlock->inodeStartIndex, &inodes[0], sizeof(fileControlBlock), 0);

	if(flushAllInodesTodisk(TRUE) != TRUE)	
		log_msg("\n There was a problem trying to set inodes to disk \n");

	return 1;
}

BOOL flushAllInodesTodisk(BOOL firstTime)
{
	if (firstTime == TRUE) {
		int currBlockFillNum = 1, currBlock = sBlock->inodeStartIndex;
		int i = 1;
		int endCond = sBlock->numInodes;

		for(; i < (endCond-1); i++) {
			fileControlBlock curr = inodes[i];

			curr.fileName[0] = '\0';
			curr.fileSize = 0;
			curr.parentDir[0] = '\0'; //TODO CHANGE ALL PARENT DIR TO ACCOUNT FOR PARENT INUMBERS
			curr.inumber = i;
			curr.parent_inumber = -1;
			curr.mode = S_IFDIR | 0755;
			curr.uid = getuid();
			curr.time = time(NULL);
			initDirContents(&curr);
			sBlock->ibmap[i] = 0;

			block_write_padded(currBlock, &inodes[i], sizeof(fileControlBlock),
				 currBlockFillNum * sizeof(fileControlBlock));

			//Need blockwrite here for inodes
			currBlockFillNum++;
			if (currBlockFillNum >= sBlock->numInodesPerBlock) {
				currBlockFillNum = 0;
				currBlock++;
			}
		}

		return TRUE;

	} else {
			//write sblock to disk
			sBlock_write_padded(0, sBlock, (BLOCK_SIZE * sBlock->inodeStartIndex) );
	
		   int currBlockFillNum = 0, currBlock = sBlock->inodeStartIndex;
		   int i = 0;
		   int endCond = sBlock->numInodes;


		   for(; i < (endCond-1); i++){
			   fileControlBlock curr = inodes[i];
			   block_write_padded(currBlock, &inodes[i], sizeof(fileControlBlock), 
					currBlockFillNum * sizeof(fileControlBlock));

			   currBlockFillNum++;
			   if (currBlockFillNum >= sBlock->numInodesPerBlock) {
				   currBlockFillNum = 0;
				   currBlock++;
			   }
   			}
		   

		log_msg("\n [flushAllInodesTodisk] was bugging out so I'm not doing anything for now \n");
		return TRUE;
	}

	return FALSE;
}
