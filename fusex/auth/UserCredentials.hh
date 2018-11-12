// ----------------------------------------------------------------------
// File: UserCredentials.hh
// Author: Georgios Bitzes - CERN
// ----------------------------------------------------------------------

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

#ifndef EOS_FUSEX_USER_CREDENTIALS_HH
#define EOS_FUSEX_USER_CREDENTIALS_HH

#include "JailedPath.hh"

//------------------------------------------------------------------------------
// Designates what kind of user credentials we're dealing with:
// - KRB5: Kerberos file-based ticket cache
// - KRK5: Kerberos kernel-keyring-based ticket cache
// - X509: GSI user certificates
// - SSS: SSS ticket delegation
// - NOBODY: Identify as nobody, no user credentails whatsoever
//------------------------------------------------------------------------------
enum class CredentialType : std::uint32_t {
  KRB5,
  KRK5,
  X509,
  SSS,
  NOBODY
};

//------------------------------------------------------------------------------
// This class stores information about an instance of user credentials.
//------------------------------------------------------------------------------
struct UserCredentials {
  CredentialType type;
  JailedPath fname; // credential file
  std::string keyring; // kernel keyring
  std::string endorsement; // endorsement for sss
  time_t mtime;

  //----------------------------------------------------------------------------
  // Comparator for storing such objects in maps.
  //----------------------------------------------------------------------------
  bool operator<(const UserCredentials& src) const
  {
    if (type != src.type) {
      return type < src.type;
    }

    if (fname != src.fname) {
      return fname < src.fname;
    }

    if (keyring != src.keyring) {
      return keyring < src.keyring;
    }

    if (endorsement < src.endorsement) {
      return endorsement < src.endorsement;
    }

    return mtime < src.mtime;
  }
};

#endif
