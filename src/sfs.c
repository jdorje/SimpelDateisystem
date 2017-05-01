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
#include "dateisteuerblock.h"
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
int diskSize;

void *sfs_init(struct fuse_conn_info *conn)
{
	fprintf(stderr, "in bb-init\n");


	//log_conn(conn);
	//log_fuse_context(fuse_get_context());
	disk_open(diskFile);
	char buf[BLOCK_SIZE];
	struct stat st;
	stat(diskFile, &st);
	diskSize = st.st_size;

	int ret = block_read(0, buf);
	log_msg("\n [sfs_init] First block read result: %d \n", ret);
	if(ret == 0){
		log_msg("\n [sfs_init] Did not block read anything so creating from the start. \n");
		formatDisk(sBlock);

	} else if( ret > 0){
		log_msg("\n [sfs_init] Found something but formatting disk anyway. \n");
		formatDisk(sBlock);
		memcpy(sBlock, buf, BLOCK_SIZE);
		log_msg("\n [sfs_init] Loaded disk contents (from before formatting) into sBlock. \n");
		// check if file system matches ours
		if(sBlock->magicNum == 666){
			log_msg("\n [sfs_init] Magic number MATCHES OURS.\n");
		}
		else {
			log_msg("\n [sfs_init] Magic number %d does not match ours.\n", sBlock->magicNum);
			//TODO impl method to convert fs to match ours
		}

		// read in root dir
		if( inode_read(1, &inodes[0], 0) < 0) {
			log_msg("\n [sfs_init] Problem reading root dir! \n");
		}

		//i is equal to 1 since root is already created
		int i = 1;
		int endCond = sBlock->numInodes;
		log_msg("\n [sfs_init] endCond == sBlock->numInodes == %d \n", sBlock->numInodes);
		// each block can fit 6 inodes
		int currBlockFillNum = 1;
		int currBlock = 1;
		while(i < endCond){

			int isSucc = inode_read(currBlock, &inodes[i], 
					currBlockFillNum * sizeof(fileControlBlock) );
			//log_msg("\n Reading inodes...\n");

			if(isSucc){
				//check types of files, ex. data vs directory
				fileControlBlock *fcb = &inodes[i];

				//check if dir 
				if(fcb->fileType == IS_DIR){

					//log_msg("\n dir found.\n");
					//block_write_padded(i, &fcb, sizeof(fileControlBlock));			
				} 
				//check if we have file
				else if(fcb->fileType == IS_FILE){

					// set the data bmap
					if(fcb->fileSize < 1)
						sBlock->dbmap[i] = NOT_USED;
					else
						sBlock->dbmap[i] = USED;


				}

				currBlockFillNum++;
				if(currBlockFillNum >= 6){
					currBlockFillNum = 0;
					currBlock++;
				} 


			} else {
				log_msg("\n [sfs_init] Error trying to block read inode[%d].\n", i);
			}

			i++;
		}


	} else{

		log_msg("\n [sfs_init] Error trying to initiate disk! \n");
	}

	return SFS_DATA;
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
	char fpath[PATH_MAX];

	// check if path name is too long
	if(strlen(path) > PATH_MAX) {
		log_msg("\n [sfs_getattr] Name too long for me, oops\n");
		errno = ENAMETOOLONG;
		return -errno;
	}

	if (statbuf == NULL) {
		log_msg("\n [sfs_getattr] What are you doing, statbuf is null\n");
		errno = EIO;
		return -errno;
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

		log_msg("\n [sfs_getattr] getattr() on trash or root (%s), returning success. \n", path);
		return 0;
	} else {
		// find file: note, findFileOrDir already strips this path to relative name, you don't need to do it here
		fileControlBlock *fileHandle = findFileOrDir(path, FALSE);
		if(fileHandle == NULL){
			log_msg("\n [sfs_getattr] findFileOrDir could not find %s so returning ENOENT\n", path);
			errno = ENOENT;
			return -errno;
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

	log_msg("\n [sfs_getattr] on %s success\n", path);
	return 0;	
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
	log_msg("\n [sfs_create] passing %s to create_inode \n", path);

	if (create_inode(IS_FILE, path) == NULL) {
		return -errno;
	}

	return 0;
}

/** Remove a file */
int sfs_unlink(const char *path)
{
        log_msg("\n[sfs_unlink] passing %s to remove_inode\n", path);

        if (remove_inode(IS_FILE, path) == FALSE) {
                errno = ENOENT;
                return -errno;
        }

        return 0;
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
	fileControlBlock *fcb = findFileOrDir(path, FALSE);
	if (fcb != NULL) {
		log_msg("\n [sfs_open] found %s, return success \n", path);
		return 0;
	} else {
		log_msg("\n [sfs_open] could not find %s, return -ENOENT \n ", path);
		errno = ENOENT;
		return -errno;
	}

	return -1;
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
	fileControlBlock *fcb = findFileOrDir(path, FALSE);
	if (fcb != NULL) {
		log_msg("\n [sfs_release] found %s, return success \n", path);
		return 0;
	} else {
		log_msg("\n [sfs_release] could not find %s, return -ENOENT \n", path);
		errno = ENOENT;
		return -errno;
	}
	
	return -1;
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

        fileControlBlock *fc = findFileOrDir(path, FALSE);
        if (fc != NULL) {

		log_msg("\n [sfs_read] to be implemented \n");

        } else {
                log_msg("\n [sfs_read] cannot find the file \n");
                errno = ENOENT;
		return -errno;
        }

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

	fileControlBlock *fc = findFileOrDir(path, FALSE);
	if (fc != NULL) {
		int size_of_data_block = 4096;
		int how_many_pointers = sizeof(fc->block)/sizeof(fc->block[0]); //15
		int capacity = how_many_pointers * size_of_data_block;
		if (size <= capacity) {
			log_msg("\n [sfs_write] size %d less than capacity %d, returning success\n", size, capacity);

			//If this requires writing more than fc->numBlocks, allocate new blocks

			int i = 0;
			for(; i < fc->numBlocks; i++) {
				
			}

		} else {
			log_msg("\n [sfs_write] you're asking to write more than this file sytem can support \n");
			errno = EFBIG;
		}
	} else {
		log_msg("\n [sfs_write] cannot find the file \n");
		errno = ENOENT;
	}

	return bytes_written;
}

/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
	log_msg("\n [sfs_mkdir] passing %s directly to create_inode \n", path);

	if (create_inode(IS_DIR, path) != NULL) {
		return 0;
	}

	return -1;
}

/** Remove a directory */
int sfs_rmdir(const char *path)
{
	log_msg("\n[sfs_rmdir] passing %s to remove_inode\n", path); 

	if (remove_inode(IS_DIR, path) == FALSE) {
		errno = ENOENT;
		return -errno;
	}

	return 0;
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

        fileControlBlock *fcb = findFileOrDir(path, TRUE);
        if (fcb != NULL) {
                log_msg("\n [sfs_opendir] found %s, return success \n", path);
                return 0;
        } else {
                log_msg("\n [sfs_opendir] could not find %s, return -ENOENT \n", path);
                errno = ENOENT;
                return -errno;
        }

	return -1;
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

	log_msg("\n [sfs_readdir] on %s, calling filler \n", path);
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	fileControlBlock *directory = findFileOrDir(path, TRUE);
	if(directory == NULL){
		log_msg("\n [sfs_readdir] Could not find directory using path: %s \n", path);
		return -1;
	} else {
		// end condition if true
		if(directory->dirContents == NULL) {
			log_msg("\n directory->dircontents == NULL, returning success \n");
			return retstat;
		} else {
			fileControlBlock *curr = directory->dirContents[0];
			int i = 1;
			while(curr != NULL){
				log_msg("\n [sfs_readdir] calling filler on %s, i=%d \n", curr->fileName, i);
				filler(buf, curr->fileName, NULL, 0);
				i++;
				curr = directory->dirContents[i];
			}

			log_msg("\n [sfs_readdir] ending on i=%d \n", i);
		}
	}

	return retstat;
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
