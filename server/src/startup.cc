/**
 * \file	bootstrap/server/src/startup.cc
 * \brief	Main functions
 *
 * \date	09/2004
 * \author	Torsten Frenzel <frenzel@os.inf.tu-dresden.de>,
 *		Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *		Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *		Alexander Warg <aw11@os.inf.tu-dresden.de>
 *		Sebastian Sumpf <sumpf@os.inf.tu-dresden.de>
 */

/*
 * (c) 2005-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* LibC stuff */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>

/* L4 stuff */
#include <l4/bid_config.h>
#include <l4/sys/compiler.h>
#include <l4/sys/consts.h>
#include <l4/sys/kip.h>
#include <l4/util/mb_info.h>
#include <l4/util/l4_macros.h>
#include "panic.h"

/* local stuff */
#include "platform.h"
#include "exec.h"
#include "macros.h"
#include "region.h"
#include "memcpy_aligned.h"
#include "module.h"
#include "startup.h"
#include "support.h"
#include "init_kip.h"
#include "koptions.h"

#undef getchar

/* management of allocated memory regions */
static Region_list regions;
static Region __regs[300];

/* management of conventional memory regions */
static Region_list ram;
static Region __ram[32];

static Memory _mem_manager = { &ram, &regions, nullptr };
Memory *mem_manager = &_mem_manager;

L4_kernel_options::Uart kuart;
unsigned int kuart_flags;

/**
 * Calculate the maximum memory limit in MB.
 *
 * The limit is the highest physical address where conventional RAM is allowed.
 * The memory is limited to 3 GB IA32 and unlimited on other systems.
 */
static constexpr
unsigned long long
get_memory_max_address()
{
#if !defined(__LP64__) && defined(CONFIG_MMU)
  /* Limit memory, we cannot really handle more right now. In fact, the
   * problem is roottask. It maps as many superpages/pages as it gets.
   * After that, the remaining pages are mapped using l4sigma0_map_anypage()
   * with a receive window of L4_WHOLE_ADDRESS_SPACE. In response Sigma0
   * could deliver pages beyond the 3GB user space limit. */
  return 3024ULL << 20;
#endif

  return ~0ULL;
}

/**
 * Maximum virtual address accessible by bootstrap.
 *
 * \note The default sensible value mimics the behavior without this upper
 *       bound. If some platform-specific or architecture-specific code
 *       requires a stricter limit, it has to override the default value.
 */
l4_uint64_t mem_end = get_memory_max_address();

/*
 * IMAGE_MODE means that all boot modules are linked together to one
 * big binary.
 */
#ifdef IMAGE_MODE
static l4_addr_t _mod_addr = l4_addr_t{RAM_BASE} + MODADDR;
#else
static l4_addr_t _mod_addr;
#endif

static const char *builtin_cmdline = CMDLINE;


/// Info passed through our ELF interpreter code
struct Load_info : Elf_handle
{
  l4_addr_t offset;
};

struct Elf_info : Elf_handle
{
  Region::Type type;
  l4_addr_t offset;
};

struct Hdr_info : Elf_handle
{
  unsigned hdr_type;
  l4_addr_t start;
  l4_size_t size;
};

struct Section_info : Elf_handle
{
  l4_addr_t start = ~0UL;
  l4_addr_t end = 0;
  l4_addr_t align = 1;
  bool has_dynamic = false;
  bool needs_relocation = false;
};

static exec_handler_func_t l4_exec_read_exec;
static exec_handler_func_t l4_exec_add_region;
static exec_handler_func_t l4_exec_find_hdr;
static exec_handler_func_t l4_exec_gather_info;

// this function can be provided per architecture
void __attribute__((weak)) print_cpu_info();

#if 0
static void
dump_mbi(l4util_mb_info_t *mbi)
{
  printf("%p-%p\n", (void*)(mbi->mem_lower << 10), (void*)(mbi->mem_upper << 10));
  printf("MBI:     [%p-%p]\n", mbi, mbi + 1);
  printf("MODINFO: [%p-%p]\n", (char*)mbi->mods_addr,
      (l4util_mb_mod_t*)(mbi->mods_addr) + mbi->mods_count);

  printf("VBEINFO: [%p-%p]\n", (char*)mbi->vbe_ctrl_info,
      (l4util_mb_vbe_ctrl_t*)(mbi->vbe_ctrl_info) + 1);
  printf("VBEMODE: [%p-%p]\n", (char*)mbi->vbe_mode_info,
      (l4util_mb_vbe_mode_t*)(mbi->vbe_mode_info) + 1);

  l4util_mb_mod_t *m = (l4util_mb_mod_t*)(mbi->mods_addr);
  l4util_mb_mod_t *me = m + mbi->mods_count;
  for (; m < me; ++m)
    {
      printf("  MOD: [%p-%p]\n", (void*)m->mod_start, (void*)m->mod_end);
      printf("  CMD: [%p-%p]\n", (char*)m->cmdline,
	  (char*)m->cmdline + strlen((char*)m->cmdline));
    }
}
#endif


/**
 * Scan the memory regions with type == Region::Kernel for a
 * kernel interface page (KIP).
 *
 * After loading the kernel we scan for the magic number at page boundaries.
 */
static
l4_kernel_info_t *find_kip(Boot_modules::Module const &mod, l4_addr_t offset,
                           unsigned node)
{
  const char *error_msg;
  Hdr_info hdr;
  hdr.mod = mod;
  hdr.hdr_type = EXEC_SECTYPE_KIP;
  int r = exec_load_elf(l4_exec_find_hdr, &hdr, &error_msg, NULL);

  if (r == 1)
    {
      hdr.start += offset;
      auto *kip = reinterpret_cast<l4_kernel_info_t *>(hdr.start);
      // AMP kernels have multiple KIPs that are L4_PAGESIZE spaced. On some UP
      // or SMP kernels the KIP ELF region only covers the static part.
      while (hdr.size >= sizeof(*kip))
        {
          if (kip->node == node)
            {
              printf("  found node %u kernel info page (via ELF) at %p\n", node, kip);
              return kip;
            }
          kip = reinterpret_cast<l4_kernel_info_t *>(
                  reinterpret_cast<char *>(kip) + L4_PAGESIZE);
          hdr.size -= L4_PAGESIZE;
        }

      return nullptr;
    }

  for (Region const &m : regions)
    {
      if (m.type() != Region::Kernel)
        continue;

      l4_addr_t end;
      if (sizeof(unsigned long) < 8 && m.end() >= (1ULL << 32))
        end = ~0UL - 0x1000;
      else
        end = m.end();

      for (l4_addr_t p = l4_round_size(m.begin(), 12); p < end; p += 0x1000)
        {
          if ( *reinterpret_cast<l4_uint32_t *>(p) == L4_KERNEL_INFO_MAGIC)
            {
              printf("  found kernel info page at %lx\n", p);
              return reinterpret_cast<l4_kernel_info_t *>(p);
            }
        }
    }

  panic("could not find kernel info page, maybe your kernel is too old");
}

static
void copy_kip_feature_string(l4_kernel_info_t *dst, l4_kernel_info_t const *src)
{
  // find last last KIP feature string
  const char *src_version_str = l4_kip_version_string(src);
  const char *features_end = src_version_str;
  while (*features_end)
    features_end += strlen(features_end) + 1;

  // copy version and feature strings
  dst->offset_version_strings = src->offset_version_strings;
  memcpy(const_cast<char *>(l4_kip_version_string(dst)),
         src_version_str, features_end - src_version_str);
}

static
L4_kernel_options::Options *find_kopts(Boot_modules::Module const &mod,
                                       void *kip, l4_addr_t offset,
                                       unsigned node)
{
  L4_kernel_options::Options *ko = nullptr;
  const char *error_msg;
  Hdr_info hdr;
  hdr.mod = mod;
  hdr.hdr_type = EXEC_SECTYPE_KOPT;
  int r = exec_load_elf(l4_exec_find_hdr, &hdr, &error_msg, NULL);

  if (r == 1)
    {
      hdr.start += offset;
      ko = reinterpret_cast<L4_kernel_options::Options *>(hdr.start);
      while (hdr.size >= sizeof(*ko))
        {
          if (ko->node == node)
            {
              printf("  found node %u kernel options (via ELF) at %p\n", node, ko);
              break;
            }
          ko++;
          hdr.size -= sizeof(*ko);
        }

      if (!ko)
        panic("Cannot find node in kernel options page");
    }
  else
    {
      printf("  assuming kernel options directly following the KIP.\n");
      auto a = l4_round_page(reinterpret_cast<unsigned long>(kip)
                             + sizeof(l4_kernel_info_t));
      ko = reinterpret_cast<L4_kernel_options::Options *>(a);
    }

  if (ko->magic != L4_kernel_options::Magic)
    panic("Could not find kernel options page");

  if (ko->version != L4_kernel_options::Version_current)
    panic("Cannot boot kernel with incompatible options version: %lu, need %u",
          static_cast<unsigned long>(ko->version),
          L4_kernel_options::Version_current);

  return ko;
}

static char const *
check_arg_str(char const *cmdline, const char *arg)
{
  char const *s = cmdline;
  while ((s = strstr(s, arg)))
    {
      if (s == cmdline
          || isspace(s[-1]))
        return s;
    }
  return NULL;
}

/**
 * Scan the command line for the given argument.
 *
 * The cmdline string may either be including the calling program
 * (.../bootstrap -arg1 -arg2) or without (-arg1 -arg2) in the realmode
 * case, there, we do not have a leading space
 *
 * return pointer after argument, NULL if not found
 */
char const *
check_arg(char const *cmdline, const char *arg)
{
  if (cmdline)
    return check_arg_str(cmdline, arg);

  return NULL;
}

/*
 * If available the '-maxmem=xx' command line option is used.
 */
static
unsigned long long
get_memory_max_size(char const *cmdline)
{
  /* maxmem= parameter? */
  if (char const *c = check_arg(cmdline, "-maxmem="))
    return strtoul(c + 8, NULL, 10) << 20;

  return ~0ULL;
}

static int
parse_memvalue(const char *s, unsigned long *val, char **ep)
{

  *val = strtoul(s, ep, 0);
  if (*val == ~0UL)
    return 1;

  switch (**ep)
    {
    case 'G': *val <<= 10; /* FALLTHRU */
    case 'M': *val <<= 10; /* FALLTHRU */
    case 'k': case 'K': *val <<= 10; (*ep)++;
    };

  return 0;
}

/*
 * Parse a memory layout string: size@offset
 * E.g.: 256M@0x40000000, or 128M@128M
 */
static int
parse_mem_layout(const char *s, unsigned long *sz, unsigned long *offset)
{
  char *ep;

  if (parse_memvalue(s, sz, &ep))
    return 1;

  if (*sz == 0)
    return 1;

  if (*ep != '@')
    return 1;

  if (parse_memvalue(ep + 1, offset, &ep))
    return 1;

  return 0;
}

static void
dump_ram_map(bool show_total = false)
{
  // print RAM summary
  unsigned long long sum = 0;
  for (Region *i = ram.begin(); i < ram.end(); ++i)
    {
      printf("  RAM: %016llx - %016llx: %lldkB\n",
             i->begin(), i->end(), i->size() >> 10);
      sum += i->size();
    }
  if (show_total)
    printf("  Total RAM: %lldMB\n", sum >> 20);
}

static void
setup_memory_map(char const *cmdline)
{
  bool parsed_mem_option = false;
  const char *s = cmdline;

  if (s)
    {
      while ((s = check_arg_str(s, "-mem=")))
        {
          s += 5;
          unsigned long sz, offset = 0;
          if (!parse_mem_layout(s, &sz, &offset))
            {
              parsed_mem_option = true;
              ram.add(Region::start_size(offset, sz, ".ram", Region::Ram));
            }
        }
    }

  if (!parsed_mem_option)
    // No -mem option given, use the one given by the platform
    Platform_base::platform->setup_memory_map();

  dump_ram_map(true);
}

/**
 * Fill memory range with a value with a printout.
 *
 * \param begin  The starting address of the range to fill.
 * \param end    The inclusive ending address of the range to fill.
 * \param val    The value to fill the memory range with.
 */
static void verbose_memset(unsigned long begin, unsigned long end,
                           l4_uint8_t val)
{
  assert(begin <= end);

  printf("Presetting memory %16lx - %16lx to '0x%x'\n", begin, end, val);
  memset((void *)begin, val, end - begin + 1);
}

/**
 * Fill physical memory with a value.
 *
 * Every physical memory range (except those addresses that are occupied by
 * allocated regions and that are beyond \ref mem_end) is filled by the given
 * value.
 *
 * \param fill_value  The value to fill the memory ranges with.
 */
static void fill_mem(l4_uint8_t fill_value)
{
  for (Region const &ram_region : ram)
    {
      // <ram_region_begin, ram_region_end> is the working range.
      unsigned long long ram_region_begin = ram_region.begin();
      unsigned long long ram_region_end = ram_region.end();

      // The working range is completely outside of accessible memory.
      if (ram_region_begin > mem_end)
        continue;

      // The tail of the working range is outside of accessible memory.
      if (ram_region_end > mem_end)
        ram_region_end = mem_end;

      // Avoid allocated memory regions during the filling of the working
      // range. The algorithm assumes that the regions list is sorted.
      for (Region const &region : regions)
        {
          // The region lies completely in front of the working range.
          if (region.end() < ram_region_begin)
            continue;

          // The region lies completely behind the working range. We skip all
          // further regions due to the regions list being sorted.
          if (region.begin() > ram_region_end)
            break;

          // At this point, we know that there is and overlap between the
          // region and the working range.

          // There is a leading non-overlapping space before the region within
          // the working range. Fill the leading space.
          if (region.begin() > ram_region_begin)
            verbose_memset(ram_region_begin, region.begin() - 1, fill_value);

          // The further space that we examine starts after the region.
          ram_region_begin = region.end() + 1;
        }

      // Fill the tailing non-overlapping space after the last region. We need
      // to guard against the last overlapping region going beyond the working
      // range.
      if (ram_region_begin <= ram_region_end)
        verbose_memset(ram_region_begin, ram_region_end, fill_value);
    }
}

static Region bootstrap_region()
{
  extern int _start;	/* begin of image -- defined in crt0.S */
  extern int _end;	/* end   of image -- defined by bootstrap.ld */

  auto *p = Platform_base::platform;

  unsigned long long pstart
    = p->to_phys(reinterpret_cast<unsigned long>(&_start));
  unsigned long long pend
    = p->to_phys(reinterpret_cast<unsigned long>(&_end));
  return Region::start_size(pstart, pend - pstart, ".bootstrap", Region::Boot);
}

/**
 * Add the bootstrap binary itself to the allocated memory regions.
 */
static void
init_regions()
{ regions.add(bootstrap_region()); }

/**
 * Remove the bootstrap binary itself as it ceases to exist when jumping to the
 * kernel.
 */
static void
finalize_regions()
{ regions.sub(bootstrap_region()); }

/**
 * Add all sections of the given ELF binary to the allocated regions.
 * Actually does not load the ELF binary (see load_elf_module()).
 */
static void
add_elf_regions(Boot_modules::Module const &m, Region::Type type,
                l4_addr_t *offset, unsigned node, l4_addr_t min_align = 0)
{
  Section_info si;
  Elf_info info;
  int r;
  const char *error_msg;

  si.mod = m;
  si.needs_relocation = !m.attrs.find("reloc").empty();
  info.type = type;
  info.mod = m;

  printf("  Scanning %s\n", m.cmdline);

  r = exec_load_elf(l4_exec_gather_info, &si, &error_msg, NULL);
  if (r)
    panic("\nCould not gather load section infos (%s)", error_msg);

  // Just do relocation if it's required. Otherwise it might break working
  // setups by provoking collisions with other (unrelocatable) binaries.
  if (si.needs_relocation && si.has_dynamic && si.start < si.end)
    {
      if (si.align < min_align)
        si.align = min_align;

      /*
       * Normally the load sections are all properly aligned already. But if a
       * larger alignment is enforced either by min_align or there are PHDRs
       * with different alignments, we must round down the start address for
       * the free spot search accordingly. The search for the free spot is done
       * with the same alignment, so that the final offset is a multiple of the
       * required alignment.
       */
      si.start &= ~(si.align - 1U);
      unsigned align_shift = sizeof(unsigned long) * 8
                             - __builtin_clzl(si.align) - 1;
      l4_addr_t addr = _mem_manager.find_free_ram(si.end - si.start + 1U,
                                                  0, ~0UL, align_shift, node);
      if (!addr)
        panic("Not enough free memory to load binary");

      *offset = info.offset = addr - si.start;
    }
  else
    *offset = info.offset = 0;

  r = exec_load_elf(l4_exec_add_region, &info, &error_msg, NULL);

  if (r)
    {
      if (Verbose_load)
        {
          printf("\n%p: ", m.start);
          for (int i = 0; i < 4; ++i)
            printf("%08x ", *(reinterpret_cast<const unsigned *>(m.start) + i));
          printf("  ");
          for (int i = 0; i < 16; ++i)
            {
              char *c_ptr = const_cast<char *>(m.start + i);
              unsigned char c = *(reinterpret_cast<unsigned char *>(c_ptr));
              printf("%c", c < 32 ? '.' : c);
            }
        }
      panic("\n\nThis is an invalid binary, fix it (%s).", error_msg);
    }
}


/**
 * Load the given ELF binary into memory and free the source
 * memory region.
 */
static l4_addr_t
load_elf_module(Boot_modules::Module const &mod, l4_addr_t offset)
{
  l4_addr_t entry;
  int r;
  const char *error_msg;
  Load_info info;

  info.mod = mod;
  info.offset = offset;

  r = exec_load_elf(l4_exec_read_exec, &info, &error_msg, &entry);

  if (r)
    panic("Can't load module (%s)\n", error_msg);

  Region m = Region::start_size(mod.start, l4_round_page(mod.end) - mod.start);
  if (!regions.sub(m))
    {
      Region m = Region::start_size(mod.start, mod.end - mod.start);
      regions.sub(m);
    }

  return entry + offset;
}

#ifdef ARCH_mips
extern "C" void syncICache(unsigned long start, unsigned long size);
#endif

/*
 * Replace the placeholder string in the utest_opts feature with the config
 * string given on the command line.
 *
 * If the argument is found on the given command line and the KIP contains the
 * feature string of the kernel unit test framework, the argument is written
 * over the feature string's palceholder.
 *
 * \param cmdline  Kernel command line to search for argument.
 * \param info     Kernel info page.
 */
static void
search_and_setup_utest_feature(char const *cmdline, l4_kernel_info_t *info)
{
  char const *arg = "-utest_opts=";
  size_t arg_len = strlen(arg);
  char const *config = check_arg(cmdline, arg);

  if (!config)
    return;

  config += arg_len;

  char const *feat_prefix = "utest_opts=";
  size_t prefix_len = strlen(feat_prefix);
  char const *s = l4_kip_version_string(info);

  if (!s)
    return;

  l4_kip_for_each_feature(s)
    if (0 == strncmp(s, feat_prefix, prefix_len))
      {
        size_t max_len = strlen(s) - prefix_len;
        size_t opts_len = 0;
        for (char const *s = config; *s && !isspace(*s); ++s, ++opts_len)
          ;
        size_t cpy_len = opts_len < max_len ? opts_len : max_len;

        if (opts_len > max_len)
          printf("Warning: %s argument too long for feature placeholder. Truncated to fit.\n", arg);

        // We explicitly want to replace the placeholder string in this
        // feature, thus the const_cast. Don't copy the null terminator.
        strncpy(const_cast<char *>(s) + prefix_len, config, cpy_len);
      }
}

static unsigned long
load_elf_module(Boot_modules::Module const &mod, char const *n, l4_addr_t offset)
{
  printf("  Loading ");
  print_module_name(mod.cmdline, n);
  if (offset)
    {
      bool neg = offset >= ~(l4_addr_t)0U / 2U;
      printf(" (offset %c0x%lx)", neg?'-':'+', neg ? (~offset + 1U) : offset);
    }
  putchar('\n');
  return load_elf_module(mod, offset);
}

/**
 * \brief  Startup, started from crt0.S
 */
/* entry point */
void
startup(char const *cmdline)
{
  if (!cmdline || !*cmdline)
    cmdline = builtin_cmdline;

  if (check_arg(cmdline, "-noserial"))
    {
      set_stdio_uart(NULL);
      kuart_flags |= L4_kernel_options::F_noserial;
    }

  if (!Platform_base::platform)
    {
      // will we ever see this?
      printf("No platform found, hangup.");
      l4_infinite_loop();
    }

  Platform_base *plat = Platform_base::platform;

  if (check_arg(cmdline, "-wait"))
    {
      puts("\nL4 Bootstrapper is waiting for key input to continue...");
      if (getchar() == -1)
        puts("   ...no key input available.");
      else
        puts("    ...going on.");
    }

  puts("\nL4 Bootstrapper");
  puts("  Build: #" BUILD_NR " " BUILD_DATE
#ifdef ARCH_x86
      ", x86-32"
#endif
#ifdef ARCH_amd64
      ", x86-64"
#endif
#ifdef __VERSION__
       ", " __VERSION__
#endif
      );

  if (print_cpu_info)
    print_cpu_info();

  Internal_module_list internal_mods;

  plat->init_dt(internal_mods);

  regions.init(__regs, "regions");
  ram.init(__ram, "RAM", get_memory_max_size(cmdline), get_memory_max_address());

  setup_memory_map(cmdline);

  // No EFI services after this point. Must be done directly after we parsed
  // the memory map to make sure the EFI memory map does not change anymore.
  Platform_base::platform->exit_boot_services();

  /* basically add the bootstrap binary to the allocated regions */
  init_regions();
  plat->init_regions();
  plat->modules()->reserve();

  if (const char *s = check_arg(cmdline, "-modaddr"))
    {
      l4_addr_t addr = strtoul(s + 9, 0, 0);
      if (addr >= ULONG_MAX - RAM_BASE)
        panic("Bogus '-modaddr 0x%lx' parameter\n", addr);
      _mod_addr = RAM_BASE + addr;
    }

  _mod_addr = l4_round_page(_mod_addr);

  l4_uint8_t presetmem_value = 0;
  bool presetmem = false;

  if (char const *s = check_arg(cmdline, "-presetmem="))
    {
      presetmem_value = static_cast<l4_uint8_t>(strtoul(s + 11, NULL, 0));
      presetmem = true;
    }

  Boot_modules *mods = plat->modules();

  int idx_kern = mods->base_mod_idx(Mod_info_flag_mod_kernel);
  l4_addr_t fiasco_offset = 0;
  unsigned first_node = plat->first_node();
  unsigned num_nodes = plat->num_nodes();
  l4_addr_t sigma0_offset[num_nodes];
  l4_addr_t roottask_offset[num_nodes];

  if (idx_kern < 0)
    panic("No kernel module available");

  // The kernel must be loaded super-page aligned, even if the ELF file PHDRs
  // do not require it!
  add_elf_regions(mods->module(idx_kern), Region::Kernel, &fiasco_offset,
                  first_node, L4_SUPERPAGESIZE);

  for (unsigned i = 0; i < num_nodes; i++)
    {
      unsigned n = first_node + i;
      int idx_sigma0 = mods->base_mod_idx(Mod_info_flag_mod_sigma0, n);
      if (idx_sigma0 >= 0)
        add_elf_regions(mods->module(idx_sigma0), Region::Sigma0,
                        &sigma0_offset[i], n);

      int idx_roottask = mods->base_mod_idx(Mod_info_flag_mod_roottask, n);
      if (idx_roottask >= 0)
        add_elf_regions(mods->module(idx_roottask), Region::Root,
                        &roottask_offset[i], n);
    }

  l4util_l4mod_info *mbi = plat->modules()->construct_mbi(_mod_addr, internal_mods);
  cmdline = nullptr;

  assert(mbi->mods_count <= MODS_MAX);
  assert(plat->current_node() == first_node);

  boot_info_t boot_info;
  l4util_l4mod_mod *mb_mod
    = reinterpret_cast<l4util_l4mod_mod *>(
        static_cast<unsigned long>(mbi->mods_addr));
  regions.optimize();

  /* setup kernel PART ONE */
  boot_info.kernel_start = load_elf_module(mods->module(idx_kern), "[KERNEL]",
                                           fiasco_offset);

  l4_kernel_info_t *kip = find_kip(mods->module(idx_kern), fiasco_offset,
                                   first_node);
  if (!kip)
    panic("No KIP found!");

  /* setup kernel PART TWO (sigma0 and root task initialization) */
  for (unsigned i = 0; i < num_nodes; i++)
    {
      unsigned n = first_node + i;
      l4_kernel_info_t *l4i = i == 0 ? kip : find_kip(mods->module(idx_kern),
                                                      fiasco_offset, n);
      if (!l4i)
        continue;

      // Additional KIPs are missing the version and feature strings.
      if (l4i != kip)
        copy_kip_feature_string(l4i, kip);

      plat->setup_kernel_config(l4i);
      boot_info.sigma0_start = 0;
      boot_info.roottask_start = 0;

      /* setup sigma0 */
      int idx_sigma0 = mods->base_mod_idx(Mod_info_flag_mod_sigma0, n);
      if (idx_sigma0 >= 0)
        boot_info.sigma0_start = load_elf_module(mods->module(idx_sigma0),
                                                 "[SIGMA0]",
                                                 sigma0_offset[i]);
      else
        printf("  WARNING: No sigma0 module for node %d -- setup might not boot!\n", n);

      /* setup roottask */
      int idx_roottask = mods->base_mod_idx(Mod_info_flag_mod_roottask, n);
      if (idx_roottask >= 0)
        boot_info.roottask_start = load_elf_module(mods->module(idx_roottask),
                                                   "[ROOTTASK]",
                                                   roottask_offset[i]);
      else
        printf("  WARNING: No roottask module for node %d -- setup might not boot!\n", n);

      plat->late_setup(l4i);

      L4_kernel_options::Options *lko = find_kopts(mods->module(idx_kern), l4i,
                                                   fiasco_offset, n);

      kcmdline_parse(L4_CONST_CHAR_PTR(mb_mod[0].cmdline), lko);
      lko->uart   = kuart;
      lko->flags |= kuart_flags;

      search_and_setup_utest_feature(L4_CONST_CHAR_PTR(mb_mod[0].cmdline), l4i);

      // setup the L4 kernel info page before booting the L4 microkernel:
      init_kip(l4i, &boot_info, mbi, &ram, &regions);
      plat->setup_kernel_options(lko);
#if defined(ARCH_ppc32)
      init_kip_v2_arch((l4_kernel_info_t*)l4i);

      printf("CPU at %lu Khz/Bus at %lu Hz\n",
             ((l4_kernel_info_t*)l4i)->frequency_cpu,
             ((l4_kernel_info_t*)l4i)->frequency_bus);
#endif
    }

  // Note: we have to ensure that the original ELF binaries are not modified
  // or overwritten up to this point. However, the memory regions for the
  // original ELF binaries are freed during load_elf_module() but might be
  // used up to here.
  // ------------------------------------------------------------------------

  // The ELF binaries for the kernel, sigma0, and roottask must no
  // longer be used from here on.
  if (presetmem)
    fill_mem(presetmem_value);

  finalize_regions();
  regions.optimize();
  regions.dump();

  printf("  Starting kernel ");
  print_module_name(L4_CONST_CHAR_PTR(mb_mod[0].cmdline),
		    "[KERNEL]");
  printf(" at " l4_addr_fmt "\n", boot_info.kernel_start);

#if defined(ARCH_mips)
  {
    printf("  Flushing caches ...\n");
    for (Region *i = ram.begin(); i < ram.end(); ++i)
      {
        if (i->end() >= (512 << 20))
          continue;

        printf("  [%08lx, %08lx)\n", (unsigned long)i->begin(), (unsigned long)i->size());
        syncICache((unsigned long)i->begin(), (unsigned long)i->size());
      }
    printf("  done\n");
  }
#endif

  plat->boot_kernel(boot_info.kernel_start);
  /*NORETURN*/
}

static int
l4_exec_read_exec(Elf_handle *handle,
		  l4_addr_t file_ofs, l4_size_t file_size,
		  l4_addr_t mem_addr, l4_addr_t /*v_addr*/,
		  l4_size_t mem_size, l4_size_t /*align*/,
		  exec_sectype_t section_type)
{
  Load_info const *info = static_cast<Load_info const *>(handle);
  Boot_modules::Module const &m = info->mod;
  if (!mem_size)
    return 0;

  if (! (section_type & EXEC_SECTYPE_ALLOC))
    return 0;

  if (! (section_type & (EXEC_SECTYPE_ALLOC|EXEC_SECTYPE_LOAD)))
    return 0;

  mem_addr += info->offset;

  if (Verbose_load)
    printf("    [%p-%p]\n", (void *) mem_addr, (void *) (mem_addr + mem_size));

  if (!ram.contains(Region::start_size(mem_addr, mem_size)))
    {
      printf("To be loaded binary region is out of memory region.\n");
      printf(" Binary region: %lx - %lx\n", mem_addr, mem_addr + mem_size);
      dump_ram_map();
      panic("Binary outside memory");
    }

  auto *src = m.start + file_ofs;
  auto *dst = reinterpret_cast<char *>(mem_addr);
  if (reinterpret_cast<unsigned long>(src) % 8
      || reinterpret_cast<unsigned long>(dst) % 8)
    memcpy(dst, src, file_size);
  else
    memcpy_aligned(dst, src, file_size);

  if (file_size < mem_size)
    memset(dst + file_size, 0, mem_size - file_size);

  Region *f = regions.find(mem_addr);
  if (!f)
    {
      printf("could not find %lx\n", mem_addr);
      regions.dump();
      panic("Oops: region for module not found\n");
    }

  f->name(m.cmdline ? m.cmdline :  ".[Unknown]");

  return 0;
}

static Region *
find_region_overlap(Region const &n)
{
  for (Region *r = regions.begin(); r != regions.end(); ++r)
    if (r->overlaps(n) && r->name() != Mod_info::Mod_reg
        && !(r->type() == Region::Boot && r->sub_type() == Region::Boot_temporary))
      return r;

  return nullptr;
}

static int
l4_exec_add_region(Elf_handle *handle,
		  l4_addr_t /*file_ofs*/, l4_size_t /*file_size*/,
		  l4_addr_t mem_addr, l4_addr_t /*v_addr*/,
		  l4_size_t mem_size, l4_size_t /*align*/,
		  exec_sectype_t section_type)
{
  Elf_info const &info = static_cast<Elf_info const&>(*handle);

  if (!mem_size)
    return 0;

  if (! (section_type & EXEC_SECTYPE_ALLOC))
    return 0;

  if (! (section_type & (EXEC_SECTYPE_ALLOC|EXEC_SECTYPE_LOAD)))
    return 0;

#if defined(CONFIG_BOOTSTRAP_ROOTTASK_NX)
  unsigned short rights = L4_FPAGE_RO;
  if (section_type & EXEC_SECTYPE_WRITE)
    rights |= L4_FPAGE_W;
  if (section_type & EXEC_SECTYPE_EXECUTE)
    rights |= L4_FPAGE_X;
#else
  unsigned short rights = L4_FPAGE_RWX;
#endif

  // The subtype is used only for Root regions. For other types set subtype to 0
  // in order to allow merging regions with the same subtype.
  Region n = Region::start_size(mem_addr + info.offset, mem_size,
                                info.mod.cmdline ? info.mod.cmdline : ".[Unknown]",
                                info.type, info.type == Region::Root ? rights : 0,
                                info.type != Region::Kernel);

  if (Region *r = find_region_overlap(n))
    {
      printf("  New region for list %s:\t", n.name());
      n.vprint();
      printf("  overlaps with:         \t");
      r->vprint();
      regions.dump();
      panic("region overlap");
    }

  regions.add(n, true);
  return 0;
}

static int
l4_exec_find_hdr(Elf_handle *handle,
                 l4_addr_t /*file_ofs*/, l4_size_t /*file_size*/,
                 l4_addr_t mem_addr, l4_addr_t /*v_addr*/,
                 l4_size_t mem_size, l4_size_t /*align*/,
                 exec_sectype_t section_type)
{
  Hdr_info &hdr = static_cast<Hdr_info&>(*handle);
  if (hdr.hdr_type == (section_type & EXEC_SECTYPE_TYPE_MASK))
    {
      hdr.start = mem_addr;
      hdr.size = mem_size;
      return 1;
    }
  return 0;
}

static int
l4_exec_gather_info(Elf_handle *handle,
                    l4_addr_t /*file_ofs*/, l4_size_t /*file_size*/,
                    l4_addr_t mem_addr, l4_addr_t /*v_addr*/,
                    l4_size_t mem_size, l4_size_t align,
                    exec_sectype_t section_type)
{
  Section_info *info = static_cast<Section_info *>(handle);

  if (!mem_size)
    return 0;

  if ((section_type & EXEC_SECTYPE_TYPE_MASK) == EXEC_SECTYPE_DYNAMIC)
    info->has_dynamic = true;

  if (! (section_type & EXEC_SECTYPE_ALLOC))
    return 0;

  if (mem_addr < info->start)
    info->start = mem_addr;

  if ((mem_addr + mem_size - 1U) > info->end)
    info->end = mem_addr + mem_size - 1U;

  if (align > info->align)
    info->align = align;

  auto r = Region::start_size(mem_addr, mem_size);
  if (!ram.contains(r) || find_region_overlap(r))
    info->needs_relocation = true;

  return 0;
}
