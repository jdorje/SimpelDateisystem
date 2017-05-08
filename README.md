# Simple File System (sfs) for FUSE

SimpelDateisystem (sfs) is a FUSE file system that supports a hierarchical tree structure. You can create, remove, and navigate through directories; and create and delete files, although read and write functionality is not implemented.

Note that SimpelDateisystem was tested exclusively on CentOS 5, and there are certain hard-coded accomodations for it, like returning dummy values for "~/.Trash". We suggest debug-mounting through ```montiere.sh``` and testing file creation through ```simplecreate.py```.


## sfs_init
Metadata for the whole file system is contained within the "superblock", which calculates how many "inodes" the disk space can accomodate. An inode, synonymous with ```fileControlBlock``` in our implementation, contains the relative file name; last date modified; parent directory, etc.


## sfs_getattr
The most important part of implementing this ```stat()``` equivalent for FUSE is setting the correct "mode": for files, 0666 gives read and write permission to everyone, while 0755 gives only the owner read write and execute permission.


## sfs_readdir
Each directory's inode contains an array of pointers to the inodes of the files within its directory. The ```filler``` function only accepts relative filenames without slashes.
