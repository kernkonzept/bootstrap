/*
 * Copyright (C) 2023-2024 Kernkonzept GmbH.
 * Author(s): Marcus Haehnel <marcus.haehnel@kernkonzept.com>
 *
 * License: see LICENSE.spdx (in this directory or the directories above)
 */

#include <stdio.h>

#include <mod_info.h>

/**
 * Check if `node` applies to the module.
 *
 * Modules that have no 'nodes' attribute apply to all node IDs. Otherwise the
 * 'nodes' attribute is parsed as a colon separated list of node ranges. A
 * range can also be a single number. Examples:
 *
 * - nodes=1      -- Only node 1
 * - nodes=1-3    -- Nodes 1 to 3 (inclusive)
 * - nodes=0:2-3  -- Nodes 0, 2 and 3
 *
 * All numbers are assumed to be decimal.
 */
bool Mod_info::is_for_node(unsigned node) const
{
  cxx::String nodes = attrs().find("nodes");
  if (nodes.empty())
    return true;

  do
    {
      char const *colon = nodes.find(':');
      cxx::String range = nodes.head(colon);
      nodes = nodes.substr(colon + 1);

      unsigned start_node;
      unsigned end_node;

      int next = range.from_dec(&start_node);
      if (next >= range.len())
        end_node = start_node;
      else if (   next < 0
               || range[next] != '-'
               || range.substr(next + 1).from_dec(&end_node) + next + 1
                  != range.len())
          {
            printf("Invalid 'nodes' range: %.*s\n", range.len(), range.start());
            return false;
          }

      if (start_node <= node && node <= end_node)
        return true;
    }
  while (!nodes.empty());

  return false;
}

short Mod_info::index() const
{ return this - mod_header->mods().begin(); }
