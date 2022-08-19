// ----------------------------------------------------------------------
// File: WalkDirTree
// Author: Abhishek Lekshmanan - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2022 CERN/Switzerland                                  *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 ************************************************************************/
#pragma once
#include <vector>
#include "common/Logging.hh"
#include "common/StringUtils.hh"
#include <fts.h>
#include <utility>
#include <type_traits>
#include <system_error>

namespace eos::fst
{

static constexpr std::string_view XSMAP_EXT = "xsmap";

inline bool exclude_xs_map(std::string_view filename)
{
  return common::endsWith(filename, XSMAP_EXT);
}

// A function to walk the dir tree and apply a function with arguments
// It is necessary that the function's first argument is a const char* path
// This function uses FTS to walk through directory entries and
// doesn't follow symlinks and only operates on regular files atm
template <typename ExcludeFn, typename PathOp>
uint64_t
WalkDirTree(std::vector<char*>&& paths, ExcludeFn exclude_fn, PathOp path_op,
            std::error_code& ec)
{
  FTS* tree = fts_open(paths.data(), FTS_NOCHDIR, 0);

  if (!tree) {
    eos_static_err("msg=\"fts_open failed\" errno=%d", errno);
    ec = std::make_error_code(static_cast<std::errc>(errno));
    return 0;
  }

  uint64_t cnt {0};
  FTSENT* node;

  while ((node = fts_read(tree))) {
    if (node->fts_level > 0 && node->fts_name[0] == '.') {
      fts_set(tree, node, FTS_SKIP);
    } else {
      if (node->fts_info == FTS_F) {
        if (!exclude_fn(node->fts_accpath)) {
          path_op(node->fts_path);
          ++cnt;
        }
      }
    }
  }

  if (fts_close(tree)) {
    eos_static_err("msg=\"fts_close failed\" errno=%d", errno);
    ec = std::make_error_code(static_cast<std::errc>(errno));
  }

  return cnt;
}

// A function useful for walking FST trees, where xsmap files are usually excluded
// This variant expects a member function to be applied across the tree
template <typename UnaryOp>
uint64_t
WalkFSTree(std::string path, UnaryOp&& op, std::error_code& ec)
{
  return WalkDirTree({path.data(), nullptr},
                     exclude_xs_map,
                     std::forward<UnaryOp>(op),
                     ec);
}



} // namespace eos::fst
