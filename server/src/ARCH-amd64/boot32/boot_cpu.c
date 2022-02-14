/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "types.h"
#include "boot_cpu.h"
#include "cpu_info.h"
#include "paging.h"
#include "support.h"


unsigned  KERNEL_CS_64		= 0x20; // XXX

// enum of 32-bit size members
enum
{
  CR0_PG		= 0x80000000,
  CR4_PSE		= 1U << 4,
  CR4_PAE		= 1U << 5,
  CR4_OSFXSR		= 1U << 9,
  EFL_AC		= 0x00040000,
  EFL_ID		= 0x00200000,
  EFER_LME		= 0x00000100,

  BASE_TSS		= 0x08,
  KERNEL_CS		= 0x10,
  KERNEL_DS		= 0x18,

  DBF_TSS		= 0x28, // XXX check this value

  ACC_TSS		= 0x09,
  ACC_TSS_BUSY		= 0x02,
  ACC_CODE_R		= 0x1a,
  ACC_DATA_W		= 0x12,
  ACC_PL_K		= 0x00,
  ACC_P			= 0x80,
  SZ_32			= 0x4,
  SZ_16			= 0x0,
  SZ_G			= 0x8,
  SZ_CODE_64		= 0x2, // XXX 64 Bit Code Segment

  GDTSZ			= (0x30/8), // XXX check this value
  IDTSZ			= 256,
};


struct pseudo_descriptor
{
  l4_uint16_t pad;
  l4_uint16_t limit;
  l4_uint32_t linear_base;
};

struct x86_desc
{
  l4_uint16_t limit_low;		/* limit 0..15 */
  l4_uint16_t base_low;		/* base  0..15 */
  l4_uint8_t  base_med;		/* base  16..23 */
  l4_uint8_t  access;		/* access byte */
  l4_uint8_t  limit_high:4;	/* limit 16..19 */
  l4_uint8_t  granularity:4;	/* granularity */
  l4_uint8_t  base_high;		/* base 24..31 */
} __attribute__((packed));

struct x86_gate
{
  l4_uint16_t offset_low;	/* offset 0..15 */
  l4_uint16_t selector;
  l4_uint8_t  word_count;
  l4_uint8_t  access;
  l4_uint16_t offset_high;	/* offset 16..31 */
} __attribute__((packed));

struct x86_tss
{
  l4_uint32_t back_link;
  l4_uint32_t esp0, ss0;
  l4_uint32_t esp1, ss1;
  l4_uint32_t esp2, ss2;
  l4_uint32_t cr3;
  l4_uint32_t eip, eflags;
  l4_uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
  l4_uint32_t es, cs, ss, ds, fs, gs;
  l4_uint32_t ldt;
  l4_uint16_t trace_trap;
  l4_uint16_t io_bit_map_offset;
};

struct gate_init_entry
{
  l4_uint32_t entrypoint;
  l4_uint16_t vector;
  l4_uint16_t type;
};

struct trap_state
{
  l4_uint32_t gs, fs, es, ds;
  l4_uint32_t edi, esi, ebp, cr2, ebx, edx, ecx, eax;
  l4_uint32_t trapno, err;
  l4_uint32_t eip, cs, eflags, esp, ss;
};

static l4_uint32_t      base_pml4_pa;
static struct x86_tss   base_tss;
static struct x86_desc  base_gdt[GDTSZ];
static struct x86_gate  base_idt[IDTSZ];

static void handle_dbf(void);
static char           dbf_stack[2048];
static struct x86_tss dbf_tss =
  {
    0/*back_link*/,
    0/*esp0*/, 0/*ss0*/, 0/*esp1*/, 0/*ss1*/, 0/*esp2*/, 0/*ss2*/,
    0/*cr3*/,
    (l4_uint32_t)handle_dbf/*eip*/, 0x00000082/*eflags*/,
    0/*eax*/, 0/*ecx*/, 0/*edx*/, 0/*ebx*/,
    (l4_uint32_t)dbf_stack + sizeof(dbf_stack)/*esp*/,
    0/*ebp*/, 0/*esi*/, 0/*edi*/,
    KERNEL_DS/*es*/, KERNEL_CS/*cs*/, KERNEL_DS/*ss*/,
    KERNEL_DS/*ds*/, KERNEL_DS/*fs*/, KERNEL_DS/*gs*/,
    0/*ldt*/, 0/*trace_trap*/, 0x8000/*io_bit_map_offset*/
  };

static inline l4_uint32_t get_eflags(void)
{ l4_uint32_t efl; asm volatile("pushf ; popl %0" : "=r" (efl)); return efl; }

static inline void set_eflags(l4_uint32_t efl)
{ asm volatile("pushl %0 ; popf" : : "r" (efl) : "memory"); }

static inline void set_ds(l4_uint16_t ds)
{ asm volatile("movw %w0,%%ds" : : "r" (ds)); }

static inline void set_es(l4_uint16_t es)
{ asm volatile("movw %w0,%%es" : : "r" (es)); }

static inline void set_fs(l4_uint16_t fs)
{ asm volatile("movw %w0,%%fs" : : "r" (fs)); }

static inline void set_gs(l4_uint16_t gs)
{ asm volatile("movw %w0,%%gs" : : "r" (gs)); }

static inline void set_ss(l4_uint16_t ss)
{ asm volatile("movw %w0,%%ss" : : "r" (ss)); }

static inline l4_uint16_t get_ss(void)
{ l4_uint16_t ss; asm volatile("movw %%ss,%w0" : "=r" (ss)); return ss; }

#define set_idt(pseudo_desc) \
 asm volatile("lidt %0" : : "m" ((pseudo_desc)->limit) : "memory")

#define set_gdt(pseudo_desc) \
 asm volatile("lgdt %0" : : "m" ((pseudo_desc)->limit) : "memory")

#define	set_tr(seg) \
 asm volatile("ltr %0" : : "rm" ((l4_uint16_t)(seg)))

#define get_esp() \
 ({ register l4_uint32_t _temp__; \
    asm("movl %%esp, %0" : "=r" (_temp__)); _temp__; })

#define	get_cr0() \
 ({ register l4_uint32_t _temp__; \
    asm volatile("mov %%cr0, %0" : "=r" (_temp__)); _temp__; })

#define	set_cr3(value) \
 ({ register l4_uint32_t _temp__ = (value); \
    asm volatile("mov %0, %%cr3" : : "r" (_temp__)); })

#define get_cr4() \
 ({ register l4_uint32_t _temp__; \
    asm volatile("mov %%cr4, %0" : "=r" (_temp__)); _temp__; })

#define set_cr4(value) \
 ({ register l4_uint32_t _temp__ = (value); \
    asm volatile("mov %0, %%cr4" : : "r" (_temp__)); })


static inline void enable_longmode(void)
{
  l4_uint32_t dummy;
  asm volatile("rdmsr; bts $8, %%eax; wrmsr"
               :"=a"(dummy), "=d"(dummy) : "c"(0xc0000080));
}

static inline void
fill_descriptor(struct x86_desc *desc, l4_uint32_t base, l4_uint32_t limit,
		l4_uint8_t access, l4_uint8_t sizebits)
{
  if (limit > 0xfffff)
    {
      limit >>= 12;
      sizebits |= SZ_G;
    }
  desc->limit_low = limit & 0xffff;
  desc->base_low = base & 0xffff;
  desc->base_med = (base >> 16) & 0xff;
  desc->access = access | ACC_P;
  desc->limit_high = limit >> 16;
  desc->granularity = sizebits;
  desc->base_high = base >> 24;
}

static inline void
fill_gate(struct x86_gate *gate, l4_uint32_t offset,
	  l4_uint16_t selector, l4_uint8_t access)
{
  gate->offset_low  = offset & 0xffff;
  gate->selector    = selector;
  gate->word_count  = 0;
  gate->access      = access | ACC_P;
  gate->offset_high = (offset >> 16) & 0xffff;
}

static inline void
paging_enable(l4_uint32_t pml4)
{
  /* Enable Physical l4_uint64_t Extension (PAE). */
  set_cr4(get_cr4() | CR4_PAE);

  /* Load the page map level 4.  */
  set_cr3(pml4);

  /* Enable long mode. */
  enable_longmode();

  /* Turn on paging and switch to long mode. */
  asm volatile("movl  %0,%%cr0 ; jmp  1f ; 1:" : : "r" (get_cr0() | CR0_PG));
}

static void
cpuid(void)
{
  int orig_eflags = get_eflags();

  /* Check for a dumb old 386 by trying to toggle the AC flag.  */
  set_eflags(orig_eflags ^ EFL_AC);
  if ((get_eflags() ^ orig_eflags) & EFL_AC)
    {
      /* It's a 486 or better.  Now try toggling the ID flag.  */
      set_eflags(orig_eflags ^ EFL_ID);
      if ((get_eflags() ^ orig_eflags) & EFL_ID)
	{
          init_cpu_info();
	}
    }

  set_eflags(orig_eflags);
}

static void
sse_enable(void)
{
  /*
   * Intel manual:
   * - "SSE instructions cannot be used unless XR4.OSFXSR = 1.
   * - CPUID.01H: EDX.FXSR[bit 24]: "... Presence of this bit also indicates
   *   that CR4.OSFXSR is available for an operating system to indicate that it
   *   supports the FXSAVE/FXRSTOR instructions."
   * - CPUID.01H: EDX.SSE[bit 25]: "The processor supports SSE extensions."
   *
   * So it seem that CPUID.01H/EDX.FXSR is sufficient to signal the presence of
   * CR4.OSFXSR while CPUID.01H/EDX.SSE is required to set this bit.
   */
  if (cpu_info.feature_flags & (1 << 25))
    set_cr4(get_cr4() | CR4_OSFXSR);
}

extern struct gate_init_entry boot_idt_inittab[];
static void
base_idt_init(void)
{
  struct x86_gate *dst = base_idt;
  const struct gate_init_entry *src = boot_idt_inittab;

  while (src->entrypoint)
    {
      if ((src->type & 0x1f) == 0x05)
	// task gate
	fill_gate(&dst[src->vector], 0, src->entrypoint, src->type);
      else
	// interrupt gate
	fill_gate(&dst[src->vector], src->entrypoint, KERNEL_CS, src->type);
      src++;
    }
}

static void
base_gdt_init(void)
{
  /* Initialize the base TSS descriptor.  */
  fill_descriptor(&base_gdt[BASE_TSS / 8],
		  (l4_uint32_t)&base_tss, sizeof(base_tss) - 1,
                  ACC_PL_K | ACC_TSS, 0);
  /* Initialize the TSS descriptor for the double fault handler */
  fill_descriptor(&base_gdt[DBF_TSS / 8],
		  (l4_uint32_t)&dbf_tss, sizeof(dbf_tss) - 1,
		  ACC_PL_K | ACC_TSS, 0);
  /* Initialize the 32-bit kernel code and data segment descriptors
     to point to the base of the kernel linear space region.  */
  fill_descriptor(&base_gdt[KERNEL_CS / 8], 0, 0xffffffff,
                  ACC_PL_K | ACC_CODE_R, SZ_32);
  fill_descriptor(&base_gdt[KERNEL_DS / 8], 0, 0xffffffff,
                  ACC_PL_K | ACC_DATA_W, SZ_32);
  /* XXX Initialize the 64-bit kernel code segment descriptor */
  fill_descriptor(&base_gdt[KERNEL_CS_64 / 8], 0, 0xffffffff,
                  ACC_PL_K | ACC_CODE_R, SZ_CODE_64);
}

static void
base_tss_init(void)
{
  base_tss.ss0 = KERNEL_DS;
  base_tss.esp0 = get_esp(); /* only temporary */
  base_tss.io_bit_map_offset = sizeof(base_tss);
}

static void
base_gdt_load(void)
{
  struct pseudo_descriptor pdesc;

  /* Create a pseudo-descriptor describing the GDT.  */
  pdesc.limit = sizeof(base_gdt) - 1;
  pdesc.linear_base = (l4_uint32_t)&base_gdt;

  /* Load it into the CPU.  */
  set_gdt(&pdesc);

  /* Reload all the segment registers from the new GDT. */
  asm volatile("ljmp  %0,$1f ;  1:" : : "i" (KERNEL_CS));
  set_ds(KERNEL_DS);
  set_es(KERNEL_DS);
  set_ss(KERNEL_DS);
  set_fs(0);
  set_gs(0);
}

static void
base_idt_load(void)
{
  struct pseudo_descriptor pdesc;

  /* Create a pseudo-descriptor describing the GDT.  */
  pdesc.limit = sizeof(base_idt) - 1;
  pdesc.linear_base = (l4_uint32_t)&base_idt;
  set_idt(&pdesc);
}

static void
base_tss_load(void)
{
  /* Make sure the TSS isn't marked busy.  */
  base_gdt[BASE_TSS / 8].access &= ~ACC_TSS_BUSY;
  asm volatile ("" : : : "memory");
  set_tr(BASE_TSS);
}

void
base_cpu_setup(void)
{
  cpuid();
  sse_enable();
  base_idt_init();
  base_gdt_init();
  base_tss_init();
  // force tables to memory before loading segment registers
  asm volatile ("" : : : "memory");
  base_gdt_load();
  base_idt_load();
  base_tss_load();
}

struct boot32_info_t boot32_info;

void
ptab_alloc(l4_uint32_t *out_ptab_pa)
{
  static char pool[6 << 12] __attribute__((aligned(4096)));
  static l4_uint32_t pdirs;
  static int initialized;

  if (! initialized)
    {
      initialized = 1;
      boot32_info.ptab64_addr = (l4_uint32_t)pool;
      boot32_info.ptab64_size = sizeof(pool);
      memset(pool, 0, sizeof(pool));
      pdirs = ((l4_uint32_t)pool + PAGE_SIZE - 1) & ~PAGE_MASK;
    }

  if (pdirs > (l4_uint32_t)pool + sizeof(pool))
    panic("Cannot allocate page table -- increase ptab_alloc::pool");

  *out_ptab_pa = pdirs;
  pdirs += PAGE_SIZE;
}

void
base_paging_init(l4_uint64_t phys_mem_max)
{
  ptab_alloc(&base_pml4_pa);

  // Establish one-to-one mappings for the physical memory
  ptab_map_range(base_pml4_pa, 0, 0, phys_mem_max, PTAB_WRITE | PTAB_USER);

  //dbf_tss.cr3 = base_pml4_pa;

  // XXX Turn on paging and activate long mode
  paging_enable(base_pml4_pa);
}

void trap_dump_panic(const struct trap_state *st) L4_NORETURN;
void trap_dump_panic(const struct trap_state *st)
{
  int from_user = st->cs & 3;
  int i;

  printf("EAX %08x EBX %08x ECX %08x EDX %08x\n",
         st->eax, st->ebx, st->ecx, st->edx);
  printf("ESI %08x EDI %08x EBP %08x ESP %08x\n",
         st->esi, st->edi, st->ebp,
         from_user ? st->esp : (l4_uint32_t)&st->esp);
  printf("EIP %08x EFLAGS %08x\n", st->eip, st->eflags);
  printf("CS %04x SS %04x DS %04x ES %04x FS %04x GS %04x\n",
         st->cs & 0xffff, from_user ? st->ss & 0xffff : get_ss(),
         st->ds & 0xffff, st->es & 0xffff,
         st->fs & 0xffff, st->gs & 0xffff);
  printf("trapno %d, error %08x, from %s mode\n",
         st->trapno, st->err, from_user ? "user" : "kernel");

  if (st->trapno == 0x0d)
    {
      if (st->err & 1)
	printf("(external event");
      else
	printf("(internal event");
      if (st->err & 2)
        printf(" regarding IDT gate descriptor no. 0x%02x)\n", st->err >> 3);
      else
        printf(" regarding %s entry no. 0x%02x)\n",
               st->err & 4 ? "LDT" : "GDT", st->err >> 3);
    }
  else if (st->trapno == 0x0e)
    printf("page fault linear address %08x\n", st->cr2);

  if (!from_user)
    for (i = 0; i < 32; i++)
      printf("%08x%c", (&st->esp)[i], ((i & 7) == 7) ? '\n' : ' ');

  panic("Unexpected trap while booting Fiasco!");
}

static void
handle_dbf(void)
{
  /*
  printf("\n"
         "EAX %08x EBX %08x ECX %08x EDX %08x\n"
	 "ESI %08x EDI %08x EBP %08x ESP %08x\n"
	 "EIP %08x EFLAGS %08x\n"
	 "CS %04x SS %04x DS %04x ES %04x FS %04x GS %04x\n\n",
	 base_tss.eax, base_tss.ebx, base_tss.ecx, base_tss.edx,
	 base_tss.esi, base_tss.edi, base_tss.ebp, base_tss.esp,
	 base_tss.eip, base_tss.eflags,
	 base_tss.cs & 0xffff, base_tss.ss & 0xffff, base_tss.ds & 0xffff,
	 base_tss.es & 0xffff, base_tss.fs & 0xffff, base_tss.gs & 0xffff);
  */
  panic("Unexpected DOUBLE FAULT while booting Fiasco!");
}
