#pragma once

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;

#define HZ 1000
//#define MASTERCPU

#define NFS_MAXPATHLEN 1024
#define GroupSizeInDomain	8u
#define DomainSizeInSystem	16u
#define COMM_DEVICE_COUNT	4u
#define NR_LUN_PER_TETRIS       7
#define ByteSizeInPage	4096u
#define NR_DEV			2
#define NR_TETRIS		48	/* MAX tetris number */
#define PB_LOCKROM_MAGIC	(0xAB)
#define PB_LOCKROM_MASK		(0xff)

struct FirmwareFileName
{
	char aFileName[NFS_MAXPATHLEN];
};

#define	FIRMWARE_PATH_LENGTH	(128)
struct FrimwareMapEntry
{
	char PSN_END[8];
	char Path[FIRMWARE_PATH_LENGTH];
};

#define LATENCY_BUCKET	32
#define LATENCY_BUCKET_STAT	8
#define LATENCY_COUNTER	2


#define ST_STAGE_RUN	2
#define LATENCY_BUCKET	32
#define LATENCY_BUCKET_STAT	8
#define LATENCY_COUNTER	2

struct Latency_Mon
{
	u32 Count;
	u32 Total_Latency;
	u32 Max_Latency;
	u32 Min_Latency;
	u32 aBucketCount[LATENCY_BUCKET];
};
struct DiskPersistentMon
{
	u64 LogicalWrite;
	u64 FlashWrite;
	u64 LogicalRead;
	u64 FlashRead;
};
#define DRVVER_SIZE	64
struct InitDisk_Mon
{
	char aRom[ByteSizeInPage];
	char aDriverVersion[DRVVER_SIZE];	
};
struct Disk_Mon
{
	u64 LogicalWrite;
	u64 FlashWrite;
	u64 LogicalRead;
	u64 FlashRead;
	u32 dead_block;
	u32 dead_die;
	u32 InitCount;
	u32 StartCount;
	u32 aCommStatus[COMM_DEVICE_COUNT];
	u32 CurrentTick;
	u32 ReadIOPS;
	u32 ReadSize;
	u32 WriteIOPS;
	u32 WriteSize;
	u32 DiskSizeMB;
        u32 tetris_num;
	u32 LinkWidth;
        u32 LinkGen;
	u32 DriverMemSize;
	u16 Temperature;
	u16 TemperatureMax;
	u16 TemperatureMin;
	u16 VCore;
	u16 VCoreMax;
	u16 VCoreMin;
	u16 VPLL;
	u16 VPLLMax;
	u16 VPLLMin;
	u16 LockRom;
	u16 VDDR;
	u16 OnboardTemperature_Max;
	u16 OnboardTemperature_Min;
	u16 TemperatureUnsafe;
        u16 OnboardTemperature;
	u16 FlyingPipePercent;	
        u64 ErrorCode;
	struct Latency_Mon aLatency[LATENCY_COUNTER];
};

struct dead_block_value {
	char PSN_END[8];
	int warn_val;
	int error_val;
};

#define IOCTL_MAGIC	162
#define IOCTL_RESTART	_IO(IOCTL_MAGIC,0)
#define IOCTL_REINITIALIZE	_IOW(IOCTL_MAGIC,1,u32)
#define IOCTL_DIO	_IOWR(IOCTL_MAGIC,2,struct DirectIO)
#define IOCTL_BAECON	_IOW(IOCTL_MAGIC,3,u32)
#define IOCTL_QUERYSTAT _IOR(IOCTL_MAGIC,4,struct Disk_Mon)
#define IOCTL_INITIALQUERYSTAT	_IOR(IOCTL_MAGIC,5,struct InitDisk_Mon)
#define IOCTL_UPDATEFIRMWARE	_IOW(IOCTL_MAGIC,6,struct FirmwareFileName)
#define IOCTL_ATTACH	_IO(IOCTL_MAGIC,7)
#define IOCTL_DETACH	_IO(IOCTL_MAGIC,8)
#define IOCTL_LOCKROM   _IOW(IOCTL_MAGIC,9,u32)

const char *FindValueBasedonKey(const char* pRom,const char* pKey)
{
	bool IsTerminalNull=true;
	char cRom,cKey;
	const char *pRomStart=pRom;
	bool IsKey=true;
	while(true) {
		if((cRom=*pRom++)=='\0') {
			if(IsTerminalNull)
				return --pRom;
			if(IsKey && strcmp(pRomStart,pKey)==0)
				return pRom;
			IsKey=!IsKey;
			IsTerminalNull=true;
			pRomStart=pRom;
		} else {
			IsTerminalNull=false;
		}
	}	
}

const char* TranslateDeviceName(const char* pSrcDeviceName,char* pDesDeviceName,int DesDeviceNameLen)
{
	const char* pSrc=pSrcDeviceName;
	char aLinkName[NFS_MAXPATHLEN];
	ssize_t result=readlink(pSrcDeviceName,aLinkName,NFS_MAXPATHLEN-1);

	if(result>=0) {
		aLinkName[NFS_MAXPATHLEN-1]='\0';
		pSrc=pSrcDeviceName;
	}
	size_t len=strlen(pSrc);
	//"memdiska"
	size_t lendiskdev=strlen("memdisk");
	size_t lencondev=strlen("memcon");
	if(len>=lendiskdev+1 && strncmp(pSrc+len-(1+lendiskdev),"memdisk",lendiskdev)==0 && DesDeviceNameLen>=len+lencondev-lendiskdev+1) {
		memcpy(pDesDeviceName,pSrc,len-1-lendiskdev);
		memcpy(pDesDeviceName+len-1-lendiskdev,"memcon",lencondev);
		pDesDeviceName[len+lencondev-lendiskdev-1]=pSrc[len-1];
		pDesDeviceName[len+lencondev-lendiskdev]='\0';
		return pDesDeviceName;
	}
	return pSrcDeviceName;
}
