//------------------------------------------------------------------------------
//! @file RocksKVTest.cc
//! @author Georgios Bitzes CERN
//! @brief tests for kv persistency class based on rocksdb
//------------------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2016 CERN/Switzerland                                  *
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

#include "kv/RocksKV.hh"
#include <gtest/gtest.h>

TEST(RocksKV, BasicSanity) {
  ASSERT_EQ(system("rm -rf /tmp/eos-fusex-tests"), 0);

  RocksKV kv;
  ASSERT_EQ(kv.connect("/tmp/eos-fusex-tests"), 0);

  std::string key("123");
  std::string value("asdf");

  ASSERT_EQ(kv.put(key, value), 0);

  std::string tmp;
  ASSERT_EQ(kv.get(key, tmp), 0);
  ASSERT_EQ(tmp, "asdf");
}
