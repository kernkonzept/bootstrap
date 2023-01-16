/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2023 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#pragma once

#include <l4/sys/l4int.h>

/**
 * Binding for QEMU Firmware Configuration (fw_cfg) Interface.
 */
class Fw_cfg
{
public:
  /**
   * Initialize fw_cfg for access via I/O Ports.
   *
   * \return Whether fw_cfg initialization was successful.
   */
  static bool init_io();

  /**
   * Initialize fw_cfg for access via MMIO.
   *
   * \return Whether fw_cfg initialization was successful.
   */
  static bool init_mmio(l4_addr_t base_addr);


  /**
   * Determine if fw_cfg is available, i.e. successfully initialized.
   */
  static bool is_present();

  /**
   * Select firmware configuration item.
   *
   * \param selector  Selector key of the configuration item to select.
   */
  static void select(l4_uint16_t selector);

  /**
   * Lookup firmware configuration item by path name.
   *
   * \param      name      Path name of the configuration item to lookup.
   * \param[out] selector  Selector of the configuration item, if found.
   * \param[out] size      Size of the configuration item, if found.
   *
   * \return Whether a configuration item was found for the path name.
   */
  static bool find_file(char const *name, l4_uint16_t *selector, l4_uint32_t *size);

  /**
   * Select firmware configuration item by path name.
   *
   * \param name Path name of the configuration item to lookup.
   *
   * \retval 0  If no configuration item was found and thus nothing selected.
   * \retval >0 Size of the selected configuration item.
   */
  static l4_uint32_t select_file(char const *name);

  /**
   * Read from the currently selected configuration item and advance the data
   * offset accordingly.
   *
   * \param      size    Number of bytes to read.
   * \param[out] buffer  Buffer to store the read bytes.
   */
  static void read_bytes(l4_uint32_t size, l4_uint8_t *buffer);

  /**
   * Write to the currently selected configuration item and advance the data
   * offset accordingly.
   *
   * \param size    Number of bytes to write.
   * \param buffer  Buffer containing the bytes to write.
   */
  static void write_bytes(l4_uint32_t size, l4_uint8_t const *buffer);

  /**
   * Advance data offset to the currently selected configuration item.
   *
   * \param size    Number of bytes to skip.
   */
  static void skip_bytes(l4_uint32_t size);

  template<typename T>
  static T read()
  {
    T val;
    read_bytes(sizeof(T), reinterpret_cast<l4_uint8_t *>(&val));
    return val;
  }

  template<typename T>
  static void write(T const &val)
  {
    write_bytes(sizeof(T), reinterpret_cast<l4_uint8_t const *>(&val));
  }

  template<unsigned N>
  static void read_str(l4_uint32_t len, char(&buffer)[N])
  {
    if (N == 0)
      return;

    // Limit string length to buffer size minus terminating null-byte.
    if (len >= N)
      len = N;
    read_bytes(len, reinterpret_cast<l4_uint8_t *>(&buffer));
    // Null-terminate string.
    buffer[len] = 0;
  }

private:
  static bool init();
  static void dma_transfer(l4_uint32_t control, l4_uint32_t size, l4_uint8_t *buffer);
  static void trigger_dma(l4_addr_t dma_desc);
};
