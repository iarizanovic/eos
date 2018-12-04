// ----------------------------------------------------------------------
// File: SteadyClock.hh
// Author: Georgios Bitzes - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2018 CERN/Switzerland                                  *
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

#include <chrono>
#include <mutex>
#include "common/Namespace.hh"

EOSCOMMONNAMESPACE_BEGIN

//------------------------------------------------------------------------------
//! A clock which behaves similarly to std::chrono::steady_clock, but can be
//! faked. During faking, you can advance time manually.
//------------------------------------------------------------------------------
class SteadyClock {
public:
  //----------------------------------------------------------------------------
  //! Constructor: Specify whether we're faking time, or not.
  //----------------------------------------------------------------------------
  SteadyClock(bool fake_) : fake(fake_) {}

  //----------------------------------------------------------------------------
  //! Get current time.
  //----------------------------------------------------------------------------
  std::chrono::steady_clock::time_point now() const {
    if(fake) {
      std::lock_guard<std::mutex> lock(mtx);
      return fakeTimepoint;
    }

    return std::chrono::steady_clock::now();
  }

  //----------------------------------------------------------------------------
  //! Advance current time - only call if you're faking the clock, otherwise
  //! has no effect...
  //----------------------------------------------------------------------------
  template<typename T>
  void advance(T duration) {
    std::lock_guard<std::mutex> lock(mtx);
    fakeTimepoint += duration;
  }

private:
  bool fake;
  mutable std::mutex mtx;
  std::chrono::steady_clock::time_point fakeTimepoint;
};

EOSCOMMONNAMESPACE_END
