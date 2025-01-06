/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2024 Kernkonzept GmbH.
 * Author(s): Georg Kotheimer <georg.kotheimer@kernkonzept.com>
 */

#pragma once

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include <l4/cxx/minmax>
#include <l4/cxx/type_traits>
#include <l4/sys/l4int.h>

#include "koptions-def.h"

extern "C" {
#include <libfdt.h>
}

/**
 * Read-only wrapper around a flattened device tree.
 */
class Dt
{
public:
  enum { Verbose_dt = 0 };

  /**
   * Optional return value to control callback-driven loops.
   */
  enum Cb
  {
    Break,
    Continue,
  };

  /**
   * Accessor for array encoded properties.
   */
  class Array
  {
  public:
    Array()
      : _prop(nullptr), _len(0)
    {}

    Array(fdt32_t const *data, unsigned len)
      : _prop(data), _len(len)
    {}

    bool is_present() const
    { return _prop != nullptr; }

    template<typename T>
    T get(unsigned off) const
    {
      assert(off < _len);
      assert(off + (sizeof(T) / sizeof(fdt32_t)) <= _len);

      int len = cxx::min<unsigned>(sizeof(T) / sizeof(fdt32_t), _len - off);
      return read_value<T>(_prop + off, len);
    }

    unsigned len() const
    { return _len; }

    Array view(unsigned off, unsigned len) const
    {
      assert(off + len <= _len);
      return Array(_prop + off, len);
    }

  private:
    fdt32_t const *_prop;
    unsigned _len;
  };

  /**
   * Accessor for array properties with regularly sized cells.
   */
  template<unsigned N>
  class Array_prop
  {
  public:
    Array_prop()
      : _prop(nullptr), _len(0)
    {}

    Array_prop(fdt32_t const *prop, unsigned len, unsigned const cell_size[N])
      : _prop(prop), _len(len)
    {
      _elem_size = 0;
      for (unsigned i = 0; i < N; i++)
        {
          _cell_size[i] = cell_size[i];
          _elem_size += cell_size[i];
        }
    }

    bool is_present() const
    { return _prop != nullptr; }

    bool is_valid() const
    { return is_present() && (_len % _elem_size) == 0; }

    template<typename T = l4_uint64_t>
    T get(unsigned element, unsigned cell) const
    {
      assert(cell < N);
      assert(element < elements());

      unsigned off = element * _elem_size;
      for (unsigned i = 0; i < cell; i++)
        off += _cell_size[i];

      return read_value<T>(_prop + off, _cell_size[cell]);
    }

    unsigned elements() const
    { return _len / _elem_size; }

  private:
    fdt32_t const *_prop;
    unsigned _len;
    unsigned _cell_size[N];
    unsigned _elem_size;
  };

  /**
   * Data and methods associated with a range property in a device tree
   *
   * Ranges in a device tree describe the translation of regions from one
   * domain to another.
   */
  class Range
  {
  public:
    using Cell = l4_uint64_t;

    /**
     * Translate an address from one domain to another
     *
     * This function takes an address and a size and translates the address
     * from one domain to another if there is a matching range.
     *
     * \param[inout]  address    Address that shall be translated
     * \param[in]     size Size  Size associated with the address
     */
    bool translate(Cell &address, Cell const &size) const
    {
      if (match(address, size))
        {
          address = (address - _child) + _parent;
          return true;
        }
      return false;
    }

    Range(Cell const &child, Cell const &parent, Cell const &length)
    : _child{child}, _parent{parent}, _length{length} {};

  private:
    // Child bus address
    Cell _child;
    // Parent bus address
    Cell _parent;
    // Range size
    Cell _length;

    // [address, address + size] subset of [child, child + length] ?
    bool match(Cell const &address, Cell const &size) const
    {
      auto address_max = address + size;
      auto child_max = _child + _length;
      return (_child <= address) && (address_max <= child_max);
    }
  };

  class Node
  {
  public:
    explicit Node(void const *fdt = nullptr) : _fdt(fdt), _off(-1) {}
    Node(void const *fdt, int off) : _fdt(fdt), _off(off) {}

    bool is_valid() const
    { return _off >= 0; }

    bool is_root_node() const
    { return _off == 0; }

    Node parent_node() const
    { return Node(_fdt, fdt_parent_offset(_fdt, _off)); }

    char const *get_name(char const *default_name = nullptr) const
    {
      if (is_root_node())
        return "/";

      auto name = fdt_get_name(_fdt, _off, nullptr);
      return name ? name : default_name;
    }

    bool has_prop(char const *name) const
    {
      return fdt_getprop_namelen(_fdt, _off, name, strlen(name), nullptr);
    }

    template<typename R>
    R const *get_prop(char const *name, int *size = nullptr) const
    {
      auto prop = static_cast<R const *>(
        fdt_getprop_namelen(_fdt, _off, name, strlen(name), size));

      if (prop && size)
        *size /= sizeof(R);

      return prop;
    }

    template<typename T>
    bool get_prop_val(char const *name, T &val) const
    {
      int len;
      auto prop = get_prop<fdt32_t>(name, &len);
      if (!prop)
        return false;

      val = read_value<T>(prop, len);
      return true;
    }

    bool get_prop_u32(char const *name, l4_uint32_t &val) const
    { return get_prop_val(name, val); }

    bool get_prop_u64(char const *name, l4_uint64_t &val) const
    { return get_prop_val(name, val); }

    char const *get_prop_str(char const *name) const;

    Array get_array(char const *name) const
    {
      int len;
      const fdt32_t *cells = get_prop<fdt32_t>(name, &len);
      return Array(cells, len);
    }

    template<unsigned N>
    Array_prop<N> get_prop_array(char const *name,
                                 unsigned const (&elems)[N]) const
    {
      int len;
      const fdt32_t *cells = get_prop<fdt32_t>(name, &len);
      if (cells)
        return Array_prop<N>(cells, len, elems);
      else
        return Array_prop<N>();
    }

    /**
     * Check if a string list contains a string.
     *
     * \param name   Name of the string list property.
     * \param value  String to search for,
     *
     * \retval true   If the string list contains the string.
     * \retval false  Otherwise.
     */
    bool stringlist_contains(char const *name, char const *value) const;

    /**
     * Find a string in a string list and return its index.
     *
     * \param name   Name of the string list property.
     * \param value  String to search for,
     *
     * \retval >= 0  On success, the index of the string in the list.
     * \retval  < 0  On failure, FDT error code.
     */
    int stringlist_search(char const *name, char const *value) const;

    bool get_addr_size_cells(unsigned &addr_cells, unsigned &size_cells) const;

    template<typename CB>
    void for_each_reg(CB &&cb) const
    {
      Node parent = parent_node();
      Reg_array_prop regs;
      if (!get_reg_array(parent, regs))
        return;

      for (unsigned i = 0; i < regs.elements(); i++)
        {
          l4_uint64_t addr, size;
          if (get_reg_val(parent, regs, i, &addr, &size))
            if (invoke_cb(cb, addr, size) == Break)
              break;
        }
    }

    bool get_reg(unsigned index, l4_uint64_t *addr,
                 l4_uint64_t *size = nullptr) const
    {
      Node parent = parent_node();
      Reg_array_prop regs;
      if (!get_reg_array(parent, regs))
        return false;

      return get_reg_val(parent, regs, index, addr, size);
    }

    bool check_compatible(char const *compatible) const
    { return fdt_node_check_compatible(_fdt, _off, compatible) == 0; }

    bool check_device_type(char const *device_type) const
    {
      char const *prop = get_prop_str("device_type");
      return prop && !strcmp(prop, device_type);
    }

    bool is_enabled() const
    {
      char const *status = get_prop_str("status");
      return !status || !strcmp("ok", status) || !strcmp("okay", status);
    }

    template<typename CB>
    void for_each_subnode(CB &&cb) const
    {
      int node;
      fdt_for_each_subnode(node, _fdt, _off)
        if (invoke_cb(cb, Node(_fdt, node)) == Break)
          return;
    }

    /**
     * Iterate over a phandle list.
     *
     * \param  list_name   Property name that contains the phandle list.
     * \param  cells_name  Property name that specifies the argument count of a
     *                     phandle, i.e. the property is defined in each of the
     *                     referenced nodes.
     * \param  cb          Callback invoked for each phandle, taking the
     *                     referenced node and the arguments as an Dt::Array.
     */
    template<typename CB>
    void for_each_phandle(char const *list_name,
                          char const *cells_name, CB &&cb) const
    {
      Array list = get_array(list_name);
      if (!list.is_present())
        return;

      unsigned cur_cell = 0;
      while (cur_cell < list.len())
        {
          l4_uint32_t phandle = list.get<l4_uint32_t>(cur_cell++);
          Node node = dt().node_by_phandle(phandle);
          l4_uint32_t arg_cells;
          if (!node.is_valid() || !node.get_prop_u32(cells_name, arg_cells))
            // If referenced node is invalid or if we cannot figure out the
            // number of argument cells and therefore cannot just skip the
            // node, so we have to abort the iteration.
            return;

          if (invoke_cb(cb, node, list.view(cur_cell, arg_cells)) == Break)
            return;

          cur_cell += arg_cells;
        }
    }

    __attribute__ ((format (printf, 2, 3)))
    void warn(char const *format, ...) const
    {
      va_list args;
      va_start(args, format);
      log(format, args);
      va_end(args);
    }

    __attribute__ ((format (printf, 2, 3)))
    void info(char const *format, ...) const
    {
      if (!Verbose_dt)
        return;

      va_list args;
      va_start(args, format);
      log(format, args);
      va_end(args);
    }

  private:
    void log(char const *format, va_list args) const
    {
      if (auto name = get_name())
        printf("DT[%s]: ", name);
      else
        printf("DT[@%d]: ", _off);

      vprintf(format, args);
    }

    using Reg_array_prop = Array_prop<2>;

    bool translate_reg(Node parent,
                       l4_uint64_t &addr, l4_uint64_t const &size) const;

    bool get_reg_array(Node parent, Reg_array_prop &regs) const;

    bool get_reg_val(Node parent, Reg_array_prop const &regs, unsigned index,
                     l4_uint64_t *out_addr, l4_uint64_t *out_size) const;

    Dt const dt() const
    { return Dt(_fdt); }

    void const *_fdt;
    int _off;
  };

  Dt() = default;

  void init(unsigned long fdt_addr);
  void check_for_dt() const;

  bool have_fdt() const { return _fdt; }
  const void *fdt() const { return _fdt; }
  unsigned fdt_size() const { return _fdt ? fdt_totalsize(_fdt) : 0; }

  Node node_by_path(char const *path) const
  { return node_by_path(path, strlen(path)); }

  Node node_by_path(char const *path, int namelen) const;
  Node node_by_phandle(uint32_t phandle) const;
  Node node_by_compatible(char const *compatible) const;

  template<typename CB, typename NEXT>
  void nodes_by(CB &&cb, NEXT &&next) const
  {
    for (int node = next(-1); node >= 0; node = next(node))
      if (invoke_cb(cb, Node(_fdt, node)) == Break)
        return;
  }

  template<typename CB>
  void nodes_by_prop_value(char const *name, void const *val, int len,
                           CB &&cb) const
  {
    nodes_by(cb, [=](int node)
      { return fdt_node_offset_by_prop_value(_fdt, node, name, val, len); });
  }

  template<typename CB>
  void nodes_by_compatible(char const *compatible, CB &&cb) const
  {
    nodes_by(cb, [=](int node)
      { return fdt_node_offset_by_compatible(_fdt, node, compatible); });
  }

  template<typename T = l4_uint64_t>
  static T read_value(fdt32_t const *cells, unsigned size)
  {
    l4_uint64_t val = 0;
    for (unsigned i = 0; i < size; i++)
      val = (val << 32) | fdt32_to_cpu(cells[i]);

    if (size * sizeof(fdt32_t) > sizeof(T))
      warn("Cell size %zu larger than provided storage type %zu!\n",
           size * sizeof(fdt32_t), sizeof(T));

    return static_cast<T>(val);
  }

  void setup_memory() const;
  l4_uint64_t cpu_release_addr() const;

  /**
   * Get the clock with the given name or index for the given node.
   *
   * \param node   Node to get the clock for.
   * \param name   Name of the clock, optional.
   * \param index  Index of the clock, if no name was specified.
   *
   * \return Clock node, or invalid node if the `node` does not have a clock
   *         with the given name or index.
   */
  Node get_clock(Node node, char const *name, int index) const;

  /// Function that translates "interrupts" property into IRQ number.
  using Parse_irq_fn = int (*)(Node);

  /**
   * Get the UART selected for boot output via stdout-path.
   *
   * \param compatible  Optional compatible identifier the UART node must comply
   *                    with.
   *
   * \note For documentation of the remaining parameters and return values see
   *       `parse_uart()`.
   */
  Node get_stdout_uart(char const *compatible, Parse_irq_fn parse_irq,
                       L4_kernel_options::Uart *kuart,
                       unsigned int *kuart_flags) const;

  /**
   * Populate kernel UART options and flags from UART node.
   *
   * \param       uart         UART node to parse.
   * \param       parse_irq    Function that translates "interrupts" property
   *                           into IRQ number.
   * \param[out]  kuart        Kernel UART options.
   * \param[out]  kuart_flags  Kernel UART flags.
   *
   * \retval false  Not a valid/supported UART.
   * \retval true   Supported UART, the following options and flags are populated:
   *                - Always sets `kuart.base_address` and `F_uart_base` flag.
   *                - If available, sets `kuart.irqno` field and `F_uart_irq` flag.
   *                - If available, sets `kuart.baud` field and `F_uart_baud` flag.
   *                - If available, sets `kuart.baud_base` field.
   *                - If available, sets `kuart.reg_shift` field.
   */
  Node parse_uart(Node uart, Parse_irq_fn parse_irq,
                  L4_kernel_options::Uart *kuart, unsigned *kuart_flags) const;

  void dump() const;

  __attribute__ ((format (printf, 1, 2)))
  static void warn(char const *format, ...)
  {
    va_list args;
    va_start(args, format);
    log(format, args);
    va_end(args);
  }

  __attribute__ ((format (printf, 1, 2)))
  static void info(char const *format, ...)
  {
    if (!Verbose_dt)
      return;

    va_list args;
    va_start(args, format);
    log(format, args);
    va_end(args);
  }

protected:
  Dt(void const *fdt) : _fdt(fdt) {}

  static void log(char const *format, va_list args)
  {
    printf("DT: ");
    vprintf(format, args);
  }

  template<typename F, typename... Args>
  inline static Cb invoke_cb(F f, Args... args)
  {
    if constexpr (cxx::is_same_v<void, decltype(f(args...))>)
      {
        f(args...);
        return Continue;
      }
    else
      return f(args...);
  }

  void const *_fdt = nullptr;
};

