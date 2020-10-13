// ----------------------------------------------------------------------
// File: SharedDequeProvider.cc
// Author: Georgios Bitzes - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2020 CERN/Switzerland                                  *
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

#include "mq/SharedDequeProvider.hh"
#include <qclient/shared/SharedDeque.hh>

EOSMQNAMESPACE_BEGIN

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
SharedDequeProvider::SharedDequeProvider(qclient::SharedManager *manager)
: mSharedManager(manager) {}

//------------------------------------------------------------------------------
// Get shared deque
//------------------------------------------------------------------------------
std::shared_ptr<qclient::SharedDeque> SharedDequeProvider::get(const std::string &key) {
  std::unique_lock lock(mMutex);

  auto it = mStore.find(key);
  if(it != mStore.end()) {
    return it->second;
  }

  std::shared_ptr<qclient::SharedDeque> deque;
  deque.reset(new qclient::SharedDeque(mSharedManager, key));

  mStore[key] = deque;
  return deque;
}

EOSMQNAMESPACE_END
