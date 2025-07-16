#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include <WinHvPlatform.h>
#include <WinHvEmulation.h>
#include "vmdef.h"

#define CGA_COLS 80
#define CGA_ROWS 25
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
        for(UINT32 r=0;r<CGA_ROWS;r++)
        {
                for(UINT32 c=0;c<CGA_COLS;c++)
                {
                        char ch = (char)(CgaBuffer[r*CGA_COLS+c] & 0xFF);
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
	if (hr != S_OK)goto Cleanup;
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

BOOL LoadVirtualMachineProgram(IN PSTR FileName, IN ULONG Offset)
{
	BOOL Result = FALSE;
	HANDLE hFile = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		DWORD FileSize = GetFileSize(hFile, NULL);
		if (FileSize != INVALID_FILE_SIZE)
		{
			DWORD dwSize = 0;
			PVOID ProgramAddress = (PVOID)((ULONG_PTR)VirtualMemory + Offset);
			Result = ReadFile(hFile, ProgramAddress, FileSize, &dwSize, NULL);
		}
		CloseHandle(hFile);
	}
	return Result;
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

HRESULT SwEmulatorIoCallback(IN PVOID Context, IN OUT WHV_EMULATOR_IO_ACCESS_INFO* IoAccess)
{
        auto GetPortName=[&](USHORT port)->const char*
        {
                switch(port)
                {
                case IO_PORT_STRING_PRINT: return "STRING_PRINT";
                case IO_PORT_KEYBOARD_INPUT: return "KEYBOARD_INPUT";
                case IO_PORT_DISK_DATA: return "DISK_DATA";
                case IO_PORT_POST: return "POST";
                default: return "UNKNOWN";
                }
        };
        if (IoAccess->Direction == 0)
        {
                printf("IN  port 0x%04X (%s), size %u\n", IoAccess->Port, GetPortName(IoAccess->Port), IoAccess->AccessSize);
                if (IoAccess->Port == IO_PORT_KEYBOARD_INPUT)
                {
                        for (UINT8 i = 0; i < IoAccess->AccessSize; i++)
                        {
                                int ch = getchar();
                                ((PUCHAR)&IoAccess->Data)[i] = (UCHAR)ch;
                        }
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
        else
        {
                printf("Unknown I/O Port: 0x%04X is accessed!\n", IoAccess->Port);
                return E_NOTIMPL;
        }
}

HRESULT SwEmulatorMmioCallback(IN PVOID Context, IN OUT WHV_EMULATOR_MEMORY_ACCESS_INFO* MemoryAccess)
{
	PVOID HvaAddress = (PVOID)((ULONG_PTR)VirtualMemory + MemoryAccess->GpaAddress);
	if(MemoryAccess->GpaAddress+MemoryAccess->AccessSize>=GuestMemorySize)
	{
		printf("Memory-Access Overflow is detected! GPA=0x%016llX, Access-Size=%u bytes\n", MemoryAccess->GpaAddress, MemoryAccess->AccessSize);
		return E_FAIL;
	}
	if (MemoryAccess->Direction)
		RtlCopyMemory(HvaAddress, MemoryAccess->Data, MemoryAccess->AccessSize);
	else
		RtlCopyMemory(MemoryAccess->Data, HvaAddress, MemoryAccess->AccessSize);
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
				ContinueExecution = _bittest64(&ExitContext.VpContext.Rflags, 9);
				Rip.Reg64 += ExitContext.VpContext.InstructionLength;
				hr = WHvSetVirtualProcessorRegisters(hPart, 0, &RipName, 1, &Rip);
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
	SwCheckSystemHypervisor();
	if (ExtExitFeat.X64CpuidExit && ExtExitFeat.X64MsrExit)
	{
		HRESULT hr = SwInitializeVirtualMachine();
		if (hr == S_OK)
		{
                        BOOL LoadProgramResult = LoadVirtualMachineProgram(ProgramFileName, 0x10100);
                        BOOL LoadIvtFwResult = LoadVirtualMachineProgram("ivt.fw", 0xF0000);
                        if (LoadIvtFwResult)
                        {
                                // Place a far jump at the x86 reset vector (FFFF0h)
                                // so the CPU jumps into the loaded firmware.
                                PUCHAR mem = (PUCHAR)VirtualMemory;
                                mem[0xFFFF0] = 0xEA;        // jmp far ptr
                                mem[0xFFFF1] = 0x00;        // offset 0x0000
                                mem[0xFFFF2] = 0x00;
                                mem[0xFFFF3] = 0x00;        // segment 0xF000
                                mem[0xFFFF4] = 0xF0;
                        }
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
