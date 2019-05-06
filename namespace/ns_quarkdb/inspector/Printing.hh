/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2019 CERN/Switzerland                                  *
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
//! @author Georgios Bitzes <georgios.bitzes@cern.ch>
//! @brief Class for formatting and printing namespace protobuf objects
//------------------------------------------------------------------------------

#pragma once
#include "namespace/Namespace.hh"
#include "proto/FileMd.pb.h"
#include "proto/ContainerMd.pb.h"
#include <ostream>

EOSNSNAMESPACE_BEGIN

//------------------------------------------------------------------------------
//! Printing class
//------------------------------------------------------------------------------
class Printing {
public:
  //----------------------------------------------------------------------------
  //! Print the given FileMd protobuf using multiple lines, full information
  //----------------------------------------------------------------------------
  static void printMultiline(const eos::ns::FileMdProto &proto, std::ostream &stream);
  static std::string printMultiline(const eos::ns::FileMdProto &proto);


};

EOSNSNAMESPACE_END
