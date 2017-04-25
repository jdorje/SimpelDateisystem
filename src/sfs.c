/*
   Simple File System

   This code is derived from function prototypes found /usr/include/fuse/fuse.h
   Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
   His code is licensed under the LGPLv2.

*/

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
#include "log.h"


///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */

//TODO superblock (4KB)
//TODO ibitmap (4KB) and data bitmap (4KB)

fileControlBlock inodes[64];
char *diskFile;
superblock *sBlock;

void *sfs_init(struct fuse_conn_info *conn)
{
	fprintf(stderr, "in bb-init\n");
	log_msg("\nsfs_init()\n");


	//log_conn(conn);
	//log_fuse_context(fuse_get_context());
	disk_open(diskFile);
	char buf[BLOCK_SIZE];
	sBlock = malloc(sizeof(superblock));

	int ret = block_read(0, buf);
	log_msg("\n First block read result: %d \n", ret);
	//check if superblock is created, if not create one
	if( ret != 0){

		log_msg("\n Superblock loaded...\n");
		memcpy(sBlock, buf, BLOCK_SIZE);
		// check if file system matches ours
		if(sBlock->magicNum == 666){
			log_msg("\n Magic number MATCHES OURS.\n");
			formatDisk(sBlock);
		}
		//convert to our file system
		else {
			log_msg("\n Magic number does not match.\n");
			log_msg("\n The number received was: %d\n", sBlock->magicNum);
			formatDisk(sBlock); //NEED TO CHANGE METHOD TO INCLUDE FLAG TO CONVERT TO OUR FS
		}

		//check if root dir
		if(strcmp(inodes[1].fileName, "/") == 0){


		}


		int i = 1;
		for(; i < 20; i++){

			int isSucc = block_read(i, &inodes[i]);
			//log_msg("\n Reading inodes...\n");

			if(isSucc){
				//check types of files, ex. data vs directory
				fileControlBlock fcb = inodes[i];

				//check if dir 

				if(fcb.fileType == IS_DIR){

					//log_msg("\n dir found.\n");
					block_write_padded(i, &fcb, sizeof(fileControlBlock));			
				} 
				//check if we have file
				else if(fcb.fileType == IS_FILE){

					//log_msg("\n file found.\n");

					// set the data bmap
					if(fcb.fileSize < 1)
						sBlock->dbmap[i] = NOT_USED;
					else
						sBlock->dbmap[i] = USED;

					block_write_padded(i, &fcb, sizeof(fileControlBlock));			

				}



			} else {


				//log_msg("\n Error trying to block read inode[].\n");

			}


		}


	} 
	// Create everything from the start
	else {

		formatDisk(sBlock);

	}

	//use block read to 
	showInodeNames();
	return SFS_DATA;
}

int formatDisk(superblock *sBlock)
{
	log_msg("\n Creating file system from scratch...\n");
	sBlock->magicNum = 666;
	sBlock->numInodes = 64;
	sBlock->numDataBlocks = 45000; //TODO: CALCULATE EXACT NUMBER OF BLOCKS USING THE SIZE OF THE STRUCTS
	sBlock->inodeStartIndex = 1; // index of the first inode struct


	// set up root dir

	inodes[0].fileName[0] = '/';
	inodes[0].fileName[1] = '\0';
	inodes[0].fileSize = 0;
	inodes[0].parentDir[0] = '\0';
	inodes[0].fileType = IS_DIR;
	inodes[0].mode = S_IFDIR;

	inodes[0].uid = getuid();
	inodes[0].time = time(NULL);
	inodes[0].dirContents = NULL;

	//TODO: FORMAT ALL INODES HERE!! THE ENTIRE ARRAY

	//********************************

	block_write_padded(0, sBlock, sizeof(superblock));
	block_write_padded(1, &inodes[0], sizeof(fileControlBlock));
	/* TODO: lookup these fields and assign to root dir appropriately
	   short; //can this file be read/written/executed?
	   char block[60]; //a set of disk pointers (15 total)
	   long time; //what time was this file last accessed?
	   */
}


/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{
	log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int sfs_getattr(const char *path, struct stat *statbuf)
{
	int retstat = 0;
	char fpath[PATH_MAX];

	// check if path name is too long
	if(strlen(path) > PATH_MAX) {
		log_msg("\nName too long for me, oops\n");
		errno = ENAMETOOLONG;
		return -1;
	}

	if (statbuf == NULL) {
		log_msg("\nWhat are you doing, statbuf is null\n");
		errno = EIO;
		return -1;
	}

	if(strcmp(path, "/.Trash") == 0 ||
			(
			 strncmp(path, "/.Trash-", 8) == 0) &&
			(strlen(path) == 13) || // It is /.Trash-XXXXX
		(strcmp(path, "/") == 0)
	  )
	{
		statbuf->st_dev = 0;
		statbuf->st_ino = 0;
		statbuf->st_mode = (strcmp(path,"/")==0) ? inodes[0].mode : S_IFREG;
		statbuf->st_nlink = 0;
		statbuf->st_uid = inodes[0].uid;
		statbuf->st_gid = getgid();
		statbuf->st_rdev = 0;
		statbuf->st_size = inodes[0].fileSize;
		statbuf->st_atime = inodes[0].time;
		statbuf->st_mtime = 0;
		statbuf->st_ctime = 0;
		statbuf->st_blksize = BLOCK_SIZE; // IS THIS THE PREFERRED I/O BLOCK SIZE??
		statbuf->st_blocks = 0;
		retstat = 0;
	} else {


		// find file 
		fileControlBlock *fileHandle = findFileOrDir(path, &inodes[0], FALSE);
		if(fileHandle == NULL){

			log_msg("\nEIO\n");
			log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
					path, statbuf);
			retstat = -1;
			errno = ENOENT;
			return retstat;

		}

		//modify attributes in statbuf according to the fileHandle
		//meaningless fields will be set to 0
		statbuf->st_dev = 0;
		statbuf->st_ino = 0;
		statbuf->st_mode = 0;
		statbuf->st_nlink = 0;
		statbuf->st_uid = fileHandle->uid;
		statbuf->st_gid = getgid();
		statbuf->st_rdev = 0;
		statbuf->st_size = fileHandle->fileSize;
		statbuf->st_atime = fileHandle->time;
		statbuf->st_mtime = 0;
		statbuf->st_ctime = 0;
		statbuf->st_blksize = BLOCK_SIZE; // IS THIS THE PREFERRED I/O BLOCK SIZE??
		statbuf->st_blocks = 0;

	}


	log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
			path, statbuf);

	return retstat;	
}

/*
 *  Finds the specific file control block for the given
 *   file path passed in the parameter
 */

fileControlBlock *findFileOrDir(const char *filePath, fileControlBlock *curr, BOOL isDir){

	// check for valid path length
	
	if (strcmp(filePath, "/") == 0) {
		return findRootOrDieTrying();
	}
	
	if(strlen(filePath) <  2) 
		log_msg("\nEIO\n");

	BOOL found = FALSE;
	BOOL lastToken = FALSE;
	char *temp = malloc(sizeof(char) * strlen(filePath));
	//add one to account for forward slash
	char compareChar = *(filePath + 1);
	int offset = 1;

	while(compareChar != '/'){

		if(compareChar == '\0'){
			lastToken = TRUE;
			break;
		}

		offset++;
		compareChar = *(filePath + offset);


	}

	//copy temp name to temp string
	memcpy(temp, filePath, sizeof(char) * offset);

	int x = 0;
	while(found == FALSE && x < sBlock->numInodes){


		fileControlBlock *currFCB = &inodes[x];
		// found the file name
		if(strcmp(currFCB->fileName, temp) == 0){

			if(lastToken == TRUE){
				//check if dir or not
				if(currFCB->fileType == IS_DIR && isDir == TRUE){
					found = TRUE;
					return currFCB;
				} else if(currFCB->fileType == IS_FILE && isDir == FALSE){

					//end condition HERE, this is what we want
					found = TRUE;
					return currFCB;

				}	

			} else if(lastToken == FALSE){
				//check if dir or not

				if(currFCB->fileType == IS_DIR){


					// recursively search subdirectories
					// will use the fcbNodes
					findFileOrDir(filePath + strlen(temp), currFCB, isDir);


				} else if(currFCB->fileType == IS_FILE){

					//error file name in middle of path name
					log_msg("\nENOTDIR: A component in the path prefix is not a directory.\n");

				}	



			}


		} 
		// temp name did not match, search current directory
		else {


			fileControlBlock *tempFCB = curr;

			fcbNode *currNode = tempFCB->dirContents; //NEED TO ACCOUNT FOR FILES BC DIRCONTENTS WILL BE NULL


			while(currNode != NULL){


				if(strcmp(temp, currNode->fileOrDir->fileName) == 0){
					// recursively search subdirectories
					// will use the fcbNodes
					findFileOrDir(filePath + strlen(temp), currNode->fileOrDir, isDir);

				}

				currNode = currNode->next;
			}



		}


		x++;
	}

	log_msg("\nENOENT: Component does not exist or is empty string.\n");
	return NULL;

}

fileControlBlock *findRootOrDieTrying()
{
	fileControlBlock *root = &inodes[0];
	if (root == NULL) {
		log_msg("\n ROOT is NULL\n");
		return NULL;
	}

	return root;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
			path, mode, fi);

	// find inode with the specific path
	// if it does not exist, create an inode for it
	fileControlBlock *root = findRootOrDieTrying();
	if (root != NULL) {

		fileControlBlock *inode = findFileOrDir(path, root, FALSE);

		if(inode == NULL){

			int i = 0;
			while(i < sBlock->numDataBlocks){

				// free inode space found!
				if(inodes[i].fileName != '\0'){

					memcpy(&inodes[i].fileName, path, strlen(path));
					inodes[i].fileSize = 0; //no data yet, still unknown

					//parent dir is string between previous set of slashes
					int firstSlash;
					int secondSlash;
					char curr = *path;
					int numSlashesFound = 0;
					int index = 0;
					while(numSlashesFound != 2){

						if(curr == '/' && numSlashesFound == 0){
							numSlashesFound++;
							firstSlash = index;
						}
						else if(curr == '/' && numSlashesFound == 1){
							numSlashesFound++;
							secondSlash = index;
						}

						curr = *(path + index);
						index++;

					}

					// accounts for slashes
					memcpy(&inodes[i].parentDir, (path + firstSlash + 1), (secondSlash - 1) );

					//handle linked list structure
					fcbNode *prev = inodes[i - 1].dirContents;
					fcbNode *currNode = inodes[i].dirContents;
					prev->next = currNode;
					currNode->fileOrDir = &inodes[i];
					currNode->next = NULL;
					//do we need to set head here!?

					//set rest of inode fields
					inodes[i].fileType = IS_FILE;
					inodes[i].mode = S_IFREG;
					inodes[i].uid = getuid();
					inodes[i].time = time(NULL);
					inodes[i].dirContents = currNode;

				}


			}



		} 
		// open the inode
		else {
			//TODO: how to open a file??


		}
	}

	return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
	int retstat = 0;
	log_msg("sfs_unlink(path=\"%s\")\n", path);

	fileControlBlock *root = findRootOrDieTrying();
	if(root != NULL) {
		fileControlBlock *fcbToUnlink = findFileOrDir(path, root, FALSE);
		if (fcbToUnlink != NULL) {
			//free block data.
			//update the inode bitmap entry
		} else {
			log_msg("unable to find file you want to remove");
			retstat = -1;
			errno = ENOENT;
		}
	} else {
		errno = EIO;
		retstat = -1;
	}

	return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
			path, fi);


	return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
			path, fi);


	return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
			path, buf, size, offset, fi);


	return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
			path, buf, size, offset, fi);


	return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
	int retstat = 0;
	log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
			path, mode);


	return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
	int retstat = 0;
	log_msg("sfs_rmdir(path=\"%s\")\n",
			path);


	return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;
	log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
			path, fi);


	return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
		struct fuse_file_info *fi)
{
	int retstat = 0;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	fileControlBlock *root = &inodes[0];

	fileControlBlock *directory = findFileOrDir(path, root, TRUE);
	if(directory == NULL){
		log_msg("\n Could not find directory using path: %s \n", path);
		return -1;
	}

	fcbNode *curr = directory->dirContents;

	while(curr != NULL){
		log_msg("\n inLoop: filler on \n", curr->fileOrDir->fileName);
		filler(buf, curr->fileOrDir->fileName, NULL, 0);
		curr = curr->next;
	}

	return retstat;
}


void showInodeNames(){

	int i = 0;
	for(i; i < 64; i++){
		log_msg("Inode Name: %s\n", inodes[i].fileName);
	}
}
/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
	int retstat = 0;


	return retstat;
}

struct fuse_operations sfs_oper = {
	.init = sfs_init,
	.destroy = sfs_destroy,

	.getattr = sfs_getattr,
	.create = sfs_create,
	.unlink = sfs_unlink,
	.open = sfs_open,
	.release = sfs_release,
	.read = sfs_read,
	.write = sfs_write,

	.rmdir = sfs_rmdir,
	.mkdir = sfs_mkdir,

	.opendir = sfs_opendir,
	.readdir = sfs_readdir,
	.releasedir = sfs_releasedir
};

void sfs_usage()
{
	fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
	abort();
}

int main(int argc, char *argv[])
{
	int fuse_stat;
	struct sfs_state *sfs_data;

	// sanity checking on the command line
	if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
		sfs_usage();

	sfs_data = malloc(sizeof(struct sfs_state));
	if (sfs_data == NULL) {
		perror("main calloc");
		abort();
	}

	// Pull the diskfile and save it in internal data
	sfs_data->diskfile = argv[argc-2];
	diskFile = argv[argc - 2];
	argv[argc-2] = argv[argc-1];
	argv[argc-1] = NULL;
	argc--;

	sfs_data->logfile = log_open();

	// turn over control to fuse
	fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
	fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
	fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

	return fuse_stat;
}
