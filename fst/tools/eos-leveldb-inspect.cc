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

#include "common/DbMap.hh"
#include "common/LayoutId.hh"
#include "namespace/interface/IFileMD.hh"
#include "fst/Fmd.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include <iostream>
#include <algorithm>
#include <stdlib.h>
#include <getopt.h>

//------------------------------------------------------------------------------
// Display help message
//------------------------------------------------------------------------------
void print_usage(const char* prg_name)
{
  std::cerr << "Usage: : " << prg_name << " --dbpath <full_path> "
            << "[--count] [--dump_ids] [--fid <fid_dec>] [--fsck] [--verbose_fsck]"
            << std::endl
            << "   --count         :"
            << " diplay number of entries in the DB" << std::endl
            << "   --dump_ids      :"
            << "dumpd the decimal file ids stored in the DB" << std::endl
            << "   --fid <fid_dec> : "
            << " display stored metadata info about given file id"
            << "   --fsck          : "
            << " display fsck inconsistencies counters" << std::endl
            << "   --verbose_fsck  : "
            << " display fsck counters together with the hex file ids"
            << std::endl;
}

//------------------------------------------------------------------------------
// Dump the all the file ids stored in the local dabase in decimal format
//------------------------------------------------------------------------------
void DumpAllFids(eos::common::DbMap& db)
{
  const eos::common::DbMapTypes::Tkey* k;
  const eos::common::DbMapTypes::Tval* v;
  eos::common::DbMapTypes::Tval val;

  if (db.size() == 0) {
    std::cout << "info: db is empty!" << std::endl;
    return;
  }

  int max_per_row = 10;
  uint64_t count {0ull};
  std::cout << "fid(dec) : " << std::endl;

  for (db.beginIter(false); db.iterate(&k, &v, false); /*no progress*/) {
    eos::fst::Fmd fmd;
    fmd.ParseFromString(v->value);
    std::cout << std::setw(10) << fmd.fid() << " ";
    ++count;

    if (count % max_per_row == 0) {
      std::cout << std::endl;
    }
  }

  std::cout << std::endl;
}

//------------------------------------------------------------------------------
// Dump all the metadata stored in the local database corresponding to a
// particular file id.
//------------------------------------------------------------------------------
bool DumpFileInfo(eos::common::DbMap& db, const std::string& sfid)
{
  eos::IFileMD::id_t fid = std::stoull(sfid);
  eos::common::DbMap::Tval val;
  bool found = db.get(eos::common::Slice((const char*)&fid, sizeof(fid)), &val);

  if (!found) {
    std::cerr << "error: fid " << sfid << " not found in the DB" << std::endl;
    return false;
  }

  eos::fst::Fmd fmd;
  fmd.ParseFromString(val.value);
  eos::fst::FmdHelper fmd_helper;
  fmd_helper.Replicate(fmd);
  auto opaque = fmd_helper.FullFmdToEnv();
  int envlen;
  std::string data {opaque->Env(envlen)};
  std::replace(data.begin(), data.end(), '&', ' ');
  std::cout << "fxid=" << std::hex << fid << std::dec
            << data << std::endl;
  return true;
}

//------------------------------------------------------------------------------
// Dump fsck statistics
//------------------------------------------------------------------------------
void DumpFsckStats(eos::common::DbMap& db, bool verbose = false)
{
  using eos::common::LayoutId;
  std::map<std::string, size_t> statistics;
  std::map<std::string, std::set < eos::common::FileId::fileid_t>> fid_set;
  statistics["mem_n"] = 0; // number of files in DB
  statistics["d_sync_n"] = 0; // number of synced files from disk
  statistics["m_sync_n"] = 0; // number of synced files from MGM server
  // number of files with disk and reference size mismatch
  statistics["d_mem_sz_diff"] = 0;
  // number of files with MGM and reference size mismatch
  statistics["m_mem_sz_diff"] = 0;
  // number of files with disk and reference checksum mismatch
  statistics["d_cx_diff"] = 0;
  // number of files with MGM and reference checksum mismatch
  statistics["m_cx_diff"] = 0;
  statistics["orphans_n"] = 0; // number of orphaned replicas
  statistics["unreg_n"] = 0; // number of unregistered replicas
  statistics["rep_diff_n"] = 0; // number of files with replica number mismatch
  statistics["rep_missing_n"] = 0; // number of files which are missing on disk
  fid_set["m_mem_sz_diff"].clear();
  fid_set["d_mem_sz_diff"].clear();
  fid_set["m_cx_diff"].clear();
  fid_set["d_cx_diff"].clear();
  fid_set["orphans_n"].clear();
  fid_set["unreg_n"].clear();
  fid_set["rep_diff_n"].clear();
  fid_set["rep_missing_n"].clear();
  const eos::common::DbMapTypes::Tkey* k;
  const eos::common::DbMapTypes::Tval* v;
  eos::common::DbMapTypes::Tval val;

  for (db.beginIter(false); db.iterate(&k, &v, false);) {
    eos::fst::Fmd f;
    f.ParseFromString(v->value);

    if (f.layouterror()) {
      if (f.layouterror() & LayoutId::kOrphan) {
        statistics["orphans_n"]++;
        fid_set["orphans_n"].insert(f.fid());
      }

      if (f.layouterror() & LayoutId::kUnregistered) {
        statistics["unreg_n"]++;
        fid_set["unreg_n"].insert(f.fid());
      }

      if (f.layouterror() & LayoutId::kReplicaWrong) {
        statistics["rep_diff_n"]++;
        fid_set["rep_diff_n"].insert(f.fid());
      }

      if (f.layouterror() & LayoutId::kMissing) {
        statistics["rep_missing_n"]++;
        fid_set["rep_missing_n"].insert(f.fid());
      }
    }

    if (f.mgmsize() != 0xfffffffffff1ULL) {
      statistics["m_sync_n"]++;

      if (f.size() != 0xfffffffffff1ULL) {
        if (f.size() != f.mgmsize()) {
          statistics["m_mem_sz_diff"]++;
          fid_set["m_mem_sz_diff"].insert(f.fid());
        }
      }
    }

    if (!f.layouterror()) {
      if (f.size() && f.diskchecksum().length() &&
          (f.diskchecksum() != f.checksum())) {
        statistics["d_cx_diff"]++;
        fid_set["d_cx_diff"].insert(f.fid());
      }

      if (f.size() && f.mgmchecksum().length() && (f.mgmchecksum() != f.checksum())) {
        statistics["m_cx_diff"]++;
        fid_set["m_cx_diff"].insert(f.fid());
      }
    }

    statistics["mem_n"]++;

    if (f.disksize() != 0xfffffffffff1ULL) {
      statistics["d_sync_n"]++;

      if (f.size() != 0xfffffffffff1ULL) {
        // Report missmatch only for replica layout files
        if ((f.size() != f.disksize()) &&
            (eos::common::LayoutId::GetLayoutType(f.lid())
             == eos::common::LayoutId::kReplica)) {
          statistics["d_mem_sz_diff"]++;
          fid_set["d_mem_sz_diff"].insert(f.fid());
        }
      }
    }
  }

  // Display summary
  if (verbose) {
    // Helper function for printing
    auto print_fids = [](const std::set<eos::common::FileId::fileid_t>& set)
    -> std::string {
      std::ostringstream oss;
      int count = 0;
      int max_per_line = 10;
      oss << std::hex;

      for (const auto& elem : set)
      {
        oss << elem << " ";
        ++count;

        if (count % max_per_line == 0) {
          oss << std::endl;
        }
      }

      oss << std::endl;
      return oss.str();
    };
    std::cout << "Num entries in DB[mem_n]:                      "
              << statistics["mem_n"] << std::endl
              << "Num. files synced from disk[d_sync_n]:         "
              << statistics["d_sync_n"] << std::endl
              << "Num, files synced from MGM[m_sync_n]:          "
              << statistics["m_sync_n"] << std::endl
              << "Disk/referece size missmatch[d_mem_sz_diff]:   "
              << statistics["d_mem_sz_diff"] << std::endl
              << print_fids(fid_set["d_mem_sz_diff"])
              << "MGM/reference size missmatch[m_mem_sz_diff]:   "
              << statistics["m_mem_sz_diff"] << std::endl
              << print_fids(fid_set["m_mem_sz_diff"])
              << "Disk/reference checksum missmatch[d_cx_diff]:  "
              << statistics["d_cx_diff"] << std::endl
              << print_fids(fid_set["d_cx_diff"])
              << "MGM/reference checksum missmatch[m_cx_diff]:   "
              << statistics["m_cx_diff"] << std::endl
              << print_fids(fid_set["m_cx_diff"])
              << "Num. of orphans[orphans_n]:                    "
              << statistics["orphans_n"] << std::endl
              << print_fids(fid_set["orphans_n"])
              << "Num. of unregistered replicas[unreg_n]:        "
              << statistics["unreg_n"] << std::endl
              << print_fids(fid_set["unreg_n"])
              << "Files with num. replica missmatch[rep_diff_n]: "
              << statistics["rep_diff_n"] << std::endl
              << print_fids(fid_set["rep_diff_n"])
              << "Files missing on disk[rep_missing_n]:          "
              << statistics["rep_missing_n"] << std::endl
              << print_fids(fid_set["rep_diff_n"]) << std::endl;
  } else {
    std::cout << "Num. entries in DB[mem_n]:                     "
              << statistics["mem_n"] << std::endl
              << "Num. files synced from disk[d_sync_n]:         "
              << statistics["d_sync_n"] << std::endl
              << "Num, files synced from MGM[m_sync_n]:          "
              << statistics["m_sync_n"] << std::endl
              << "Disk/referece size missmatch[d_mem_sz_diff]:   "
              << statistics["d_mem_sz_diff"] << std::endl
              << "MGM/reference size missmatch[m_mem_sz_diff]:   "
              << statistics["m_mem_sz_diff"] << std::endl
              << "Disk/reference checksum missmatch[d_cx_diff]:  "
              << statistics["d_cx_diff"] << std::endl
              << "MGM/reference checksum missmatch[m_cx_diff]:   "
              << statistics["m_cx_diff"] << std::endl
              << "Num. of orphans[orphans_n]:                    "
              << statistics["orphans_n"] << std::endl
              << "Num. of unregistered replicas[unreg_n]:        "
              << statistics["unreg_n"] << std::endl
              << "Files with num. replica missmatch[rep_diff_n]: "
              << statistics["rep_diff_n"] << std::endl
              << "Files missing on disk[rep_missing_n]:          "
              << statistics["rep_missing_n"] << std::endl;
  }
}

int main(int argc, char* argv[])
{
  int retc = 0;
  int c;
  int long_index = 0;
  bool count_entries = false;
  bool dump_entry_ids = false;
  bool dump_fsck = false;
  bool verbose_fsck = false;
  std::string dbpath, sfid;
  extern char* optarg;
  static struct option long_options[] = {
    {"dbpath",           required_argument, 0,   0 },
    {"fid",              required_argument, 0,   0 },
    {"count",            no_argument,       0,  'c'},
    {"dump_ids",         no_argument,       0,  'e'},
    {"fsck",             no_argument,       0,  'f'},
    {"verbose_fsck",     no_argument,       0,  'v'},
    {0,                  0,                 0,   0 }
  };

  while ((c = getopt_long(argc, argv, "", long_options, &long_index)) != -1) {
    switch (c) {
    case 0:
      if (strcmp(long_options[long_index].name, "dbpath") == 0) {
        dbpath = optarg;
      } else if (strcmp(long_options[long_index].name, "fid") == 0) {
        sfid = optarg;
      }

      break;

    case 'c':
      count_entries = true;
      break;

    case 'e':
      dump_entry_ids = true;
      break;

    case 'f':
      dump_fsck = true;
      break;

    case 'v':
      dump_fsck = true;
      verbose_fsck = true;
      break;

    default:
      print_usage(argv[0]);
      return -1;
    }
  }

  eos::common::DbMap db;
  eos::common::LvDbDbMapInterface::Option options;
  options.CacheSizeMb = 0;
  options.BloomFilterNbits = 0;

  if (!db.attachDb(dbpath, false, 0, &options)) {
    std::cerr << "error: failed to attach db: " << dbpath << std::endl;
    return -1;
  } else {
    db.outOfCore(true);
  }

  // Display the number of entries in the DB
  if (count_entries) {
    std::cout << "info: " << db.size() << " entries in the DB" << std::endl;
    db.detachDb();
    return 0;
  }

  // Dump the list of all fids
  if (dump_entry_ids) {
    DumpAllFids(db);
  }

  // Display file info stored in the local database
  if (!sfid.empty()) {
    if (DumpFileInfo(db, sfid) == false) {
      retc = -1;
    }
  }

  // Dispaly fsck inconsistencies from the local database
  if (dump_fsck) {
    DumpFsckStats(db, verbose_fsck);
  }

  db.detachDb();
  return retc;
}
