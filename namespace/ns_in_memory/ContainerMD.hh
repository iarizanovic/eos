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
// desc:   Class representing the container metadata
//------------------------------------------------------------------------------

#ifndef __EOS_NS_CONTAINER_MD_HH__
#define __EOS_NS_CONTAINER_MD_HH__

#include "namespace/interface/IContainerMD.hh"
#include "namespace/interface/IFileMD.hh"
#include <cstring>
#include <vector>
#include <features.h>

#if __GNUC_PREREQ(4,8)
#include <atomic>
#endif

EOSNSNAMESPACE_BEGIN

//! Forward declaration
class IContainerMDSvc;
class IFileMDSvc;

//------------------------------------------------------------------------------
//! Class holding the metadata information concerning a single container
//------------------------------------------------------------------------------
class ContainerMD: public IContainerMD
{
public:

#if __GNUC_PREREQ(4,8)
  struct tmtime_atomic_t {
    tmtime_atomic_t()
    {
      tv_sec.store(0);
      tv_nsec.store(0);
    }
    std::atomic_ulong tv_sec;
    std::atomic_ulong tv_nsec;

    tmtime_atomic_t& operator= (const tmtime_atomic_t& other)
    {
      tv_sec.store(other.tv_sec.load());
      tv_nsec.store(other.tv_nsec.load());
      return *this;
    }

    void load(tmtime_t& tmt)
    {
      tmt.tv_sec = tv_sec.load();
      tmt.tv_nsec = tv_nsec.load();
    }

    void store(const tmtime_t& tmt)
    {
      tv_sec.store(tmt.tv_sec);
      tv_nsec.store(tmt.tv_nsec);
    }
  };
#else
#endif

  //----------------------------------------------------------------------------
  //! Constructor
  //!
  //! @param id container id
  //! @param file_svc file metadata service
  //! @param cont_svc container metadata service
  //----------------------------------------------------------------------------
  ContainerMD(id_t id, IFileMDSvc* file_svc, IContainerMDSvc* cont_svc);

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  ~ContainerMD();

  //----------------------------------------------------------------------------
  //! Virtual copy constructor
  //----------------------------------------------------------------------------
  virtual ContainerMD* clone() const override;

  //----------------------------------------------------------------------------
  //! Copy constructor
  //----------------------------------------------------------------------------
  ContainerMD(const ContainerMD& other);

  //----------------------------------------------------------------------------
  //! Assignment operator
  //----------------------------------------------------------------------------
  ContainerMD& operator= (const ContainerMD& other);

  //----------------------------------------------------------------------------
  //! Children inheritance
  //!
  // @param other container from which to inherit children
  //----------------------------------------------------------------------------
  void InheritChildren(const IContainerMD& other) override;

  //----------------------------------------------------------------------------
  //! Add container
  //----------------------------------------------------------------------------
  virtual void addContainer(IContainerMD* container) override;

  //----------------------------------------------------------------------------
  //! Remove container
  //----------------------------------------------------------------------------
  void removeContainer(const std::string& name) override;

  //----------------------------------------------------------------------------
  //! Find sub container
  //----------------------------------------------------------------------------
  std::shared_ptr<IContainerMD> findContainer(const std::string& name) override;

  //----------------------------------------------------------------------------
  //! Get number of containers
  //----------------------------------------------------------------------------
  size_t getNumContainers() override
  {
    return mSubcontainers.size();
  }

  //----------------------------------------------------------------------------
  //! Add file
  //----------------------------------------------------------------------------
  virtual void addFile(IFileMD* file) override;

  //----------------------------------------------------------------------------
  //! Remove file
  //----------------------------------------------------------------------------
  void removeFile(const std::string& name) override;

  //----------------------------------------------------------------------------
  //! Find file
  //----------------------------------------------------------------------------
  virtual std::shared_ptr<IFileMD> findFile(const std::string& name) override;

  //----------------------------------------------------------------------------
  //! Get number of files
  //----------------------------------------------------------------------------
  size_t getNumFiles() override
  {
    return mFiles.size();
  }

  //----------------------------------------------------------------------------
  //! Get container id
  //----------------------------------------------------------------------------
  id_t getId() const override
  {
    return pId;
  }

  //----------------------------------------------------------------------------
  //! Get parent id
  //----------------------------------------------------------------------------
  id_t getParentId() const override
  {
    return pParentId;
  }

  //----------------------------------------------------------------------------
  //! Set parent id
  //----------------------------------------------------------------------------
  void setParentId(id_t parentId) override
  {
    pParentId = parentId;
  }

  //----------------------------------------------------------------------------
  //! Get the flags
  //----------------------------------------------------------------------------
  uint16_t getFlags() const override
  {
    return pFlags;
  }

  //----------------------------------------------------------------------------
  //! Set flags
  //----------------------------------------------------------------------------
  virtual void setFlags(uint16_t flags) override
  {
    pFlags = flags;
  }

  //----------------------------------------------------------------------------
  //! Set creation time
  //----------------------------------------------------------------------------
  void setCTime(ctime_t ctime) override
  {
    pCTime.tv_sec = ctime.tv_sec;
    pCTime.tv_nsec = ctime.tv_nsec;
  }

  //----------------------------------------------------------------------------
  //! Set creation time to now
  //----------------------------------------------------------------------------
  void setCTimeNow() override
  {
#ifdef __APPLE__
    struct timeval tv;
    gettimeofday(&tv, 0);
    pCTime.tv_sec = tv.tv_sec;
    pCTime.tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(CLOCK_REALTIME, &pCTime);
#endif
  }

  //----------------------------------------------------------------------------
  //! Get creation time
  //----------------------------------------------------------------------------
  void getCTime(ctime_t& ctime) const override
  {
    ctime.tv_sec = pCTime.tv_sec;
    ctime.tv_nsec = pCTime.tv_nsec;
  }

  //----------------------------------------------------------------------------
  //! Set creation time
  //----------------------------------------------------------------------------
  void setMTime(mtime_t mtime) override;

  //----------------------------------------------------------------------------
  //! Set creation time to now
  //----------------------------------------------------------------------------
  void setMTimeNow() override;

  //----------------------------------------------------------------------------
  //! Trigger an mtime change event
  //----------------------------------------------------------------------------
  void notifyMTimeChange(IContainerMDSvc* containerMDSvc) override;

  //----------------------------------------------------------------------------
  //! Get modification time
  //----------------------------------------------------------------------------
  void getMTime(mtime_t& mtime) const override
  {
    mtime.tv_sec = pMTime.tv_sec;
    mtime.tv_nsec = pMTime.tv_nsec;
  }

  //----------------------------------------------------------------------------
  //! Set propagated modification time (if newer)
  //----------------------------------------------------------------------------
  bool setTMTime(tmtime_t tmtime) override
  {
    while (1) {
#if __GNUC_PREREQ(4,8)
      pTMTime_atomic.load(pTMTime);
#endif

      if ((tmtime.tv_sec > pTMTime.tv_sec) ||
          ((tmtime.tv_sec == pTMTime.tv_sec) &&
           (tmtime.tv_nsec > pTMTime.tv_nsec))) {
#if __GNUC_PREREQ(4,8)
        uint64_t ts = (uint64_t) pTMTime.tv_sec;
        uint64_t tns = (uint64_t) pTMTime.tv_nsec;
        bool retry1 = pTMTime_atomic.tv_sec.compare_exchange_weak(ts, tmtime.tv_sec,
                      std::memory_order_relaxed,
                      std::memory_order_relaxed);
        bool retry2 = pTMTime_atomic.tv_nsec.compare_exchange_weak(tns, tmtime.tv_nsec,
                      std::memory_order_relaxed,
                      std::memory_order_relaxed);

        if (!retry1 || !retry2) {
          continue;
        }

        pTMTime_atomic.load(pTMTime);
#else
        pTMTime.tv_sec = tmtime.tv_sec;
        pTMTime.tv_nsec = tmtime.tv_nsec;
#endif
      }

      return true;
    }

    return false;
  }

  //----------------------------------------------------------------------------
  //! Set propagated modification time to now
  //----------------------------------------------------------------------------
  void setTMTimeNow() override
  {
    tmtime_t tmtime;
#ifdef __APPLE__
    struct timeval tv;
    gettimeofday(&tv, 0);
    tmtime..tv_sec = tv.tv_sec;
    tmtime.tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(CLOCK_REALTIME, &tmtime);
#endif
    setTMTime(tmtime);
    return;
  }

  //----------------------------------------------------------------------------
  //! Get creation time
  //----------------------------------------------------------------------------
  void getTMTime(tmtime_t& tmtime) override
  {
#if __GNUC_PREREQ(4,8)
    pTMTime_atomic.load(pTMTime);
#endif
    tmtime.tv_sec = pTMTime.tv_sec;
    tmtime.tv_nsec = pTMTime.tv_nsec;
  }

  //----------------------------------------------------------------------------
  //! Get tree size
  //----------------------------------------------------------------------------
  uint64_t getTreeSize() const override
  {
#if __GNUC_PREREQ(4,8)
    return pTreeSize.load();
#else
    return pTreeSize;
#endif
  }

  //----------------------------------------------------------------------------
  //! Set tree size
  //----------------------------------------------------------------------------
  void setTreeSize(uint64_t treesize) override
  {
#if __GNUC_PREREQ(4,8)
    pTreeSize.store(treesize);
#else
    pTreeSize = treesize;
#endif
  }

  //----------------------------------------------------------------------------
  //! Add to tree size
  //----------------------------------------------------------------------------
  uint64_t addTreeSize(uint64_t addsize) override
  {
    pTreeSize += addsize;
    return getTreeSize();
  }

  //----------------------------------------------------------------------------
  //! Remove from tree size
  //----------------------------------------------------------------------------
  uint64_t removeTreeSize(uint64_t removesize) override
  {
    pTreeSize -= removesize;
    return pTreeSize;
  }

  //----------------------------------------------------------------------------
  //! Get name
  //----------------------------------------------------------------------------
  const std::string& getName() const override
  {
    return pName;
  }

  //----------------------------------------------------------------------------
  //! Set name
  //----------------------------------------------------------------------------
  void setName(const std::string& name) override
  {
    pName = name;
  }

  //----------------------------------------------------------------------------
  //! Get uid
  //----------------------------------------------------------------------------
  uid_t getCUid() const override
  {
    return pCUid;
  }

  //----------------------------------------------------------------------------
  //! Set uid
  //----------------------------------------------------------------------------
  void setCUid(uid_t uid) override
  {
    pCUid = uid;
  }

  //----------------------------------------------------------------------------
  //! Get gid
  //----------------------------------------------------------------------------
  gid_t getCGid() const override
  {
    return pCGid;
  }

  //----------------------------------------------------------------------------
  //! Set gid
  //----------------------------------------------------------------------------
  void setCGid(gid_t gid) override
  {
    pCGid = gid;
  }

  //----------------------------------------------------------------------------
  //! Get mode
  //----------------------------------------------------------------------------
  mode_t getMode() const override
  {
    return pMode;
  }

  //----------------------------------------------------------------------------
  //! Set mode
  //----------------------------------------------------------------------------
  void setMode(mode_t mode) override
  {
    pMode = mode;
  }

  //----------------------------------------------------------------------------
  //! Add extended attribute
  //----------------------------------------------------------------------------
  void setAttribute(const std::string& name, const std::string& value) override
  {
    pXAttrs[name] = value;
  }

  //----------------------------------------------------------------------------
  //! Remove attribute
  //----------------------------------------------------------------------------
  void removeAttribute(const std::string& name) override
  {
    XAttrMap::iterator it = pXAttrs.find(name);

    if (it != pXAttrs.end()) {
      pXAttrs.erase(it);
    }
  }

  //----------------------------------------------------------------------------
  //! Check if the attribute exist
  //----------------------------------------------------------------------------
  bool hasAttribute(const std::string& name) const override
  {
    return pXAttrs.find(name) != pXAttrs.end();
  }

  //----------------------------------------------------------------------------
  //! Return number of attributes
  //----------------------------------------------------------------------------
  size_t numAttributes() const override
  {
    return pXAttrs.size();
  }

  //----------------------------------------------------------------------------
  // Get the attribute
  //----------------------------------------------------------------------------
  std::string getAttribute(const std::string& name) const override
  {
    XAttrMap::const_iterator it = pXAttrs.find(name);

    if (it == pXAttrs.end()) {
      MDException e(ENOENT);
      e.getMessage() << "Attribute: " << name << " not found";
      throw e;
    }

    return it->second;
  }

  //----------------------------------------------------------------------------
  //! Get map copy of the extended attributes
  //!
  //! @return std::map containing all the extended attributes
  //----------------------------------------------------------------------------
  eos::IFileMD::XAttrMap getAttributes() const override;

  //------------------------------------------------------------------------------
  //! Check the access permissions
  //!
  //! @return true if all the requested rights are granted, false otherwise
  //------------------------------------------------------------------------------
  bool access(uid_t uid, gid_t gid, int flags = 0) override;

  //----------------------------------------------------------------------------
  //! Clean up the entire contents for the container. Delete files and
  //! containers recurssively
  //----------------------------------------------------------------------------
  void cleanUp() override;

  //----------------------------------------------------------------------------
  //! Get set of file names contained in the current object
  //!
  //! @return set of file names
  //----------------------------------------------------------------------------
  std::set<std::string> getNameFiles() const override;

  //----------------------------------------------------------------------------
  //! Get set of subcontainer names contained in the current object
  //!
  //! @return set of subcontainer names
  //----------------------------------------------------------------------------
  std::set<std::string> getNameContainers() const override;

  //----------------------------------------------------------------------------
  //! Serialize the object to a buffer
  //----------------------------------------------------------------------------
  void serialize(Buffer& buffer) const override;

  //----------------------------------------------------------------------------
  //! Deserialize the class to a buffer
  //----------------------------------------------------------------------------
  void deserialize(Buffer& buffer) override;

protected:
  id_t         pId;
  id_t         pParentId;
  uint16_t     pFlags;
  ctime_t      pCTime;
  std::string  pName;
  uid_t        pCUid;
  gid_t        pCGid;
  mode_t       pMode;
  uint16_t     pACLId;
  XAttrMap     pXAttrs;

#if __GNUC_PREREQ(4,8)
  // Atomic (thread-safe) types
  std::atomic_ulong pTreeSize;
  tmtime_atomic_t pTMTime_atomic;
#else
  uint64_t     pTreeSize;
#endif

private:
  // Non-presistent data members
  mtime_t      pMTime;
  tmtime_t     pTMTime;
  IFileMDSvc* pFileSvc; ///< File metadata service
  IContainerMDSvc* pContSvc; ///< Container metadata service
};

EOSNSNAMESPACE_END

#endif // __EOS_NS_CONTAINER_MD_HH__
