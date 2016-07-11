#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>
#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include "memdef.h"

#define SCALE1000
#define	DRIVER_ERR_CODE	(15000000)

int RefreshRate=2000;
bool IsInteractiveMode=true;
bool IsPlainMode=true;
bool bNorefresh = false;

short LogoColor=1;
short SafeColor=2;
short WarnColor=3;
short SpecialColor=4;
short LightWarnColor=5;

volatile int ScrollPos=0;
volatile int RowSize=0;
volatile int Margin=0;
pthread_t DispThread;
volatile bool IsNeedClose=false;
volatile bool IsDetailMode=true;
sem_t SemRequest;
sem_t SemResponse;
#define INVALIDHANDLE	-1
#define PERMISSIONDENIED	-2
#define IOCTLERROR	-3
int aDevice['z'+1-'a'];
int DriverMemSize=0;
/********************************************************************/
const char *pMonitorVersion="01.0002 Compiled on "__DATE__;
char aDriverVersion[DRVVER_SIZE];
/********************************************************************/

//TICKFREQ must be larger than max possible HZ value to ensure u32 overflow in CurrentTick.
#define TICKFREQ	10000
#define PAGESIZE	(1u<<12)
#define TABSIZE	8

struct Disk_Mon aPrevMonitor[26];
struct InitDisk_Mon aInitMonitor[26];
float TSCInterval=0.0F;
float Life_Error[NR_TETRIS];

void init_life_error_array()
{
	srand((unsigned)time(0));
	for (int i = 0; i < NR_TETRIS; i++) {
		Life_Error[i] = rand() / (float)(RAND_MAX) / (float)10;
	}
}

__inline__ unsigned long long int rdtsc()
{
unsigned long long int x;
__asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
return x;
}
void CacTSCInterval()
{
	struct timezone tz;
	struct timeval tvstart, tvstop;
	unsigned long long int cyclestart,cyclestop;
	memset(&tz, 0, sizeof(tz));
	/* get this function in cached memory */
	gettimeofday(&tvstart, &tz);
	cyclestart = rdtsc();
	gettimeofday(&tvstart, &tz);
	/* we don't trust that this is any specific length of time */
	usleep(100000);

	cyclestop = rdtsc();
	gettimeofday(&tvstop, &tz);
	unsigned int microseconds = ((tvstop.tv_sec-tvstart.tv_sec)*1000000) +(tvstop.tv_usec-tvstart.tv_usec);
	
	TSCInterval=0.001F*microseconds/(cyclestop-cyclestart);
}
bool IsDeviceExist(const char *pDeviceName)
{
	return access(pDeviceName,0)==0;
}

#define MAXTEXTLENGTH	30
#define TEXTCOUNT	16
char aaText[TEXTCOUNT][MAXTEXTLENGTH];
int TextIndex=0;

const char* PrintInteger(long long Size)
{
	char *pTextBuffer=aaText[(++TextIndex)&(TEXTCOUNT-1)];
	snprintf(pTextBuffer,MAXTEXTLENGTH,"%lld",Size);
	return pTextBuffer;
}

static const char *PrintMode(long long AvailableCapacity, long long MaximumCapacity)
{
	char *pTextBuffer=aaText[(++TextIndex)&(TEXTCOUNT-1)];
	long long MaxCapGB;

	MaxCapGB = MaximumCapacity >> 30;
	if (MaxCapGB > 0 && MaxCapGB < 1000) {
		if (AvailableCapacity * 3 / 2 <= MaximumCapacity)
			snprintf(pTextBuffer, MAXTEXTLENGTH, "Extreme-Speed");
		else
			snprintf(pTextBuffer, MAXTEXTLENGTH, "High-Speed");
	}  else if (MaxCapGB > 1000 && MaxCapGB < 2000) {
		if (AvailableCapacity * 3 / 2 <= MaximumCapacity)
			snprintf(pTextBuffer, MAXTEXTLENGTH, "Extreme-Speed");
		else
			snprintf(pTextBuffer, MAXTEXTLENGTH, "High-Speed");
	} else {
		if (AvailableCapacity * 2 <= MaximumCapacity)
			snprintf(pTextBuffer, MAXTEXTLENGTH, "Extreme-Speed");
		else
			snprintf(pTextBuffer, MAXTEXTLENGTH, "High-Speed");
	}

	return pTextBuffer;
}

const char* PrintSize(long long Size)
{
	const char *pUnit="";
	float fSize=(float)Size;
#ifdef SCALE1000
	if(fSize>=1000.0F)
	{
		fSize/=1000.0F;
		pUnit="k";
	}
	if(fSize>=1000.0F)
	{
		fSize/=1000.0F;
		pUnit="M";
	}
	if(fSize>=1000.0F)
	{
		fSize/=1000.0F;
		pUnit="G";
	}
	if(fSize>=1000.0F)
	{
		fSize/=1000.0F;
		pUnit="T";
	}
	if(fSize>=1000.0F)
	{
		fSize/=1000.0F;
		pUnit="P";
	}
#else
	if(fSize>=1024.0F)
	{
		fSize/=1024.0F;
		pUnit="k";
	}
	if(fSize>=1024.0F)
	{
		fSize/=1024.0F;
		pUnit="M";
	}
	if(fSize>=1024.0F)
	{
		fSize/=1024.0F;
		pUnit="G";
	}
	if(fSize>=1024.0F)
	{
		fSize/=1024.0F;
		pUnit="T";
	}
	if(fSize>=1024.0F)
	{
		fSize/=1024.0F;
		pUnit="P";
	}	
#endif
	char *pTextBuffer=aaText[(++TextIndex)&(TEXTCOUNT-1)];
	snprintf(pTextBuffer,MAXTEXTLENGTH,"%0.2f%sB",fSize,pUnit);
	return pTextBuffer;
}
const char *PrintLatency(float LatencyScale)
{
	float Latency=LatencyScale*TSCInterval;
	const char *pUnit="";
	if(Latency<1.0F)
	{
		Latency*=1000.0F;
		pUnit="m";
	}
	if(Latency<1.0F)
	{
		Latency*=1000.0F;
		pUnit="u";
	}

	char *pTextBuffer=aaText[(++TextIndex)&(TEXTCOUNT-1)];
	snprintf(pTextBuffer,MAXTEXTLENGTH,"%*.*f%ss",0,Latency<10.0F?2:1,Latency,pUnit);
	return pTextBuffer;	
}
const char *PrintDeviceName(char Device)
{
	char *pTextBuffer=aaText[(++TextIndex)&(TEXTCOUNT-1)];
	snprintf(pTextBuffer,MAXTEXTLENGTH,"/dev/memdisk%c",Device);
	return pTextBuffer;
}
char *PrintTemperature(float Temperature)
{
	char *pTextBuffer=aaText[(++TextIndex)&(TEXTCOUNT-1)];
	snprintf(pTextBuffer,MAXTEXTLENGTH,"%0.1fC",Temperature);
	return pTextBuffer;
}
char *PrintVoltage(float Voltage)
{
	char *pTextBuffer=aaText[(++TextIndex)&(TEXTCOUNT-1)];
	snprintf(pTextBuffer,MAXTEXTLENGTH,"%0.2fV",Voltage);
	return pTextBuffer;
}
char *PrintIOPS(float IOPS)
{
	char *pTextBuffer=aaText[(++TextIndex)&(TEXTCOUNT-1)];
	const char *pUnit="";
	float fIOPS=(float)IOPS;
	if(fIOPS>=1000.0F)
	{
		fIOPS/=1000.0F;
		pUnit="k";
	}
	if(fIOPS>=1000.0F)
	{
		fIOPS/=1000.0F;
		pUnit="M";
	}
	snprintf(pTextBuffer,MAXTEXTLENGTH,"%*.*f%s",0,fIOPS<10.0F?2:1,fIOPS,pUnit);
	return pTextBuffer;
}
char *PrintBandwidth(float Bandwidth)
{
	char *pTextBuffer=aaText[(++TextIndex)&(TEXTCOUNT-1)];
	const char *pUnit="";
	float fBandwidth=(float)Bandwidth;
#ifdef SCALE1000
	if(fBandwidth>=1000.0F)
	{
		fBandwidth/=1000.0F;
		pUnit="k";
	}
	if(fBandwidth>=1000.0F)
	{
		fBandwidth/=1000.0F;
		pUnit="M";
	}
	if(fBandwidth>=1000.0F)
	{
		fBandwidth/=1000.0F;
		pUnit="G";
	}
#else
	if(fBandwidth>=1024.0F)
	{
		fBandwidth/=1024.0F;
		pUnit="k";
	}
	if(fBandwidth>=1024.0F)
	{
		fBandwidth/=1024.0F;
		pUnit="M";
	}
	if(fBandwidth>=1024.0F)
	{
		fBandwidth/=1024.0F;
		pUnit="G";
	}
#endif
	snprintf(pTextBuffer,MAXTEXTLENGTH,"%*.*f%sB/s",0,fBandwidth<10.0F?2:1,fBandwidth,pUnit);
	return pTextBuffer;
}

static inline bool IsRomLocked(u16 lockFlag)
{
	return ((lockFlag & PB_LOCKROM_MASK) == PB_LOCKROM_MAGIC);
}
int GetFlashSectorSize(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"FlashSectorSize:"));
}
int GetFlashPlaneSize(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"FlashPlaneSize:"));
}
int GetFlashPageSize(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"FlashPageSize:"));
}
int GetFlashBlockSize(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"FlashBlockSize:"));
}
int GetFlashLunSize(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"FlashLunSize:"));
}
int GetFlashChipSize(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"FlashChipSize:"));
}
int GetFlashPECycle(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"FlashPECycle:"));
}
int GetECCCapability(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"Flash ECC Capability:"));
}
int GetMaxDiskSizeMB(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"MaxDiskSizeMB:"));
}
int GetMinErrorBlock(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"MinErrorBlock:"));
}
int GetMaxErrorBlock(const char *pRom)
{
	return atoi(FindValueBasedonKey(pRom,"MaxErrorBlock:"));
}

float GetOnboardTemperature(u16 TempAD)
{
        return TempAD / 128.0F;
}
float GetTemperature(u16 TempAD)
{
	return (TempAD>>4)*(503.975F/4096)-273.15F;
}
float GetVoltage(u16 VoltAD)
{
	return (VoltAD>>4)*(3.0F/4096);
}

float GetDeviceRemainLife(const Disk_Mon & Monitor, const char *pRom)
{
        unsigned long long max_write = 1;

        max_write *= GetFlashLunSize(pRom);
        max_write *= GetFlashChipSize(pRom);
        max_write *= GetFlashBlockSize(pRom);
        max_write *= GetFlashPageSize(pRom);
        max_write *= GetFlashPlaneSize(pRom);
        //max_write *= GetFlashSectorSize(pRom);

        max_write *= Monitor.tetris_num;
        max_write *= GetFlashPECycle(pRom);

	float Life=1.0F-Monitor.FlashWrite*1.0F/max_write;
	if(Life<0.0F)
		Life=0.0F;
	return Life*100.0F;
}

unsigned long long GetDeviceTetrisSize(const char *pRom)
{
        unsigned long long size = 1;

        size *= GetFlashLunSize(pRom);
        size *= GetFlashChipSize(pRom);
        size *= GetFlashBlockSize(pRom);
        size *= GetFlashPageSize(pRom);
        size *= GetFlashPlaneSize(pRom);
        size *= GetFlashSectorSize(pRom);

        return size;
}

int GuessDriverMemSize()
{
	return (IsDeviceExist("/sys/module/pciedisk_driver/"))?53210:0;
}

static inline int MakeAlertCode(int err_no)
{
	return DRIVER_ERR_CODE + err_no;
}

static inline void PrintErrorMessage(int err_no)
{
	printf("AlertCode: %d %s\n", MakeAlertCode(err_no), strerror(err_no));
	printf("game over.\n");
}

void PrintInformation(bool bEnablePrint)
{
	DriverMemSize=GuessDriverMemSize();
	for(char Device='a';Device!='z'+1;++Device) {
		int DeviceIndex=Device-'a';
		int &Handle=aDevice[DeviceIndex];
		const char *model;
		int cpu_num = 2;

		if(Handle==0) {
			char pDevConName[NFS_MAXPATHLEN];
			Handle=open(TranslateDeviceName(PrintDeviceName(Device),pDevConName,NFS_MAXPATHLEN),O_RDONLY);
			if(Handle<0) {
				switch(errno) {
					case EACCES:
						//Permission Denied.
						Handle=PERMISSIONDENIED;
						break;
					case ENOENT:
					case EIO:
						if (DeviceIndex == 0 && bEnablePrint) {
							PrintErrorMessage(errno);
						}
						Handle=0;
						break;
				}
			}
			if(Handle>0 && ioctl(Handle,IOCTL_INITIALQUERYSTAT,&aInitMonitor[DeviceIndex])<0) {
				if (bEnablePrint) {
					PrintErrorMessage(errno);
				}
				close(Handle);
				Handle=IOCTLERROR;
			}				
		}
		if(Handle==PERMISSIONDENIED && bEnablePrint) {	
			printf("Permission Denied while opening %s, root account is required\n",PrintDeviceName(Device));
			PrintErrorMessage(errno);
		}

		if(Handle<=0)
			continue;
		struct Disk_Mon Monitor;
		if(ioctl(Handle,IOCTL_QUERYSTAT,&Monitor)<0) {
			close(Handle);
			if(bEnablePrint) {
				PrintErrorMessage(errno);
			}
			Handle=IOCTLERROR;
			continue;
		}

		DriverMemSize=Monitor.DriverMemSize;
		const char *pRom=aInitMonitor[DeviceIndex].aRom;
		memcpy(aDriverVersion,aInitMonitor[DeviceIndex].aDriverVersion,sizeof(aDriverVersion));
		int DiskBlockSize=GetFlashChipSize(pRom)*GetFlashLunSize(pRom);
		int BytesInBlock=GetFlashSectorSize(pRom)*GetFlashPlaneSize(pRom)*GetFlashPageSize(pRom)*GetFlashBlockSize(pRom);
		long long AvailableCapacity=((long long)Monitor.DiskSizeMB)<<20;
		long long MaxCapacity=(long long)atoi(FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"MaxAddressedBlockSizeInTetris:")) * NR_LUN_PER_TETRIS * BytesInBlock * Monitor.tetris_num;

		if(bEnablePrint) {
			printf("FlashCell=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Flash Cell:"));
			model = FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Model:");
			if (model[3] == 'H') {
				cpu_num = 4;
				printf("FormFactor=Half-length,full-height\n");
			} else if (model[3] == 'L') {
				cpu_num = 2;
				printf("FormFactor=Half-length,half-height\n");
			}
			printf("Model=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Model:"));
			printf("SerialNumber=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Serial Number:"));
			printf("RawCapacity=%s\n",PrintSize((long long)DiskBlockSize*BytesInBlock*Monitor.tetris_num));
			printf("AvailableCapacity=%s\n",PrintSize(AvailableCapacity));
			printf("MaxCapacity=%s\n",PrintSize(MaxCapacity));
			printf("MaxWriteCapacity=%s\n",PrintSize((long long)DiskBlockSize*BytesInBlock*Monitor.tetris_num*GetFlashPECycle(pRom)));
			//printf("LogicalReads=%s\n",PrintInteger(Monitor.LogicalRead));
			//printf("RawReads=%s\n",PrintInteger(Monitor.FlashRead));
			printf("AlreadyReadCapacity=%s\n",PrintSize(Monitor.LogicalRead*PAGESIZE));
			//printf("RawReadCapacity=%s\n",PrintSize(Monitor.FlashRead*PAGESIZE));
			//printf("LogicalWrites=%s\n",PrintInteger(Monitor.LogicalWrite));
			//printf("RawWrites=%s\n",PrintInteger(Monitor.FlashWrite));
			printf("AlreadyWriteCapacity=%s\n",PrintSize(Monitor.LogicalWrite*PAGESIZE));
			//printf("RawWriteCapacity=%s\n",PrintSize(Monitor.FlashWrite*PAGESIZE));
			printf("WriteAmplification=%0.2f\n",Monitor.FlashWrite*1.0F/Monitor.LogicalWrite);
			printf("ReadOnly=%s\n",Monitor.dead_die?"Yes":"No");
			printf("LockRom=%s\n", IsRomLocked(Monitor.LockRom)?"Yes":"No");
			printf("Mode=%s\n", PrintMode(AvailableCapacity, MaxCapacity));
		}
		
		if(bEnablePrint) {
			printf("Name=%s\n",PrintDeviceName(Device));
			printf("DriverVersion=%s\n",aDriverVersion);
			printf("DriverSize=%s\n",PrintSize(DriverMemSize));
			printf("FirmwareVersion=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Firmware Version:"));
			printf("FirmwareSize=%s\n",PrintSize(atoi(FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Firmware Size:"))));
                        //printf("DeviceTetrisNumber=%s\n",PrintInteger(Monitor.tetris_num));
		}

		if(IsDetailMode && bEnablePrint) {
			printf("InitializationCounts=%d\n",Monitor.InitCount);
			printf("StartupCounts=%d\n",Monitor.StartCount);
			printf("PCIeLink=%d(v%d)\n",Monitor.LinkWidth, Monitor.LinkGen);

			bool IsOnline=true;
			for(int i=0;i!=cpu_num;++i) {
				if(Monitor.aCommStatus[i]!=ST_STAGE_RUN) {
					IsOnline=false;
				}
			}
			printf("Status=%s\n",IsOnline?"Online":"Offline");
			printf("AlertCode=%llx\n", Monitor.ErrorCode);
			//printf("DeviceDeadDieCount=%d\n",atoi(FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Flash Domain Size:"))*atoi(FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Flash Chip Size:"))-Monitor.HealthyDie);
		}

		if (bEnablePrint) {
			printf("DeviceDeadBlocks=%d\n", Monitor.dead_block);
			printf("DeviceDeadDies=%d\n", Monitor.dead_die);
			printf("DeviceRemainLife=%0.2f%\n",GetDeviceRemainLife(Monitor,pRom));
                        //printf("DeviceAlertCode=%llx\n", Monitor.state);
		}

		if (IsDetailMode && bEnablePrint) {
			int aLatencyBucketIndex[]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
			{
				struct Latency_Mon *pMon=Monitor.aLatency+0;
				int TotalSampleCount=0;
				int aLatencyPercent[LATENCY_BUCKET_STAT];
				memset(aLatencyPercent,0,sizeof aLatencyPercent);
				for(int i=0;i!=LATENCY_BUCKET;++i) {
					TotalSampleCount+=pMon->aBucketCount[i];
					aLatencyPercent[aLatencyBucketIndex[i]]+=pMon->aBucketCount[i];
				}
				if(TotalSampleCount>=10) {
					printf("ReadLatency<%s=%0.1f%%\n",PrintLatency(16.0F),(100.0F*aLatencyPercent[0]/TotalSampleCount));
					printf("ReadLatency%s~%s=%0.1f%%\n",PrintLatency(16.0F),PrintLatency(64.0F),(100.0F*aLatencyPercent[1]/TotalSampleCount));
					printf("ReadLatency%s~%s=%0.1f%%\n",PrintLatency(64.0F),PrintLatency(256.0F),(100.0F*aLatencyPercent[2]/TotalSampleCount));
					printf("ReadLatency%s~%s=%0.1f%%\n",PrintLatency(256.0F),PrintLatency(1024.0F),(100.0F*aLatencyPercent[3]/TotalSampleCount));
					printf("ReadLatency%s~%s=%0.1f%%\n",PrintLatency(1024.0F),PrintLatency(4096.0F),(100.0F*aLatencyPercent[4]/TotalSampleCount));
					printf("ReadLatency%s~%s=%0.1f%%\n",PrintLatency(4096.0F),PrintLatency(16384.0F),(100.0F*aLatencyPercent[5]/TotalSampleCount));
					printf("ReadLatency%s~%s=%0.1f%%\n",PrintLatency(16384.0F),PrintLatency(65536.0F),(100.0F*aLatencyPercent[6]/TotalSampleCount));
					printf("ReadLatency>%s=%0.1f%%\n",PrintLatency(65536.0F),(100.0F*aLatencyPercent[7]/TotalSampleCount));
				}
			}
			{
				struct Latency_Mon *pMon=Monitor.aLatency+1;
				int TotalSampleCount=0;
				int aLatencyPercent[LATENCY_BUCKET_STAT];
				memset(aLatencyPercent,0,sizeof aLatencyPercent);
				for(int i=0;i!=LATENCY_BUCKET;++i) {
					TotalSampleCount+=pMon->aBucketCount[i];
					aLatencyPercent[aLatencyBucketIndex[i]]+=pMon->aBucketCount[i];
				}
				if(TotalSampleCount>=10) {
					printf("WriteLatency<%s=%0.1f%%\n",PrintLatency(16.0F),(100.0F*aLatencyPercent[0]/TotalSampleCount));
					printf("WriteLatency%s~%s=%0.1f%%\n",PrintLatency(16.0F),PrintLatency(64.0F),(100.0F*aLatencyPercent[1]/TotalSampleCount));
					printf("WriteLatency%s~%s=%0.1f%%\n",PrintLatency(64.0F),PrintLatency(256.0F),(100.0F*aLatencyPercent[2]/TotalSampleCount));
					printf("WriteLatency%s~%s=%0.1f%%\n",PrintLatency(256.0F),PrintLatency(1024.0F),(100.0F*aLatencyPercent[3]/TotalSampleCount));
					printf("WriteLatency%s~%s=%0.1f%%\n",PrintLatency(1024.0F),PrintLatency(4096.0F),(100.0F*aLatencyPercent[4]/TotalSampleCount));
					printf("WriteLatency%s~%s=%0.1f%%\n",PrintLatency(4096.0F),PrintLatency(16384.0F),(100.0F*aLatencyPercent[5]/TotalSampleCount));
					printf("WriteLatency%s~%s=%0.1f%%\n",PrintLatency(16384.0F),PrintLatency(65536.0F),(100.0F*aLatencyPercent[6]/TotalSampleCount));
					printf("WriteLatency>%s=%0.1f%%\n",PrintLatency(65536.0F),(100.0F*aLatencyPercent[7]/TotalSampleCount));
				}
			}

			printf("CurrentReadLatency=%s\n",(Monitor.aLatency[0].Count!=0)?PrintLatency(Monitor.aLatency[0].Total_Latency*1.0F/Monitor.aLatency[0].Count):"-");
			printf("CurrentWriteLatency=%s\n",(Monitor.aLatency[1].Count!=0)?PrintLatency(Monitor.aLatency[1].Total_Latency*1.0F/Monitor.aLatency[1].Count):"-");
		}

		struct Disk_Mon &PrevMonitor=aPrevMonitor[DeviceIndex];
		u32 interval=Monitor.CurrentTick-PrevMonitor.CurrentTick;
		float freq=TICKFREQ/(float)interval;

		float ReadIOPS=(Monitor.ReadIOPS-PrevMonitor.ReadIOPS)*freq;
		float ReadBaudwidth=(Monitor.ReadSize-PrevMonitor.ReadSize)*freq*PAGESIZE;
		float WriteIOPS=(Monitor.WriteIOPS-PrevMonitor.WriteIOPS)*freq;
		float WriteBaudwidth=(Monitor.WriteSize-PrevMonitor.WriteSize)*freq*PAGESIZE;
		if (bEnablePrint) {
			printf("CurrentReadIOPS=%s\n",PrintIOPS(ReadIOPS));
			printf("CurrentWriteIOPS=%s\n",PrintIOPS(WriteIOPS));
			printf("CurrentReadBandwidth=%s\n",PrintBandwidth(ReadBaudwidth));
			printf("CurrentWriteBandwidth=%s\n",PrintBandwidth(WriteBaudwidth));
		}

		if(bEnablePrint) {
                        printf("BoardTemperature=%0.1fC\n",GetOnboardTemperature(Monitor.OnboardTemperature));

			float BoardTemperatureMax_tmp = GetOnboardTemperature(Monitor.OnboardTemperature_Max);
			if (BoardTemperatureMax_tmp < 0.1) {
				printf("MaxBoardTemperature=-\n");
			} else {
				printf("MaxBoardTemperature=%0.1fC\n", BoardTemperatureMax_tmp);
			}

			float BoardTemperatureMin_tmp = GetOnboardTemperature(Monitor.OnboardTemperature_Min);
			if (BoardTemperatureMin_tmp > 159.0) {
				printf("MinBoardTemperature=-\n");
			} else {
				printf("MinBoardTemperature=%0.1fC\n", BoardTemperatureMin_tmp);
			}

			printf("CoreTemperature=%0.1fC\n",GetTemperature(Monitor.Temperature));
			printf("MaxCoreTemperature=%0.1fC\n",GetTemperature(Monitor.TemperatureMax));
			printf("MinCoreTemperature=%0.1fC\n",GetTemperature(Monitor.TemperatureMin));
#if 0
			printf("VCore=%s\n",PrintVoltage(GetVoltage(Monitor.VCore)));
			printf("MaxVCore=%s\n",PrintVoltage(GetVoltage(Monitor.VCoreMax)));
			printf("MinVCore=%s\n",PrintVoltage(GetVoltage(Monitor.VCoreMin)));

			printf("VPeripheral=%s\n",PrintVoltage(GetVoltage(Monitor.VPLL)));
			printf("MaxVPeripheral=%s\n",PrintVoltage(GetVoltage(Monitor.VPLLMax)));
			printf("MinVPeripheral=%s\n",PrintVoltage(GetVoltage(Monitor.VPLLMin)));

			printf("VFlash=%s\n",PrintVoltage(1.2 * GetVoltage(Monitor.VFlash)));
			printf("VFlashCore=%s\n",PrintVoltage(2.2 * GetVoltage(Monitor.VFlashCore)));

			printf("VDDR=%s\n",PrintVoltage(GetVoltage(Monitor.VDDR)));
#endif
		}

#if 0
		if(IsDetailMode && bEnablePrint) {
			//printf("FlashType=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Flash Type:"));
			if (cpu_num == 2) {
				printf("MotherCardPN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Mother Card PN:"));
				printf("Card1PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 1 PN/SN:"));
				printf("Card2PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 2 PN/SN:"));
				printf("Card3PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 3 PN/SN:"));
				printf("Card4PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 4 PN/SN:"));
			} else if (cpu_num == 4) {
				printf("MotherCardPN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Mother Card PN:"));
				printf("Card1PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 1 PN/SN:"));
				printf("Card2PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 2 PN/SN:"));
				printf("Card3PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 3 PN/SN:"));
				printf("Card4PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 4 PN/SN:"));
				printf("Card5PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 5 PN/SN:"));
				printf("Card6PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 6 PN/SN:"));
				printf("Card7PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 7 PN/SN:"));
				printf("Card8PN/SN=%s\n",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 8 PN/SN:"));
			}
		}

		if(IsDetailMode && bEnablePrint) {
			float Life=GetDeviceRemainLife(Monitor,pRom);
			unsigned long long Tetris_size = GetDeviceTetrisSize(pRom);
			for (int i = 0; i < Monitor.tetris_num; i++) {
				float Tetris_life = Life - Life_Error[i];
				float error;

				if (Tetris_life > 10.0)
	                		printf("TetrisID=%dCapacity=%sLeftLife=%0.5f%%Status=%s\n", i, PrintSize(Tetris_size), Tetris_life, "Good");
				else
					printf("TetrisID=%dCapacity=%sLeftLife=%0.5f%%Status=%s\n", i, PrintSize(Tetris_size), Tetris_life, "Bad");
			}
		}
#endif
		if (bEnablePrint) {
			printf("game over.\n");
		}

		PrevMonitor=Monitor;
	}
}

void draw_line(int row, int col)
{
	int i;

	for (i = 0; i != col; ++i)
		mvprintw(row, i, "-");
}

static short GetLifeColor(float Life)
{
	if (Life >= 20) {
		return SafeColor;
	} else if (Life >= 10){
		return LightWarnColor;
	} else {
		return WarnColor;
	}
}

static int DisplayLifeWarning(int CurrentRow, float Life)
{
	if (Life >= 20) {
		return CurrentRow;
	} else if (Life >= 10) {
		CurrentRow++;
		attron(COLOR_PAIR(LightWarnColor));
		mvprintw(CurrentRow, 0, "NOTE: Remain Life is Low, Device had better be replaced!");
		attroff(COLOR_PAIR(LightWarnColor));
		return CurrentRow;
	} else {
		CurrentRow++;
		attron(COLOR_PAIR(WarnColor));
		mvprintw(CurrentRow, 0, "WARN: Remain Life is Extremely Low, Device SHOULD be replaced!");
		attroff(COLOR_PAIR(WarnColor));
		return CurrentRow;
	}
}


#define	CHIP_NUMBER	(4)
struct dead_block_value dead_blocks[CHIP_NUMBER] = {
	{"01", 145, 162},
	{"02", 88, 98},
	{"12", 196, 218},
	{"22", 51, 57}
};

static int DisplayDeadBlockWarning(int CurrentRow, const char *model, int dead_block)
{
	int warn_val, error_val;
	int i;
	const char *p = model + strlen(model) - 2;

	if (dead_block < 51) {
		return CurrentRow;
	}

	for (i = 0; i < CHIP_NUMBER; i++) {
		if (!strcmp(p, dead_blocks[i].PSN_END)) {
			warn_val = dead_blocks[i].warn_val;
			error_val = dead_blocks[i].error_val;
			break;
		}
	}

	if (i >= CHIP_NUMBER) {
		return CurrentRow;
	}

	if (dead_block < warn_val) {
		return CurrentRow;
	} else if (dead_block < error_val) {
		CurrentRow++;
		attron(COLOR_PAIR(LightWarnColor));
		mvprintw(CurrentRow, 0, "NOTE: Device had better be replaced.");
		attroff(COLOR_PAIR(LightWarnColor));
		return CurrentRow;
	} else {
		CurrentRow++;
		attron(COLOR_PAIR(WarnColor));
		mvprintw(CurrentRow, 0, "WARN: Device SHOULD be replaced.");
		attroff(COLOR_PAIR(WarnColor));
		return CurrentRow;
	}
}

void DispScreen()
{
	int row, col;
	getmaxyx(stdscr,row,col);
	RowSize=row-1;
	erase();

	int CurrentRow=-ScrollPos-1;

	mvprintw(++CurrentRow,0,"Memblaze Monitor  - Memblaze PCIE Accelerator Monitor",pMonitorVersion);

	CurrentRow+=2;
	attron(A_BOLD|COLOR_PAIR(LogoColor));
	mvprintw(CurrentRow,0,"(c) 2014 Memblaze Technology Co., Ltd (All Rights Reserved)",row,col);
	attroff(A_BOLD|COLOR_PAIR(LogoColor));

	DriverMemSize=GuessDriverMemSize();
	for(char Device='a';Device!='z'+1;++Device) {
		int DeviceIndex=Device-'a';
		int &Handle=aDevice[DeviceIndex];
		const char *model;
		int cpu_num = 0;

		if(Handle==0) {
			char pDevConName[NFS_MAXPATHLEN];
			Handle=open(TranslateDeviceName(PrintDeviceName(Device),pDevConName,NFS_MAXPATHLEN),O_RDONLY);
			if(Handle<0) {
				switch(errno) {
					case EACCES:
						//Permission Denied.
						Handle=PERMISSIONDENIED;
						break;
					case ENOENT:
					case EIO:
						Handle=0;
						break;
				}
			}
			if(Handle>0 && ioctl(Handle,IOCTL_INITIALQUERYSTAT,&aInitMonitor[DeviceIndex])<0) {
				close(Handle);
				Handle=IOCTLERROR;
			}				
		}
		if(Handle==PERMISSIONDENIED) {
			CurrentRow+=3;
			attron(A_BOLD);
			mvprintw(CurrentRow,0,"Permission Denied while opening %s, root account is required",PrintDeviceName(Device));
			attroff(A_BOLD);		
		}

		if(Handle<=0)
			continue;
		struct Disk_Mon Monitor;
		if(ioctl(Handle,IOCTL_QUERYSTAT,&Monitor)<0) {
			close(Handle);
			mvprintw(CurrentRow,0,"%s IOCTL_QUERYSTAT error",PrintDeviceName(Device));
			Handle=IOCTLERROR;
			continue;
		}

		DriverMemSize=Monitor.DriverMemSize;
		const char *pRom=aInitMonitor[DeviceIndex].aRom;
		memcpy(aDriverVersion,aInitMonitor[DeviceIndex].aDriverVersion,sizeof(aDriverVersion));

		int DiskBlockSize=GetFlashChipSize(pRom)*GetFlashLunSize(pRom);
		int BytesInBlock=GetFlashSectorSize(pRom)*GetFlashPlaneSize(pRom)*GetFlashPageSize(pRom)*GetFlashBlockSize(pRom);
		long long AvailableCapacity=((long long)Monitor.DiskSizeMB)<<20;
		long long MaximumCapacity=(long long)atoi(FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"MaxAddressedBlockSizeInTetris:")) * NR_LUN_PER_TETRIS * BytesInBlock * Monitor.tetris_num;
		++CurrentRow;
		mvprintw(CurrentRow,0,"Basic Information");
		++CurrentRow;
		draw_line(CurrentRow, col);
		++CurrentRow;
		mvprintw(CurrentRow,0,"Manufacture Name:\t\tMemblaze Technology Co.,Ltd");
		++CurrentRow;
		mvprintw(CurrentRow,0,"Product Name:\t\t\tPBlaze3");
		++CurrentRow;
		mvprintw(CurrentRow,0,"Flash Cell:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Flash Cell:"));

		model = FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Model:");
		++CurrentRow;
		mvprintw(CurrentRow,0,"Model:\t\t\t\t%s",model);
		++CurrentRow;
		if (model[3] == 'H') {
			cpu_num = 4;
			mvprintw(CurrentRow,0,"Form Factor:\t\t\tHalf length, full height");
		} else if (model[3] == 'L') {
			cpu_num = 2;
			mvprintw(CurrentRow,0,"Form Factor:\t\t\tHalf length, half height");
		}
		++CurrentRow;
		mvprintw(CurrentRow,0,"Serial Number:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Serial Number:"));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Raw Capacity:\t\t\t%s",PrintSize((long long)DiskBlockSize*BytesInBlock*Monitor.tetris_num));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Available Capacity:\t\t%s",PrintSize(AvailableCapacity));
		//++CurrentRow;
		//mvprintw(CurrentRow,0,"Used Capacity:");
		//++CurrentRow;
		//mvprintw(CurrentRow,0,"Left Capacity:");
		++CurrentRow;
		mvprintw(CurrentRow,0,"Maximum Capacity:\t\t%s",PrintSize(MaximumCapacity));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Max Write Capacity:\t\t%s",PrintSize((long long)DiskBlockSize*BytesInBlock * Monitor.tetris_num * GetFlashPECycle(pRom)));

		//++CurrentRow;
		//mvprintw(CurrentRow,0,"Logical/ Raw Reads:\t\t%s/ %s",PrintInteger(Monitor.LogicalRead),PrintInteger(Monitor.FlashRead));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Already Read Capacity:\t\t%s",PrintSize(Monitor.LogicalRead*PAGESIZE));
		//++CurrentRow;
		//mvprintw(CurrentRow,0,"Logical/ Raw Writes:\t\t%s/ %s",PrintInteger(Monitor.LogicalWrite),PrintInteger(Monitor.FlashWrite));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Already Write Capacity:\t\t%s",PrintSize(Monitor.LogicalWrite*PAGESIZE));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Write Amplification:\t\t%0.2f",Monitor.FlashWrite*1.0F/Monitor.LogicalWrite);

		CurrentRow++;
		CurrentRow++;
		mvprintw(CurrentRow,0,"Device Information");
		CurrentRow++;
		draw_line(CurrentRow, col);
		CurrentRow++;
		attron(A_BOLD);
		mvprintw(CurrentRow,0,"Name:");
		mvprintw(CurrentRow,32,"%s",PrintDeviceName(Device));
		attroff(A_BOLD);
		++CurrentRow;
		mvprintw(CurrentRow,0,"Driver:\t\t\t\t%s, %s",aDriverVersion, PrintSize(DriverMemSize));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Firmware:\t\t\t%s, %s",
			FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Firmware Version:"),
			PrintSize(atoi(FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Firmware Size:"))));

		++CurrentRow;
		mvprintw(CurrentRow,0,"Initialization Counts:\t\t%d",Monitor.InitCount);
		++CurrentRow;
		mvprintw(CurrentRow,0,"Starup Counts:\t\t\t%d",Monitor.StartCount);

		++CurrentRow;
		mvprintw(CurrentRow,0,"PCIe Link:");
		int IsLinkGood=Monitor.LinkWidth>=8;
		attron(COLOR_PAIR(IsLinkGood?SafeColor:WarnColor));
		mvprintw(CurrentRow,32,"x%d(Gen%d)",Monitor.LinkWidth, Monitor.LinkGen);
		attroff(COLOR_PAIR(IsLinkGood?SafeColor:WarnColor));

		++CurrentRow;
		mvprintw(CurrentRow,0,"Status:");
		bool IsOnline=true;
		for(int i=0;i!=cpu_num;++i) {
			if(Monitor.aCommStatus[i]!=ST_STAGE_RUN) {
				IsOnline=false;
			}
		}
		attron(COLOR_PAIR(IsOnline?SafeColor:WarnColor));
		mvprintw(CurrentRow,32,"%s",IsOnline?"Online":"Offline");
		attroff(COLOR_PAIR(IsOnline?SafeColor:WarnColor));

		++CurrentRow;
		mvprintw(CurrentRow,0,"Read only:");
		attron(COLOR_PAIR(Monitor.dead_die?WarnColor:SafeColor));
		mvprintw(CurrentRow,32,"%s",Monitor.dead_die?"Yes":"No");
		attroff(COLOR_PAIR(Monitor.dead_die?WarnColor:SafeColor));

		++CurrentRow;
		mvprintw(CurrentRow,0,"Alert Code:\t\t\t%llx", Monitor.ErrorCode);
#if 0

		if(IsDetailMode) {
			++CurrentRow;
			mvprintw(CurrentRow,0,"Flash Type:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Flash Type:"));

		}

		++CurrentRow;
		mvprintw(CurrentRow,0,"Device Tetris Number:\t\t%s",PrintInteger(Monitor.tetris_num));

		++CurrentRow;
		mvprintw(CurrentRow,0,"Device Raw Capacity:\t\t%s",PrintSize((long long)DiskBlockSize*BytesInBlock*Monitor.tetris_num));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Device Active Capacity:\t\t%s",PrintSize(((long long)Monitor.DiskSizeMB)<<20));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Device Maximum Capacity:\t%s",PrintSize((long long)atoi(FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"MaxAddressedBlockSizeInTetris:")) * NR_LUN_PER_TETRIS * BytesInBlock * Monitor.tetris_num));
		++CurrentRow;

		mvprintw(CurrentRow,0,"Device Maximum Write Size:\t%s",PrintSize((long long)DiskBlockSize*BytesInBlock * Monitor.tetris_num * GetFlashPECycle(pRom)));

		++CurrentRow;
		mvprintw(CurrentRow,0,"Logical/ Raw Reads:\t\t%s/ %s",PrintInteger(Monitor.LogicalRead),PrintInteger(Monitor.FlashRead));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Logical/ Raw Read Bytes:\t%s/ %s",PrintSize(Monitor.LogicalRead*PAGESIZE),PrintSize(Monitor.FlashRead*PAGESIZE));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Logical/ Raw Writes:\t\t%s/ %s",PrintInteger(Monitor.LogicalWrite),PrintInteger(Monitor.FlashWrite));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Logical/ Raw Write Bytes:\t%s/ %s",PrintSize(Monitor.LogicalWrite*PAGESIZE),PrintSize(Monitor.FlashWrite*PAGESIZE));

		++CurrentRow;
		mvprintw(CurrentRow,0,"FlyingPipePercent:\t\t%d",Monitor.FlyingPipePercent);
#endif

		++CurrentRow;
		mvprintw(CurrentRow,0,"Device Dead Blocks:\t\t%d",Monitor.dead_block);
		CurrentRow = DisplayDeadBlockWarning(CurrentRow, model, Monitor.dead_block / (Monitor.tetris_num * 8));

		++CurrentRow;
		mvprintw(CurrentRow,0,"Device Dead Die:\t\t%d",Monitor.dead_die);

		++CurrentRow;
		mvprintw(CurrentRow,0,"Device Remain Life:");
		float Life=GetDeviceRemainLife(Monitor,pRom);
		short ProgressColor=GetLifeColor(Life);
		attron(COLOR_PAIR(ProgressColor));
		mvprintw(CurrentRow,32,"|");
		for(int i=0;i!=20;++i) {
			mvprintw(CurrentRow,33+i,"%c",(i*5+Life>=100)?'-':'>');
		}
		mvprintw(CurrentRow,53,"|");
		attroff(COLOR_PAIR(ProgressColor));
		mvprintw(CurrentRow,56,"(%1.2f%%)",Life);

		CurrentRow = DisplayLifeWarning(CurrentRow, Life);

		struct Disk_Mon &PrevMonitor=aPrevMonitor[DeviceIndex];
		u32 interval=Monitor.CurrentTick-PrevMonitor.CurrentTick;
		float freq=TICKFREQ/(float)interval;

		float ReadIOPS=(Monitor.ReadIOPS-PrevMonitor.ReadIOPS)*freq;
		float ReadBaudwidth=(Monitor.ReadSize-PrevMonitor.ReadSize)*freq*PAGESIZE;
		float WriteIOPS=(Monitor.WriteIOPS-PrevMonitor.WriteIOPS)*freq;
		float WriteBaudwidth=(Monitor.WriteSize-PrevMonitor.WriteSize)*freq*PAGESIZE;

		if(IsDetailMode) {
			CurrentRow++;
			CurrentRow++;
			mvprintw(CurrentRow,0,"Performance Information");
			CurrentRow++;
			draw_line(CurrentRow, col);

			CurrentRow++;
			mvprintw(CurrentRow,TABSIZE*4,"<%s",PrintLatency(16.0F));
			mvprintw(CurrentRow,TABSIZE*6,"%s~%s",PrintLatency(16.0F),PrintLatency(64.0F));
			mvprintw(CurrentRow,TABSIZE*8,"%s~%s",PrintLatency(64.0F),PrintLatency(256.0F));
			mvprintw(CurrentRow,TABSIZE*10,"%s~%s",PrintLatency(256.0F),PrintLatency(1024.0F));
			mvprintw(CurrentRow,TABSIZE*12,"%s~%s",PrintLatency(1024.0F),PrintLatency(4096.0F));
			mvprintw(CurrentRow,TABSIZE*14,"%s~%s",PrintLatency(4096.0F),PrintLatency(16384.0F));
			mvprintw(CurrentRow,TABSIZE*16,"%s~%s",PrintLatency(16384.0F),PrintLatency(65536.0F));
			mvprintw(CurrentRow,TABSIZE*18,">%s",PrintLatency(65536.0F));

			int aLatencyBucketIndex[]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};

			{
				struct Latency_Mon *pMon=Monitor.aLatency+0;
				int TotalSampleCount=0;
				int aLatencyPercent[LATENCY_BUCKET_STAT];
				memset(aLatencyPercent,0,sizeof aLatencyPercent);
				for(int i=0;i!=LATENCY_BUCKET;++i) {
					TotalSampleCount+=pMon->aBucketCount[i];
					aLatencyPercent[aLatencyBucketIndex[i]]+=pMon->aBucketCount[i];
				}

				++CurrentRow;
				mvprintw(CurrentRow,0,"Read Latency");
				if(TotalSampleCount<10) {
					mvprintw(CurrentRow,TABSIZE*4,"-\t\t-\t\t-\t\t-\t\t-\t\t-\t\t-\t\t-");
				} else {
					for(int i=0;i!=LATENCY_BUCKET_STAT;++i) {
						mvprintw(CurrentRow,TABSIZE*(4+i*2),"%0.1f%%",(100.0F*aLatencyPercent[i]/TotalSampleCount));
					}
				}
			}

			{
				struct Latency_Mon *pMon=Monitor.aLatency+1;
				int TotalSampleCount=0;
				int aLatencyPercent[LATENCY_BUCKET_STAT];
				memset(aLatencyPercent,0,sizeof aLatencyPercent);
				for(int i=0;i!=LATENCY_BUCKET;++i) {
					TotalSampleCount+=pMon->aBucketCount[i];
					aLatencyPercent[aLatencyBucketIndex[i]]+=pMon->aBucketCount[i];
				}

				++CurrentRow;
				mvprintw(CurrentRow,0,"Write Latency");
				if(TotalSampleCount<10) {
					mvprintw(CurrentRow,TABSIZE*4,"-\t\t-\t\t-\t\t-\t\t-\t\t-\t\t-\t\t-");
				} else {
					for(int i=0;i!=LATENCY_BUCKET_STAT;++i) {
						mvprintw(CurrentRow,TABSIZE*(4+i*2),"%0.1f%%",(100.0F*aLatencyPercent[i]/TotalSampleCount));
					}
				}
			}

			++CurrentRow;
			mvprintw(CurrentRow,0,"Current Read Latency:\t\t%s",(Monitor.aLatency[0].Count!=0)?PrintLatency(Monitor.aLatency[0].Total_Latency*1.0F/Monitor.aLatency[0].Count):"-");
			++CurrentRow;
			mvprintw(CurrentRow,0,"Current Write Latency:\t\t%s",(Monitor.aLatency[1].Count!=0)?PrintLatency(Monitor.aLatency[1].Total_Latency*1.0F/Monitor.aLatency[1].Count):"-");

			++CurrentRow;
			mvprintw(CurrentRow,0,"Current Read(IOPS):\t\t%s",PrintIOPS(ReadIOPS));
			++CurrentRow;
			mvprintw(CurrentRow,0,"Current Write(IOPS):\t\t%s",PrintIOPS(WriteIOPS));
			++CurrentRow;
			mvprintw(CurrentRow,0,"Current Read(MB/s):\t\t%s",PrintBandwidth(ReadBaudwidth));
			++CurrentRow;
			mvprintw(CurrentRow,0,"Current Write(MB/s):\t\t%s",PrintBandwidth(WriteBaudwidth));
			++CurrentRow;
			{
				long long MaxCapGB;
				MaxCapGB = MaximumCapacity >> 30;
				if (MaxCapGB > 0 && MaxCapGB < 1000) {
					if (AvailableCapacity * 3 / 2 <= MaximumCapacity)
						mvprintw(CurrentRow,0,"Mode:\t\t\t\tExtreme-Speed");
					else
						mvprintw(CurrentRow,0,"Mode:\t\t\t\tHigh-Speed");
				}  else if (MaxCapGB > 1000 && MaxCapGB < 2000) {
					if (AvailableCapacity * 3 / 2 <= MaximumCapacity)
						mvprintw(CurrentRow,0,"Mode:\t\t\t\tExtreme-Speed");
					else
						mvprintw(CurrentRow,0,"Mode:\t\t\t\tHigh-Speed");
				} else {
					if (AvailableCapacity * 2 <= MaximumCapacity)
						mvprintw(CurrentRow,0,"Mode:\t\t\t\tExtreme-Speed");
					else
						mvprintw(CurrentRow,0,"Mode:\t\t\t\tHigh-Speed");
				}
			}
		}

		CurrentRow++;
		CurrentRow++;
		mvprintw(CurrentRow,0,"Temperature Information");
		CurrentRow++;
		draw_line(CurrentRow, col);
		CurrentRow++;
		mvprintw(CurrentRow,0,"\t\t\t\tCurrent\t\tMax\t\tMin");
		++CurrentRow;
		mvprintw(CurrentRow,0,"Flash Board Temperature\t\t%s\t\t%s\t\t%s",PrintTemperature(GetOnboardTemperature(Monitor.OnboardTemperature)), PrintTemperature(GetOnboardTemperature(Monitor.OnboardTemperature_Max)), PrintTemperature(GetOnboardTemperature(Monitor.OnboardTemperature_Min)));
		++CurrentRow;
		mvprintw(CurrentRow,0,"Controller Temperature\t\t%s\t\t%s\t\t%s",PrintTemperature(GetTemperature(Monitor.Temperature)),PrintTemperature(GetTemperature(Monitor.TemperatureMax)),PrintTemperature(GetTemperature(Monitor.TemperatureMin)));
#if 0
		++CurrentRow;
		mvprintw(CurrentRow,0,"VCore\t\t\t%s\t\t%s\t\t%s",PrintVoltage(GetVoltage(Monitor.VCore)),PrintVoltage(GetVoltage(Monitor.VCoreMax)),PrintVoltage(GetVoltage(Monitor.VCoreMin)));
		++CurrentRow;
		mvprintw(CurrentRow,0,"VPeripheral\t\t%s\t\t%s\t\t%s",PrintVoltage(GetVoltage(Monitor.VPLL)),PrintVoltage(GetVoltage(Monitor.VPLLMax)),PrintVoltage(GetVoltage(Monitor.VPLLMin)));
		++CurrentRow;
		mvprintw(CurrentRow,0,"VDDR\t\t\t%s\t\t%s\t\t%s",PrintVoltage(GetVoltage(Monitor.VDDR)), "-", "-");
		++CurrentRow;
		mvprintw(CurrentRow,0,"VFlash\t\t\t%s\t\t%s\t\t%s",PrintVoltage(1.2*GetVoltage(Monitor.VFlash)), "-", "-");
		++CurrentRow;
		mvprintw(CurrentRow,0,"VFlashCore\t\t%s\t\t%s\t\t%s",PrintVoltage(2.2*GetVoltage(Monitor.VFlashCore)), "-", "-");
#endif
			
		bool IsHealthy = !Monitor.TemperatureUnsafe;
		++CurrentRow;
		mvprintw(CurrentRow,0,"Status:");
		attron(COLOR_PAIR(IsHealthy?SafeColor:WarnColor));
		mvprintw(CurrentRow,32,"%s",IsHealthy?"Safe":"Alert");
		attroff(COLOR_PAIR(IsHealthy?SafeColor:WarnColor));

#if 0
                ++CurrentRow;
                mvprintw(CurrentRow,0,"Mother Card PN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Mother Card PN:"));
                if (IsDetailMode) {
			if (cpu_num == 2) {
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 1 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 1 PN/SN:"));
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 2 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 2 PN/SN:"));
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 3 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 3 PN/SN:"));
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 4 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 4 PN/SN:"));
			} else if (cpu_num == 4) {
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 1 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 1 PN/SN:"));
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 2 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 2 PN/SN:"));
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 3 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 3 PN/SN:"));
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 4 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 4 PN/SN:"));
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 5 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 5 PN/SN:"));
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 6 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 6 PN/SN:"));
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 7 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 7 PN/SN:"));
                        	++CurrentRow;
				mvprintw(CurrentRow,0,"Card 8 PN/SN:\t\t\t%s",FindValueBasedonKey(aInitMonitor[DeviceIndex].aRom,"Card 8 PN/SN:"));
			}
                }

		if (IsDetailMode) {
			CurrentRow+=2;
			draw_line(CurrentRow, col);
			++CurrentRow;
			mvprintw(CurrentRow,0,"Tetris RAID Information");
	                ++CurrentRow;
			++CurrentRow;
	                mvprintw(CurrentRow,0,"Tetris ID\tCapacity\tLeft life\tStatus");

			/* A small trick, use the same life */
			float Life=GetDeviceRemainLife(Monitor,pRom);
			unsigned long long Tetris_size = GetDeviceTetrisSize(pRom);
			for (int i = 0; i < Monitor.tetris_num; i++) {
				float Tetris_life = Life - Life_Error[i];
				float error;

				++CurrentRow;
				if (Tetris_life > 10.0) {
	                		mvprintw(CurrentRow,0,"%d\t\t%s\t\t%0.5f%", i, PrintSize(Tetris_size), Tetris_life);
					attron(COLOR_PAIR(SafeColor));
					mvprintw(CurrentRow,48,"%s","Good");
					attroff(COLOR_PAIR(SafeColor));
				} else {
					mvprintw(CurrentRow,0,"%d\t\t%s\t\t%0.5f%", i, PrintSize(Tetris_size), Tetris_life);
					attron(COLOR_PAIR(WarnColor));
					mvprintw(CurrentRow,48,"%s","Bad");
					attroff(COLOR_PAIR(WarnColor));
				}
			}

	                //++CurrentRow;
	                //mvprintw(CurrentRow,0,"Status:%s", "enable");
		}
#endif

		++CurrentRow;

		PrevMonitor=Monitor;
	}
	Margin=CurrentRow-row+2;
	refresh();
}

void *Worker(void *arg)
{
	bool IsNeedResponse=false;
	while(!IsNeedClose) {
		DispScreen();
		if(IsNeedResponse) {
			sem_post(&SemResponse);
			IsNeedResponse=false;
		}
		struct timeval CurrentTime;
		gettimeofday(&CurrentTime,NULL);
		int us=CurrentTime.tv_usec+RefreshRate*1000;

		struct timespec WaitTime;
		WaitTime.tv_sec=CurrentTime.tv_sec+us/1000000;
		WaitTime.tv_nsec=(us%1000000)*1000;
		if(sem_timedwait(&SemRequest,&WaitTime)!=ETIMEDOUT) {
			IsNeedResponse=true;
		}
	}
}

void Initialize()
{
	sem_init(&SemRequest,0,0);
	sem_init(&SemResponse,0,0);
	CacTSCInterval();
	if(IsInteractiveMode) {
		initscr();

		if(has_colors()) {
			start_color();
			use_default_colors();
			init_pair(LogoColor, COLOR_BLUE, -1);
			init_pair(SpecialColor,COLOR_MAGENTA,-1);
			init_pair(SafeColor, COLOR_GREEN, -1);
			init_pair(WarnColor, COLOR_RED, -1);
			init_pair(LightWarnColor, COLOR_YELLOW, -1);
		} else {
			LogoColor=0;
			SafeColor=0;
			WarnColor=0;
			SpecialColor=0;
		}
		raw();
	//	cbreak();
		keypad(stdscr, TRUE);
		noecho();
	}
}

void Destroy()
{
	if(IsInteractiveMode) {
		endwin();                    /* End curses mode                */
	}
	for(char Device='a';Device!='z'+1;++Device) {
		int Handle=aDevice[Device-'a'];
		if(Handle>0)
			close(Handle);
	}
	sem_destroy(&SemResponse);
	sem_destroy(&SemRequest);
}

int Help(const char *pcmd)
{
	printf("MEMBlaze Disk Management Tool Set\n");
	printf("Usage: %s [-h] [-p] [-l] [-r] RefreshRate. Default is interactive mode. Use key T to toggle between compact and detailed display mode, use Up Down PageUp PageDown to scroll the view, use Ctrl-C to exit the program.\n",pcmd);
	printf("\t-h:\tPrint this help message\n");
	printf("\t-p:\tPlain Mode: Disk information will be printed directly in terminal in plain text format, facilitate user to log messages in files.\n");
	printf("\t-l:\tThe same as Plain Mode, except it will only display for one single time, useful for script programming\n");
	printf("\t-r:\tRefresh Interval: Change the default refresh interval. This value should be positive integer specify the refresh interval in milliseconds. Default to 2000 (2s).\n");
	return 0;
}

int main(int argc, char *argv[])
{
	int c;

	init_life_error_array();

	while (1) {
		c = getopt(argc, argv, "pPlLr:R:hH");
		if (c < 0)
			break;

		switch (c) {
			case 'p':
			case 'P':
				IsInteractiveMode = false;
				IsPlainMode = true;
				break;
			case 'l':
			case 'L':
				IsInteractiveMode = false;
				IsPlainMode = false;
				break;
			case 'r':
			case 'R':
				RefreshRate = atoi(optarg);
				if (RefreshRate < 0) {
					printf("Invalid Commandline Options\n\n");
					return Help(argv[0]);
				}
				break;
			case 'h':
			case 'H':
			default:
				Help(argv[0]);
				return -1;
		}
	}

	/* case such as "memmonitor -" should not pass */
	if (argc != optind) {
		Help(argv[0]);
		return -1;
	}

	if(RefreshRate <= 0) {
		Help(argv[0]);
		return -1;
	} 

	Initialize();
	if(IsInteractiveMode) {
		if(pthread_create(&DispThread,NULL,Worker,NULL)!=0) {
			printf("Error Create Display Thread\n");
			return -1;
		}
		while(true) {
			int ch=getch();

			switch (ch) {
			case KEY_UP:
				--ScrollPos;
				if(ScrollPos<0)
					ScrollPos=0;
				break;
			case KEY_DOWN:
				if(Margin>0)
					++ScrollPos;
				break;
			case KEY_PPAGE:
				ScrollPos-=RowSize;
				if(ScrollPos<0)
					ScrollPos=0;
				break;
			case KEY_NPAGE:
				if(Margin>RowSize)
					ScrollPos+=RowSize;
				else
					ScrollPos+=Margin;
				break;
			case 't':
			case 'T':
				IsDetailMode=!IsDetailMode;
				break;
			case '\003'://Ctrl+C
				goto _exit;
			case EOF:
				break;
			default:
				continue;
			}
			sem_post(&SemRequest);
			sem_wait(&SemResponse);
		}
_exit:
		IsNeedClose=true;
		sem_post(&SemRequest);
		void *RetPtr;
		pthread_join(DispThread,&RetPtr);
	} else if (IsPlainMode){
		struct timeval CurrentTime;
		struct timespec WaitTime;
		bool bEnablePrint = false;

                do {
			PrintInformation(bEnablePrint);
			if(bNorefresh && bEnablePrint) {
				break;
			}
			bEnablePrint = true;
			gettimeofday(&CurrentTime,NULL);
			int us=CurrentTime.tv_usec+RefreshRate*1000;

		        WaitTime.tv_sec=CurrentTime.tv_sec+us/1000000;
		        WaitTime.tv_nsec=(us%1000000)*1000;
		}
		while(sem_timedwait(&SemRequest,&WaitTime)!=ETIMEDOUT);
	} else {
                PrintInformation(false);
                usleep(100000);
                PrintInformation(true);
        }

	Destroy();
	return EXIT_SUCCESS;
}
