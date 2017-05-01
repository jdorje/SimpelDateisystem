#include<stdlib.h>
#include<stdio.h>
#include<sys/stat.h>
#include <fcntl.h>
#include<errno.h>
int main()
{
//	int fd = open("/.autofs/ilab/ilab_users/jis63/Desktop/assignment3/SimpelDateisystem/test", O_RDWR | O_CREAT);//, S_IRUSR | S_IRGRP | S_IROTH);

	int fd2 = open("/.autofs/ilab/ilab_users/jis63/Desktop/assignment3/SimpelDateisystem/example/mountdir/test", O_CREAT);//, S_IRUSR | S_IRGRP | S_IROTH);
	if(fd2==-1)
	{
		printf("%d\n", strerror(errno));
	}

}


