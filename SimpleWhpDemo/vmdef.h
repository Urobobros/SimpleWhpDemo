#pragma once

// Give the guest 1MiB of memory.
#define GuestMemorySize         0x100000

#define IO_PORT_STRING_PRINT	0x0000
#define IO_PORT_KEYBOARD_INPUT  0x0001 /* legacy */
#define IO_PORT_KBD_DATA        0x0060
#define IO_PORT_KBD_STATUS      0x0064
#define IO_PORT_DISK_DATA       0x00FF
#define IO_PORT_POST            0x0080
#define IO_PORT_PIC_MASTER_CMD  0x0020
#define IO_PORT_PIC_MASTER_DATA 0x0021
#define IO_PORT_SYS_CTRL        0x0061
#define IO_PORT_SYS_PORTC       0x0062
#define IO_PORT_PIC_SLAVE_CMD   0x00A0
#define IO_PORT_PIC_SLAVE_DATA  0x00A1
#define IO_PORT_MDA_MODE        0x03B8
#define IO_PORT_CGA_MODE        0x03D8
#define IO_PORT_DMA_PAGE3       0x0083
#define IO_PORT_DMA_MASK        0x000A
#define IO_PORT_VIDEO_MISC_B8   0x00B8
#define IO_PORT_SPECIAL_213     0x0213
#define IO_PORT_PIT_CMD         0x0008
#define IO_PORT_DMA_MODE        0x000B
#define IO_PORT_DMA_TEMP        0x000D
#define IO_PORT_PIT_COUNTER0    0x0040
#define IO_PORT_PIT_COUNTER1    0x0041
#define IO_PORT_PIT_CONTROL     0x0043
#define IO_PORT_TIMER_MISC      0x0063

// Hypervisor Capability.
BOOL HypervisorPresence;
WHV_CAPABILITY_FEATURES CapFeat;
WHV_EXTENDED_VM_EXITS ExtExitFeat;
WHV_PROCESSOR_FEATURES ProcFeat;
WHV_PROCESSOR_XSAVE_FEATURES XsaveFeat;

// Virtual Machine Definitions.
WHV_EMULATOR_HANDLE hEmu;
WHV_PARTITION_HANDLE hPart;
PVOID VirtualMemory = NULL;

// Emulator
HRESULT SwEmulatorIoCallback(IN PVOID Context, IN OUT WHV_EMULATOR_IO_ACCESS_INFO* IoAccess);
HRESULT SwEmulatorMmioCallback(IN PVOID Context, IN OUT WHV_EMULATOR_MEMORY_ACCESS_INFO* MemoryAccess);
HRESULT SwEmulatorGetVirtualRegistersCallback(IN PVOID Context, IN CONST WHV_REGISTER_NAME* RegisterNames, IN UINT32 RegisterCount, OUT WHV_REGISTER_VALUE* RegisterValues);
HRESULT SwEmulatorSetVirtualRegistersCallback(IN PVOID Context, IN CONST WHV_REGISTER_NAME* RegisterNames, IN UINT32 RegisterCount, IN CONST WHV_REGISTER_VALUE* RegisterValues);
HRESULT SwEmulatorTranslateGvaPageCallback(IN PVOID Context, IN WHV_GUEST_VIRTUAL_ADDRESS GvaPage, IN WHV_TRANSLATE_GVA_FLAGS TranslateFlags, OUT WHV_TRANSLATE_GVA_RESULT_CODE* TranslationResult, OUT WHV_GUEST_PHYSICAL_ADDRESS* GpaPage);
WHV_EMULATOR_CALLBACKS EmuCallbacks =
{
	sizeof(WHV_EMULATOR_CALLBACKS),
	0,
	SwEmulatorIoCallback,
	SwEmulatorMmioCallback,
	SwEmulatorGetVirtualRegistersCallback,
	SwEmulatorSetVirtualRegistersCallback,
	SwEmulatorTranslateGvaPageCallback
};

// Virtual Processor Initial State.
WHV_REGISTER_NAME SwInitGprNameGroup[0x12] =
{
	WHvX64RegisterRax,
	WHvX64RegisterRcx,
	WHvX64RegisterRdx,
	WHvX64RegisterRbx,
	WHvX64RegisterRsp,
	WHvX64RegisterRbp,
	WHvX64RegisterRsi,
	WHvX64RegisterRdi,
	WHvX64RegisterR8,
	WHvX64RegisterR9,
	WHvX64RegisterR10,
	WHvX64RegisterR11,
	WHvX64RegisterR12,
	WHvX64RegisterR13,
	WHvX64RegisterR14,
	WHvX64RegisterR15,
	WHvX64RegisterRip,
	WHvX64RegisterRflags
};

WHV_REGISTER_VALUE SwInitGprValueGroup[0x12] =
{
	{0},{0},{0},{0},{0xFFF0},{0},{0},{0},
	{0},{0},{0},{0},{0},{0},{0},{0},
	{0xFFF0},{0x2}
};

WHV_REGISTER_NAME SwInitSrNameGroup[8] =
{
	WHvX64RegisterEs,
	WHvX64RegisterCs,
	WHvX64RegisterSs,
	WHvX64RegisterDs,
	WHvX64RegisterFs,
	WHvX64RegisterGs,
	WHvX64RegisterLdtr,
	WHvX64RegisterTr
};

WHV_REGISTER_VALUE SwInitSrValueGroup[8] =
{
	{.Segment = {0,0xFFFF,0,{3,1,0,1,0,1,0,0,0}}},
	{.Segment = {0xF0000,0xFFFF,0xF000,{11,1,0,1,0,1,0,0,0}}},
	{.Segment = {0,0xFFFF,0,{3,1,0,1,0,1,0,0,0}}},
	{.Segment = {0,0xFFFF,0,{3,1,0,1,0,1,0,0,0}}},
	{.Segment = {0,0xFFFF,0,{3,1,0,1,0,1,0,0,0}}},
	{.Segment = {0,0xFFFF,0,{3,1,0,1,0,1,0,0,0}}},
	{.Segment = {0,0xFFFF,0,{2,0,0,1,0,1,0,0,0}}},
	{.Segment = {0,0xFFFF,0,{3,0,0,1,0,1,0,0,0}}}
};

WHV_REGISTER_NAME SwInitDescriptorNameGroup[2] =
{
	WHvX64RegisterIdtr,
	WHvX64RegisterGdtr
};

WHV_REGISTER_VALUE SwInitDescriptorValueGroup[2] =
{
	{.Table = {{0,0,0},0xFFFF,0}},
	{.Table = {{0,0,0},0xFFFF,0}}
};

WHV_REGISTER_NAME SwInitCrNameGroup[4] =
{
	WHvX64RegisterCr0,
	WHvX64RegisterCr2,
	WHvX64RegisterCr3,
	WHvX64RegisterCr4
};

WHV_REGISTER_VALUE SwInitCrValueGroup[4] =
{
	{0x60000010},
	{0},{0},{0}
};

WHV_REGISTER_NAME SwInitDrNameGroup[6] =
{
	WHvX64RegisterDr0,
	WHvX64RegisterDr1,
	WHvX64RegisterDr2,
	WHvX64RegisterDr3,
	WHvX64RegisterDr6,
	WHvX64RegisterDr7,
};

WHV_REGISTER_VALUE SwInitDrValueGroup[6] =
{
	{0},{0},{0},{0},
	{0xFFFF0FF0},{0x400}
};

WHV_REGISTER_NAME SwInitXcrNameGroup[1] =
{
	WHvX64RegisterXCr0
};

WHV_REGISTER_VALUE SwInitXcrValueGroup[1] =
{
	{1}
};

WHV_REGISTER_NAME SwInitFpcsName = WHvX64RegisterFpControlStatus;
WHV_REGISTER_VALUE SwInitFpcsValue =
{
	.FpControlStatus = {0x40,0x0,0x5555,0x0,0x0,{0}}
};

PSTR SwGprNameGroup[0x12] =
{
	"rax","rcx","rdx","rbx","rsp",
	"rbp","rsi","rdi","r8","r9",
	"r10","r11","r12","r13",
	"r14","r15","rip","rflags"
};

PSTR SwSrNameGroup[8] =
{
	"es","cs","ss","ds",
	"fs","gs","ldtr","tr"
};

// Virtual Machine Property
UINT32 SwProcessorCount = 1;
