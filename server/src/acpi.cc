/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Jan Klötzke <jan.kloetzke@kernkonzept.com>
 */

#include <ctype.h>
#include "acpi.h"

namespace Acpi {

enum { Print_info = 0 };

static void
print_acpi_id(char const *id, int len)
{
  char ID[len];
  for (int i = 0; i < len; ++i)
    ID[i] = isalnum(id[i]) ? id[i] : '.';
  printf("%.*s", len, ID);
}

struct Rsdp
{
  char       signature[8];
  l4_uint8_t  chk_sum;
  char       oem[6];
  l4_uint8_t  rev;
  l4_uint32_t rsdt_phys;
  l4_uint32_t len;
  l4_uint64_t xsdt_phys;
  l4_uint8_t  ext_chk_sum;
  char       reserved[3];

  bool checksum_ok() const
  {
    // ACPI 1.0 checksum
    l4_uint8_t sum = 0;
    for (unsigned i = 0; i < 20; i++)
      sum += *((l4_uint8_t *)this + i);

    if (sum)
      return false;

    if (rev == 0)
      return true;

    // Extended Checksum
    for (unsigned i = 0; i < len && i < 4096; ++i)
      sum += *((l4_uint8_t *)this + i);

    return !sum;
  }

  void print_info() const
  {
    printf("ACPI: RSDP[%p]\tr%02x OEM:", this, (unsigned)rev);
    print_acpi_id(oem, 6);
    printf("\n");
  }
} __attribute__((packed));


bool
check_signature(char const *sig, char const *reference)
{
  for (; *reference; ++sig, ++reference)
    if (*reference != *sig)
      return false;

  return true;
}

bool
Table_head::checksum_ok() const
{
  l4_uint8_t sum = 0;
  for (unsigned i = 0; i < len; ++i)
    sum += *((l4_uint8_t *)this + i);

  return !sum;
}

void
Table_head::print_info() const
{
  printf("ACPI: ");
  print_acpi_id(signature, 4);
  printf("[%p]\tr%02x OEM:", this, (unsigned)rev);
  print_acpi_id(oem_id, 6);
  printf(" OEMTID:");
  print_acpi_id(oem_tid, 8);
  printf("\n");
}

bool
Sdt::init(void *root)
{
  Rsdp const *rsdp = reinterpret_cast<Rsdp const *>(root);
  if (!check_signature(rsdp->signature, "RSD PTR ") || !rsdp->checksum_ok())
    return false;

  if (Print_info)
    rsdp->print_info();

  if (rsdp->rev && rsdp->xsdt_phys)
    {
      auto *x = (const Xsdt *)rsdp->xsdt_phys;
      if (!x->checksum_ok())
        printf("ACPI: Checksum mismatch in XSDT\n");
      else
        {
          _xsdt = x;
          if (Print_info)
            {
              x->print_info();
              print_summary();
            }
          return true;
        }
    }

  if (rsdp->rsdt_phys)
    {
      auto *r = (const Rsdt *)(unsigned long)rsdp->rsdt_phys;
      if (!r->checksum_ok())
        printf("ACPI: Checksum mismatch in RSDT\n");
      else
        {
          _rsdt = r;
          if (Print_info)
            {
              r->print_info();
              print_summary();
            }
          return true;
        }
    }

  return false;
}

Table_head const *
Sdt::find_head(char const *sig) const
{
  for (unsigned i = 0; i < entries(); ++i)
    {
      Table_head const *t = entry(i);
      if (!t)
	continue;

      if (check_signature(t->signature, sig) && t->checksum_ok())
	return t;
    }

  return nullptr;
}

unsigned
Sdt::entries() const
{
  if (_xsdt)
    return (_xsdt->len - sizeof(Table_head)) / sizeof(_xsdt->ptrs[0]);
  else if (_rsdt)
    return (_rsdt->len - sizeof(Table_head)) / sizeof(_rsdt->ptrs[0]);
  else
    return 0;
}

Table_head const *
Sdt::entry(unsigned i) const
{
  if (_xsdt)
    return (Table_head const *)_xsdt->ptrs[i];
  else if (_rsdt)
    return (Table_head const *)(unsigned long)_rsdt->ptrs[i];
  else
    return nullptr;
}

void
Sdt::print_summary() const
{
  for (unsigned i = 0; i < entries(); ++i)
    if (entry(i))
      entry(i)->print_info();
}

}
