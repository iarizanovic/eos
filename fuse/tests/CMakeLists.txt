# ------------------------------------------------------------------------------
# File: CMakeLists.txt
# Author: Elvin-Alin Sindrilaru <esindril@cern.ch> CERN
# ------------------------------------------------------------------------------

# ************************************************************************
# * EOS - the CERN Disk Storage System                                   *
# * Copyright (C) 2014 CERN/Switzerland                                  *
# *                                                                      *
# * This program is free software: you can redistribute it and/or modify *
# * it under the terms of the GNU General Public License as published by *
# * the Free Software Foundation, either version 3 of the License, or    *
# * (at your option) any later version.                                  *
# *                                                                      *
# * This program is distributed in the hope that it will be useful,      *
# * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
# * GNU General Public License for more details.                         *
# *                                                                      *
# * You should have received a copy of the GNU General Public License    *
# * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
# ************************************************************************

include_directories(${CMAKE_SOURCE_DIR}
	            ${CMAKE_CURRENT_SOURCE_DIR}
                    ${CPPUNIT_INCLUDE_DIRS})

link_directories(${CPPUNIT_LIBRARY})

add_library(
  EosFuseTests MODULE
  Namespace.hh
  FuseFsTest.cc
  FuseFileTest.cc
  TestEnv.cc       TestEnv.hh)

target_link_libraries(
 EosFuseTests 
 FuseCache-Static
 ${CPPUNIT_LIBRARIES})

install(
  TARGETS EosFuseTests
  LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR})
	  
