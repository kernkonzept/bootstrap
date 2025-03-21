/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2009-2021, 2023 Kernkonzept GmbH.
 * Authors: Alexander Warg <warg@os.inf.tu-dresden.de>
 *          Frank Mehnert <fm3@os.inf.tu-dresden.de>
 *          Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <l4/util/elf.h>

#include "load_elf.h"
#include "mm_alloc.h"
#include "support.h"

// Even relocate if the memory region the binary is linked to is free.
static int const always_relocate = 0;

/**
 * Function to relocate a value based on an ELF relocation entry.
 *
 * \param rel     A RELA relocation entry structure from the ELF .rela section
 * \param offset  The signed offset of by how much to relocate the target
 */
static void relocate_rela_entry(Elf64_Rela* rel, Elf64_Sxword offset)
{
  Elf32_Addr reloc_addr = (Elf32_Addr)(rel->r_offset + offset);
  switch (ELF64_R_TYPE(rel->r_info))
    {
      case R_X86_64_RELATIVE:
        *(Elf64_Addr *)reloc_addr = rel->r_addend + offset;
        break;
      // Should not appear with --no-dynamic-linker and -Bsymbolic
      case R_X86_64_GLOB_DAT:
      default:
        printf("Info: type=0x%llx @ %llx -- ", rel->r_info, rel->r_offset);
        panic("not supported");
    }
}

/**
 * Check if the specified range fits into the address range 0..4GiB.
 *
 * \param start  Start of the range.
 * \param size   Size of the range.
 *
 * \retval 1 if whole range is within 4GB and not overlapping a reservation.
 * \retval 0 otherwise.
 */
static int usable_range(Elf64_Addr start, Elf64_Xword size)
{
  return is_range_in_4g(start, size)
         && !overlaps_reservation((void *)(Elf32_Addr)start, size);
}

/**
 * Perform some light consistency checks on an ELF binary.
 */
static void
sanity_elf(Elf64_Ehdr const *eh)
{
  if (   eh->e_ident[EI_MAG0] != ELFMAG0
      || eh->e_ident[EI_MAG1] != ELFMAG1
      || eh->e_ident[EI_MAG2] != ELFMAG2
      || eh->e_ident[EI_MAG3] != ELFMAG3)
    panic("Invalid ELF file: bad header magic");

  if (eh->e_ident[EI_CLASS] != ELFCLASS64)
    panic("Invalid ELF file: wrong class");

  if (eh->e_machine != EM_X86_64)
    panic("Invalid ELF file: wrong machine");
}

/**
 * Reserve all loadable program sections of an ELF binary.
 *
 * \param elf      Start of the ELF binary.
 */
void
reserve_elf(void const *elf)
{
  char const *_elf = (char const *)elf;
  Elf64_Ehdr const *eh = (Elf64_Ehdr const *)(_elf);
  sanity_elf(eh);
  Elf64_Phdr const *ph = (Elf64_Phdr const *)(_elf + eh->e_phoff);

  for (unsigned i = 0; i < eh->e_phnum; ++i)
    if (ph[i].p_type == PT_LOAD)
      reservation_add(ph[i].p_paddr, ph[i].p_memsz);
}

/**
 * Load an ELF binary into memory.
 */
l4_uint32_t
load_elf(void const *elf)
{
  char const *_elf = (char const *)elf;
  Elf64_Ehdr const *eh = (Elf64_Ehdr const *)(_elf);
  sanity_elf(eh);
  Elf64_Shdr const *sh = (Elf64_Shdr const *)(_elf + eh->e_shoff);
  Elf64_Phdr const *ph = (Elf64_Phdr const *)(_elf + eh->e_phoff);

  Elf64_Sxword reloc = 0; // per default we do not relocate

  int need_reloc = 0;

  // Find out where to allocate memory by going through all phdrs and finding
  // total required memory size and maximum required alignment.
  Elf64_Addr min_addr = ~0ULL;
  Elf64_Addr max_addr = 0ULL; // the address *after* the last byte
  Elf64_Word max_align = 0ULL;

  for (unsigned i = 0; i < eh->e_phnum; ++i)
    {
      if (ph[i].p_type != PT_LOAD)
        continue;

      if (max_align < ph[i].p_align && ph[i].p_align > 1)
        max_align = ph[i].p_align;

      if (min_addr > ph[i].p_paddr)
        min_addr = ph[i].p_paddr;

      if (max_addr < ph[i].p_paddr + ph[i].p_memsz)
        max_addr = ph[i].p_paddr + ph[i].p_memsz;

      need_reloc |= !usable_range(ph[i].p_paddr, ph[i].p_memsz);
    }

  // Only relocate when memory is not free anyways
  if (always_relocate || need_reloc)
    {
      if (!mm_alloc_alloc)
        panic("Memory already used and relocation not enabled!");

      // Round down to max alignment to ensure all sections are correctly aligned
      min_addr &= ~( max_align - 1 );

      void* alloc_base = mm_alloc_alloc(max_addr - min_addr);
      if (alloc_base == NULL)
        panic("Unable to allocate memory for relocation!");
      reloc = (Elf32_Addr)alloc_base - min_addr;
    }

  for (unsigned i = 0; i < eh->e_phnum; ++i, ++ph)
    {
      if (ph->p_type != PT_LOAD)
        continue;

      void* target = (void*)(Elf32_Addr)(ph->p_paddr + reloc);

      if (!is_range_in_4g((uintptr_t)_elf, ph->p_offset))
        panic("Could not load PHDR. Location in ELF exceeds 32-bit limits!");

      memcpy(target, _elf + ph->p_offset, ph->p_filesz);

      if (ph->p_filesz < ph->p_memsz)
        memset(target + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
   }

  if (reloc != 0)
    {
      for (unsigned j = 0; j < eh->e_shnum; ++j, ++sh)
        {
          switch (sh->sh_type)
            {
            case SHT_RELA:
              {
                // Process each entry in the table and relocate if in range
                Elf64_Rela *rel = (Elf64_Rela *)(_elf + sh->sh_offset);
                for (; (char *)rel < _elf + sh->sh_offset + sh->sh_size; rel++)
                  relocate_rela_entry(rel, reloc);
                break;
              }
            case SHT_REL:
              panic("Unsupported relocation type SHT_REL");
            }
        }
    }

  return eh->e_entry + reloc;
}
