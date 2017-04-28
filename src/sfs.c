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

fileControlBlock inodes[100];
char *diskFile;
superblock sBlockData;
superblock *sBlock = &sBlockData;


void *sfs_init(struct fuse_conn_info *conn)
{
	fprintf(stderr, "in bb-init\n");


	//log_conn(conn);
	//log_fuse_context(fuse_get_context());
	disk_open(diskFile);
	char buf[BLOCK_SIZE];

	int ret = block_read(0, buf);
	log_msg("\n First block read result: %d \n", ret);
	
	// Create everything from the start
	if(ret == 0){
		int x = 1024 * 1024 - 1;
        FILE *fp = fopen("../output.file", "w");
        fseek(fp, x , SEEK_SET);
        fputc('\0', fp);
        
		formatDisk(sBlock);

	}
	// see if filesystem already exists
	else if( ret > 0){

		log_msg("\n Superblock loaded...\n");
		memcpy(sBlock, buf, BLOCK_SIZE);
		// check if file system matches ours
		if(sBlock->magicNum == 666){
			log_msg("\n Magic number MATCHES OURS.\n");
		}
		//convert to our file system
		else {
			log_msg("\n Magic number does not match.\n");
			log_msg("\n The number received was: %d\n", sBlock->magicNum);

			//TODO impl method to convert fs to match ours
		}

		// read in root dir
		if( inode_read(1, &inodes[0], 0) < 0)
			log_msg("\n Problem reading root dir! \n");

		//i is equal to 1 since root is already created
		int i = 1;
		int endCond = sBlock->numInodes;
		// each block can fit 6 inodes
		int currBlockFillNum = 1;
		int currBlock = 1;
		while(i < endCond){

			int isSucc = inode_read(currBlock, &inodes[i], 
				currBlockFillNum * sizeof(fileControlBlock) );
			//log_msg("\n Reading inodes...\n");

			if(isSucc){
				//check types of files, ex. data vs directory
				fileControlBlock fcb = inodes[i];

				//check if dir 

				if(fcb.fileType == IS_DIR){

					//log_msg("\n dir found.\n");
					//block_write_padded(i, &fcb, sizeof(fileControlBlock));			
				} 
				//check if we have file
				else if(fcb.fileType == IS_FILE){

					//log_msg("\n file found.\n");

					// set the data bmap
					if(fcb.fileSize < 1)
						sBlock->dbmap[i] = NOT_USED;
					else
						sBlock->dbmap[i] = USED;

					//block_write_padded(i, &fcb, sizeof(fileControlBlock));			

				}

				currBlockFillNum++;
				if(currBlockFillNum >= 6){
					currBlockFillNum = 0;
					currBlock++;
				} 


			} else {


				//log_msg("\n Error trying to block read inode[].\n");

			}

			i++;
		}


	} else{

		log_msg("\n Error trying to initiate disk! \n");
	}

	return SFS_DATA;
}

int formatDisk(superblock *sBlock)
{
	log_msg("\n Creating file system from scratch...\n");

	// set up sBlock located in block 0
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
	inodes[0].mode = S_IFDIR | 0755;

	inodes[0].uid = getuid();
	inodes[0].time = time(NULL);
	inodes[0].dirContents = NULL;

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
		curr.fileType = IS_DIR;
		curr.mode = S_IFDIR;

		curr.uid = getuid();
		curr.time = time(NULL);
		curr.dirContents = NULL;

		block_write_padded( currBlock, &inodes[i], 
			sizeof(fileControlBlock), currBlockFillNum * sizeof(fileControlBlock));

		//NEED BLOCK WRITE HERE FOR THE INODES
		currBlockFillNum++;
		if(currBlockFillNum >= 6){
			currBlockFillNum = 0;
			currBlock++;
		} 

	}

	//********************************


	block_write_padded(0, sBlock, sizeof(superblock));
	block_write_padded(1, &inodes[0], sizeof(fileControlBlock));

	sBlock->ibmap[0]=USED;

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
		statbuf->st_mode = (strcmp(path,"/")==0) ? inodes[0].mode : S_IFREG | 0666;
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

			log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x);",
					path, statbuf);
			retstat = -1;
			//	errno = ENOENT;
			//return retstat;
			return -ENOENT;
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

	
	char *pLastSlash = strrchr(filePath, '/');
     char *fileBaseName = pLastSlash ? pLastSlash + 1 : filePath;

	int x = 0;
	while(found == FALSE && x < sBlock->numInodes){


		fileControlBlock *currFCB = &inodes[x];
		// found the file name
		if(strcmp(currFCB->fileName, fileBaseName) == 0){

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
					return findFileOrDir(filePath + strlen(fileBaseName), currFCB, isDir);


				} else if(currFCB->fileType == IS_FILE){

					//error file name in middle of path name
					log_msg("\nENOTDIR: A component in the path prefix is not a directory.\n");

				}	

			}


		} 
		// temp name did not match, search current directory
		else {

			fileControlBlock *root = findRootOrDieTrying();


			fileControlBlock *parent = findFileOrDir(currFCB->parentDir, root, TRUE);
			fileControlBlock *currNode = parent->dirContents[0]; //NEED TO ACCOUNT FOR FILES BC DIRCONTENTS WILL BE NULL

			int i = 0;
			while(currNode != NULL){

				if(strcmp(fileBaseName, currNode->fileName) == 0){
					// recursively search subdirectories
					return currFCB;
				}
				
				i++;
				currNode = parent->dirContents[i];
			
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

		if(inode == NULL)
		{

			create_inode(IS_FILE ,path);			

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
	int retstat = -1;
	log_msg("sfs_unlink(path=\"%s\")\n", path);

	fileControlBlock *root = findRootOrDieTrying();
	if(root != NULL) {
		fileControlBlock *fcbToUnlink = findFileOrDir(path, root, FALSE);
		if (fcbToUnlink != NULL) {
			//unlink from the linked list structure
			char* parentDname = &(fcbToUnlink->parentDir[0]);
			if (parentDname != NULL) {
				fileControlBlock *parentDir = findFileOrDir(parentDname, root, TRUE);

				if ((parentDir != NULL) && (parentDir->dirContents[0] != NULL)) {


					// New code, just use array properties
					int i = 0;
					// find where parent points to the unlinked node
					while (parentDir->dirContents[i] != fcbToUnlink) {
						
						parentDir->dirContents[i] = NULL;

					}


					//TODO: free block data.
					//TODO: update the inode bitmap entry


				} else {
					log_msg("cannot get a handle on the parent directory\n OR parentDir is null so I can't get head");
					errno = EIO;
				}
			} else {
				log_msg("no name for parent directory?");
				errno = EIO;
			}
		} else {
			log_msg("unable to find file you want to remove");
			errno = ENOENT;
		}
	} else {
		errno = EIO;
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
	int bytes_written = -1;
	char buffaway[BLOCK_SIZE];
	log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
			path, buf, size, offset, fi);

	fileControlBlock *root = findRootOrDieTrying();
	if (root != NULL) {
		fileControlBlock *fc = findFileOrDir(path, root, FALSE);
		if (fc != NULL) {
			int i = 0;
		} else {
			log_msg("\n cannot find the file \n");
			errno = ENOENT;
		}
	} else {
		log_msg("\n cannot find root dir, uhhhh? \n");
	}

	return bytes_written;
}




fileControlBlock *create_inode(fileType ftype, char * path)
{
	int i =0;
	while(i < sBlock->numDataBlocks)
	{
		// free inode space found!
		if(sBlock->ibmap[i]!=USED)
		{

			memcpy(&inodes[i].fileName, path, strlen(path));
			inodes[i].fileSize = 0; //no data yet, still unknown

			//parent dir is string between previous set of slashes
			char curr = *( path + strlen(path) - 1);
			BOOL slashFound = FALSE;
			int offset = 0;
			while(slashFound = FALSE){
	
				if(curr == '/' || curr == '\0')
					slashFound == TRUE;

				offset++;
				curr = *(path + strlen(path) - offset);

			}

			char *temp;
			memcpy(temp, path, strlen(path) - offset );
			// accounts for slashes
			memcpy(&inodes[i].parentDir, path, strlen(path) - offset );
			//find parent
			fileControlBlock *parent = findFileOrDir(temp, findRootOrDieTrying(), TRUE);

			//check dirContents if empty, if so add to head
			if(parent->dirContents == NULL){
				//TODO malloc the pointers
				parent->dirContents[0] = &inodes[i];
				parent->dirContents[1] = NULL;
			} else {
				
				//find next free space in array
				int x = 0;
				for(; x < MAX_FILES_IN_DIR; x++){

					if(parent->dirContents[x] == NULL){
						//TODO malloc the pointers
						parent->dirContents[x] = &inodes[i];
						parent->dirContents[x + 1] = NULL;
					}

				}

			}

			//set rest of inode fields
			inodes[i].fileType = ftype;

			switch(ftype)
			{
				case IS_DIR:
					inodes[i].mode = S_IFDIR | 0755;
					break;
				case IS_FILE:
					inodes[i].mode = S_IFREG | 0666;
					break;
				default:
					log_msg("\n What are you doing, file type is not correct! \n");
					inodes[i].mode = S_IFREG | 0666;
			}

			inodes[i].uid = getuid();
			inodes[i].time = time(NULL);
			//inodes[i].dirContents = currNode; set at line 719
			sBlock->ibmap[i]=USED;
			return &inodes[i];
		}
			
		i++;


	}

	return NULL;


}

/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
	int retstat = 0;
	log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
			path, mode);

	create_inode(IS_DIR, path);


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
	
	// end condition if true
	if(directory->dirContents == NULL)
		return retstat;

	fileControlBlock *curr = directory->dirContents[0];
	int i = 1;
	while(curr != NULL){
		log_msg("\n inLoop: filler on \n", curr->fileName);
		filler(buf, curr->fileName, NULL, 0);
		curr = directory->dirContents[i];
	}

	return retstat;
}


void showInodeNames(){

	int i = 0;
	for(i; i < 64; i++){

		if(inodes[i].fileName[0] == '\0'){
			log_msg("Inode Name [%d]: NULL \n", i );
		}
		else {
			log_msg("Inode Name [%d]: %s\n", i, inodes[i].fileName);
			log_msg("Inode Name [%d]: NULL\n", i );
		}
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
