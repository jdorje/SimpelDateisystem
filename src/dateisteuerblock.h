fileControlBlock *findFileOrDir(const char *filePath, fileControlBlock *curr, BOOL isDir);
fileControlBlock *findRootOrDieTrying();
fileControlBlock *getParentFcb(fileControlBlock *child);
fileControlBlock *create_inode(fileType type, const char *filePath);

BOOL remove_inode(fileType type, const char *filePath);

char *getRelativeParentName(const char *filePath);

void showInodeNames();
void initDirContents(fileControlBlock *fcb);

int getFreeDataBlockNum();
int getFreeInodeNum(superblock *sBlock);
int createAllInodes();
int formatDisk(superblock *sBlock);
