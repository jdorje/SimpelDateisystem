fileControlBlock *findFileOrDir(const char *filePath);
fileControlBlock *findRootOrDieTrying();
fileControlBlock *getParentFcb(fileControlBlock *child);
fileControlBlock *create_inode(const char *filePath, mode_t mode);

BOOL remove_inode(const char *filePath);
BOOL add_to_direntry(fileControlBlock *parent, fileControlBlock *child);
BOOL indexed_remove_from_direntry(fileControlBlock* parent, int dirContentIndex);
BOOL remove_from_direntry(fileControlBlock *parent, fileControlBlock *child);
BOOL flushAllInodesTodisk(BOOL firstTime);

char *getRelativeParentName(const char *filePath);

void showInodeNames();
void initDirContents(fileControlBlock *fcb);

int getFreeDataBlockNum();
int getFreeInodeNum(superblock *sBlock);
int createAllInodes();
int formatDisk(superblock *sBlock);
