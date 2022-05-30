/*!
 * \file   support_x86.cc
 * \brief  Support for the x86 platform
 *
 * \date   2008-01-02
 * \author Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *
 */
/*
 * (c) 2008-2009 Author(s)
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <l4/sys/kip>
#include <l4/util/mb_info.h>
#include "support.h"
#include "platform.h"
#include "boot_modules.h"
#include "x86_pc-base.h"

#include <string.h>
#include "startup.h"
#include "panic.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef ARCH_amd64
#include "cpu_info.h"
#include "paging.h"
#endif

static char rsdp_tmp_buf[36];
l4_uint32_t rsdp_start;
l4_uint32_t rsdp_end;

enum { Verbose_mbi = 1 };

static void pci_quirks();

extern char ebda_segment[];

namespace {

struct Platform_x86_1 : Platform_x86
{
  l4util_mb_info_t *mbi;
  l4util_l4mod_info *l4mi;

  void setup_memory_map() override
  {
    Region_list *ram = mem_manager->ram;
    Region_list *regions = mem_manager->regions;

#ifdef ARCH_amd64
    // add the page-table on which we're running in 64bit mode
    regions->add(Region::start_size(boot32_info->ptab64_addr,
                                    boot32_info->ptab64_size,
                                    ".bootstrap-ptab64", Region::Boot));
    // also ensure boot32_info is reserved
    regions->add(Region::start_size(boot32_info,
                                    sizeof(struct boot32_info_t),
                                    ".boot32_info", Region::Boot));
#endif

#ifdef ARCH_amd64
    rsdp_start = boot32_info->rsdp_start;
    rsdp_end = boot32_info->rsdp_end;
    mem_end = boot32_info->mem_end;
#endif

   // if we were passed the RSDP structure, save it aside
   if ((rsdp_end - rsdp_start) <= sizeof(rsdp_tmp_buf))
     memcpy(rsdp_tmp_buf, (void *)(l4_addr_t)rsdp_start, rsdp_end - rsdp_start);
   else
     {
       rsdp_start = 0;
       rsdp_end = 0;
     }

   if (!(mbi->flags & L4UTIL_MB_MEM_MAP))
      {
        assert(mbi->flags & L4UTIL_MB_MEMORY);
        ram->add(Region::start_size(0ULL, mbi->mem_lower << 10, ".ram",
                                    Region::Ram));
        ram->add(Region::start_size(1ULL << 20, mbi->mem_upper << 10, ".ram",
                                    Region::Ram));

        // Fix EBDA in conventional memory
        unsigned long p = *(l4_uint16_t const *)ebda_segment << 4;

        if (p > 0x400)
          {
            unsigned long e = p + 1024;
            Region *r = ram->find(Region(p, e - 1));
            if (r)
              {
                if (e - 1 < r->end())
                  ram->add(Region(e, r->end(), ".ram", Region::Ram), true);
                r->end(p);
              }
          }
      }
    else
      {
        l4util_mb_addr_range_t *mmap;
        l4util_mb_for_each_mmap_entry(mmap, mbi)
          {
            switch (mmap->type)
              {
              case MB_ART_MEMORY:
                ram->add(Region::start_size(mmap->addr, mmap->size, ".ram",
                                            Region::Ram));
                break;
              case MB_ART_RESERVED:
              case MB_ART_ACPI:
              case MB_ART_NVS:
                static_assert(MB_ART_ACPI == Region::Arch_acpi,
                              "Multiboot ACPI tables memory type matches");
                static_assert(MB_ART_NVS == Region::Arch_nvs,
                              "Multiboot ACPI NVS memory type matches");
                regions->add(Region::start_size(mmap->addr, mmap->size, ".BIOS",
                                                Region::Arch, mmap->type));
                break;
              case MB_ART_UNUSABLE:
                regions->add(Region::start_size(mmap->addr, mmap->size, ".BIOS",
                                                Region::No_mem));
                break;
              case 20:
                regions->add(Region::start_size(mmap->addr, mmap->size, ".BIOS",
                                                Region::Arch, mmap->type));
                break;
              default:
                break;
              }
          }
      }

    regions->add(Region::start_size(0ULL, 0x1000, ".BIOS", Region::Arch, 0));
  }

  void late_setup(l4_kernel_info_t *) override
  {
    pci_quirks();
  }
};

#ifdef IMAGE_MODE

class Platform_x86_loader_mbi :
  public Platform_x86_1,
  public Boot_modules_image_mode
{
public:
  Boot_modules *modules() { return this; }

};

Platform_x86_loader_mbi _x86_pc_platform;

#else // IMAGE_MODE

class Platform_x86_multiboot : public Platform_x86_1, public Boot_modules
{
public:
  Boot_modules *modules() override { return this; }
  int base_mod_idx(Mod_info_flags mod_info_mod_type, unsigned) override
  {
    switch (mod_info_mod_type)
      {
      case Mod_info_flag_mod_kernel:
      case Mod_info_flag_mod_sigma0:
      case Mod_info_flag_mod_roottask:
        if (mod_info_mod_type - 1 < (int)num_modules())
          return mod_info_mod_type - 1;
        // fall through
      default:
        return -1;
      }
  }

  Module module(unsigned index, bool) const override
  {
    Module m;

    if (l4mi)
      {
        l4util_l4mod_mod *l4m = (l4util_l4mod_mod *)(unsigned long)l4mi->mods_addr;

        m.start   = (char const *)(l4_addr_t)l4m[index].mod_start;
        m.end     = (char const *)(l4_addr_t)l4m[index].mod_end;
        m.cmdline = (char const *)(l4_addr_t)l4m[index].cmdline;
      }
    else if (mbi)
      {
        l4util_mb_mod_t *mb_mod = (l4util_mb_mod_t*)(unsigned long)mbi->mods_addr;

        m.start   = (char const *)(l4_addr_t)mb_mod[index].mod_start;
        m.end     = (char const *)(l4_addr_t)mb_mod[index].mod_end;
        m.cmdline = (char const *)(l4_addr_t)mb_mod[index].cmdline;
      }
    else
      assert(0);
    return m;
  }

  unsigned num_modules() const override
  { return l4mi ? l4mi->mods_count : mbi->mods_count; }

  void reserve() override
  {
    Region_list *regions = mem_manager->regions;

    regions->add(Region::start_size(mbi, sizeof(*mbi), ".mbi",
                                    Region::Boot, Region::Boot_temporary));

    if (mbi->flags & L4UTIL_MB_CMDLINE)
      regions->add(Region::start_size(mbi->cmdline,
                                      strlen((char const *)
                                             (l4_addr_t)mbi->cmdline) + 1, ".mbi",
                                      Region::Boot, Region::Boot_temporary));

    l4util_mb_mod_t *mb_mod = (l4util_mb_mod_t*)(unsigned long)mbi->mods_addr;
    l4_size_t const size = (unsigned long)&mb_mod[mbi->mods_count]
                           - (unsigned long)mb_mod;
    regions->add(Region::start_size(mb_mod, size, ".mbi", Region::Boot,
                                    Region::Boot_temporary));

    if (mbi->flags & L4UTIL_MB_VIDEO_INFO)
      {
        if (mbi->vbe_mode_info)
          regions->add(Region::start_size(mbi->vbe_mode_info,
                                          sizeof(l4util_mb_vbe_mode_t), ".mbi",
                                          Region::Boot, Region::Boot_temporary));
        if (mbi->vbe_ctrl_info)
          regions->add(Region::start_size(mbi->vbe_ctrl_info,
                                          sizeof(l4util_mb_vbe_ctrl_t), ".mbi",
                                          Region::Boot, Region::Boot_temporary));
      }


    for (unsigned i = 0; i < mbi->mods_count; ++i)
      regions->add(Region::start_size(mb_mod[i].cmdline,
                                      strlen((char const *)
                                             (l4_addr_t)mb_mod[i].cmdline) + 1,
                                      ".mbi", Region::Boot, Region::Boot_temporary));

    for (unsigned i = 0; i < mbi->mods_count; ++i)
      {
        /*
         * Avoid overflow on size calculation of empty modules,
         * i.e. mod_start == mod_end. Grub generates MBI entries
         * with start == end == 0 for empty files loaded as modules.
         */
        if (mb_mod[i].mod_start >= mb_mod[i].mod_end)
          panic("Found a module with implausible size %d (%s). Aborting.\n",
                mb_mod[i].mod_end - mb_mod[i].mod_start,
                (char const *)(l4_addr_t)mb_mod[i].cmdline);
        regions->add(mod_region(i, mb_mod[i].mod_start,
                                mb_mod[i].mod_end - mb_mod[i].mod_start));
      }
  }

  void move_module(unsigned index, void *dest) override
  {
    l4util_l4mod_mod *mod = (l4util_l4mod_mod *)(unsigned long)l4mi->mods_addr + index;
    unsigned long size = mod->mod_end - mod->mod_start;
    _move_module(index, dest, (char const *)(l4_addr_t)mod->mod_start, size);

    assert ((l4_addr_t)dest < 0xfffffff0);
    assert ((l4_addr_t)dest < 0xfffffff0 - size);
    mod->mod_start = (l4_addr_t)dest;
    mod->mod_end   = (l4_addr_t)dest + size;
  }

  l4util_l4mod_info *construct_mbi(unsigned long mod_addr,
                                   Internal_module_list const &) override
  {
    // calculate the size needed to cover the full MBI, including command lines
    unsigned long total_size = sizeof(l4util_l4mod_info);

    // consider the global command line
    if (mbi->flags & L4UTIL_MB_CMDLINE)
      total_size += round_wordsize(strlen((char const *)(l4_addr_t)mbi->cmdline) + 1);

    // consider VBE info
    if (mbi->flags & L4UTIL_MB_VIDEO_INFO)
      {
        if (mbi->vbe_mode_info)
          total_size += sizeof(l4util_mb_vbe_mode_t);

        if (mbi->vbe_ctrl_info)
          total_size += sizeof(l4util_mb_vbe_ctrl_t);
      }

    // consider modules
    total_size += sizeof(l4util_l4mod_mod) * mbi->mods_count;

    // scan through all modules and add the command line
    l4util_mb_mod_t *mods = (l4util_mb_mod_t*)(unsigned long)mbi->mods_addr;
    for (l4util_mb_mod_t *m = mods; m != mods + mbi->mods_count; ++m)
      if (m->cmdline)
        total_size += round_wordsize(strlen((char const *)(l4_addr_t)m->cmdline) + 1);

    if (Verbose_mbi)
      printf("  need %ld bytes to copy MBI\n", total_size);

    // try to find a free region for the MBI
    char *_mb = (char *)mem_manager->find_free_ram(total_size);
    if (!_mb)
      panic("fatal: could not allocate memory for multi-boot info\n");

    // mark the region as reserved
    mem_manager->regions->add(Region::start_size(_mb, total_size, ".mbi_rt",
                                                 Region::Root, L4_FPAGE_RWX));
    if (Verbose_mbi)
      printf("  reserved %ld bytes at %p\n", total_size, _mb);

    // copy over from MBI to l4mods
    l4mi = (l4util_l4mod_info *)_mb;
    memset(l4mi, 0, total_size);

    l4mi->mods_count = mbi->mods_count;

    l4util_l4mod_mod *l4m_mods = (l4util_l4mod_mod *)(l4mi + 1);
    l4mi->mods_addr = (l4_addr_t)l4m_mods;
    _mb = (char *)(l4m_mods + l4mi->mods_count);

    if (mbi->flags & L4UTIL_MB_VIDEO_INFO)
      {
        if (mbi->vbe_mode_info)
          {
            l4util_mb_vbe_mode_t *d = (l4util_mb_vbe_mode_t *)_mb;
            *d = *((l4util_mb_vbe_mode_t *)(l4_addr_t)mbi->vbe_mode_info);
            l4mi->vbe_mode_info = (l4_addr_t)d;
            _mb = (char *)(d + 1);
          }

        if (mbi->vbe_ctrl_info)
          {
            l4util_mb_vbe_ctrl_t *d = (l4util_mb_vbe_ctrl_t *)_mb;
            *d = *((l4util_mb_vbe_ctrl_t *)(l4_addr_t)mbi->vbe_ctrl_info);
            l4mi->vbe_ctrl_info = (l4_addr_t)d;
            _mb = (char *)(d + 1);
          }
      }

    if (mbi->cmdline)
      {
        strcpy(_mb, (char const *)(l4_addr_t)mbi->cmdline);
        l4mi->cmdline = (l4_addr_t)_mb;
        _mb += round_wordsize(strlen(_mb) + 1);
      }

    for (unsigned i = 0; i < l4mi->mods_count; ++i)
      {
        switch (i)
          {
          case 0:
            l4m_mods[i].flags = Mod_info_flag_mod_kernel;
            break;
          case 1:
            l4m_mods[i].flags = Mod_info_flag_mod_sigma0;
            break;
          case 2:
            l4m_mods[i].flags = Mod_info_flag_mod_roottask;
            break;
          default:
            l4m_mods[i].flags = 0;
            break;
          };
        l4m_mods[i].mod_start = mods[i].mod_start;
        l4m_mods[i].mod_end   = mods[i].mod_end;
        if (char const *c = (char const *)(l4_addr_t)(mods[i].cmdline))
          {
            unsigned l = strlen(c) + 1;
            l4m_mods[i].cmdline = (l4_addr_t)_mb;
            memcpy(_mb, c, l);
            _mb += round_wordsize(l);
          }
      }

    assert(_mb - (char *)l4mi == (long)total_size);

    mbi = 0;

    // remove the old MBI from the reserved memory
    for (Region *r = mem_manager->regions->begin();
         r != mem_manager->regions->end();)
      {
        if (strcmp(r->name(), ".mbi"))
          ++r;
        else
          r = mem_manager->regions->remove(r);
      }

    move_modules(mod_addr);

    // Our aim here is to allocate an aligned chunk of memory for our copy of
    // RSDP and create a region out of it.
    //
    // XXX: For now, we are piggybacking on this callback because there is
    // currently no better one that is called this late.
    if (rsdp_start)
      {
        char *rsdp_buf =
          (char *)mem_manager->find_free_ram(sizeof(rsdp_tmp_buf));
        if (!rsdp_buf)
          panic("fatal: could not allocate memory for RSDP\n");
        memcpy(rsdp_buf, rsdp_tmp_buf, sizeof(rsdp_tmp_buf));

        mem_manager->regions->add(
          Region::start_size(rsdp_buf, sizeof(rsdp_tmp_buf),
                             ".ACPI", Region::Info, Region::Info_acpi_rsdp));
      }

    return l4mi;
  }
};

Platform_x86_multiboot _x86_pc_platform;

#endif // !IMAGE_MODE
}

namespace /* usb_xhci_handoff */ {

static unsigned
xhci_first_cap(L4::Io_register_block const *base)
{
  return ((base->read<l4_uint32_t>(0x10) >> 16) & 0xffff) << 2;
}

static unsigned
xhci_next_cap(L4::Io_register_block const *base, unsigned cap)
{
  l4_uint32_t next = (base->read<l4_uint32_t>(cap) >> 8) & 0xff;

  if (!next)
    return 0;

  return cap + (next << 2);
}

static void
udelay(int usecs)
{
  // Assume that we have a 2GHz machine and it needs approximately
  // two cycles per inner loop, this delay function should wait the
  // given amount of micro seconds.
  for (; usecs >= 0; --usecs)
    for (unsigned long i = 1000; i > 0; --i)
      asm volatile ("nop; pause" : : : "memory");
}

static void
do_handoff_cap(L4::Io_register_block const *base, unsigned cap)
{
  enum : l4_uint32_t
  {
    HC_BIOS_OWNED_SEM = 1U << 16,
    HC_OS_OWNED_SEM   = 1U << 24,
    USBLEGCTLSTS      = 0x04,
    DISABLE_SMI       = 0xfff1e011U,
    SMI_EVENTS        = 0x7U << 29,
  };

  l4_uint32_t val = base->read<l4_uint32_t>(cap);

  if (val & HC_BIOS_OWNED_SEM) // BIOS has the HC
    {
      base->write(cap, val | HC_OS_OWNED_SEM); // We want it

      int to = 300; // wait some seconds for BIOS to release
      while (--to > 0 && (base->read<l4_uint32_t>(cap) & HC_BIOS_OWNED_SEM))
        udelay(10);

      if (to == 0)
        {
          printf("Forcing ownership of XHCI controller from BIOS\n");
          base->write(cap, val & ~HC_BIOS_OWNED_SEM);
        }
    }

  // Disable any BIOS SMIs and clear all SMI events
  base->modify<l4_uint32_t>(cap + USBLEGCTLSTS, DISABLE_SMI, SMI_EVENTS);
}

static bool
xhci_probe_bar(Pci_iterator const &dev, l4_uint64_t *addr, l4_uint64_t *size)
{
  enum : l4_uint32_t
  {
    PCI_BAR_MEM_TYPE_MASK = 3 << 1,
    PCI_BAR_MEM_TYPE_64   = 2 << 1,
    PCI_BAR_MEM_ATTR_MASK = 0xf,
  };

  // read BAR[0]
  l4_uint32_t bar = dev.pci_read(0x10, 32);

  // invalid -> skip
  if (!bar)
    return false;

  // BAR[0] is an IO BAR, not XHCI compliant -> skip
  if (bar & 1)
    return false;

  *addr = bar & ~PCI_BAR_MEM_ATTR_MASK;
  *size = dev.read_bar_size(0x10, bar);
  if (*size == 0xffffffffU)
    // Invalid size, device not working correctly, at least one bit in BAR
    // attributes is always zero.
    return false;

  // 64-bit BAR?
  if ((bar & PCI_BAR_MEM_TYPE_MASK) == PCI_BAR_MEM_TYPE_64)
    {
      // read BAR[1] and set it as the upper 32-bits of the 64-bit address
      bar = dev.pci_read(0x14, 32);
      *addr |= ((l4_uint64_t)bar) << 32;
      *size |= ((l4_uint64_t)dev.read_bar_size(0x14, bar)) << 32;
    }

  // Clear attribute part
  *size &= ~((l4_uint64_t)PCI_BAR_MEM_ATTR_MASK);
  // Decode BAR size
  *size = ~(*size) + 1;
  return true;
}

static void
xhci_handoff(Pci_iterator const &dev)
{
  enum : l4_uint32_t
  {
    HC_CAP_ID_MASK               = 0xff,
    HC_CAP_ID_USB_LEGACY_SUPPORT = 1,
  };

  using Pci = Pci_iterator;

  unsigned cmd = dev.pci_read(Pci::Cmd, 32);

  // no mmio enabled -> skip
  if (!(cmd & Pci::Memory_space))
    return;

  if (0)
    // no bus-master -> skip
    if (!(cmd & Pci::Bus_master))
      return;

  // Temporarily disable memory space access, because to determine the BAR's
  // size requires writing to it.
  dev.pci_write(Pci::Cmd, cmd & ~Pci::Memory_space, 32);

  l4_uint64_t addr, size;
  bool bar_valid = xhci_probe_bar(dev, &addr, &size);

  // Restore memory space access.
  dev.pci_write(Pci::Cmd, cmd, 32);

  // invalid bar -> skip
  if (!bar_valid)
    return;

#ifdef ARCH_amd64
  // Ensure that the BAR is mapped into our linear address space.
  ptab_map_range(_x86_pc_platform.boot32_info->ptab64_addr, addr, addr, size,
                 PTAB_WRITE | PTAB_USER);
#else
  // Skip if we cannot access the BAR. The cast to "unsigned long" is required
  // as Io_register_block_mmio takes an "unsigned long".
  if ((addr + size) != (unsigned long)(addr + size))
    return;
#endif

  L4::Io_register_block_mmio base(addr);

  // Find the Legacy Support Capability register
  for (unsigned cap = xhci_first_cap(&base); cap != 0;
       cap = xhci_next_cap(&base, cap))
    {
      if (cap >= size)
        {
          printf("xHCI handoff failed: Extended capability outside of memory space.\n");
          break;
        }

      l4_uint32_t v = base.read<l4_uint32_t>(cap);
      if ((v & HC_CAP_ID_MASK) == HC_CAP_ID_USB_LEGACY_SUPPORT)
        {
          do_handoff_cap(&base, cap);
          break;
        }
    }
}

}

enum
{
  PCI_CLASS_SERIAL_USB_XHCI = 0x0c0330,
};

static void
pci_quirks()
{
  for (Pci_iterator i; i != Pci_iterator::end(); ++i)
    {
      unsigned cc = i.pci_class();
      if (cc == PCI_CLASS_SERIAL_USB_XHCI)
        xhci_handoff(i);
    }
}

extern "C"
void __main(l4util_mb_info_t *mbi, unsigned long p2, char const *realmode_si,
            boot32_info_t const *boot32_info);

void __main(l4util_mb_info_t *mbi, unsigned long p2, char const *realmode_si,
            boot32_info_t const *boot32_info)
{
  ctor_init();
  Platform_base::platform = &_x86_pc_platform;
  _x86_pc_platform.init();
  init_modules_infos();
#ifdef ARCH_amd64
  // remember this info to reserve the memory in setup_memory_map later
  _x86_pc_platform.boot32_info = boot32_info;
  init_cpu_info();
#else
  (void)boot32_info;
#endif
  char const *cmdline;
  (void)realmode_si;
  assert(p2 == L4UTIL_MB_VALID); /* we need to be multiboot-booted */
  _x86_pc_platform.mbi = mbi;
  cmdline = (char const *)(l4_addr_t)mbi->cmdline;
#if defined (IMAGE_MODE)
  if (!cmdline)
    cmdline = mod_header->mbi_cmdline();
#endif
  static Uart_vga vga_uart;
  _x86_pc_platform.setup_uart(cmdline, &vga_uart);
  _x86_pc_platform.disable_pci_bus_master();

  startup(cmdline);
}


