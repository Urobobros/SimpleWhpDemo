#![allow(static_mut_refs)]
use std::fs::File;
use std::io::Read;
use std::{
    ffi::c_void,
    ptr::null_mut,
    slice,
    sync::atomic::{AtomicPtr, Ordering},
};

use aligned::*;
use windows::{
    Win32::{
        Foundation::*,
        Storage::FileSystem::*,
        System::{Hypervisor::*, Memory::*},
    },
    core::*,
};

const DEFAULT_BIOS: &str = "ami_8088_bios_31jan89.bin\0";
const FALLBACK_BIOS: &str = "ivt.fw\0";

static GLOBAL_EMULATOR_HANDLE: AtomicPtr<c_void> = AtomicPtr::new(null_mut());
static GLOBAL_EMULATOR_CALLBACKS: WHV_EMULATOR_CALLBACKS = WHV_EMULATOR_CALLBACKS {
    Size: size_of::<WHV_EMULATOR_CALLBACKS>() as u32,
    Reserved: 0,
    WHvEmulatorIoPortCallback: Some(emu_io_port_callback),
    WHvEmulatorMemoryCallback: Some(emu_memory_callback),
    WHvEmulatorGetVirtualProcessorRegisters: Some(emu_get_vcpu_reg_callback),
    WHvEmulatorSetVirtualProcessorRegisters: Some(emu_set_vcpu_reg_callback),
    WHvEmulatorTranslateGvaPage: Some(emu_translate_gva_callback),
};

const IO_PORT_STRING_PRINT: u16 = 0x0000;
const IO_PORT_KEYBOARD_INPUT: u16 = 0x0001;
const IO_PORT_DISK_DATA: u16 = 0x00FF;
const IO_PORT_POST: u16 = 0x0080;
const IO_PORT_PIC_MASTER_CMD: u16 = 0x0020;
const IO_PORT_PIC_MASTER_DATA: u16 = 0x0021;
const IO_PORT_PIC_SLAVE_CMD: u16 = 0x00A0;
const IO_PORT_PIC_SLAVE_DATA: u16 = 0x00A1;
const IO_PORT_SYS_CTRL: u16 = 0x0061;
const IO_PORT_MDA_MODE: u16 = 0x03B8;
const IO_PORT_CGA_MODE: u16 = 0x03D8;
const IO_PORT_DMA_PAGE3: u16 = 0x0083;
const IO_PORT_VIDEO_MISC_B8: u16 = 0x00B8;
const IO_PORT_SPECIAL_213: u16 = 0x0213;
const IO_PORT_PIT_CMD: u16 = 0x0008;
const IO_PORT_PIT_CONTROL: u16 = 0x0043;
const IO_PORT_TIMER_MISC: u16 = 0x0063;

fn port_name(port: u16) -> &'static str {
    match port {
        IO_PORT_STRING_PRINT => "STRING_PRINT",
        IO_PORT_KEYBOARD_INPUT => "KEYBOARD_INPUT",
        IO_PORT_DISK_DATA => "DISK_DATA",
        IO_PORT_POST => "POST",
        IO_PORT_PIC_MASTER_CMD => "PIC_MASTER_CMD",
        IO_PORT_PIC_MASTER_DATA => "PIC_MASTER_DATA",
        IO_PORT_PIC_SLAVE_CMD => "PIC_SLAVE_CMD",
        IO_PORT_PIC_SLAVE_DATA => "PIC_SLAVE_DATA",
        IO_PORT_SYS_CTRL => "SYS_CTRL",
        IO_PORT_MDA_MODE => "MDA_MODE",
        IO_PORT_CGA_MODE => "CGA_MODE",
        IO_PORT_DMA_PAGE3 => "DMA_PAGE3",
        IO_PORT_VIDEO_MISC_B8 => "VIDEO_MISC_B8",
        IO_PORT_SPECIAL_213 => "PORT_213",
        IO_PORT_PIT_CONTROL => "PIT_CONTROL",
        IO_PORT_PIT_CMD => "PIT_CMD",
        IO_PORT_TIMER_MISC => "TIMER_MISC",
        _ => "UNKNOWN",
    }
}

const DISK_IMAGE_SIZE: usize = 512;
static mut DISK_IMAGE: [u8; DISK_IMAGE_SIZE] = [0; DISK_IMAGE_SIZE];
static mut DISK_OFFSET: usize = 0;
static mut LAST_UNKNOWN_PORT: u16 = 0;
static mut UNKNOWN_PORT_COUNT: u32 = 0;
static mut PIC_MASTER_IMR: u8 = 0;
static mut PIC_SLAVE_IMR: u8 = 0;
static mut SYS_CTRL: u8 = 0;
static mut PIT_CONTROL: u8 = 0;
static mut CGA_MODE: u8 = 0;
static mut MDA_MODE: u8 = 0;

const CGA_COLS: usize = 80;
const CGA_ROWS: usize = 25;
static mut CGA_BUFFER: [u16; CGA_COLS * CGA_ROWS] = [0x0720; CGA_COLS * CGA_ROWS];
static mut CGA_CURSOR: usize = 0;

fn cga_put_char(ch: u8) {
    unsafe {
        if ch == b'\r' {
            CGA_CURSOR -= CGA_CURSOR % CGA_COLS;
            return;
        }
        if ch == b'\n' {
            CGA_CURSOR += CGA_COLS;
        } else {
            if CGA_CURSOR >= CGA_COLS * CGA_ROWS {
                CGA_BUFFER.copy_within(CGA_COLS.., 0);
                for i in 0..CGA_COLS {
                    CGA_BUFFER[CGA_COLS * (CGA_ROWS - 1) + i] = 0x0720;
                }
                CGA_CURSOR -= CGA_COLS;
            }
            CGA_BUFFER[CGA_CURSOR] = 0x0700 | (ch as u16);
            CGA_CURSOR += 1;
        }
        if CGA_CURSOR >= CGA_COLS * CGA_ROWS {
            CGA_CURSOR = CGA_COLS * CGA_ROWS - 1;
        }
    }
}

fn print_cga_buffer() {
    unsafe {
        println!("\n----- CGA Text Buffer -----");
        for r in 0..CGA_ROWS {
            for c in 0..CGA_COLS {
                let mut ch = (CGA_BUFFER[r * CGA_COLS + c] & 0xFF) as u8;
                if ch == 0 {
                    ch = b' ';
                }
                print!("{}", ch as char);
            }
            println!("");
        }
    }
}

const INITIAL_VCPU_COUNT: usize = 40;
const INITIAL_VCPU_REGISTER_NAMES: [WHV_REGISTER_NAME; INITIAL_VCPU_COUNT] = [
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
    WHvX64RegisterRflags,
    WHvX64RegisterEs,
    WHvX64RegisterCs,
    WHvX64RegisterSs,
    WHvX64RegisterDs,
    WHvX64RegisterFs,
    WHvX64RegisterGs,
    WHvX64RegisterLdtr,
    WHvX64RegisterTr,
    WHvX64RegisterIdtr,
    WHvX64RegisterGdtr,
    WHvX64RegisterCr0,
    WHvX64RegisterCr2,
    WHvX64RegisterCr3,
    WHvX64RegisterCr4,
    WHvX64RegisterDr0,
    WHvX64RegisterDr1,
    WHvX64RegisterDr2,
    WHvX64RegisterDr3,
    WHvX64RegisterDr6,
    WHvX64RegisterDr7,
    WHvX64RegisterXCr0,
    WHvX64RegisterFpControlStatus,
];
// Note: WHV_REGISTER_VALUE should be aligned on 16-byte boundary. However, the definition of it isn't aligned to 16-byte boundary.
const INITIAL_VCPU_REGISTER_VALUES: Aligned<A16, [WHV_REGISTER_VALUE; INITIAL_VCPU_COUNT]> =
    Aligned([
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0xFFF0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0xFFF0 },
        WHV_REGISTER_VALUE { Reg64: 0x2 },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x93 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0xF0000,
                Limit: 0xFFFF,
                Selector: 0xF000,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x9B },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x93 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x93 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x93 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x93 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x82 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x83 },
            },
        },
        WHV_REGISTER_VALUE {
            Table: WHV_X64_TABLE_REGISTER {
                Base: 0,
                Pad: [0; 3],
                Limit: 0xFFFF,
            },
        },
        WHV_REGISTER_VALUE {
            Table: WHV_X64_TABLE_REGISTER {
                Base: 0,
                Pad: [0; 3],
                Limit: 0xFFFF,
            },
        },
        WHV_REGISTER_VALUE { Reg64: 0x10 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0xFFFF0FF0 },
        WHV_REGISTER_VALUE { Reg64: 0x400 },
        WHV_REGISTER_VALUE { Reg64: 1 },
        WHV_REGISTER_VALUE {
            FpControlStatus: WHV_X64_FP_CONTROL_STATUS_REGISTER {
                Anonymous: WHV_X64_FP_CONTROL_STATUS_REGISTER_0 {
                    FpControl: 0x40,
                    FpStatus: 0,
                    FpTag: 0x55,
                    Reserved: 0,
                    LastFpOp: 0,
                    Anonymous: WHV_X64_FP_CONTROL_STATUS_REGISTER_0_0 { LastFpRip: 0 },
                },
            },
        },
    ]);

#[repr(C)]
struct SimpleVirtualMachine {
    handle: WHV_PARTITION_HANDLE,
    vmem: *mut c_void,
    size: usize,
}

impl SimpleVirtualMachine {
    fn new(memory_size: usize) -> Result<Self> {
        match unsafe { WHvCreatePartition() } {
            Ok(h) => {
                let vcpu_count_prop = WHV_PARTITION_PROPERTY { ProcessorCount: 1 };
                if let Err(e) = unsafe {
                    WHvSetPartitionProperty(
                        h,
                        WHvPartitionPropertyCodeProcessorCount,
                        (&raw const vcpu_count_prop).cast(),
                        size_of::<WHV_PARTITION_PROPERTY>() as u32,
                    )
                } {
                    panic!("Failed to setup vCPU Count! Reason: {e}");
                }
                if let Err(e) = unsafe { WHvSetupPartition(h) } {
                    panic!("Failed to setup partition! Reason: {e}");
                }
                if let Err(e) = unsafe { WHvCreateVirtualProcessor(h, 0, 0) } {
                    panic!("Failed to create vCPU! Reason: {e}");
                } else {
                    if let Err(e) = unsafe {
                        WHvSetVirtualProcessorRegisters(
                            h,
                            0,
                            INITIAL_VCPU_REGISTER_NAMES.as_ptr(),
                            INITIAL_VCPU_COUNT as u32,
                            INITIAL_VCPU_REGISTER_VALUES.as_ptr(),
                        )
                    } {
                        panic!("Failed to initialize vCPU registers! Reason: {e}");
                    }
                }
                let p = unsafe { VirtualAlloc(None, memory_size, MEM_COMMIT, PAGE_READWRITE) };
                if p.is_null() {
                    panic!(
                        "Failed to allocate virtual memory! Reason: {}",
                        unsafe { GetLastError() }.to_hresult().message()
                    );
                }
                if let Err(e) = unsafe {
                    WHvMapGpaRange(
                        h,
                        p,
                        0,
                        memory_size as u64,
                        WHvMapGpaRangeFlagRead
                            | WHvMapGpaRangeFlagWrite
                            | WHvMapGpaRangeFlagExecute,
                    )
                } {
                    panic!("Failed to map guest memory! Reason: {e}");
                }
                if let Err(e) = unsafe {
                    WHvMapGpaRange(
                        h,
                        p,
                        memory_size as u64,
                        memory_size as u64,
                        WHvMapGpaRangeFlagRead
                            | WHvMapGpaRangeFlagWrite
                            | WHvMapGpaRangeFlagExecute,
                    )
                } {
                    panic!("Failed to map high memory mirror! Reason: {e}");
                }
                Ok(Self {
                    handle: h,
                    vmem: p,
                    size: memory_size,
                })
            }
            Err(e) => Err(e),
        }
    }

    fn load_program(&self, file_name: &str, offset: usize) -> Result<usize> {
        let path = file_name.encode_utf16();
        let v: Vec<u16> = path.collect();
        match unsafe {
            CreateFileW(
                PCWSTR(v.as_ptr()),
                GENERIC_READ.0,
                FILE_SHARE_READ,
                None,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                None,
            )
        } {
            Ok(h) => {
                let mut file_size = 0;
                let r = match unsafe { GetFileSizeEx(h, &raw mut file_size) } {
                    Ok(_) => {
                        if offset + file_size as usize > self.size {
                            panic!("Overflow happens while loading {file_name}!");
                        }
                        let mut size = 0;
                        let addr: *mut u8 = unsafe { self.vmem.byte_add(offset).cast() };
                        let buffer: &mut [u8] =
                            unsafe { slice::from_raw_parts_mut(addr, file_size as usize) };
                        unsafe { ReadFile(h, Some(buffer), Some(&raw mut size), None) }
                            .map(|_| size as usize)
                    }
                    Err(e) => Err(e),
                };
                let _ = unsafe { CloseHandle(h) };
                r
            }
            Err(e) => Err(e),
        }
    }

    fn run(&self) {
        let mut exit_ctxt: WHV_RUN_VP_EXIT_CONTEXT = WHV_RUN_VP_EXIT_CONTEXT::default();
        let mut cont_exec = true;
        while cont_exec {
            if let Err(e) = unsafe {
                WHvRunVirtualProcessor(
                    self.handle,
                    0,
                    (&raw mut exit_ctxt).cast(),
                    size_of::<WHV_RUN_VP_EXIT_CONTEXT>() as u32,
                )
            } {
                println!("Failed to run vCPU! Reason: {e}");
                cont_exec = false;
            } else {
                #[allow(non_upper_case_globals)]
                match exit_ctxt.ExitReason {
                    WHvRunVpExitReasonX64IoPortAccess => {
                        match self.try_emulate_io(&raw const exit_ctxt.VpContext, unsafe {
                            &raw const exit_ctxt.Anonymous.IoPortAccess
                        }) {
                            Ok(st) => {
                                let s = unsafe { st.AsUINT32 };
                                if s != 1 {
                                    println!(
                                        "Failed to emulate I/O instruction! Status=0x{:08X}",
                                        s
                                    )
                                }
                            }
                            Err(e) => {
                                println!("Failed to emulate I/O instruction! Reason: {e}");
                                cont_exec = false;
                            }
                        }
                    }
                    WHvRunVpExitReasonX64Halt => {
                        // Treat HLT as a NOP so BIOS busy
                        // loops keep running even with
                        // interrupts disabled.
                        let rip_name = WHV_REGISTER_NAME::WHvX64RegisterRip;
                        let mut rip_val = WHV_REGISTER_VALUE {
                            AsUINT64: exit_ctxt.VpContext.Rip,
                        };
                        rip_val.AsUINT64 += exit_ctxt.VpContext.InstructionLength as u64;
                        unsafe {
                            WHvSetVirtualProcessorRegisters(partition, 0, &rip_name, 1, &rip_val);
                        }
                        cont_exec = true;
                    }
                    _ => {
                        println!("Unknown Exit Reason: 0x{:X}!", exit_ctxt.ExitReason.0);
                        cont_exec = false;
                    }
                }
            }
        }
    }

    fn try_emulate_io(
        &self,
        vcpu_ctxt: *const WHV_VP_EXIT_CONTEXT,
        io_ctxt: *const WHV_X64_IO_PORT_ACCESS_CONTEXT,
    ) -> std::result::Result<WHV_EMULATOR_STATUS, Error> {
        unsafe {
            WHvEmulatorTryIoEmulation(
                GLOBAL_EMULATOR_HANDLE.load(Ordering::Relaxed),
                (self as *const Self).cast(),
                vcpu_ctxt,
                io_ctxt,
            )
        }
    }

    fn patch_reset_vector(&self) {
        unsafe {
            let mem = self.vmem as *mut u8;
            let jump: [u8; 5] = [0xEA, 0x00, 0x00, 0x00, 0xF0];
            std::ptr::copy_nonoverlapping(jump.as_ptr(), mem.add(0xFFFF0), 5);
        }
    }

    fn mirror_region(&self, offset: usize, size: usize, total: usize) {
        if size == 0 || size >= total {
            return;
        }
        unsafe {
            let mem = self.vmem as *mut u8;
            let src = mem.add(offset);
            let mut pos = size;
            while pos < total {
                std::ptr::copy_nonoverlapping(src, mem.add(offset + pos), size);
                pos += size;
            }
        }
    }
}

fn load_disk_image(path: &str) -> bool {
    if let Ok(mut f) = File::open(path) {
        let mut buf = [0u8; DISK_IMAGE_SIZE];
        if f.read_exact(&mut buf).is_ok() {
            unsafe {
                DISK_IMAGE = buf;
            }
            return true;
        }
    }
    false
}

impl Drop for SimpleVirtualMachine {
    fn drop(&mut self) {
        if let Err(e) = unsafe { WHvDeletePartition(self.handle) } {
            panic!("Failed to delete virtual machine! Reason: {e}");
        }
        if let Err(e) = unsafe { VirtualFree(self.vmem, 0, MEM_RELEASE) } {
            panic!("Failed to release virtual memory! Reason: {e}");
        }
        println!("Successfully deleted virtual machine!");
    }
}

unsafe extern "system" fn emu_io_port_callback(
    _context: *const c_void,
    io_access: *mut WHV_EMULATOR_IO_ACCESS_INFO,
) -> HRESULT {
    unsafe {
        if (*io_access).Direction == 0 {
            println!(
                "IN  port 0x{:04X} ({}) , size {}",
                (*io_access).Port,
                port_name((*io_access).Port),
                (*io_access).AccessSize
            );
            if (*io_access).Port == IO_PORT_KEYBOARD_INPUT {
                for i in 0..(*io_access).AccessSize {
                    let mut buf = [0u8; 1];
                    if std::io::stdin().read_exact(&mut buf).is_ok() {
                        (*io_access).Data |= (buf[0] as u32) << (i * 8);
                    } else {
                        return E_FAIL;
                    }
                }
                S_OK
            } else if (*io_access).Port == IO_PORT_STRING_PRINT {
                (*io_access).Data = 0;
                S_OK
            } else if (*io_access).Port == IO_PORT_DISK_DATA {
                for i in 0..(*io_access).AccessSize as usize {
                    (*io_access).Data |= (DISK_IMAGE[DISK_OFFSET] as u32) << (i * 8);
                    DISK_OFFSET = (DISK_OFFSET + 1) % DISK_IMAGE_SIZE;
                }
                S_OK
            } else if (*io_access).Port == IO_PORT_POST {
                (*io_access).Data = 0;
                S_OK
            } else if (*io_access).Port == IO_PORT_SYS_CTRL {
                (*io_access).Data = SYS_CTRL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_CGA_MODE {
                (*io_access).Data = CGA_MODE as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_MDA_MODE {
                (*io_access).Data = MDA_MODE as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIT_CONTROL {
                (*io_access).Data = PIT_CONTROL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_MASTER_DATA {
                (*io_access).Data = PIC_MASTER_IMR as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_SLAVE_DATA {
                (*io_access).Data = PIC_SLAVE_IMR as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_MASTER_CMD
                || (*io_access).Port == IO_PORT_PIC_SLAVE_CMD
            {
                (*io_access).Data = 0;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_PAGE3
                || (*io_access).Port == IO_PORT_VIDEO_MISC_B8
                || (*io_access).Port == IO_PORT_SPECIAL_213
                || (*io_access).Port == IO_PORT_PIT_CMD
                || (*io_access).Port == IO_PORT_PIT_CONTROL
                || (*io_access).Port == IO_PORT_TIMER_MISC
            {
                (*io_access).Data = 0;
                S_OK
            } else {
                println!(
                    "Input from port 0x{:04X} ({}) is not implemented!",
                    (*io_access).Port,
                    port_name((*io_access).Port)
                );
                E_NOTIMPL
            }
        } else {
            println!(
                "OUT port 0x{:04X} ({}) , size {}, value 0x{:X}",
                (*io_access).Port,
                port_name((*io_access).Port),
                (*io_access).AccessSize,
                (*io_access).Data
            );
            if (*io_access).Port == IO_PORT_STRING_PRINT {
                for i in 0..(*io_access).AccessSize {
                    let ch = (((*io_access).Data >> (i * 8)) as u8);
                    print!("{}", ch as char);
                    cga_put_char(ch);
                }
                S_OK
            } else if (*io_access).Port == IO_PORT_DISK_DATA {
                for i in 0..(*io_access).AccessSize as usize {
                    DISK_IMAGE[DISK_OFFSET] = ((*io_access).Data >> (i * 8)) as u8;
                    DISK_OFFSET = (DISK_OFFSET + 1) % DISK_IMAGE_SIZE;
                }
                S_OK
            } else if (*io_access).Port == IO_PORT_POST {
                S_OK
            } else if (*io_access).Port == IO_PORT_SYS_CTRL {
                SYS_CTRL = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_CGA_MODE {
                CGA_MODE = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_MDA_MODE {
                MDA_MODE = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIT_CONTROL {
                PIT_CONTROL = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_MASTER_CMD {
                PIC_MASTER_IMR = (*io_access).Data as u8; // treat command as IMR for simplicity
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_SLAVE_CMD {
                PIC_SLAVE_IMR = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_MASTER_DATA {
                PIC_MASTER_IMR = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_SLAVE_DATA {
                PIC_SLAVE_IMR = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_PAGE3
                || (*io_access).Port == IO_PORT_VIDEO_MISC_B8
                || (*io_access).Port == IO_PORT_SPECIAL_213
                || (*io_access).Port == IO_PORT_PIT_CMD
                || (*io_access).Port == IO_PORT_PIT_CONTROL
                || (*io_access).Port == IO_PORT_TIMER_MISC
            {
                // These ports are touched by the BIOS during POST but
                // are not modeled. Simply accept the write.
                S_OK
            } else {
                if (*io_access).Port == LAST_UNKNOWN_PORT {
                    UNKNOWN_PORT_COUNT += 1;
                } else {
                    LAST_UNKNOWN_PORT = (*io_access).Port;
                    UNKNOWN_PORT_COUNT = 1;
                }
                println!(
                    "Unknown I/O Port (0x{:04X}) is accessed!",
                    (*io_access).Port
                );
                if UNKNOWN_PORT_COUNT >= 2 {
                    println!(
                        "Repeated access to unknown port 0x{:04X}, terminating.",
                        (*io_access).Port
                    );
                    std::process::exit(1);
                }
                E_NOTIMPL
            }
        }
    }
}

unsafe extern "system" fn emu_memory_callback(
    context: *const c_void,
    memory_access: *mut WHV_EMULATOR_MEMORY_ACCESS_INFO,
) -> HRESULT {
    unsafe {
        let ctxt: &SimpleVirtualMachine = &(*context.cast());
        let len: usize = (*memory_access).AccessSize as usize;
        let mut gpa = (*memory_access).GpaAddress as usize % ctxt.size;
        for i in 0..len {
            let hva: *mut u8 = ctxt.vmem.byte_add((gpa + i) % ctxt.size).cast();
            if (*memory_access).Direction != 0 {
                unsafe { *hva = (*memory_access).Data[i] };
            } else {
                unsafe { (*memory_access).Data[i] = *hva };
            }
        }
        S_OK
    }
}

unsafe extern "system" fn emu_get_vcpu_reg_callback(
    context: *const c_void,
    reg_names: *const WHV_REGISTER_NAME,
    reg_count: u32,
    reg_values: *mut WHV_REGISTER_VALUE,
) -> HRESULT {
    unsafe {
        let ctxt: &SimpleVirtualMachine = &(*context.cast());
        match WHvGetVirtualProcessorRegisters(ctxt.handle, 0, reg_names, reg_count, reg_values) {
            Ok(_) => S_OK,
            Err(e) => e.code(),
        }
    }
}

unsafe extern "system" fn emu_set_vcpu_reg_callback(
    context: *const c_void,
    reg_names: *const WHV_REGISTER_NAME,
    reg_count: u32,
    reg_values: *const WHV_REGISTER_VALUE,
) -> HRESULT {
    unsafe {
        let ctxt: &SimpleVirtualMachine = &(*context.cast());
        match WHvSetVirtualProcessorRegisters(ctxt.handle, 0, reg_names, reg_count, reg_values) {
            Ok(_) => S_OK,
            Err(e) => e.code(),
        }
    }
}

unsafe extern "system" fn emu_translate_gva_callback(
    context: *const c_void,
    gva_page: u64,
    translate_flags: WHV_TRANSLATE_GVA_FLAGS,
    translation_result: *mut WHV_TRANSLATE_GVA_RESULT_CODE,
    gpa_page: *mut u64,
) -> HRESULT {
    unsafe {
        let ctxt: &SimpleVirtualMachine = &(*context.cast());
        let mut r: WHV_TRANSLATE_GVA_RESULT = WHV_TRANSLATE_GVA_RESULT::default();
        match WHvTranslateGva(
            ctxt.handle,
            0,
            gva_page,
            translate_flags,
            &raw mut r,
            gpa_page,
        ) {
            Ok(_) => {
                *translation_result = r.ResultCode;
                S_OK
            }
            Err(e) => e.code(),
        }
    }
}

fn init_whpx() -> HRESULT {
    let mut hv_present: WHV_CAPABILITY = WHV_CAPABILITY::default();
    let r = unsafe {
        WHvGetCapability(
            WHvCapabilityCodeHypervisorPresent,
            (&raw mut hv_present).cast(),
            size_of::<WHV_CAPABILITY>() as u32,
            None,
        )
    };
    match r {
        Ok(_) => {
            if unsafe { hv_present.HypervisorPresent }.as_bool() {
                let mut eh: *mut c_void = null_mut();
                match unsafe {
                    WHvEmulatorCreateEmulator(&raw const GLOBAL_EMULATOR_CALLBACKS, &raw mut eh)
                } {
                    Ok(_) => {
                        GLOBAL_EMULATOR_HANDLE.store(eh, Ordering::Relaxed);
                        S_OK
                    }
                    Err(e) => e.code(),
                }
            } else {
                S_FALSE
            }
        }
        Err(e) => {
            println!("Failed to query Windows Hypervisor Platform presence! Reason: {e}");
            e.code()
        }
    }
}

fn main() {
    println!("SimpleWhpDemo version {}", env!("CARGO_PKG_VERSION"));
    println!("IVT firmware version 0.1.0");
    let args: Vec<String> = std::env::args().collect();
    let program = args.get(1).map(String::as_str).unwrap_or("hello.com");
    let bios = args.get(2).map(String::as_str).unwrap_or(DEFAULT_BIOS);
    if init_whpx() == S_OK {
        println!("WHPX is present and initalized!");
        if let Ok(vm) = SimpleVirtualMachine::new(0x100000) {
            println!("Successfully created virtual machine!");
            let bios_size = match vm.load_program(bios, 0xF0000) {
                Ok(size) => size,
                Err(_) => {
                    if bios == DEFAULT_BIOS {
                        println!("AMI BIOS not found, falling back to {FALLBACK_BIOS}");
                        let size = vm
                            .load_program(FALLBACK_BIOS, 0xF0000)
                            .expect("Failed to load firmware!");
                        vm.patch_reset_vector();
                        size
                    } else {
                        panic!("Failed to load firmware!");
                    }
                }
            };
            if bios == FALLBACK_BIOS {
                vm.patch_reset_vector();
            }
            if bios_size < 0x10000 {
                vm.mirror_region(0xF0000, bios_size, 0x10000);
            }
            if let Err(e) = vm.load_program(program, 0x10100) {
                panic!("Failed to load program! Reason: {e}");
            }
            if !load_disk_image("disk.img\0") {
                println!("Warning: disk image not loaded, disk reads will return zeros.");
            }
            println!("============ Program Start ============");
            vm.run();
            println!("============= Program End =============");
            print_cga_buffer();
        }
        let _ =
            unsafe { WHvEmulatorDestroyEmulator(GLOBAL_EMULATOR_HANDLE.load(Ordering::Relaxed)) };
    }
}
