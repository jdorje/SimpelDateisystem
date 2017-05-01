fileControlBlock *findFileOrDir(const char *filePath, BOOL isDir);
fileControlBlock *findFileOrDirInternal(const char *filePath, fileControlBlock *curr, BOOL isDir);
fileControlBlock *findRootOrDieTrying();
fileControlBlock *getParentFcb(fileControlBlock *child);
fileControlBlock *create_inode(fileType type, const char *filePath);

BOOL remove_inode(fileType type, const char *filePath);
BOOL add_to_direntry(fileControlBlock *parent, fileControlBlock *child);
BOOL indexed_remove_from_direntry(fileControlBlock* parent, int dirContentIndex);
BOOL remove_from_direntry(fileControlBlock *parent, fileControlBlock *child);

char *getRelativeParentName(const char *filePath);

void showInodeNames();
void initDirContents(fileControlBlock *fcb);

int getFreeDataBlockNum();
int getFreeInodeNum(superblock *sBlock);
int createAllInodes();
int formatDisk(superblock *sBlock);
