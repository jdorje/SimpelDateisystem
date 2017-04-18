#ifndef __KNOTEN_H__
#define __KNOTEN_H__

typedef struct {
	short mode; //can this file be read/written/executed?
	short uid; //who owns this file?
	long size; //how many bytes are in this file?
	long time; //what time was this file last accessed?
	long ctime; //what time was this file created?
	long mtime; //what time was this file last modified?
	long dtime; //what time was this inode deleted?
	short gid; //what group does this file belong to?
	short links_count; //how many hard links are there to this file?
	long blocks; //how many blocks have been allocated to this file?
	long flags; //how should ext2 use this inode?
	long osd1; //an OS-dependent field
	char block[60]; //a set of disk pointers (15 total)
	long generation; //file version (used by NFS)
	long file_acl; //a new permissions model beyond mode bits
	long dir_acl; //called access control lists
} iknoten;

#endif
