/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2011 CERN/Switzerland                                  *
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

//------------------------------------------------------------------------------
// author: Lukasz Janyst <ljanyst@cern.ch>
// desc:   Some constants concerning the change log data
//------------------------------------------------------------------------------

#ifndef EOS_NS_CHANGELOG_CONSTANTS_HH
#define EOS_NS_CHANGELOG_CONSTANTS_HH

#include <stdint.h>

namespace eos
{
  extern const uint8_t  UPDATE_RECORD_MAGIC;
  extern const uint8_t  DELETE_RECORD_MAGIC;
  extern const uint8_t  COMPACT_STAMP_RECORD_MAGIC;
  extern const uint16_t FILE_LOG_MAGIC;
  extern const uint16_t CONTAINER_LOG_MAGIC;
  extern const uint8_t  LOG_FLAG_COMPACTED;
}

#endif // EOS_NS_CHANGELOG_CONSTANTS_HH
