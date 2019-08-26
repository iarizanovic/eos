// ----------------------------------------------------------------------
// File: GrpcNsInterface.cc
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

/*----------------------------------------------------------------------------*/
#include "GrpcNsInterface.hh"
/*----------------------------------------------------------------------------*/
#include "common/LayoutId.hh"
#include "common/SymKeys.hh"
#include "common/Path.hh"
#include "mgm/Acl.hh"
#include "mgm/proc/IProcCommand.hh"
#include "mgm/proc/user/RmCmd.hh"
#include "mgm/XrdMgmOfs.hh"
#include "namespace/Prefetcher.hh"
#include "namespace/MDException.hh"
#include "namespace/interface/ContainerIterators.hh"

#include <regex.h>
/*----------------------------------------------------------------------------*/


EOSMGMNAMESPACE_BEGIN

#ifdef EOS_GRPC

bool
GrpcNsInterface::Filter(std::shared_ptr<eos::IFileMD> md,
		   const eos::rpc::MDSelection& filter)
{
  errno = 0;
  // see if filtering is enabled
  if (!filter.select())
    return false;

  eos::IFileMD::ctime_t ctime;
  eos::IFileMD::ctime_t mtime;
  md->getCTime(ctime);
  md->getMTime(mtime);  

  // empty file
  if (filter.size().zero()) {
    if (!md->getSize()) {
      // accepted
    } else {
      return true;
    }
  } else {
    if ( (filter.size().min() <= md->getSize()) && 
	 ( (md->getSize() <= filter.size().max()) || (!filter.size().max())) ) {
      // accepted
    } else {
      return true;
    }
  }

  if (filter.ctime().zero()) {
    if (!ctime.tv_sec && !ctime.tv_nsec) {
      // accepted
    } else {
      return true;
    }
  } else {
    if ( (filter.ctime().min() <= (uint64_t)(ctime.tv_sec)) &&
	 ( (filter.ctime().max() >= (uint64_t)(ctime.tv_sec)) || (!filter.ctime().max()) ) ) {
      // accepted
    } else {
      return true;
    }
  }

  if (filter.mtime().zero()) {
    if (!mtime.tv_sec && !mtime.tv_nsec) {
      // accepted
    } else {
      return true;
    }
  } else {
    if ( (filter.mtime().min() <= (uint64_t)(mtime.tv_sec)) &&
	 ( (filter.mtime().max() >= (uint64_t)(mtime.tv_sec)) || (!filter.mtime().max()) ) ) {
      // accepted
    } else {
      return true;
    }
  }

  if (filter.mtime().zero()) {
    if (!mtime.tv_sec && !mtime.tv_nsec) {
      // accepted
    } else {
      return true;
    }
  } else {
    if ( (filter.mtime().min() <= (uint64_t)(mtime.tv_sec)) &&
	 ( (filter.mtime().max() >= (uint64_t)(mtime.tv_sec)) || (!filter.mtime().max()) ) ) {
      // accepted
    } else {
      return true;
    }
  }

  if (filter.locations().zero()) {
    if (!mtime.tv_sec && !mtime.tv_nsec) {
      // accepted
    } else {
      return true;
    }
  } else {
    if ( (filter.locations().min() <= (uint64_t)(mtime.tv_sec)) &&
	 ( (filter.locations().max() >= (uint64_t)(mtime.tv_sec)) || (!filter.locations().max()) ) ) {
      // accepted
    } else {
      return true;
    }
  }

  if (filter.owner_root()) {
    if (!md->getCUid()) {
      // accepted
    } else {
      return true;
    }
  } else {
    if (filter.owner()) {
      if (filter.owner() == md->getCUid()) {
	// accepted
      } else {
	return true;
      }
    }
  }

  if (filter.group_root()) {
    if (!md->getCGid()) {
      // accepted
    } else {
      return true;
    }
  } else {
    if (filter.group()) {
      if (filter.group() == md->getCGid()) {
	// accepted
      } else {
	return true;
      }
    }
  } 

  if (filter.layoutid()) {
    if (md->getLayoutId() != filter.layoutid()) {
      return true;
    }
  }

  if (filter.flags()) {
    if (md->getFlags() != filter.flags()) {
      return true;
    }
  }

  if (filter.symlink()) {
    if (!md->isLink()) {
      return true;
    }
  }

  if (filter.checksum().type().length()) {
    if (filter.checksum().type() !=  eos::common::LayoutId::GetChecksumStringReal(md->getLayoutId())) {
      return true;
    }
  }

  if (filter.checksum().value().length()) {
    std::string cks(md->getChecksum().getDataPtr(), md->getChecksum().size());
    if (filter.checksum().value() != cks) {
      return true;
    }
  }

  eos::IFileMD::XAttrMap xattr = md->getAttributes();
  for (const auto& elem : filter.xattr()) {
    if (xattr.count(elem.first)) {
      if (elem.second.length()) {
	if (xattr[elem.first] == elem.second) {
	  // accepted
	} else {
	  return true;
	}
      } else {
        // accepted
      }
    } else {
      return true;
    }
  }

  if (filter.regexp_filename().length()) {
    int regexErrorCode;
    int result;
    regex_t regex;
    std::string regexString = filter.regexp_filename();

    regexErrorCode = regcomp(&regex, regexString.c_str(), REG_EXTENDED);

    if (regexErrorCode) {
      regfree(&regex);
      errno = EINVAL;
      return true;
    }

    result = regexec(&regex, md->getName().c_str(), 0, NULL, 0);
    regfree(&regex);
    if (result == 0) {
      // accepted
    } else if (result == REG_NOMATCH) {
      return true;
    } else {
      errno = ENOMEM;
      return true;
    }
  }
  return false;;
}

bool 
GrpcNsInterface::Filter(std::shared_ptr<eos::IContainerMD> md,
			const eos::rpc::MDSelection& filter)

{    
  errno = 0;
  // see if filtering is enabled
  if (!filter.select())
    return false;

  eos::IContainerMD::ctime_t ctime;
  eos::IContainerMD::ctime_t mtime;
  eos::IContainerMD::ctime_t stime;
  md->getCTime(ctime);
  md->getMTime(mtime);
  md->getTMTime(stime);

  size_t nchildren = md->getNumContainers() + md->getNumFiles();
  uint64_t treesize = md->getTreeSize();

  // empty file
  if (filter.children().zero()) {
    if (!nchildren) {
      // accepted
    } else {
      return true;
    }
  } else {
    if ( (filter.children().min() <= nchildren) && 
	 ((nchildren <= filter.children().max()) || (!filter.children().max())) ) {
      // accepted
    } else {
      return true;
    }
  }

  if (filter.treesize().zero()) {
    if (!treesize) {
      // accepted
    } else {
      return true;
    }
  } else {
    if ( (filter.treesize().min() <= treesize) && 
	 ((treesize <= filter.treesize().max()) || (!filter.treesize().max())) ) {
      // accepted
    } else {
      return true;
    }
  }

  if (filter.ctime().zero()) {
    if (!ctime.tv_sec && !ctime.tv_nsec) {
      // accepted
    } else {
      return true;
    }
  } else {
    if ( (filter.ctime().min() <= (uint64_t)(ctime.tv_sec)) &&
	 ( (filter.ctime().max() >= (uint64_t)(ctime.tv_sec)) || (!filter.ctime().max()) ) ) {
      // accepted
    } else {
      return true;
    }
  }

  if (filter.mtime().zero()) {
    if (!mtime.tv_sec && !mtime.tv_nsec) {
      // accepted
    } else {
      return true;
    }
  } else {
    if ( (filter.mtime().min() <= (uint64_t)(mtime.tv_sec)) &&
	 ( (filter.mtime().max() >= (uint64_t)(mtime.tv_sec)) || (!filter.mtime().max()) ) ) {
      // accepted
    } else {
      return true;
    }
  }

  if (filter.stime().zero()) {
    if (!stime.tv_sec && !stime.tv_nsec) {
      // accepted
    } else {
      return true;
    }
  } else {
    if ( (filter.stime().min() <= (uint64_t)(stime.tv_sec)) &&
	 ( (filter.stime().max() >= (uint64_t)(stime.tv_sec)) || (!filter.stime().max()) ) ) {
      // accepted
    } else {
      return true;
    }
  }

  if (filter.owner_root()) {
    if (!md->getCUid()) {
      // accepted
    } else {
      return true;
    }
  } else {
    if (filter.owner()) {
      if (filter.owner() == md->getCUid()) {
	// accepted
      } else {
	return true;
      }
    }
  }

  if (filter.group_root()) {
    if (!md->getCGid()) {
      // accepted
    } else {
      return true;
    }
  } else {
    if (filter.group()) {
      if (filter.group() == md->getCGid()) {
	// accepted
      } else {
	return true;
      }
    }
  } 

  if (filter.flags()) {
    if (md->getFlags() != filter.flags()) {
      return true;
    }
  }

  eos::IContainerMD::XAttrMap xattr = md->getAttributes();
  for (const auto& elem : filter.xattr()) {
    if (xattr.count(elem.first)) {
      if (elem.second.length()) {
	if (xattr[elem.first] == elem.second) {
	  // accepted
	} else {
	  return true;
	}
      } else {
        // accepted
      }
    } else {
      return true;
    }
  }

  if (filter.regexp_dirname().length()) {
    int regexErrorCode;
    int result;
    regex_t regex;
    std::string regexString = filter.regexp_dirname();

    regexErrorCode = regcomp(&regex, regexString.c_str(), REG_EXTENDED);

    if (regexErrorCode) {
      regfree(&regex);
      errno = EINVAL;
      return true;
    }

    result = regexec(&regex, md->getName().c_str(), 0, NULL, 0);
    regfree(&regex);
    if (result == 0) {
      // accepted
    } else if (result == REG_NOMATCH) {
      return true;
    } else {
      errno = ENOMEM;
      return true;
    }
  }
  return false;
}




grpc::Status
GrpcNsInterface::GetMD(eos::common::VirtualIdentity& vid,
                       grpc::ServerWriter<eos::rpc::MDResponse>* writer,
                       const eos::rpc::MDRequest* request, bool check_perms,
		       bool lock)
{
  if (request->type() == eos::rpc::FILE) {
    // stream file meta data
    eos::common::RWMutexReadLock viewReadLock;
    std::shared_ptr<eos::IFileMD> fmd;
    std::shared_ptr<eos::IContainerMD> pmd;
    unsigned long fid = 0;
    uint64_t clock = 0;
    std::string path;

    if (request->id().ino()) {
      // get by inode
      fid = eos::common::FileId::InodeToFid(request->id().ino());
    } else if (request->id().id()) {
      // get by fileid
      fid = request->id().id();
    }

    if (fid) {
      eos::Prefetcher::prefetchFileMDAndWait(gOFS->eosView, fid);
    } else {
      eos::Prefetcher::prefetchFileMDAndWait(gOFS->eosView, request->id().path());
    }

    if (lock) {
    viewReadLock.Grab(gOFS->eosViewRWMutex);
    }

    if (fid) {
      try {
        fmd = gOFS->eosFileService->getFileMD(fid, &clock);
        path = gOFS->eosView->getUri(fmd.get());

        if (check_perms) {
          pmd = gOFS->eosDirectoryService->getContainerMD(fmd->getContainerId());
        }
      } catch (eos::MDException& e) {
        errno = e.getErrno();
        eos_static_debug("caught exception %d %s\n", e.getErrno(),
                         e.getMessage().str().c_str());
        return grpc::Status((grpc::StatusCode)(errno), e.getMessage().str().c_str());
      }
    } else {
      try {
        fmd = gOFS->eosView->getFile(request->id().path());
        path = gOFS->eosView->getUri(fmd.get());

        if (check_perms) {
          pmd = gOFS->eosDirectoryService->getContainerMD(fmd->getContainerId());
        }
      } catch (eos::MDException& e) {
        errno = e.getErrno();
        eos_static_debug("caught exception %d %s\n", e.getErrno(),
                         e.getMessage().str().c_str());
        return grpc::Status((grpc::StatusCode)(errno), e.getMessage().str().c_str());
      }
    }

    if (check_perms && !Access(vid, R_OK, pmd)) {
      return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                          "access to parent container denied");
    }

    if (Filter(fmd, request->selection())) {
      // short-cut for filtered MD
      return grpc::Status::OK;
    }
    
    // create GRPC protobuf object
    eos::rpc::MDResponse gRPCResponse;
    gRPCResponse.set_type(eos::rpc::FILE);
    eos::rpc::FileMdProto gRPCFmd;
    gRPCResponse.mutable_fmd()->set_name(fmd->getName());
    gRPCResponse.mutable_fmd()->set_id(fmd->getId());
    gRPCResponse.mutable_fmd()->set_cont_id(fmd->getContainerId());
    gRPCResponse.mutable_fmd()->set_uid(fmd->getCUid());
    gRPCResponse.mutable_fmd()->set_gid(fmd->getCGid());
    gRPCResponse.mutable_fmd()->set_size(fmd->getSize());
    gRPCResponse.mutable_fmd()->set_layout_id(fmd->getLayoutId());
    gRPCResponse.mutable_fmd()->set_flags(fmd->getFlags());
    gRPCResponse.mutable_fmd()->set_link_name(fmd->getLink());
    eos::IFileMD::ctime_t ctime;
    eos::IFileMD::ctime_t mtime;
    fmd->getCTime(ctime);
    fmd->getMTime(mtime);
    gRPCResponse.mutable_fmd()->mutable_ctime()->set_sec(ctime.tv_sec);
    gRPCResponse.mutable_fmd()->mutable_ctime()->set_n_sec(ctime.tv_nsec);
    gRPCResponse.mutable_fmd()->mutable_mtime()->set_sec(mtime.tv_sec);
    gRPCResponse.mutable_fmd()->mutable_mtime()->set_n_sec(mtime.tv_nsec);
    gRPCResponse.mutable_fmd()->mutable_checksum()->set_value(
      fmd->getChecksum().getDataPtr(), fmd->getChecksum().size());
    gRPCResponse.mutable_fmd()->mutable_checksum()->set_type(
      eos::common::LayoutId::GetChecksumStringReal(fmd->getLayoutId()));

    for (const auto& loca : fmd->getLocations()) {
      gRPCResponse.mutable_fmd()->add_locations(loca);
    }

    for (const auto& loca : fmd->getUnlinkedLocations()) {
      gRPCResponse.mutable_fmd()->add_unlink_locations(loca);
    }

    for (const auto& elem : fmd->getAttributes()) {
      (*gRPCResponse.mutable_fmd()->mutable_xattrs())[elem.first] = elem.second;
    }

    gRPCResponse.mutable_fmd()->set_path(path);
    writer->Write(gRPCResponse);
    return grpc::Status::OK;
  } else if (request->type() == eos::rpc::CONTAINER) {
    // stream container meta data
    eos::common::RWMutexReadLock viewReadLock;
    std::shared_ptr<eos::IContainerMD> cmd;
    std::shared_ptr<eos::IContainerMD> pmd;
    unsigned long cid = 0;
    uint64_t clock = 0;
    std::string path;

    if (request->id().ino()) {
      // get by inode
      cid = request->id().ino();
    } else if (request->id().id()) {
      // get by containerid
      cid = request->id().id();
    }

    if (!cid) {
      eos::Prefetcher::prefetchContainerMDAndWait(gOFS->eosView,
          request->id().path());
    } else {
      eos::Prefetcher::prefetchContainerMDAndWait(gOFS->eosView,
					     cid);
    }

    if (lock) {
      viewReadLock.Grab(gOFS->eosViewRWMutex);
    }

    if (cid) {
      try {
        cmd = gOFS->eosDirectoryService->getContainerMD(cid, &clock);
        path = gOFS->eosView->getUri(cmd.get());
        pmd = gOFS->eosDirectoryService->getContainerMD(cmd->getParentId());
      } catch (eos::MDException& e) {
        errno = e.getErrno();
        eos_static_debug("caught exception %d %s\n", e.getErrno(),
                         e.getMessage().str().c_str());
        return grpc::Status((grpc::StatusCode)(errno), e.getMessage().str().c_str());
      }
    } else {
      try {
        cmd = gOFS->eosView->getContainer(request->id().path());
        path = gOFS->eosView->getUri(cmd.get());
        pmd = gOFS->eosDirectoryService->getContainerMD(cmd->getParentId());
      } catch (eos::MDException& e) {
        errno = e.getErrno();
        eos_static_debug("caught exception %d %s\n", e.getErrno(),
                         e.getMessage().str().c_str());
        return grpc::Status((grpc::StatusCode)(errno), e.getMessage().str().c_str());
      }
    }

    if (!Access(vid, R_OK, pmd)) {
      return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
                          "access to parent container denied");
    }

    if (Filter(cmd, request->selection())) {
      // short-cut for filtered MD
      return grpc::Status::OK;
    }

    // create GRPC protobuf object
    eos::rpc::MDResponse gRPCResponse;
    gRPCResponse.set_type(eos::rpc::CONTAINER);
    eos::rpc::ContainerMdProto gRPCFmd;
    gRPCResponse.mutable_cmd()->set_name(cmd->getName());
    gRPCResponse.mutable_cmd()->set_id(cmd->getId());
    gRPCResponse.mutable_cmd()->set_parent_id(cmd->getParentId());
    gRPCResponse.mutable_cmd()->set_uid(cmd->getCUid());
    gRPCResponse.mutable_cmd()->set_gid(cmd->getCGid());
    gRPCResponse.mutable_cmd()->set_tree_size(cmd->getTreeSize());
    gRPCResponse.mutable_cmd()->set_flags(cmd->getFlags());
    eos::IContainerMD::ctime_t ctime;
    eos::IContainerMD::ctime_t mtime;
    eos::IContainerMD::ctime_t stime;
    cmd->getCTime(ctime);
    cmd->getMTime(mtime);
    cmd->getTMTime(stime);
    gRPCResponse.mutable_cmd()->mutable_ctime()->set_sec(ctime.tv_sec);
    gRPCResponse.mutable_cmd()->mutable_ctime()->set_n_sec(ctime.tv_nsec);
    gRPCResponse.mutable_cmd()->mutable_mtime()->set_sec(mtime.tv_sec);
    gRPCResponse.mutable_cmd()->mutable_mtime()->set_n_sec(mtime.tv_nsec);
    gRPCResponse.mutable_cmd()->mutable_stime()->set_sec(stime.tv_sec);
    gRPCResponse.mutable_cmd()->mutable_stime()->set_n_sec(stime.tv_nsec);

    for (const auto& elem : cmd->getAttributes()) {
      (*gRPCResponse.mutable_cmd()->mutable_xattrs())[elem.first] = elem.second;
    }

    gRPCResponse.mutable_cmd()->set_path(path);
    writer->Write(gRPCResponse);
    return grpc::Status::OK;
  }

  return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "invalid argument");
}

grpc::Status
GrpcNsInterface::StreamMD(eos::common::VirtualIdentity& ivid, 
                         grpc::ServerWriter<eos::rpc::MDResponse>* writer,
                          const eos::rpc::MDRequest* request,
			  bool streamparent,
			  std::vector<uint64_t>* childdirs)
{
  eos::common::VirtualIdentity vid = ivid;

  if (request->role().uid() || request->role().gid()) {
    if ((ivid.uid != request->role().uid()) || (ivid.gid != request->role().gid())) {
      if (!ivid.sudoer) {
	return grpc::Status(grpc::StatusCode::PERMISSION_DENIED,
			    std::string("Ask an admin to map your auth key to a sudo'er account - permission denied"));
      } else {
	vid = eos::common::Mapping::Someone(request->role().uid(),
					    request->role().gid());

      }
    }
  } else {
    // we don't implement sudo to root
  }

  // stream container meta data
  eos::common::RWMutexReadLock viewReadLock;
  std::shared_ptr<eos::IContainerMD> cmd;
  unsigned long cid = 0;
  uint64_t clock = 0;
  std::string path;

  if (request->id().ino()) {
    // get by inode
    cid = request->id().ino();
  } else if (request->id().id()) {
    // get by containerid
    cid = request->id().id();
  }

  if (!cid) {
    eos::Prefetcher::prefetchContainerMDWithChildrenAndWait(gOFS->eosView,
        request->id().path());
  } else {
    eos::Prefetcher::prefetchContainerMDWithChildrenAndWait(gOFS->eosView,
        cid);
  }

  viewReadLock.Grab(gOFS->eosViewRWMutex);

  if (cid) {
    try {
      cmd = gOFS->eosDirectoryService->getContainerMD(cid, &clock);
      path = gOFS->eosView->getUri(cmd.get());
    } catch (eos::MDException& e) {
      errno = e.getErrno();
      eos_static_debug("caught exception %d %s\n", e.getErrno(),
                       e.getMessage().str().c_str());
      return grpc::Status((grpc::StatusCode)(errno), e.getMessage().str().c_str());
    }
  } else {
    try {
      cmd = gOFS->eosView->getContainer(request->id().path());
      cid = cmd->getId();
      path = gOFS->eosView->getUri(cmd.get());
    } catch (eos::MDException& e) {
      errno = e.getErrno();
      eos_static_debug("caught exception %d %s\n", e.getErrno(),
                       e.getMessage().str().c_str());
      return grpc::Status((grpc::StatusCode)(errno), e.getMessage().str().c_str());
    }
  }

  grpc::Status status;

  if (streamparent && (request->type() != eos::rpc::FILE)) {
    // stream the requested container
    eos::rpc::MDRequest c_dir;
    c_dir.mutable_selection()->CopyFrom(request->selection());
    c_dir.mutable_id()->set_id(cid);
    c_dir.set_type(eos::rpc::CONTAINER);
    status = GetMD(vid, writer, &c_dir, true, false);

    if (!status.ok()) {
      return status;
    }
  }

  bool first = true;

  // stream for listing and file type
  if (request->type() != eos::rpc::CONTAINER) {
    // stream all the children files
    for (auto it = eos::FileMapIterator(cmd); it.valid(); it.next()) {
      eos::rpc::MDRequest c_file;
      c_file.mutable_selection()->CopyFrom(request->selection());
      c_file.mutable_id()->set_id(it.value());
      c_file.set_type(eos::rpc::FILE);
      status = GetMD(vid, writer, &c_file, first);

      if (!status.ok()) {
	return status;
      }

      first = false;
    }
  }

  // stream all the children container
  for (auto it = eos::ContainerMapIterator(cmd); it.valid(); it.next()) {
    if (request->type() != eos::rpc::FILE) {
      // stream for listing and container type
      eos::rpc::MDRequest c_dir;
      c_dir.mutable_id()->set_id(it.value());
      c_dir.mutable_selection()->CopyFrom(request->selection());
      c_dir.set_type(eos::rpc::CONTAINER);
      status = GetMD(vid, writer, &c_dir, first);

      if (!status.ok()) {
	return status;
      }
    }

    if (childdirs) {
      childdirs->push_back(it.value());
    }
    first = false;
  }

  // finished streaming
  return grpc::Status::OK;
}

grpc::Status
GrpcNsInterface::Find(eos::common::VirtualIdentity& vid,
		      grpc::ServerWriter<eos::rpc::MDResponse>* writer,
		      const eos::rpc::FindRequest* request)
{
  std::string path;

  std::vector< std::vector<uint64_t> > found_dirs;

  found_dirs.resize(1);
  found_dirs[0].resize(1);
  found_dirs[0][0] = 0;

  uint64_t deepness = 0;

  // find for a single directory
  if (request->maxdepth() == 0) {
    grpc::Status status = grpc::Status::OK;
    eos::rpc::MDRequest c_dir;
    *( c_dir.mutable_id() ) = request->id();
    if (request->type() != eos::rpc::FILE) {
      c_dir.mutable_selection()->CopyFrom(request->selection());
      c_dir.set_type(eos::rpc::CONTAINER);
      status = GetMD(vid, writer, &c_dir, true, false);
    }
    return status;
  }

  // find for multiple directories/files

  do {
    bool streamparent = false;
    found_dirs.resize(deepness + 2);

    // loop over all directories in that deepness
    for (unsigned int i = 0; i < found_dirs[deepness].size(); i++) {
      uint64_t id = found_dirs[deepness][i];
      eos::rpc::MDRequest lrequest;
      if ( (deepness == 0 ) && (id == 0 ) ) {
	// that is the root of a find
	*(lrequest.mutable_id()) = request->id();
	eos_static_warning("%s %llu %llu", lrequest.id().path().c_str(),
			   lrequest.id().id(),
			   lrequest.id().ino());
	streamparent = true;
      } else {
	lrequest.mutable_id()->set_id(id);
	streamparent = false;
      }

      lrequest.set_type(request->type());
      lrequest.mutable_selection()->CopyFrom(request->selection());
      *(lrequest.mutable_role()) = request->role();
      std::vector<uint64_t> children;
      grpc::Status status = StreamMD(vid, writer, &lrequest, streamparent, &children);
      if (!status.ok()) {
	return status;
      }
      for (auto const& value: children) {
	// stream dirs under path into cpath
	found_dirs[deepness + 1].push_back(value);
      }
    }

    deepness++;
    if (deepness >= request->maxdepth()) {
      break;
    }
  } while (found_dirs[deepness].size());

  return grpc::Status::OK;
}

bool
GrpcNsInterface::Access(eos::common::VirtualIdentity& vid, int mode,
                        std::shared_ptr<eos::IContainerMD> cmd)
{
  // UNIX permissions
  if (cmd->access(vid.uid, vid.gid, mode)) {
    return true;
  }

  // ACLs - WARNING: this does not support ACLs to be linked attributes !
  eos::IContainerMD::XAttrMap xattr = cmd->getAttributes();
  eos::mgm::Acl acl(xattr, vid);

  // check for immutable
  if (vid.uid && !acl.IsMutable() && (mode & W_OK)) {
    return false;
  }

  bool permok = false;

  if (acl.HasAcl()) {
    permok = true;

    if ((mode & W_OK) && (!acl.CanWrite())) {
      permok = false;
    }

    if (mode & R_OK) {
      if (!acl.CanRead()) {
        permok = false;
      }
    }

    if (mode & X_OK) {
      if ((!acl.CanBrowse())) {
        permok = false;
      }
    }
  }

  return permok;
}

grpc::Status
GrpcNsInterface::FileInsert(eos::common::VirtualIdentity& vid,
                            eos::rpc::InsertReply* reply,
                            const eos::rpc::FileInsertRequest* request)
{
  if (!vid.sudoer) {
    // block every one who is not a sudoer
    reply->add_message("Not a sudoer, refusing to run command");
    reply->add_retc(EPERM);
    return grpc::Status::OK;
  }

  std::shared_ptr<eos::IFileMD> newfile;
  eos::common::RWMutexWriteLock lock(gOFS->eosViewRWMutex);
  std::vector<folly::Future<eos::IFileMDPtr>> conflicts;

  for (auto it : request->files()) {
    if (it.id() <= 0) {
      conflicts.emplace_back(eos::IFileMDPtr(
                               nullptr)); // folly::makeFuture<eos::IFileMDPtr>(eos::IFileMDPtr(nullptr)));
    } else {
      conflicts.emplace_back(gOFS->eosFileService->getFileMDFut(it.id()));
    }
  }

  int counter = -1;

  for (auto it : request->files()) {
    counter++;
    conflicts[counter].wait();

    if (!conflicts[counter].hasException() && conflicts[counter].get() != nullptr) {
      std::ostringstream ss;
      ss << "Attempted to create file with id=" << it.id() << ", which already exists";

      eos_static_err("%s", ss.str().c_str());
      reply->add_message(ss.str());
      reply->add_retc(EINVAL);
      continue;
    }

    eos_static_info("creating path=%s id=%lx", it.path().c_str(), it.id());

    try {
      try {
        newfile = gOFS->eosView->createFile(it.path(), it.uid(), it.gid(), it.id());
      } catch (eos::MDException& e) {
        std::ostringstream msg;
        msg << "Failed to call gOFS->eosView->createFile(): " << e.getMessage().str();
        e.getMessage().str(msg.str());
        throw;
      }
      eos::IFileMD::ctime_t ctime;
      eos::IFileMD::ctime_t mtime;
      ctime.tv_sec  = it.ctime().sec();
      ctime.tv_nsec = it.ctime().n_sec();
      mtime.tv_sec  = it.mtime().sec();
      mtime.tv_nsec = it.mtime().n_sec();
      newfile->setFlags(it.flags());
      newfile->setCTime(ctime);
      newfile->setMTime(mtime);
      newfile->setCUid(it.uid());
      newfile->setCGid(it.gid());
      newfile->setLayoutId(it.layout_id());
      newfile->setSize(it.size());
      newfile->setChecksum(it.checksum().value().c_str(),
                           it.checksum().value().size());

      for (auto attrit : it.xattrs()) {
        newfile->setAttribute(attrit.first, attrit.second);
      }

      for (auto locit : it.locations()) {
        newfile->addLocation(locit);
      }

      try {
        gOFS->eosView->updateFileStore(newfile.get());
      } catch (eos::MDException& e) {
        std::ostringstream msg;
        msg << "Failed to call gOFS->eosView->updateFileStore(): " << e.getMessage().str();
        e.getMessage().str(msg.str());
        throw;
      }

      reply->add_message("");
      reply->add_retc(0);
    } catch (eos::MDException& e) {
      eos_static_err("msg=\"exception\" ec=%d emsg=\"%s\" path=\"%s\" fxid=%08llx\n",
                     e.getErrno(), e.getMessage().str().c_str(), it.path().c_str(), it.id());
      reply->add_message(SSTR("Failed to insert fid=" << it.id() << ", errno=" << e.getErrno() << ", path=" << it.path() << ": " << e.getMessage().str()));
      reply->add_retc(-1);
    }
  }

  return grpc::Status::OK;
}


grpc::Status
GrpcNsInterface::ContainerInsert(eos::common::VirtualIdentity& vid,
                                 eos::rpc::InsertReply* reply,
                                 const eos::rpc::ContainerInsertRequest* request)
{
  if (!vid.sudoer) {
    // block every one who is not a sudoer
    reply->add_message("Not a sudoer, refusing to run command");
    reply->add_retc(EPERM);
    return grpc::Status::OK;
  }

  std::shared_ptr<eos::IContainerMD> newdir;
  eos::common::RWMutexWriteLock lock(gOFS->eosViewRWMutex);
  std::vector<folly::Future<eos::IContainerMDPtr>> conflicts;

  for (auto it : request->container()) {
    if (it.id() <= 0) {
      conflicts.emplace_back(eos::IContainerMDPtr(nullptr));
    } else {
      conflicts.emplace_back(gOFS->eosDirectoryService->getContainerMDFut(it.id()));
    }
  }

  int counter = -1;
  bool inherit = request->inherit_md();

  for (auto it : request->container()) {
    counter++;
    conflicts[counter].wait();

    if (!conflicts[counter].hasException() && conflicts[counter].get() != nullptr) {
      std::ostringstream ss;
      ss << "Attempted to create container with id=" << it.id() << ", which already exists";
      eos_static_err("%s", ss.str().c_str());
      reply->add_message(ss.str());
      reply->add_retc(EINVAL);
      continue;
    }

    eos_static_info("creating path=%s id=%lx inherit_md=%d",
                    it.path().c_str(), it.id(), inherit);

    try {
      try {
        newdir = gOFS->eosView->createContainer(it.path(), false, it.id());
      } catch (eos::MDException& e) {
        std::ostringstream msg;
        msg << "Failed to call gOFS->eosView->createContainer(): " << e.getMessage().str();
        e.getMessage().str(msg.str());
        throw;
      }

      eos::IContainerMD::ctime_t ctime;
      eos::IContainerMD::ctime_t mtime;
      eos::IContainerMD::ctime_t stime;
      ctime.tv_sec  = it.ctime().sec();
      ctime.tv_nsec = it.ctime().n_sec();
      mtime.tv_sec  = it.mtime().sec();
      mtime.tv_nsec = it.mtime().n_sec();
      stime.tv_sec  = it.stime().sec();
      stime.tv_nsec = it.stime().n_sec();
      // we can send flags or mode to store in flags ... sigh
      newdir->setFlags(it.flags());
      newdir->setCTime(ctime);
      newdir->setMTime(mtime);
      newdir->setTMTime(stime);
      newdir->setCUid(it.uid());
      newdir->setCGid(it.gid());
      newdir->setMode(it.mode() | S_IFDIR);

      std::shared_ptr<eos::IContainerMD> parent;

      if (inherit) {
        eos::common::Path cPath(it.path());

        try {
          parent = gOFS->eosView->getContainer(cPath.GetParentPath());
        } catch (eos::MDException& e) {
          std::ostringstream msg;
          msg << "Failed to call parent gOFS->eosView->getContainer(): " << e.getMessage().str();
          e.getMessage().str(msg.str());
          throw;
        }

        if (it.mode() == 0) {
          newdir->setMode(parent->getMode());
        }

        for (const auto& attrit: parent->getAttributes()) {
          newdir->setAttribute(attrit.first, attrit.second);
        }
      }

      newdir->setAttribute("sys.eos.btime",
                           SSTR(ctime.tv_sec << "." << ctime.tv_nsec));

      for (auto attrit : it.xattrs()) {
        newdir->setAttribute(attrit.first, attrit.second);
      }

      try {
        gOFS->eosView->updateContainerStore(newdir.get());

        if (parent) {
          parent->setMTime(ctime);
          parent->notifyMTimeChange(gOFS->eosDirectoryService);
          gOFS->eosView->updateContainerStore(parent.get());
        }
      } catch (eos::MDException& e) {
        std::ostringstream msg;
        msg << "Failed to call gOFS->eosView->updateContainerStore(): " << e.getMessage().str();
        e.getMessage().str(msg.str());
        throw;
      }

      reply->add_message("");
      reply->add_retc(0);
    } catch (eos::MDException& e) {
      eos_static_err("msg=\"exception\" ec=%d emsg=\"%s\" path=\"%s\" fxid=%08llx\n",
                     e.getErrno(), e.getMessage().str().c_str(), it.path().c_str(), it.id());
      reply->add_message(SSTR("Failed to insert cid=" << it.id() << ", errno=" << e.getErrno() << ", path=" << it.path() << ": " << e.getMessage().str()));
      reply->add_retc(e.getErrno());
    }
  }

  return grpc::Status::OK;
}

grpc::Status
GrpcNsInterface::Exec(eos::common::VirtualIdentity& ivid,
		      eos::rpc::NSResponse* reply,
		      const eos::rpc::NSRequest* request)
{
  eos::common::VirtualIdentity vid = ivid;

  if (request->role().uid() || request->role().gid()) {
    if ((ivid.uid != request->role().uid()) || (ivid.gid != request->role().gid())) {
      if (!ivid.sudoer) {
        reply->mutable_error()->set_code(EPERM);
        reply->mutable_error()->set_msg(
          "Ask an admin to map your auth key to a sudo'er account - permission denied");
        return grpc::Status::OK;
      } else {
        vid = eos::common::Mapping::Someone(request->role().uid(),
                                            request->role().gid());
      }
    }
  } else {
    // we don't implement sudo to root
  }

  switch(request->command_case()) {
  case eos::rpc::NSRequest::kMkdir:
    return Mkdir(vid, reply->mutable_error() ,&(request->mkdir()));
    break;
  case eos::rpc::NSRequest::kRmdir:
    return Rmdir(vid, reply->mutable_error() ,&(request->rmdir()));
    break;
  case eos::rpc::NSRequest::kTouch:
    return Touch(vid, reply->mutable_error() ,&(request->touch()));
  break;
  case eos::rpc::NSRequest::kUnlink:
    return Unlink(vid, reply->mutable_error() ,&(request->unlink()));
    break;
  case eos::rpc::NSRequest::kRm:
    return Rm(vid, reply->mutable_error() ,&(request->rm()));
    break;
  case eos::rpc::NSRequest::kRename:
    return Rename(vid, reply->mutable_error() ,&(request->rename()));
    break;
  case eos::rpc::NSRequest::kSymlink:
    return Symlink(vid, reply->mutable_error() ,&(request->symlink()));
    break;
  case eos::rpc::NSRequest::kXattr:
    return SetXAttr(vid, reply->mutable_error() ,&(request->xattr()));
    break;
  case eos::rpc::NSRequest::kVersion:
    return Version(vid, reply->mutable_version() ,&(request->version()));
    break;
  case eos::rpc::NSRequest::kRecycle:
    return Recycle(vid, reply->mutable_recycle() ,&(request->recycle()));
    break;
  case eos::rpc::NSRequest::kChown:
    return Chown(vid, reply->mutable_error() ,&(request->chown()));
    break;
  case eos::rpc::NSRequest::kChmod:
    return Chmod(vid, reply->mutable_error() ,&(request->chmod()));
    break;
  default:
    reply->mutable_error()->set_code(EINVAL);
    reply->mutable_error()->set_msg("error: command not supported");
    break;
  }
  return grpc::Status::OK;
}

grpc::Status
GrpcNsInterface::Mkdir(eos::common::VirtualIdentity& vid,
		       eos::rpc::NSResponse::ErrorResponse* reply,
		       const eos::rpc::NSRequest::MkdirRequest* request)
{
  mode_t mode = request->mode();

  if (request->recursive()){
    mode |= SFS_O_MKPTH;
  }

  std::string path;

  path = request->id().path();

  if (path.empty()) {
    reply->set_code(EINVAL);
    reply->set_msg("error:path is empty");
    return grpc::Status::OK;
  }

  XrdOucErrInfo error;
  errno = 0;
  if (gOFS->_mkdir(path.c_str(), mode, error, vid, (const char*) 0)) {
    reply->set_code(errno);
    reply->set_msg(error.getErrText());
    return grpc::Status::OK;
  }
  if (errno == EEXIST) {
    reply->set_code(EEXIST);
    std::string msg = "info: directory existed already '";
    msg += path.c_str();
    msg += "'";
    reply->set_msg(msg);
  } else {
    reply->set_code(0);
    std::string msg = "info: created directory '";
    msg += path.c_str();
    msg += "'";
    reply->set_msg(msg);
  }
  return grpc::Status::OK;
}

grpc::Status GrpcNsInterface::Rmdir(eos::common::VirtualIdentity& vid,
				    eos::rpc::NSResponse::ErrorResponse* reply,
				    const eos::rpc::NSRequest::RmdirRequest* request)
{
  std::string path;

  path = request->id().path();

  if (path.empty()) {
    try {
      eos::common::RWMutexReadLock vlock(gOFS->eosViewRWMutex);
      path =
        gOFS->eosView->getUri(gOFS->eosDirectoryService->getContainerMD(request->id().id()).get());
    } catch (eos::MDException& e) {
      path = "";
      errno = e.getErrno();
    }
  }

  if (path.empty()) {
    if (request->id().id()) {
      reply->set_code(ENOENT);
      reply->set_msg("error: directory id does not exist");
      return grpc::Status::OK;
    } else {
      reply->set_code(EINVAL);
      reply->set_msg("error: path is empty");
      return grpc::Status::OK;
    }
  }

  XrdOucErrInfo error;
  errno = 0;
  if (gOFS->_remdir(path.c_str(), error, vid, (const char*) 0)) {
    reply->set_code(errno);
    reply->set_msg(error.getErrText());
    return grpc::Status::OK;
  }

  reply->set_code(0);
  std::string msg = "info: deleted directory '";
  msg += path.c_str();
  msg += "'";
  reply->set_msg(msg);
  return grpc::Status::OK;
}

grpc::Status GrpcNsInterface::Touch(eos::common::VirtualIdentity& vid,
				    eos::rpc::NSResponse::ErrorResponse* reply,
				    const eos::rpc::NSRequest::TouchRequest* request)
{
  std::string path;

  path = request->id().path();

  if (path.empty()) {
    reply->set_code(EINVAL);
    reply->set_msg("error:path is empty");
    return grpc::Status::OK;
  }

  XrdOucErrInfo error;
  errno = 0;
  if (gOFS->_touch(path.c_str(), error, vid, (const char*) 0)) {
    reply->set_code(errno);
    reply->set_msg(error.getErrText());
    return grpc::Status::OK;
  }

  reply->set_code(0);
  std::string msg = "info: touched file '";
  msg += path.c_str();
  msg += "'";
  reply->set_msg(msg);
  return grpc::Status::OK;}


grpc::Status GrpcNsInterface::Unlink(eos::common::VirtualIdentity& vid,
				     eos::rpc::NSResponse::ErrorResponse* reply,
				     const eos::rpc::NSRequest::UnlinkRequest* request)
{
  bool norecycle = false;

  if (request->norecycle()){
    norecycle = true;
  }


  std::string path;

  path = request->id().path();

  if (path.empty()) {
    try {
      eos::common::RWMutexReadLock vlock(gOFS->eosViewRWMutex);
      path =
        gOFS->eosView->getUri(gOFS->eosDirectoryService->getContainerMD(request->id().id()).get());
    } catch (eos::MDException& e) {
      path = "";
      errno = e.getErrno();
    }
  }

  if (path.empty()) {
    if (request->id().id()) {
      reply->set_code(ENOENT);
      reply->set_msg("error: directory id does not exist");
      return grpc::Status::OK;
    } else {
      reply->set_code(EINVAL);
      reply->set_msg("error: path is empty");
      return grpc::Status::OK;
    }
  }

  XrdOucErrInfo error;
  errno = 0;
  if (gOFS->_rem(path.c_str(), error, vid, (const char*) 0, false, false, norecycle)) {
    reply->set_code(errno);
    reply->set_msg(error.getErrText());
    return grpc::Status::OK;
  }

  reply->set_code(0);
  std::string msg = "info: unlinked file '";
  msg += path.c_str();
  msg += "'";
  reply->set_msg(msg);
  return grpc::Status::OK;
}

grpc::Status GrpcNsInterface::Rm(eos::common::VirtualIdentity& vid,
				 eos::rpc::NSResponse::ErrorResponse* reply,
				 const eos::rpc::NSRequest::RmRequest* request)
{
  eos::console::RequestProto req;

  if (!request->id().path().empty()) {
    req.mutable_rm()->set_path(request->id().path());
  } else {
    if (request->id().type() == eos::rpc::FILE) {
      req.mutable_rm()->set_fileid(request->id().id());
    } else {
      req.mutable_rm()->set_containerid(request->id().id());
    }
  }

  if (request->recursive()) {
    req.mutable_rm()->set_recursive(true);
  }

  if (request->norecycle()) {
    req.mutable_rm()->set_bypassrecycle(true);
  }

  eos::mgm::RmCmd rmcmd(std::move(req), vid);

  eos::console::ReplyProto preply = rmcmd.ProcessRequest();

  if (preply.retc())
  {
    reply->set_code(preply.retc());
    reply->set_msg(preply.std_err());
    return grpc::Status::OK;
  }

  reply->set_code(0);
  std::string msg="info: ";
  msg += "deleted directory tree '";
  if (!request->id().path().empty()) {
    msg += request->id().path().c_str();
  } else {
    std::stringstream s;
    s << std::hex << request->id().id();
    msg += s.str().c_str();
  }

  reply->set_msg(msg);
  return grpc::Status::OK;
}

grpc::Status GrpcNsInterface::Rename(eos::common::VirtualIdentity& vid,
				     eos::rpc::NSResponse::ErrorResponse* reply,
				     const eos::rpc::NSRequest::RenameRequest* request)
{
  std::string path;
  std::string target;

  path = request->id().path();
  target = request->target();

  if (path.empty()) {
    reply->set_code(EINVAL);
    reply->set_msg("error:path is empty");
    return grpc::Status::OK;
  }

  if (target.empty()) {
    reply->set_code(EINVAL);
    reply->set_msg("error:target is empty");
    return grpc::Status::OK;
  }

  XrdOucErrInfo error;
  errno = 0;
  if (gOFS->_rename(
		    path.c_str(),
		    target.c_str(),
		    error,
		    vid)) {
    reply->set_code(errno);
    reply->set_msg(error.getErrText());
    return grpc::Status::OK;
  }

  reply->set_code(0);
  std::string msg = "info: renamed '";
  msg += path.c_str();
  msg += "' to '";
  msg +=target.c_str();
  msg += "'";

  reply->set_msg(msg);
  return grpc::Status::OK;
}

grpc::Status GrpcNsInterface::Symlink(eos::common::VirtualIdentity& vid,
				      eos::rpc::NSResponse::ErrorResponse* reply,
				      const eos::rpc::NSRequest::SymlinkRequest* request)
{
  std::string path;
  std::string target;

  path = request->id().path();
  target = request->target();

  if (path.empty()) {
    reply->set_code(EINVAL);
    reply->set_msg("error:path is empty");
    return grpc::Status::OK;
  }

  if (target.empty()) {
    reply->set_code(EINVAL);
    reply->set_msg("error:target is empty");
    return grpc::Status::OK;
  }

  XrdOucErrInfo error;
  errno = 0;
  if (gOFS->_symlink(
		    path.c_str(),
		    target.c_str(),
		    error,
		    vid)) {
    reply->set_code(errno);
    reply->set_msg(error.getErrText());
    return grpc::Status::OK;
  }

  reply->set_code(0);
  std::string msg = "info: symlinked '";
  msg += path.c_str();
  msg += "' to '";
  msg += target.c_str();
  msg += "'";

  reply->set_msg(msg);
  return grpc::Status::OK;
}

grpc::Status GrpcNsInterface::SetXAttr(eos::common::VirtualIdentity& vid,
				       eos::rpc::NSResponse::ErrorResponse* reply,
				       const eos::rpc::NSRequest::SetXAttrRequest* request)
{
  std::string path;

  path = request->id().path();

  if (path.empty()) {
    if (request->id().type() == eos::rpc::FILE) {
      try {
	eos::common::RWMutexReadLock vlock(gOFS->eosViewRWMutex);
	path =
	  gOFS->eosView->getUri(gOFS->eosFileService->getFileMD(request->id().id()).get());
      } catch (eos::MDException& e) {
	path = "";
	errno = e.getErrno();
      }
    } else {
      try {
	eos::common::RWMutexReadLock vlock(gOFS->eosViewRWMutex);
	path =
	  gOFS->eosView->getUri(gOFS->eosDirectoryService->getContainerMD(request->id().id()).get());
      } catch (eos::MDException& e) {
	path = "";
	errno = e.getErrno();
      }
    }

    if (path.empty()) {
      reply->set_code(EINVAL);
      reply->set_msg("error:path is empty");
      return grpc::Status::OK;
    }
  }

  XrdOucErrInfo error;
  errno = 0;

  for ( auto it = request->xattrs().begin(); it != request->xattrs().end(); ++it) {
    std::string key = it->first;
    std::string value = it->second;
    std::string b64value;
    eos::common::SymKey::Base64Encode(value.c_str(), value.length(), b64value);
    if (gOFS->_attr_set(path.c_str(), error, vid, (const char*) 0,key.c_str(), b64value.c_str())) {
      reply->set_code(errno);
      reply->set_msg(error.getErrText());
      return grpc::Status::OK;
    }
  }

  reply->set_code(0);
  std::string msg = "info: setxattr on '";
  msg += path.c_str();
  msg += "'";
  reply->set_msg(msg);
  return grpc::Status::OK;
}

grpc::Status GrpcNsInterface::Version(eos::common::VirtualIdentity& vid,
				      eos::rpc::NSResponse::VersionResponse* reply,
				      const eos::rpc::NSRequest::VersionRequest* request)
{
  reply->set_code(EINVAL);
  reply->set_msg("error: command is currently not supported");
  return grpc::Status::OK;
}

grpc::Status GrpcNsInterface::Recycle(eos::common::VirtualIdentity& vid,
				      eos::rpc::NSResponse::RecycleResponse* reply,
				      const eos::rpc::NSRequest::RecycleRequest* request)
{
  reply->set_code(EINVAL);
  reply->set_msg("error: command is currently not supported");
  return grpc::Status::OK;
}


grpc::Status
GrpcNsInterface::Chown(eos::common::VirtualIdentity& vid,
		       eos::rpc::NSResponse::ErrorResponse* reply,
		       const eos::rpc::NSRequest::ChownRequest* request)
{
  std::string path;

  path = request->id().path();

  if (path.empty()) {
    if (request->id().type() == eos::rpc::FILE) {
      try {
	eos::common::RWMutexReadLock vlock(gOFS->eosViewRWMutex);
	path =
	  gOFS->eosView->getUri(gOFS->eosFileService->getFileMD(request->id().id()).get());
      } catch (eos::MDException& e) {
	path = "";
	errno = e.getErrno();
      }
    } else {
      try {
	eos::common::RWMutexReadLock vlock(gOFS->eosViewRWMutex);
	path =
	  gOFS->eosView->getUri(gOFS->eosDirectoryService->getContainerMD(request->id().id()).get());
      } catch (eos::MDException& e) {
	path = "";
	errno = e.getErrno();
      }
    }

    if (path.empty()) {
      reply->set_code(EINVAL);
      reply->set_msg("error:path is empty");
      return grpc::Status::OK;
    }
  }

  XrdOucErrInfo error;
  errno = 0;
  uid_t uid = request->owner().uid();
  gid_t gid = request->owner().gid();
  std::string user = request->owner().username();
  std::string group = request->owner().groupname();

  if (!user.empty()) {
    int errc = 0;
    uid = eos::common::Mapping::UserNameToUid(user,errc);
    if (errc) {
      reply->set_code(EINVAL);
      std::string msg = "error: unable to translate username to uid '";
      msg += user;
      msg += "'";
      reply->set_msg(msg);
      return grpc::Status::OK;
    }
  }
  if (!group.empty()) {
    int errc = 0;
    gid = eos::common::Mapping::GroupNameToGid(group, errc);
    if (errc) {
      reply->set_code(EINVAL);
      std::string msg = "error: unable to translate groupname to gid '";
      msg += group;
      msg += "'";
      reply->set_msg(msg);
      return grpc::Status::OK;
    }
  }

  if (gOFS->_chown(path.c_str(),
		   uid,
		   gid,
		   error, vid, (const char*) 0)) {
    reply->set_code(errno);
    reply->set_msg(error.getErrText());
    return grpc::Status::OK;
  }

  reply->set_code(0);
  std::string msg = "info: chown file '";
  msg += path.c_str();
  msg += "' uid=";
  msg += std::to_string(uid);
  msg += "' gid=";
  msg += std::to_string(gid);

  reply->set_msg(msg);
  return grpc::Status::OK;
}

grpc::Status
GrpcNsInterface::Chmod(eos::common::VirtualIdentity& vid,
		       eos::rpc::NSResponse::ErrorResponse* reply,
		       const eos::rpc::NSRequest::ChmodRequest* request)
{
  std::string path;

  path = request->id().path();

  if (path.empty()) {
    if (request->id().type() == eos::rpc::FILE) {
      try {
	eos::common::RWMutexReadLock vlock(gOFS->eosViewRWMutex);
	path =
	  gOFS->eosView->getUri(gOFS->eosFileService->getFileMD(request->id().id()).get());
      } catch (eos::MDException& e) {
	path = "";
	errno = e.getErrno();
      }
    } else {
      try {
	eos::common::RWMutexReadLock vlock(gOFS->eosViewRWMutex);
	path =
	  gOFS->eosView->getUri(gOFS->eosDirectoryService->getContainerMD(request->id().id()).get());
      } catch (eos::MDException& e) {
	path = "";
	errno = e.getErrno();
      }
    }

    if (path.empty()) {
      reply->set_code(EINVAL);
      reply->set_msg("error:path is empty");
      return grpc::Status::OK;
    }
  }

  XrdOucErrInfo error;
  errno = 0;
  mode_t mode = request->mode();

  XrdSfsMode sfsmode = mode;

  if (gOFS->_chmod(path.c_str(),
		   sfsmode,
		   error, vid, (const char*) 0)) {
    reply->set_code(errno);
    reply->set_msg(error.getErrText());
    return grpc::Status::OK;
  }

  reply->set_code(0);
  std::string msg = "info: chmod file '";
  msg += path.c_str();
  msg += "' mode=";
  std::stringstream s;
  s << std::oct << mode;
  msg += s.str().c_str();
  reply->set_msg(msg);
  return grpc::Status::OK;

}

#endif

EOSMGMNAMESPACE_END
