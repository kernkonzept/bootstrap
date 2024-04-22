/*
 * Copyright (C) 2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#pragma once

#include <ctype.h>

#include <l4/cxx/string>

/**
 * RISC-V ISA string parser that extracts ISA extensions according to the
 * "ISA Extension Naming Conventions" chapter of the "The RISC-V Instruction Set
 * Manual: Unprivileged Architecture".
 */
class Isa_parser
{
public:
  /**
   * * \param isa  Null-terminated RISC-V ISA string.
   */
  explicit Isa_parser(char const *isa)
  : _isa(isa), _cur(_isa), _ext_start(nullptr), _ext_len(0), _has_err(false)
  {}

  /**
   * Advance to the next extension in the ISA string.
   *
   * \retval true   Found extension.
   * \retval false  Reached end of ISA string.
   */
  bool next_ext()
  {
    while (*_cur)
      {
        switch (*_cur) {
          case 's':
            // QEMU versions before v7.1.0 report the invalid 's' and 'u' single
            // letter extensions.
            if (_cur > _isa && _cur[-1] != '_' && _cur[1] == 'u')
            {
              // Skip the invalid extensions.
              _cur += 2;
              continue;
            }
            [[fallthrough]];
          case 'S':
          case 'z':
          case 'Z':
          case 'x':
          case 'X':
            // Multi-letter extension.
            _ext_start = _cur++;
            while (isalpha(*_cur))
              _cur++;
            _ext_len = _cur - _ext_start;
            break;

          default:
            // Single-letter extension?
            if (!isalpha(*_cur))
              {
                // Extensions are separated by underscore.
                if (*_cur != '_')
                  // Otherwise we encountered an error.
                  _has_err = true;
                _cur++;
                continue;
              }

            // Single-letter extension.
            _ext_start = _cur++;
            _ext_len = 1;
            break;
        }

        // Ignore version numbers for now, i.e. just skip them.
        skip_version();

        return true;
      }

    // Reached end of the ISA string.
    _ext_start = _cur;
    _ext_len = 0;
    return false;
  }

  /**
   * Current extension name.
   *
   * \pre Only valid if last call \ref next_ext() returned true.
   *
   * \note Not null-terminated, consult \ref ext_len().
   */
  cxx::String const ext() const
  { return cxx::String(_ext_start, _ext_len); }

  /**
   * Whether parser encountered errors.
   *
   * This purpose of this function is purely informative, the parser handles
   * errors gracefully, and simply skips the faulty parts of the ISA string.
   */
  bool has_err() const
  { return _has_err; }

private:
  void skip_version()
  {
    // Major version.
    if (!isdigit(*_cur))
      return;
    while(isdigit(*++_cur));

    // Minor version.
    if (tolower(*_cur) != 'p')
      return;
    while(isdigit(*++_cur));
  }

  char const *_isa;
  char const *_cur;

  char const *_ext_start;
  unsigned _ext_len;

  bool _has_err;
};
