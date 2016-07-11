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
	printf("Usage: %s -h|-i|-u|-b|-l\n",pcmd);
	printf("\t-h: Print this help message\n");
	printf("\n\t-i DiskSizeMB DevName: Safe erase and reinitialize the whole disk. The data inside will be erased and disk mapping table reformatted. The parameter DiskSizeMB assign the new disk size (in MB). The parameter DevName designates which device to erase, it should be a device pathname like /dev/memcona\n");
	printf("\t    Please be cautious when using this command.\n");
	printf("\t    When the disk is in use either by filesystem, disk utilities or MEMBlaze tool set, this command will not run.\n");
	printf("\n\t-u FirmwareDir DevName: Update disk firmware. The data inside will be intact. The parameter FirmwareDir designates the directory contains firmware to update and the parameter DevName designates which device to erase, it should be a device pathname like /dev/memcona\n");
	printf("\t    Please be cautious when using this command and keep power on during firmware update.\n");
	printf("\t    When the disk is in use either by filesystem, disk utilities or MEMBlaze tool set, this command will not run.\n");
	printf("\n\t-b on|off DevName: Turn On/Off the Baecon LED Indicator. The parameter DevName designates which device to erase, it should be a device pathname like /dev/memcona\n");
	printf("\n\t-l Sub_dev DevName: Lock the romtext.\n");
	printf("\n\t-s ModeName DevName: Switch the disk speed mode between High and Extreme mode.\n");
	return 0;
}

int Baecon(const char* pOnOff, const char *pDevName)
{
	char pDevConName[NFS_MAXPATHLEN];
	int Handle=open(TranslateDeviceName(pDevName,pDevConName,NFS_MAXPATHLEN),O_RDWR|O_LARGEFILE);

	if(Handle<0) {
		if(errno==13)
			printf("Please use root account to use this tool\n");
		printf("Error Open Device %s\n",pDevName);
		return -1;
	}

	u32 IsBaeconOn;
	if(strcasecmp(pOnOff,"on")==0) {
		IsBaeconOn=1;
	} else if(strcasecmp(pOnOff,"off")==0) {
		IsBaeconOn=0;
	} else {
		printf("The first parameter %s should be either on or off\n",pOnOff);
		return -1;		
	}

	if(ioctl(Handle,IOCTL_BAECON,&IsBaeconOn)<0) {
		printf("Can not get IOCTL_BAECON for %s, program exit!\n",pDevName);
		close(Handle);
		return -1;
	}
	return 0;
}

int Reinit(const char* pDiskSizeMB, const char *pDevName)
{
	char pDevConName[NFS_MAXPATHLEN];
        u64 MaxDiskSizeMB;
        u64 MinDiskSizeMB;
        u64 DiskSizeMB;

	int Handle=open(TranslateDeviceName(pDevName,pDevConName,NFS_MAXPATHLEN),O_RDWR|O_LARGEFILE);
	if(Handle<0) {
		if(errno==13)
			printf("Please use root account to use this tool\n");
		printf("Error Open Device %s\n",pDevName);
		return -1;
	}
	struct InitDisk_Mon im;
	if(ioctl(Handle,IOCTL_INITIALQUERYSTAT,&im)<0) {
		printf("Can not get IOCTL_INITIALQUERYSTAT for %s, program exit!\n",pDevName);
		close(Handle);
		return -1;
	}
	struct Disk_Mon dm;
	if(ioctl(Handle,IOCTL_QUERYSTAT,&dm)<0) {
		printf("Can not get IOCTL_QUERYSTAT for %s, program exit!\n",pDevName);
		close(Handle);
		return -1;
	}

	MaxDiskSizeMB=atoi(FindValueBasedonKey(im.aRom, "MaxAddressedBlockSizeInTetris:"));
        MinDiskSizeMB=atoi(FindValueBasedonKey(im.aRom, "MinAddressedBlockSizeInTetris:"));
        MaxDiskSizeMB *= NR_LUN_PER_TETRIS;
        MaxDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashBlockSize:"));
        MaxDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashPageSize:"));
        MaxDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashPlaneSize:"));
        MaxDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashSectorSize:"));
        MaxDiskSizeMB *= dm.tetris_num;

        MinDiskSizeMB *= NR_LUN_PER_TETRIS;
        MinDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashBlockSize:"));
        MinDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashPageSize:"));
        MinDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashPlaneSize:"));
        MinDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashSectorSize:"));
        MinDiskSizeMB *= dm.tetris_num;

        MaxDiskSizeMB >>= 20;
        MinDiskSizeMB >>= 20;

	DiskSizeMB = atoi(pDiskSizeMB);
	if(DiskSizeMB>MaxDiskSizeMB || DiskSizeMB<MinDiskSizeMB) {
		printf("The parameter DiskSizeMB (the available disk size in million bytes) can only be in the range %lld~%lld, program exit!\n",MinDiskSizeMB, MaxDiskSizeMB);
		close(Handle);
		return -1;
	}
	
	printf("Safe Erase for %s, new DiskSize is %lld. y to continue ... ", pDevName,DiskSizeMB);

	char Command=getchar();
	if(Command!='y' && Command!='Y') {
		printf("Safe Erase Aborted, program exit!\n");
		close(Handle);
		return -1;		
	}

	printf("Begin Disk Safe Erase\n");

	if(ioctl(Handle,IOCTL_REINITIALIZE,&DiskSizeMB)<0) {
		printf("Error in device IOCtl\n");
		printf("Check if %s is mounted(Used by filesystem), beening read/ written or used by other program\n",pDevName);
		return -1;
	}

	close(Handle);
	printf("Disk Safe Erase Done\n");
	return 0;
}

int SwitchMode(const char* pMode, const char *pDevName)
{
	char pDevConName[NFS_MAXPATHLEN];
        u64 MaxDiskSizeMB;
        u64 MinDiskSizeMB;
        u64 DiskSizeMB;
	u64 ExtremeGB;

	int Handle=open(TranslateDeviceName(pDevName,pDevConName,NFS_MAXPATHLEN),O_RDWR|O_LARGEFILE);
	if(Handle<0) {
		if(errno==13)
			printf("Please use root account to use this tool\n");
		printf("Error Open Device %s\n",pDevName);
		return -1;
	}
	struct InitDisk_Mon im;
	if(ioctl(Handle,IOCTL_INITIALQUERYSTAT,&im)<0) {
		printf("Can not get IOCTL_INITIALQUERYSTAT for %s, program exit!\n",pDevName);
		close(Handle);
		return -1;
	}
	struct Disk_Mon dm;
	if(ioctl(Handle,IOCTL_QUERYSTAT,&dm)<0) {
		printf("Can not get IOCTL_QUERYSTAT for %s, program exit!\n",pDevName);
		close(Handle);
		return -1;
	}

	MaxDiskSizeMB=atoi(FindValueBasedonKey(im.aRom, "MaxAddressedBlockSizeInTetris:"));
        MinDiskSizeMB=atoi(FindValueBasedonKey(im.aRom, "MinAddressedBlockSizeInTetris:"));
        MaxDiskSizeMB *= NR_LUN_PER_TETRIS;
        MaxDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashBlockSize:"));
        MaxDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashPageSize:"));
        MaxDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashPlaneSize:"));
        MaxDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashSectorSize:"));
        MaxDiskSizeMB *= dm.tetris_num;

        MinDiskSizeMB *= NR_LUN_PER_TETRIS;
        MinDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashBlockSize:"));
        MinDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashPageSize:"));
        MinDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashPlaneSize:"));
        MinDiskSizeMB *= atoi(FindValueBasedonKey(im.aRom, "FlashSectorSize:"));
        MinDiskSizeMB *= dm.tetris_num;

        MaxDiskSizeMB >>= 20;
        MinDiskSizeMB >>= 20;

	if (strncmp(pMode, "High", 4) == 0) {
		DiskSizeMB = MaxDiskSizeMB;
	} else if (strncmp(pMode, "Extreme", 7) == 0) {
		ExtremeGB = (MaxDiskSizeMB >> 10);

		if (ExtremeGB > 0 && ExtremeGB < 1000)
			DiskSizeMB = (MaxDiskSizeMB / 3) * 2;	/* 800G->600G */
		else if (ExtremeGB > 1000 && ExtremeGB < 2000)
			DiskSizeMB = (MaxDiskSizeMB / 3) * 2;	/* 1.2T->800G */
		else
			DiskSizeMB = (MaxDiskSizeMB / 2);		/* 2.4T->1.2T*/
	} else {
		printf("The mode name must be High or Extreme\n");
		close(Handle);
		return -1;
	}

	printf("Switch mode for %s, new DiskSize is %lld. y to continue ... ", pDevName,DiskSizeMB);

	char Command=getchar();
	if(Command!='y' && Command!='Y') {
		printf("Switch mode Aborted, program exit!\n");
		close(Handle);
		return -1;		
	}

	printf("Begin Disk Switch mode\n");

	if(ioctl(Handle,IOCTL_REINITIALIZE,&DiskSizeMB)<0) {
		printf("Error in device IOCtl\n");
		printf("Check if %s is mounted(Used by filesystem), beening read/ written or used by other program\n",pDevName);
		return -1;
	}

	close(Handle);
	printf("Disk Switch mode Done\n");
	return 0;
}

#define FIRMWARE_NUMBER		(4)

struct FrimwareMapEntry FirmwareMap[FIRMWARE_NUMBER] = {
	{"01", "toshiba8k/firmwareupdate/pblaze3_firmware.bin"},
	{"02", "toshiba16k/firmwareupdate/pblaze3_firmware.bin"},
	{"12", "toshibaa19_128GB/firmwareupdate/pblaze3_firmware.bin"},
	{"22", "toshibaa19_64GB/firmwareupdate/pblaze3_firmware.bin"}
};

int GetFirmwareNameByDevice(int Handle, const char *pFirmwareDir, char *pFirmwareName, int len)
{
	struct InitDisk_Mon Monitor;
	const char *aRom = Monitor.aRom;
	const char *model = NULL;
	const char *p = NULL;
	int pos, i;

	p = &pFirmwareDir[strlen(pFirmwareDir) - 4];
	if (!strcmp(p, ".bin")) {
		sprintf(pFirmwareName, "%s", pFirmwareDir);	
		return 0;
	}

	if (strlen(pFirmwareDir) + FIRMWARE_PATH_LENGTH >= len) {
		printf("Too long Frimware path %s\n", pFirmwareDir);
		return -1;
	}

	pos = sprintf(pFirmwareName, "%s", pFirmwareDir);
	if (pFirmwareName[pos - 1] != '/') {
		pFirmwareName[pos] = '/';
		pos += 1;
	}

	if (ioctl(Handle, IOCTL_INITIALQUERYSTAT, &Monitor) < 0) {
		printf("Ioctl failed for %s.\n", strerror(errno));
		return -1;
	}

	model = FindValueBasedonKey(aRom, "Model:");
	p = &model[strlen(model) - 2];

	for (i = 0; i < FIRMWARE_NUMBER; i++) {
		if (!strcmp(p, FirmwareMap[i].PSN_END)) {
			pos += sprintf(pFirmwareName + pos, "%s", FirmwareMap[i].Path);
			break;
		}
	}

	if (i >= FIRMWARE_NUMBER) {
		printf("Get error PSN: %s\n", model);
		return -1;
	}

	return 0;
}

int UpdateFirmware(const char* pFirmwareDir,const char *pDevName)
{
	char pDevConName[NFS_MAXPATHLEN];
	char pFirmwareName[NFS_MAXPATHLEN] = {0};
	int Handle=open(TranslateDeviceName(pDevName,pDevConName,NFS_MAXPATHLEN),O_RDWR|O_LARGEFILE);

	if(Handle<0) {
		if(errno==13)
			printf("Please use root account to use this tool\n");
		printf("Error Open Device %s\n",pDevName);
		return -1;
	}

	if (GetFirmwareNameByDevice(Handle, pFirmwareDir, pFirmwareName, sizeof(pFirmwareName))) {
		printf("Generate Firmware name failed.\n");
		return -1;
	}

	printf("Update Firmwre for %s with file %s. y to continue ... ", pDevName,pFirmwareName);

	char Command=getchar();
	if(Command!='y' && Command!='Y') {
		printf("Update Firmwre Aborted, program exit!\n");
		close(Handle);
		return -1;		
	}

	printf("Begin Update Firmware\n");

	char aCMD[NFS_MAXPATHLEN];
	snprintf(aCMD,NFS_MAXPATHLEN,"cp -f %s /lib/firmware/memblaze.fw",pFirmwareName);

	if(system(aCMD)!=0) {
		printf("Cannot copy firmware file to /lib/firmware/memblaze.fw");
		return -1;
	}

	struct FirmwareFileName ffn;
	strncpy(ffn.aFileName,"memblaze.fw",NFS_MAXPATHLEN);
	if(ioctl(Handle,IOCTL_UPDATEFIRMWARE,&ffn)<0) {
		printf("Error in device IOCtl\n");
		printf("Check if %s is mounted(Used by filesystem), beening read/written or used by other program,"
                       "and if the firmware version is incorrect.\n",pDevName);
		return -1;
	}

	close(Handle);
	printf("Update Firmware Done. Please turn off the computer and restart it to let new firmware work.\n");
	return 0;
	
}

int RomLock(const char *sub_dev, const char *pDevName)
{
	u32 temp;
	char pDevConName[NFS_MAXPATHLEN];	
	int Handle=open(TranslateDeviceName(pDevName,pDevConName,NFS_MAXPATHLEN),O_RDWR|O_LARGEFILE);

	if(Handle<0) {
		if(errno==13)
			printf("Please use root account to use this tool\n");
		printf("Error Open Device %s\n",pDevName);
		return -1;
	} 
	printf("Lock romtext for %s. y to continue ... ", pDevName);

	char Command=getchar();
	if(Command!='y' && Command!='Y') {
		printf("Lock Firmwre Aborted, program exit!\n");
		close(Handle);
		return -1;		
	}
	printf("Begin lock romtext\n");
	
	temp = atoi(sub_dev);
	if (temp >= NR_DEV) {
		printf("Lock device index is too large\n");
		return -1;
	}
	
	if(ioctl(Handle,IOCTL_LOCKROM,&temp)<0) {
	    printf("Error in device IOCtl\n");
		printf("Can not Lock Firmware for %s, program exit!\n",pDevName);
		close(Handle);
		return -1;
	}
	
	close(Handle);
	printf("Lock romtext Done.\n");
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

	if(argc<4 || argv[1][0]!='-')
		return Help(argv[0]);
	switch (argv[1][1]) {
		case 'i':
		case 'I':
			return Reinit(argv[2],argv[3]);
		case 'u':
		case 'U':
			return UpdateFirmware(argv[2],argv[3]);
		case 'b':
		case 'B':
			return Baecon(argv[2],argv[3]);
		case 'l':
		case 'L':
		    return RomLock(argv[2], argv[3]);
		case 's':
		case 'S':
		    return SwitchMode(argv[2], argv[3]);
		default:
			return Help(argv[0]);
	}	
}
