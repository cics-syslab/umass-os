// Physical memory layout

// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0 
// 10001000 -- virtio disk 
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

// Also useful for understanding this file are the qemu docs for the virt board
// which is used to run xv6
// https://github.com/qemu/qemu/blob/master/hw/riscv/virt.c
// https://github.com/qemu/qemu/blob/master/include/hw/riscv/virt.h
// These two files define the EEI that qemu provides to xv6, including among many things
// the plic interrupt source numbers for the uart and the virtio mmio interfaces,
// referred to as IRQs (why IRQ?, why are they not sequential?, why are they not ordered?,
// these are the unanswerable questions of OS development :) )
// enum {
//     UART0_IRQ = 10,
//     RTC_IRQ = 11,
//     VIRTIO_IRQ = 1, /* 1 to 8 */
//     VIRTIO_COUNT = 8,
//     PCIE_IRQ = 0x20, /* 32 to 35 */
//     VIRT_PLATFORM_BUS_IRQ = 64, /* 64 to 95 */
// };
// static const MemMapEntry virt_memmap[] = {
//     [VIRT_DEBUG] =        {        0x0,         0x100 },
//     [VIRT_MROM] =         {     0x1000,        0xf000 },
//     [VIRT_TEST] =         {   0x100000,        0x1000 },
//     [VIRT_RTC] =          {   0x101000,        0x1000 },
//     [VIRT_CLINT] =        {  0x2000000,       0x10000 },
//     [VIRT_ACLINT_SSWI] =  {  0x2F00000,        0x4000 },
//     [VIRT_PCIE_PIO] =     {  0x3000000,       0x10000 },
//     [VIRT_PLATFORM_BUS] = {  0x4000000,     0x2000000 },
//     [VIRT_PLIC] =         {  0xc000000, VIRT_PLIC_SIZE(VIRT_CPUS_MAX * 2) },
//     [VIRT_APLIC_M] =      {  0xc000000, APLIC_SIZE(VIRT_CPUS_MAX) },
//     [VIRT_APLIC_S] =      {  0xd000000, APLIC_SIZE(VIRT_CPUS_MAX) },
//     [VIRT_UART0] =        { 0x10000000,         0x100 },
//     [VIRT_VIRTIO] =       { 0x10001000,        0x1000 },
//     [VIRT_FW_CFG] =       { 0x10100000,          0x18 },
//     [VIRT_FLASH] =        { 0x20000000,     0x4000000 },
//     [VIRT_IMSIC_M] =      { 0x24000000, VIRT_IMSIC_MAX_SIZE },
//     [VIRT_IMSIC_S] =      { 0x28000000, VIRT_IMSIC_MAX_SIZE },
//     [VIRT_PCIE_ECAM] =    { 0x30000000,    0x10000000 },
//     [VIRT_PCIE_MMIO] =    { 0x40000000,    0x40000000 },
//     [VIRT_DRAM] =         { 0x80000000,           0x0 },
// };



// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio interface
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

// qemu puts platform-level interrupt controller (PLIC) here.
#define PLIC 0x0c000000L
// This doesn't appear to be used anywhere in the OS
#define PLIC_PRIORITY (PLIC + 0x0)
// This doesn't appear to be used anywhere in the OS
#define PLIC_PENDING (PLIC + 0x1000)

/*
We know these are the locations of the target registers from
the spec for the PLIC that qemu emulates:
https://static.dev.sifive.com/SiFive-U5-Coreplex-v1.0.pdf
Which says:
    Interrupt targets are mapped to harts sequentially, 
    with interrupt targets being added for each hart’s M-mode, 
    H-mode, S-mode, and U-mode contexts sequentially in that order.
It is also supporrted by the function that ultimately assigns
the order to the target registers by parsing a config string
https://github.com/qemu/qemu/blob/master/hw/intc/sifive_plic.c#L292
*/
// This doesn't appear to be used anywhere in the OS
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
// This doesn't appear to be used anywhere in the OS
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
// This doesn't appear to be used anywhere in the OS
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// the kernel expects there to be RAM
// for use by the kernel and user pages
// from physical address 0x80000000 to PHYSTOP.
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)

// map the trampoline page to the highest address,
// in both user and kernel space.
#define TRAMPOLINE (MAXVA - PGSIZE)

// map kernel stacks beneath the trampoline,
// each surrounded by invalid guard pages.
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ...
//   TRAPFRAME (p->trapframe, used by the trampoline)
//   TRAMPOLINE (the same page as in the kernel)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
