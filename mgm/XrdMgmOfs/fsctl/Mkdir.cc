// ----------------------------------------------------------------------
// File: Mkdir.cc
// Author: Andreas-Joachim Peters - CERN
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

#include "common/Logging.hh"
#include "mgm/Stat.hh"
#include "mgm/XrdMgmOfs.hh"
#include "mgm/Macros.hh"

#include <XrdOuc/XrdOucEnv.hh>

//----------------------------------------------------------------------------
// Make a directory and return its inode
//----------------------------------------------------------------------------
int
XrdMgmOfs::Mkdir(const char* path,
                 const char* ininfo,
                 XrdOucEnv& env,
                 XrdOucErrInfo& error,
                 eos::common::VirtualIdentity& vid,
                 const XrdSecEntity* client)
{
  ACCESSMODE_W;
  MAYSTALL;
  MAYREDIRECT;
  gOFS->MgmStats.Add("Fuse-Mkdir", vid.uid, vid.gid, 1);
  char* smode = env.Get("mode");
  int retc = 0;

  if (smode) {
    struct stat buf;
    XrdSfsMode mode = atoi(smode);
    int retc1 = _mkdir(path, mode, error, vid, 0);
    int retc2 = 0;

    if (retc1 == SFS_OK) {
      retc2 = lstat(path, &buf, error, client, 0);
    }

    if ((retc1 == SFS_OK) && (retc2 == SFS_OK)) {
      char statinfo[16384];
      // Convert into a char stream
      sprintf(statinfo,
              "mkdir: %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu\n",
              (unsigned long long) buf.st_dev,
              (unsigned long long) buf.st_ino,
              (unsigned long long) buf.st_mode,
              (unsigned long long) buf.st_nlink,
              (unsigned long long) buf.st_uid,
              (unsigned long long) buf.st_gid,
              (unsigned long long) buf.st_rdev,
              (unsigned long long) buf.st_size,
              (unsigned long long) buf.st_blksize,
              (unsigned long long) buf.st_blocks,
#ifdef __APPLE__
              (unsigned long long) buf.st_atimespec.tv_sec,
              (unsigned long long) buf.st_mtimespec.tv_sec,
              (unsigned long long) buf.st_ctimespec.tv_sec,
              (unsigned long long) buf.st_atimespec.tv_nsec,
              (unsigned long long) buf.st_mtimespec.tv_nsec,
              (unsigned long long) buf.st_ctimespec.tv_nsec
#else
              (unsigned long long) buf.st_atime,
              (unsigned long long) buf.st_mtime,
              (unsigned long long) buf.st_ctime,
              (unsigned long long) buf.st_atim.tv_nsec,
              (unsigned long long) buf.st_mtim.tv_nsec,
              (unsigned long long) buf.st_ctim.tv_nsec
#endif
             );
      error.setErrInfo(strlen(statinfo) + 1, statinfo);
      return SFS_DATA;
    } else {
      retc = error.getErrInfo();
    }
  } else {
    retc = EINVAL;
  }

  XrdOucString response = "mkdir: retc=";
  response += retc;
  error.setErrInfo(response.length() + 1, response.c_str());
  return SFS_DATA;
}
