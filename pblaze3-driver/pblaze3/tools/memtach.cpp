#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "memdef.h"


int Help(const char *pcmd)
{
	printf("MEMBlaze Disk Management Tool Set\n");
	printf("Usage: %s -h|-a|-d\n",pcmd);
	printf("\t-h: Print this help message\n");
	printf("\n\t-a DevName: Attach the disk to MEMBlaze device driver. The parameter DevName specifies which device to attach, it should be a device pathname like /dev/memdiska\n");
	printf("\t    When the disk is already attached, this command may report an error and exit.\n");
	printf("\n\t-d DevName: Detach the disk from MEMBlaze device driver. The parameter DevName specifies which device to detach, it should be a device pathname like /dev/memdiska\n");
	printf("\t    When the disk is already detached or busy, this command may report an error and exit.\n");
	return 0;
}

int Attach(const char *pDevName)
{
	char pDevConName[NFS_MAXPATHLEN];
	int Handle=open(TranslateDeviceName(pDevName,pDevConName,NFS_MAXPATHLEN),O_RDWR|O_LARGEFILE);

	if(Handle<0) {
		if(errno==13)
			printf("Please use root account to use this tool\n");
		printf("Error in open device %s\n",pDevName);
		return -1;
	}
	if(ioctl(Handle,IOCTL_ATTACH)<0) {
		switch(errno) {
		case EINVAL:
			printf("No suitable driver should be attached. Check if no driver claims this device\n");
			break;
		case EEXIST:
			printf("Device %s is already been attached to\n",pDevName);
			break;
		default:
			printf("Unknown error occurrs when ioctl %s errno is %d\n",pDevName,errno);
			break;
		}
		return -1;
	}
	printf("%s is attached successfully\n",pDevName);
	close(Handle);
	return 0;
}

int Detach(const char *pDevName)
{
	char pDevConName[NFS_MAXPATHLEN];
	int Handle=open(TranslateDeviceName(pDevName,pDevConName,NFS_MAXPATHLEN),O_RDWR|O_LARGEFILE);

	if(Handle<0) {
		if(errno==13)
			printf("Please use root account to use this tool\n");
		printf("Error in open device %s\n",pDevName);
		return -1;
	}
	if(ioctl(Handle,IOCTL_DETACH)<0) {
		switch(errno) {
		case EEXIST:
			printf("Device %s is not attached\n",pDevName);
			break;
		case EBUSY:
			printf("Device %s is busy\n",pDevName);
			break;
		default:
			printf("Unknown error occurrs when ioctl %s errno is %d\n",pDevName,errno);
			break;
		}
		return -1;
	}
	printf("%s is detached successfully\n",pDevName);
	close(Handle);
	return 0;
}
volatile bool IsWork=true;
void sig_kill(int signo)
{
	IsWork=false;
}
int main(int argc, char** argv)
{
	struct sigaction newact, oldact;
	unsigned int unslept;

	newact.sa_handler = sig_kill;
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;
	sigaction(SIGINT, &newact, &oldact);

	if(argc<3 || argv[1][0]!='-')
		return Help(argv[0]);
	switch (argv[1][1]) {
		case 'a':
		case 'A':
			return Attach(argv[2]);
		case 'd':
		case 'D':
			return Detach(argv[2]);
		default:
			return Help(argv[0]);
	}	
}
