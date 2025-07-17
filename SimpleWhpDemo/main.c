#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <WinHvPlatform.h>
#include <WinHvEmulation.h>
#include "vmdef.h"

#define CGA_COLS 80
#define CGA_ROWS 25
#define DEFAULT_BIOS "ami_8088_bios_31jan89.bin"
#define FALLBACK_BIOS "ivt.fw"
static USHORT CgaBuffer[CGA_COLS*CGA_ROWS];
static UINT32 CgaCursor = 0;

static void CgaPutChar(char ch)
{
        if(ch=='\r')
        {
                CgaCursor -= CgaCursor % CGA_COLS;
                return;
        }
        if(ch=='\n')
        {
                CgaCursor += CGA_COLS;
        }
        else
        {
                if(CgaCursor>=CGA_COLS*CGA_ROWS)
                {
                        memmove(CgaBuffer, CgaBuffer + CGA_COLS,
                                sizeof(USHORT)*(CGA_COLS*(CGA_ROWS-1)));
                        for(UINT32 i=0;i<CGA_COLS;i++)
                                CgaBuffer[CGA_COLS*(CGA_ROWS-1)+i]=0x0720;
                        CgaCursor -= CGA_COLS;
                }
                CgaBuffer[CgaCursor++] = (0x07 << 8) | (UCHAR)ch;
        }
        if(CgaCursor>=CGA_COLS*CGA_ROWS)
                CgaCursor = CGA_COLS*CGA_ROWS-1;
}

static void PrintCgaBuffer()
{
        puts("\n----- CGA Text Buffer -----");
        USHORT* vram = NULL;
        if (VirtualMemory)
                vram = (USHORT*)((PUCHAR)VirtualMemory + 0xB8000);
        for(UINT32 r=0;r<CGA_ROWS;r++)
        {
                for(UINT32 c=0;c<CGA_COLS;c++)
                {
                        USHORT cell = CgaBuffer[r*CGA_COLS+c];
                        if (vram) cell = vram[r*CGA_COLS+c];
                        char ch = (char)(cell & 0xFF);
                        if(ch==0) ch=' ';
                        putc(ch, stdout);
                }
                putc('\n', stdout);
        }
}

HRESULT SwCheckSystemHypervisor()
{
	UINT32 ReturnLength;
	HRESULT hr = WHvGetCapability(WHvCapabilityCodeHypervisorPresent, &HypervisorPresence, sizeof(BOOL), &ReturnLength);
	if (hr == S_OK && HypervisorPresence)
	{
		WHvGetCapability(WHvCapabilityCodeFeatures, &CapFeat, sizeof(CapFeat), &ReturnLength);
		WHvGetCapability(WHvCapabilityCodeExtendedVmExits, &ExtExitFeat, sizeof(ExtExitFeat), &ReturnLength);
		WHvGetCapability(WHvCapabilityCodeProcessorFeatures, &ProcFeat, sizeof(ProcFeat), &ReturnLength);
		WHvGetCapability(WHvCapabilityCodeProcessorXsaveFeatures, &XsaveFeat, sizeof(XsaveFeat), &ReturnLength);
		puts("Hypervisor is Present to run SimpleWhpDemo!");
	}
	else
	{
		printf("Failed to run SimpleWhpDemo! Hypervisor is %s! Code=0x%X!\n", HypervisorPresence ? "Present" : "Not Present", hr);
		if (!HypervisorPresence)hr = S_FALSE;
	}
	return hr;
}

void SwTerminateVirtualMachine()
{
	VirtualFree(VirtualMemory, 0, MEM_RELEASE);
	WHvDeleteVirtualProcessor(hPart, 0);
	WHvDeletePartition(hPart);
	WHvEmulatorDestroyEmulator(hEmu);
}

HRESULT SwInitializeVirtualMachine()
{
	BOOL EmulatorCreated = FALSE;
	BOOL PartitionCreated = FALSE;
	BOOL VcpuCreated = FALSE;
	BOOL MemoryAllocated = FALSE;
	// Create an emulator.
	HRESULT hr = WHvEmulatorCreateEmulator(&EmuCallbacks, &hEmu);
	if (hr == S_OK)
		EmulatorCreated = TRUE;
	else
		goto Cleanup;
	// Create a virtual machine.
	hr = WHvCreatePartition(&hPart);
	if (hr == S_OK)
		PartitionCreated = TRUE;
	else
		goto Cleanup;
	// Setup Partition Properties.
	hr = WHvSetPartitionProperty(hPart, WHvPartitionPropertyCodeProcessorCount, &SwProcessorCount, sizeof(SwProcessorCount));
	if (hr != S_OK)
	{
		printf("Failed to setup Processor Count! HRESULT=0x%X\n", hr);
		goto Cleanup;
	}
	// Setup Partition
	hr = WHvSetupPartition(hPart);
	if (hr != S_OK)
	{
		printf("Failed to setup Virtual Machine! HRESULT=0x%X\n", hr);
		goto Cleanup;
	}
	// Create Virtual Memory.
	VirtualMemory = VirtualAlloc(NULL, GuestMemorySize, MEM_COMMIT, PAGE_READWRITE);
	if (VirtualMemory)
		MemoryAllocated = TRUE;
	else
		goto Cleanup;
	RtlZeroMemory(VirtualMemory, GuestMemorySize);
        hr = WHvMapGpaRange(hPart, VirtualMemory, 0, GuestMemorySize, WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute);
        if (hr != S_OK) goto Cleanup;
        /* Map the same memory a second time to mirror addresses above 1MB
           so that real-mode wraparound (A20 line low) works correctly. */
        hr = WHvMapGpaRange(hPart, VirtualMemory, GuestMemorySize, GuestMemorySize,
                           WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite | WHvMapGpaRangeFlagExecute);
        if (hr != S_OK) goto Cleanup;
	// Create Virtual Processors.
	hr = WHvCreateVirtualProcessor(hPart, 0, 0);
	if (hr == S_OK)
		VcpuCreated = TRUE;
	else
		goto Cleanup;
	// Initialize Virtual Processor State
	hr = WHvSetVirtualProcessorRegisters(hPart, 0, SwInitGprNameGroup, 0x12, SwInitGprValueGroup);
	if (hr != S_OK)
	{
		printf("Failed to initialize General Purpose Registers! HRESULT=0x%X\n", hr);
		goto Cleanup;
	}
	hr = WHvSetVirtualProcessorRegisters(hPart, 0, SwInitSrNameGroup, 8, (WHV_REGISTER_VALUE*)SwInitSrValueGroup);
	if (hr != S_OK)
	{
		printf("Failed to initialize Segment Registers! HRESULT=0x%X\n", hr);
		goto Cleanup;
	}
	hr = WHvSetVirtualProcessorRegisters(hPart, 0, SwInitDescriptorNameGroup, 2, (WHV_REGISTER_VALUE*)SwInitDescriptorValueGroup);
	if (hr != S_OK)
	{
		printf("Failed to initialize Descriptor Tables! HRESULT=0x%X\n", hr);
		goto Cleanup;
	}
	hr = WHvSetVirtualProcessorRegisters(hPart, 0, SwInitCrNameGroup, 4, SwInitCrValueGroup);
	if (hr != S_OK)
	{
		printf("Failed to initialize Control Registers! HRESULT=0x%X\n", hr);
		goto Cleanup;
	}
	hr = WHvSetVirtualProcessorRegisters(hPart, 0, SwInitDrNameGroup, 6, SwInitDrValueGroup);
	if (hr != S_OK)
	{
		printf("Failed to initialize Debug Registers! HRESULT=0x%X\n", hr);
		goto Cleanup;
	}
	hr = WHvSetVirtualProcessorRegisters(hPart, 0, SwInitXcrNameGroup, 1, SwInitXcrValueGroup);
	if (hr != S_OK)
	{
		printf("Failed to initialize Extended Control Registers! HRESULT=0x%X\n", hr);
		goto Cleanup;
	}
	hr = WHvSetVirtualProcessorRegisters(hPart, 0, &SwInitFpcsName, 1, (WHV_REGISTER_VALUE*)&SwInitFpcsValue);
	if (hr != S_OK)
	{
		printf("Failed to initialize x87 Floating Point Control Status! HRESULT=0x%X\n", hr);
		goto Cleanup;
	}
	return S_OK;
Cleanup:
	if (MemoryAllocated)VirtualFree(VirtualMemory, 0, MEM_RELEASE);
	if (VcpuCreated)WHvDeleteVirtualProcessor(hPart, 0);
	if (PartitionCreated)WHvDeletePartition(hPart);
	if (EmulatorCreated)WHvEmulatorDestroyEmulator(hEmu);
	return S_FALSE;
}

HRESULT SwDumpVirtualProcessorGprState()
{
	WHV_REGISTER_VALUE RegVal[0x12];
	HRESULT hr = WHvGetVirtualProcessorRegisters(hPart, 0, SwInitGprNameGroup, 0x12, RegVal);
	if (hr == S_OK)
	{
		puts("============ Dumping General-Purpose Registers ============");
		puts("Name\t Value");
		for (UINT32 i = 0; i < 0x12; i++)
			printf("%s\t 0x%016llX\n", SwGprNameGroup[i], RegVal[i].Reg64);
		return hr;
	}
	printf("Failed to dump General-Purpose Registers! HRESULT=0x%X\n", hr);
	return hr;
}

HRESULT SwDumpVirtualProcessorSegmentState()
{
	WHV_REGISTER_VALUE RegVal[8];
	HRESULT hr = WHvGetVirtualProcessorRegisters(hPart, 0, SwInitSrNameGroup, 8, RegVal);
	if (hr == S_OK)
	{
		puts("============ Dumping Segment Registers ============");
		puts("Name\t Selector\t Attributes\t Limit\t\t Base");
		for (UINT32 i = 0; i < 8; i++)
			printf("%s\t 0x%04X\t\t 0x%04X\t\t 0x%08X\t 0x%016llX\n", SwSrNameGroup[i], RegVal[i].Segment.Selector, RegVal[i].Segment.Attributes, RegVal[i].Segment.Limit, RegVal[i].Segment.Base);
		return hr;
	}
	printf("Failed to dump General-Purpose Registers! HRESULT=0x%X\n", hr);
	return hr;
}

static BOOL LoadVirtualMachineProgramEx(IN PSTR FileName, IN ULONG Offset, OUT DWORD* BytesRead)
{
        BOOL Result = FALSE;
        HANDLE hFile = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
                DWORD FileSize = GetFileSize(hFile, NULL);
                if (FileSize != INVALID_FILE_SIZE)
                {
                        DWORD dwSize = 0;
                        PVOID Address = (PVOID)((ULONG_PTR)VirtualMemory + Offset);
                        Result = ReadFile(hFile, Address, FileSize, &dwSize, NULL);
                        if (Result && BytesRead)
                                *BytesRead = dwSize;
                }
                CloseHandle(hFile);
        }
        return Result;
}

BOOL LoadVirtualMachineProgram(IN PSTR FileName, IN ULONG Offset)
{
        return LoadVirtualMachineProgramEx(FileName, Offset, NULL);
}

static void MirrorBiosRegion(ULONG Offset, DWORD Size)
{
        if (Size == 0)
                return;
        PUCHAR base = (PUCHAR)VirtualMemory + Offset;
        for (DWORD pos = Size; pos < 0x10000; pos += Size)
                memcpy(base + pos, base, Size);
}

UINT32 SwDosStringLength(IN PSTR String, IN UINT32 MaximumLength)
{
	for (UINT32 i = 0; i < MaximumLength; i++)
		if (String[i] == '\0' || String[i] == '$')
			return i;
	return MaximumLength;
}

// Simple one-sector disk buffer
#define DISK_IMAGE_SIZE 512
static UCHAR DiskImage[DISK_IMAGE_SIZE];
static UINT32 DiskOffset = 0;
static USHORT LastUnknownPort = 0;
static UINT32 UnknownPortCount = 0;
static UCHAR PicMasterImr = 0;
static UCHAR PicSlaveImr = 0;
static UCHAR SysCtrl = 0;
static UCHAR CgaMode = 0;
static UCHAR MdaMode = 0;
static UCHAR PitControl = 0;
static UCHAR PitCounter0 = 0;
static UCHAR PitCounter1 = 0;
static UCHAR DmaTemp = 0;
static UCHAR DmaMode = 0;
static UCHAR DmaMask = 0;
static UCHAR DmaClear = 0;
static UCHAR DmaPage1 = 0;
static UCHAR Port0210Val = 0;
static UCHAR Port0278Val = 0;
static UCHAR Port02faVal = 0;
static UCHAR Port0378Val = 0;
static UCHAR Port03bcVal = 0;
static UCHAR Port03faVal = 0;
static UCHAR Port0201Val = 0;
static UCHAR PitCounter2 = 0;
static BOOL  SpeakerOn = FALSE;
static UCHAR CrtcMdaIndex = 0;
static UCHAR CrtcMdaData = 0;
static UCHAR CrtcMdaRegs[32] = {0};
static UCHAR AttrMda = 0;
static UCHAR CrtcCgaIndex = 0;
static UCHAR CrtcCgaData = 0;
static UCHAR CrtcCgaRegs[32] = {0};
static UCHAR AttrCga = 0;
static UCHAR CgaStatus = 0;
static UCHAR FdcDor = 0;
static UCHAR FdcStatus = 0;
static UCHAR FdcData = 0;
static UCHAR DmaChan[6] = {0};
static const UCHAR Port62MemNibble = ((GuestMemorySize / 1024 - 64) / 32);

BOOL LoadDiskImage(PCSTR FileName)
{
        HANDLE hFile = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
                return FALSE;
        DWORD dwRead = 0;
        BOOL result = ReadFile(hFile, DiskImage, DISK_IMAGE_SIZE, &dwRead, NULL);
        CloseHandle(hFile);
        return result && dwRead == DISK_IMAGE_SIZE;
}

static const char* GetPortName(USHORT port)
{
        switch (port)
        {
        case IO_PORT_STRING_PRINT:    return "STRING_PRINT";
        case IO_PORT_KEYBOARD_INPUT:  return "KEYBOARD_INPUT";
        case IO_PORT_KBD_DATA:        return "KBD_DATA";
        case IO_PORT_KBD_STATUS:      return "KBD_STATUS";
        case IO_PORT_DISK_DATA:       return "DISK_DATA";
        case IO_PORT_POST:            return "POST";
        case IO_PORT_PIC_MASTER_CMD:  return "PIC_MASTER_CMD";
        case IO_PORT_PIC_MASTER_DATA: return "PIC_MASTER_DATA";
        case IO_PORT_PIC_SLAVE_CMD:   return "PIC_SLAVE_CMD";
        case IO_PORT_PIC_SLAVE_DATA:  return "PIC_SLAVE_DATA";
        case IO_PORT_SYS_CTRL:        return "SYS_CTRL";
        case IO_PORT_SYS_PORTC:       return "SYS_PORTC";
        case IO_PORT_MDA_MODE:        return "MDA_MODE";
        case IO_PORT_CGA_MODE:        return "CGA_MODE";
        case IO_PORT_DMA_MODE:       return "DMA_MODE";
        case IO_PORT_DMA_MASK:       return "DMA_MASK";
       case IO_PORT_DMA_PAGE3:       return "DMA_PAGE3";
       case IO_PORT_DMA_TEMP:        return "DMA_TEMP";
       case IO_PORT_VIDEO_MISC_B8:   return "VIDEO_MISC_B8";
       case IO_PORT_SPECIAL_213:     return "PORT_213";
       case IO_PORT_PIT_COUNTER0:    return "PIT_COUNTER0";
       case IO_PORT_PIT_COUNTER1:    return "PIT_COUNTER1";
       case IO_PORT_PIT_COUNTER2:    return "PIT_COUNTER2";
       case IO_PORT_PIT_CONTROL:     return "PIT_CONTROL";
       case IO_PORT_PIT_CMD:         return "PIT_CMD";
       case IO_PORT_TIMER_MISC:      return "TIMER_MISC";
       case IO_PORT_DMA_CLEAR:       return "DMA_CLEAR";
       case IO_PORT_DMA_PAGE1:       return "DMA_PAGE1";
       case IO_PORT_PORT_0210:       return "PORT_0210";
       case IO_PORT_PORT_0278:       return "PORT_0278";
       case IO_PORT_PORT_02FA:       return "PORT_02FA";
       case IO_PORT_PORT_0378:       return "PORT_0378";
       case IO_PORT_PORT_03BC:       return "PORT_03BC";
       case IO_PORT_PORT_03FA:       return "PORT_03FA";
       case IO_PORT_PORT_0201:       return "PORT_0201";
       case IO_PORT_CRTC_INDEX_MDA:  return "MDA_INDEX";
       case IO_PORT_CRTC_DATA_MDA:   return "MDA_DATA";
       case IO_PORT_ATTR_MDA:        return "MDA_ATTR";
       case IO_PORT_CRTC_INDEX_CGA:  return "CGA_INDEX";
       case IO_PORT_CRTC_DATA_CGA:   return "CGA_DATA";
       case IO_PORT_ATTR_CGA:        return "CGA_ATTR";
       case IO_PORT_CGA_STATUS:      return "CGA_STATUS";
       case IO_PORT_FDC_DOR:         return "FDC_DOR";
       case IO_PORT_FDC_STATUS:      return "FDC_STATUS";
       case IO_PORT_FDC_DATA:        return "FDC_DATA";
       default:                   return "UNKNOWN";
       }
}

HRESULT SwEmulatorIoCallback(IN PVOID Context, IN OUT WHV_EMULATOR_IO_ACCESS_INFO* IoAccess)
{
        if (IoAccess->Direction == 0)
        {
                printf("IN  port 0x%04X (%s), size %u\n", IoAccess->Port, GetPortName(IoAccess->Port), IoAccess->AccessSize);
                if (IoAccess->Port == IO_PORT_KEYBOARD_INPUT || IoAccess->Port == IO_PORT_KBD_DATA)
                {
                        for (UINT8 i = 0; i < IoAccess->AccessSize; i++)
                        {
                                int ch = getchar();
                                ((PUCHAR)&IoAccess->Data)[i] = (UCHAR)ch;
                        }
                        return S_OK;
                }
                else if (IoAccess->Port == IO_PORT_KBD_STATUS)
                {
                        IoAccess->Data = 0;
                        return S_OK;
                }
                else if (IoAccess->Port == IO_PORT_STRING_PRINT)
                {
                        IoAccess->Data = 0;
                        return S_OK;
                }
               else if (IoAccess->Port == IO_PORT_DISK_DATA)
               {
                       for (UINT8 i = 0; i < IoAccess->AccessSize; i++)
                       {
                               ((PUCHAR)&IoAccess->Data)[i] = DiskImage[DiskOffset];
                               DiskOffset = (DiskOffset + 1) % DISK_IMAGE_SIZE;
                       }
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_POST)
               {
                       IoAccess->Data = 0;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_SYS_CTRL)
               {
                       IoAccess->Data = SysCtrl;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_SYS_PORTC)
               {
                       UCHAR val = (SysCtrl & 0x04) ? (Port62MemNibble & 0x0F) : ((Port62MemNibble >> 4) & 0x0F);
                       if (SysCtrl & 0x02)
                               val |= 0x20;
                       IoAccess->Data = val;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_MDA_MODE)
               {
                       IoAccess->Data = MdaMode;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_DMA_MASK)
               {
                       IoAccess->Data = DmaMask;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_DMA_MODE)
               {
                       IoAccess->Data = DmaMode;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_CGA_MODE)
               {
                       IoAccess->Data = CgaMode;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_DMA_TEMP)
               {
                       IoAccess->Data = DmaTemp;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_DMA_CLEAR)
               {
                       IoAccess->Data = DmaClear;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PIT_CONTROL)
               {
                       IoAccess->Data = PitControl;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PIT_COUNTER0)
               {
                       /* Simulate PIT channel 0 ticking */
                       PitCounter0--;
                       IoAccess->Data = PitCounter0;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PIT_COUNTER1)
               {
                       /* Simulate PIT channel 1 ticking so BIOS progress isn't stalled */
                       PitCounter1--;
                       IoAccess->Data = PitCounter1;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PIT_COUNTER2)
               {
                       PitCounter2--;
                       IoAccess->Data = PitCounter2;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PIC_MASTER_DATA)
               {
                       IoAccess->Data = PicMasterImr;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PIC_SLAVE_DATA)
               {
                       IoAccess->Data = PicSlaveImr;
                       return S_OK;
               }
               else if (IoAccess->Port >= 0x0002 && IoAccess->Port <= 0x0007)
               {
                       UINT32 idx = IoAccess->Port - 0x0002;
                       IoAccess->Data = DmaChan[idx];
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_DMA_PAGE1)
               {
                       IoAccess->Data = DmaPage1;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PORT_0210)
               {
                       IoAccess->Data = Port0210Val;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PORT_0278)
               {
                       IoAccess->Data = Port0278Val;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PORT_02FA)
               {
                       IoAccess->Data = Port02faVal;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PORT_0378)
               {
                       IoAccess->Data = Port0378Val;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PORT_03BC)
               {
                       IoAccess->Data = Port03bcVal;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PORT_03FA)
               {
                       IoAccess->Data = Port03faVal;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PORT_0201)
               {
                       IoAccess->Data = Port0201Val;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_CRTC_INDEX_MDA)
               {
                       IoAccess->Data = CrtcMdaIndex;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_CRTC_DATA_MDA)
               {
                       IoAccess->Data = CrtcMdaRegs[CrtcMdaIndex];
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_ATTR_MDA)
               {
                       IoAccess->Data = AttrMda;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_CRTC_INDEX_CGA)
               {
                       IoAccess->Data = CrtcCgaIndex;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_CRTC_DATA_CGA)
               {
                       IoAccess->Data = CrtcCgaRegs[CrtcCgaIndex];
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_ATTR_CGA)
               {
                       IoAccess->Data = AttrCga;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_CGA_STATUS)
               {
                       CgaStatus ^= 0x08; /* toggle vertical retrace bit */
                       IoAccess->Data = CgaStatus;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_FDC_DOR)
               {
                       IoAccess->Data = FdcDor;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_FDC_STATUS)
               {
                       IoAccess->Data = FdcStatus;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_FDC_DATA)
               {
                       IoAccess->Data = FdcData;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_PIC_MASTER_CMD || IoAccess->Port == IO_PORT_PIC_SLAVE_CMD)
               {
                       IoAccess->Data = 0;
                       return S_OK;
               }
               else if (IoAccess->Port == IO_PORT_DMA_PAGE3 ||
                        IoAccess->Port == IO_PORT_VIDEO_MISC_B8 ||
                        IoAccess->Port == IO_PORT_SPECIAL_213 ||
                        IoAccess->Port == IO_PORT_PIT_CMD ||
                       IoAccess->Port == IO_PORT_PIT_CONTROL ||
                       IoAccess->Port == IO_PORT_PIT_COUNTER0 ||
                       IoAccess->Port == IO_PORT_PIT_COUNTER1 ||
                       IoAccess->Port == IO_PORT_TIMER_MISC)
               {
                       IoAccess->Data = 0;
                       return S_OK;
               }
               printf("Input from port 0x%04X (%s) is not implemented!\n", IoAccess->Port, GetPortName(IoAccess->Port));
               return E_NOTIMPL;
       }
        printf("OUT port 0x%04X (%s), size %u, value 0x%X\n", IoAccess->Port, GetPortName(IoAccess->Port), IoAccess->AccessSize, IoAccess->Data);
        if (IoAccess->Port == IO_PORT_STRING_PRINT)
        {
                for (UINT8 i = 0; i < IoAccess->AccessSize; i++)
                {
                        char ch = ((PUCHAR)&IoAccess->Data)[i];
                        putc(ch, stdout);
                        CgaPutChar(ch);
                }
                return S_OK;
        }
        else if (IoAccess->Port == IO_PORT_DISK_DATA)
        {
                for (UINT8 i = 0; i < IoAccess->AccessSize; i++)
                {
                        DiskImage[DiskOffset] = ((PUCHAR)&IoAccess->Data)[i];
                        DiskOffset = (DiskOffset + 1) % DISK_IMAGE_SIZE;
                }
                return S_OK;
        }
       else if (IoAccess->Port == IO_PORT_POST)
       {
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_SYS_PORTC)
       {
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_SYS_CTRL)
       {
               SysCtrl = (UCHAR)IoAccess->Data;
               BOOL new_state = (SysCtrl & 0x03) == 0x03;
               if (new_state && !SpeakerOn)
               {
                       DWORD freq = PitCounter2 ? 1193182 / PitCounter2 : 750;
                       Beep(freq, 60);
               }
               SpeakerOn = new_state;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_MDA_MODE)
       {
               MdaMode = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_DMA_MASK)
       {
               DmaMask = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_DMA_MODE)
       {
               DmaMode = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_CGA_MODE)
       {
               CgaMode = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PIT_CONTROL)
       {
               PitControl = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PIT_COUNTER0)
       {
               /* Reload channel 0 with a new counter start value */
               PitCounter0 = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PIT_COUNTER1)
       {
               /* Reload channel 1 with a new counter start value */
               PitCounter1 = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_DMA_TEMP)
       {
               DmaTemp = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_DMA_CLEAR)
       {
               DmaClear = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port >= 0x0002 && IoAccess->Port <= 0x0007)
       {
               UINT32 idx = IoAccess->Port - 0x0002;
               DmaChan[idx] = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_DMA_PAGE1)
       {
               DmaPage1 = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PORT_0210)
       {
               Port0210Val = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PORT_0278)
       {
               Port0278Val = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PORT_02FA)
       {
               Port02faVal = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PORT_0378)
       {
               Port0378Val = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PORT_03BC)
       {
               Port03bcVal = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PORT_03FA)
       {
               Port03faVal = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PORT_0201)
       {
               Port0201Val = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PIT_COUNTER2)
       {
               PitCounter2 = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_CRTC_INDEX_MDA)
       {
               CrtcMdaIndex = (UCHAR)IoAccess->Data & 0x1F;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_CRTC_DATA_MDA)
       {
               CrtcMdaData = (UCHAR)IoAccess->Data;
               CrtcMdaRegs[CrtcMdaIndex] = CrtcMdaData;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_ATTR_MDA)
       {
               AttrMda = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_CRTC_INDEX_CGA)
       {
               CrtcCgaIndex = (UCHAR)IoAccess->Data & 0x1F;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_CRTC_DATA_CGA)
       {
               CrtcCgaData = (UCHAR)IoAccess->Data;
               CrtcCgaRegs[CrtcCgaIndex] = CrtcCgaData;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_ATTR_CGA)
       {
               AttrCga = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_CGA_STATUS)
       {
               CgaStatus = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_FDC_DOR)
       {
               FdcDor = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_FDC_STATUS)
       {
               FdcStatus = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_FDC_DATA)
       {
               FdcData = (UCHAR)IoAccess->Data;
               return S_OK;
       }
       else if (IoAccess->Port == IO_PORT_PIC_MASTER_CMD)
       {
               PicMasterImr = (UCHAR)IoAccess->Data; /* treat command as IMR for simplicity */
               return S_OK;
       }
        else if (IoAccess->Port == IO_PORT_PIC_SLAVE_CMD)
        {
                PicSlaveImr = (UCHAR)IoAccess->Data;
                return S_OK;
        }
        else if (IoAccess->Port == IO_PORT_PIC_MASTER_DATA)
        {
                PicMasterImr = (UCHAR)IoAccess->Data;
                return S_OK;
        }
        else if (IoAccess->Port == IO_PORT_PIC_SLAVE_DATA)
        {
                PicSlaveImr = (UCHAR)IoAccess->Data;
                return S_OK;
        }
        else if (IoAccess->Port == IO_PORT_KBD_DATA || IoAccess->Port == IO_PORT_KBD_STATUS || IoAccess->Port == IO_PORT_KEYBOARD_INPUT)
        {
                return S_OK;
        }
        else if (IoAccess->Port == IO_PORT_DMA_PAGE3 ||
                 IoAccess->Port == IO_PORT_VIDEO_MISC_B8 ||
                 IoAccess->Port == IO_PORT_SPECIAL_213 ||
                 IoAccess->Port == IO_PORT_PIT_CMD ||
                 IoAccess->Port == IO_PORT_PIT_CONTROL ||
                 IoAccess->Port == IO_PORT_PIT_COUNTER0 ||
                 IoAccess->Port == IO_PORT_PIT_COUNTER1 ||
                 IoAccess->Port == IO_PORT_TIMER_MISC)
        {
                /* Ports touched by the BIOS during POST but not modeled. */
                return S_OK;
        }
        else
        {
                if (IoAccess->Port == LastUnknownPort)
                        UnknownPortCount++;
                else
                {
                        LastUnknownPort = IoAccess->Port;
                        UnknownPortCount = 1;
                }
                printf("Unknown I/O Port: 0x%04X is accessed!\n", IoAccess->Port);
                if (UnknownPortCount >= 2)
                {
                        printf("Repeated access to unknown port 0x%04X, terminating.\n", IoAccess->Port);
                        exit(1);
                }
                return E_NOTIMPL;
        }
}

HRESULT SwEmulatorMmioCallback(IN PVOID Context, IN OUT WHV_EMULATOR_MEMORY_ACCESS_INFO* MemoryAccess)
{
        ULONG_PTR gpa = MemoryAccess->GpaAddress % GuestMemorySize;
        for (UINT32 i = 0; i < MemoryAccess->AccessSize; i++)
        {
                PBYTE hva = (PBYTE)VirtualMemory + ((gpa + i) % GuestMemorySize);
                if (MemoryAccess->Direction)
                        *hva = ((PBYTE)MemoryAccess->Data)[i];
                else
                        ((PBYTE)MemoryAccess->Data)[i] = *hva;
        }
        return S_OK;
}

HRESULT SwEmulatorGetVirtualRegistersCallback(IN PVOID Context, IN CONST WHV_REGISTER_NAME* RegisterNames, IN UINT32 RegisterCount, OUT WHV_REGISTER_VALUE* RegisterValues)
{
	return WHvGetVirtualProcessorRegisters(hPart, 0, RegisterNames, RegisterCount, RegisterValues);
}

HRESULT SwEmulatorSetVirtualRegistersCallback(IN PVOID Context, IN CONST WHV_REGISTER_NAME* RegisterNames, IN UINT32 RegisterCount, IN CONST WHV_REGISTER_VALUE* RegisterValues)
{
	return WHvSetVirtualProcessorRegisters(hPart, 0, RegisterNames, RegisterCount, RegisterValues);
}

HRESULT SwEmulatorTranslateGvaPageCallback(IN PVOID Context, IN WHV_GUEST_VIRTUAL_ADDRESS GvaPage, IN WHV_TRANSLATE_GVA_FLAGS TranslateFlags, OUT WHV_TRANSLATE_GVA_RESULT_CODE* TranslationResult, OUT WHV_GUEST_PHYSICAL_ADDRESS* GpaPage)
{
	WHV_TRANSLATE_GVA_RESULT Result;
	HRESULT hr = WHvTranslateGva(hPart, 0, GvaPage, TranslateFlags, &Result, GpaPage);
	*TranslationResult = Result.ResultCode;
	return hr;
}

HRESULT SwExecuteProgram()
{
	WHV_RUN_VP_EXIT_CONTEXT ExitContext = { 0 };
	BOOL ContinueExecution = TRUE;
	HRESULT hr = S_FALSE;
	while (ContinueExecution)
	{
		hr = WHvRunVirtualProcessor(hPart, 0, &ExitContext, sizeof(ExitContext));
		if (hr == S_OK)
		{
			WHV_REGISTER_NAME RipName = WHvX64RegisterRip;
			WHV_REGISTER_VALUE Rip = { ExitContext.VpContext.Rip };
			switch (ExitContext.ExitReason)
			{
			case WHvRunVpExitReasonMemoryAccess:
			{
				PSTR AccessType[4] = { "Read","Write","Execute","Unknown"};
				puts("Memory Access Violation occured!");
				printf("Access Context: GVA=0x%llX GPA=0x%0llX\n", ExitContext.MemoryAccess.Gva, ExitContext.MemoryAccess.Gpa);
				printf("Behavior: %s\t", AccessType[ExitContext.MemoryAccess.AccessInfo.AccessType]);
				printf("GVA is %s \t", ExitContext.MemoryAccess.AccessInfo.GvaValid ? "Valid" : "Invalid");
				printf("GPA is %s \n", ExitContext.MemoryAccess.AccessInfo.GpaUnmapped ? "Unmapped" : "Mapped");
				printf("Number of Instruction Bytes: %d\n Instruction Bytes: ", ExitContext.MemoryAccess.InstructionByteCount);
				for (UINT8 i = 0; i < ExitContext.MemoryAccess.InstructionByteCount; i++)
					printf("%02X ", ExitContext.MemoryAccess.InstructionBytes[i]);
				SwDumpVirtualProcessorGprState();
				SwDumpVirtualProcessorSegmentState();
				ContinueExecution = FALSE;
				break;
			}
			case WHvRunVpExitReasonX64IoPortAccess:
			{
				WHV_EMULATOR_STATUS EmuSt;
				hr = WHvEmulatorTryIoEmulation(hEmu, NULL, &ExitContext.VpContext, &ExitContext.IoPortAccess, &EmuSt);
				if (FAILED(hr))
					printf("Failed to emulate I/O instruction! HRESULT=0x%08X, Emulation Status=0x%08X\n", hr, EmuSt.AsUINT32);
				// Emulator will advance rip for us. No need to advanced rip from here.
				break;
			}
			case WHvRunVpExitReasonUnrecoverableException:
				puts("The processor went into shutdown state due to unrecoverable exception!");
				ContinueExecution = FALSE;
				break;
			case WHvRunVpExitReasonInvalidVpRegisterValue:
				puts("The specified processor state is invalid!");
				ContinueExecution = FALSE;
				break;
                        case WHvRunVpExitReasonX64Halt:
                                /* Treat HLT as a NOP so the BIOS can busy wait
                                   even when interrupts are disabled. */
                                Rip.Reg64 += ExitContext.VpContext.InstructionLength;
                                hr = WHvSetVirtualProcessorRegisters(hPart, 0, &RipName, 1, &Rip);
                                ContinueExecution = TRUE;
                                break;
			default:
				printf("Unknown VM-Exit Code=0x%X!\n", ExitContext.ExitReason);
				ContinueExecution = FALSE;
				break;
			}
		}
		else
		{
			printf("Failed to run virtual processor! HRESULT=0x%X\n", hr);
			ContinueExecution = FALSE;
		}
	}
	return hr;
}

int main(int argc, char* argv[], char* envp[])
{
       puts("SimpleWhpDemo version 1.1.1");
       puts("IVT firmware version 0.1.0");
       PSTR ProgramFileName = argc >= 2 ? argv[1] : "hello.com";
       PSTR BiosFileName = argc >= 3 ? argv[2] : DEFAULT_BIOS;
	SwCheckSystemHypervisor();
	if (ExtExitFeat.X64CpuidExit && ExtExitFeat.X64MsrExit)
	{
		HRESULT hr = SwInitializeVirtualMachine();
		if (hr == S_OK)
		{
                        BOOL LoadProgramResult = LoadVirtualMachineProgram(ProgramFileName, 0x10100);
                        DWORD BiosSize = 0;
                        BOOL LoadIvtFwResult = LoadVirtualMachineProgramEx(BiosFileName, 0xF0000, &BiosSize);
                        if (!LoadIvtFwResult && strcmp(BiosFileName, DEFAULT_BIOS) == 0)
                        {
                                puts("AMI BIOS not found, falling back to " FALLBACK_BIOS);
                                LoadIvtFwResult = LoadVirtualMachineProgramEx(FALLBACK_BIOS, 0xF0000, &BiosSize);
                                if (LoadIvtFwResult)
                                        BiosFileName = FALLBACK_BIOS;
                        }
                        if (LoadIvtFwResult && strcmp(BiosFileName, FALLBACK_BIOS) == 0)
                        {
                                /* Patch the reset vector when using the minimal
                                   fallback firmware so execution jumps to its
                                   start at F000:0000. Real BIOS images already
                                   contain their own reset vector. */
                                PUCHAR mem = (PUCHAR)VirtualMemory;
                                mem[0xFFFF0] = 0xEA;        // jmp far ptr
                                mem[0xFFFF1] = 0x00;        // offset 0x0000
                                mem[0xFFFF2] = 0x00;
                                mem[0xFFFF3] = 0x00;        // segment 0xF000
                                mem[0xFFFF4] = 0xF0;
                        }
                        else if (LoadIvtFwResult)
                        {
                                /* Verify that the real BIOS contains the expected
                                   far jump at the reset vector. */
                                PUCHAR mem = (PUCHAR)VirtualMemory;
                                if (mem[0xFFFF0] == 0xEA)
                                {
                                        printf("BIOS reset vector jumps to %02X%02X:%02X%02X\n",
                                               mem[0xFFFF4], mem[0xFFFF3], mem[0xFFFF2], mem[0xFFFF1]);
                                }
                                else
                                {
                                        puts("Warning: BIOS reset vector is unexpected; patching.");
                                        mem[0xFFFF0] = 0xEA;
                                        mem[0xFFFF1] = 0x00;
                                        mem[0xFFFF2] = 0x00;
                                        mem[0xFFFF3] = 0x00;
                                        mem[0xFFFF4] = 0xF0;
                                }
                        }
                        if (LoadIvtFwResult)
                        {
                                PUCHAR mem = (PUCHAR)VirtualMemory;
                                printf("BIOS loaded from %s (%lu bytes)\n", BiosFileName, BiosSize);
                                printf("Reset vector bytes: %02X %02X %02X %02X %02X\n",
                                       mem[0xFFFF0], mem[0xFFFF1], mem[0xFFFF2],
                                       mem[0xFFFF3], mem[0xFFFF4]);
                        }
                        if (LoadIvtFwResult && BiosSize < 0x10000)
                                MirrorBiosRegion(0xF0000, BiosSize);
                        BOOL LoadDiskResult = LoadDiskImage("disk.img");
                        puts("Virtual Machine is initialized successfully!");
                        if (LoadProgramResult)
                        {
                                puts("Program is loaded successfully!");
                                if (!LoadIvtFwResult)
                                        puts("Warning: Firmware is not loaded successfully. Your program might not function properly if it invokes BIOS interrupts.");
                                if (!LoadDiskResult)
                                        puts("Warning: disk image not loaded, disk reads will return zeros.");
                                puts("============ Program Start ============");
                               SwExecuteProgram();
                                puts("============= Program End =============");
                                PrintCgaBuffer();
			}
			else
				puts("Failed to load the program!");
			SwTerminateVirtualMachine();
		}
	}
	return 0;
}
