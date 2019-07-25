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
// desc:   Class representing the file metadata
//------------------------------------------------------------------------------

#ifndef __EOS_NS_FILE_MD_HH__
#define __EOS_NS_FILE_MD_HH__

#include "namespace/interface/IFileMD.hh"
#include "namespace/interface/IFileMDSvc.hh"
#include <stdint.h>
#include <cstring>
#include <string>
#include <vector>
#include <sys/time.h>

EOSNSNAMESPACE_BEGIN

//! Forward declaration
class IFileMDSvc;
class IContainerMD;

//------------------------------------------------------------------------------
//! Class holding the metadata information concerning a single file
//------------------------------------------------------------------------------
class FileMD: public IFileMD
{
public:
  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  FileMD(IFileMD::id_t id, IFileMDSvc* fileMDSvc);

  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  virtual ~FileMD() {};

  //----------------------------------------------------------------------------
  //! Virtual copy constructor
  //----------------------------------------------------------------------------
  virtual FileMD* clone() const override;

  //----------------------------------------------------------------------------
  //! Copy constructor
  //----------------------------------------------------------------------------
  FileMD(const FileMD& other);

  //----------------------------------------------------------------------------
  //! Assignment operator
  //----------------------------------------------------------------------------
  FileMD& operator = (const FileMD& other);

  //----------------------------------------------------------------------------
  //! Get file id
  //----------------------------------------------------------------------------
  IFileMD::id_t getId() const override
  {
    return pId;
  }

  //----------------------------------------------------------------------------
  //! Get file identifier
  //----------------------------------------------------------------------------
  FileIdentifier getIdentifier() const override
  {
    return FileIdentifier(pId);
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
  //! Get modification time
  //----------------------------------------------------------------------------
  void getMTime(ctime_t& mtime) const override
  {
    mtime.tv_sec = pMTime.tv_sec;
    mtime.tv_nsec = pMTime.tv_nsec;
  }

  //----------------------------------------------------------------------------
  //! Set modification time
  //----------------------------------------------------------------------------
  void setMTime(ctime_t mtime) override
  {
    pMTime.tv_sec = mtime.tv_sec;
    pMTime.tv_nsec = mtime.tv_nsec;
  }

  //----------------------------------------------------------------------------
  //! Set modification time to now
  //----------------------------------------------------------------------------
  void setMTimeNow() override
  {
#ifdef __APPLE__
    struct timeval tv;
    gettimeofday(&tv, 0);
    pMTime.tv_sec = tv.tv_sec;
    pMTime.tv_nsec = tv.tv_usec * 1000;
#else
    clock_gettime(CLOCK_REALTIME, &pMTime);
#endif
  }

  //----------------------------------------------------------------------------
  //! Get sync time
  //----------------------------------------------------------------------------
  void getSyncTime(ctime_t& mtime) const override
  {
    getMTime(mtime);
  }

  //----------------------------------------------------------------------------
  //! Set sync time
  //----------------------------------------------------------------------------
  void setSyncTime(ctime_t mtime) override
  {
  }

  //----------------------------------------------------------------------------
  //! Set sync time
  //----------------------------------------------------------------------------
  void setSyncTimeNow() override
  {
  }

  //----------------------------------------------------------------------------
  //! Get cloneId (dummy)
  //----------------------------------------------------------------------------
  uint64_t getCloneId() const override
  {
    return 0;
  }

  //----------------------------------------------------------------------------
  //! Set cloneId (dummy)
  //----------------------------------------------------------------------------
  void setCloneId(uint64_t id) override
  {
  }

  //----------------------------------------------------------------------------
  //! Get cloneFST (dummy)
  //----------------------------------------------------------------------------
  const std::string getCloneFST() const override
  {
    return std::string("");
  }

  //----------------------------------------------------------------------------
  //! Set cloneFST (dummy)
  //----------------------------------------------------------------------------
  void setCloneFST(const std::string& data) override
  {
  }

  //----------------------------------------------------------------------------
  //! Get size
  //----------------------------------------------------------------------------
  uint64_t getSize() const override
  {
    return pSize;
  }

  //----------------------------------------------------------------------------
  //! Set size - 48 bytes will be used
  //----------------------------------------------------------------------------
  void setSize(uint64_t size) override;

  //----------------------------------------------------------------------------
  //! Get tag
  //----------------------------------------------------------------------------
  IContainerMD::id_t getContainerId() const override
  {
    return pContainerId;
  }

  //----------------------------------------------------------------------------
  //! Set tag
  //----------------------------------------------------------------------------
  void setContainerId(IContainerMD::id_t containerId) override
  {
    pContainerId = containerId;
  }

  //----------------------------------------------------------------------------
  //! Get checksum
  //----------------------------------------------------------------------------
  const Buffer getChecksum() const override
  {
    return pChecksum;
  }

  //----------------------------------------------------------------------------
  //! Set checksum
  //----------------------------------------------------------------------------
  void setChecksum(const Buffer& checksum) override
  {
    pChecksum = checksum;
  }

  //----------------------------------------------------------------------------
  //! Clear checksum
  //----------------------------------------------------------------------------
  void clearChecksum(uint8_t size = 20) override
  {
    char zero = 0;

    for (uint8_t i = 0; i < size; i++) {
      pChecksum.putData(&zero, 1);
    }
  }

  //----------------------------------------------------------------------------
  //! Set checksum
  //!
  //! @param checksum address of a memory location string the checksum
  //! @param size     size of the checksum in bytes
  //----------------------------------------------------------------------------
  void setChecksum(const void* checksum, uint8_t size) override
  {
    pChecksum.clear();
    pChecksum.putData(checksum, size);
  }

  //----------------------------------------------------------------------------
  //! Get name
  //----------------------------------------------------------------------------
  const std::string getName() const override
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
  //! Add location
  //----------------------------------------------------------------------------
  void addLocation(location_t location) override;

  //----------------------------------------------------------------------------
  //! Get vector with all the locations
  //----------------------------------------------------------------------------
  LocationVector getLocations() const override;

  //----------------------------------------------------------------------------
  //! Get location
  //----------------------------------------------------------------------------
  location_t getLocation(unsigned int index) override
  {
    if (index < pLocation.size()) {
      return pLocation[index];
    }

    return 0;
  }

  //----------------------------------------------------------------------------
  //! Remove location that was previously unlinked
  //----------------------------------------------------------------------------
  void removeLocation(location_t location) override;

  //----------------------------------------------------------------------------
  //! Remove all locations that were previously unlinked
  //----------------------------------------------------------------------------
  void removeAllLocations() override;

  //----------------------------------------------------------------------------
  //! Get vector with all unlinked locations
  //----------------------------------------------------------------------------
  LocationVector getUnlinkedLocations() const override;

  //----------------------------------------------------------------------------
  //! Unlink location
  //----------------------------------------------------------------------------
  void unlinkLocation(location_t location) override;

  //----------------------------------------------------------------------------
  //! Unlink all locations
  //----------------------------------------------------------------------------
  void unlinkAllLocations() override;

  //----------------------------------------------------------------------------
  //! Clear unlinked locations without notifying the listeners
  //----------------------------------------------------------------------------
  void clearUnlinkedLocations() override
  {
    pUnlinkedLocation.clear();
  }

  //----------------------------------------------------------------------------
  //! Test the unlinkedlocation
  //----------------------------------------------------------------------------
  bool hasUnlinkedLocation(location_t location) override
  {
    for (unsigned int i = 0; i < pUnlinkedLocation.size(); i++) {
      if (pUnlinkedLocation[i] == location) {
        return true;
      }
    }

    return false;
  }

  //----------------------------------------------------------------------------
  //! Get number of unlinked locations
  //----------------------------------------------------------------------------
  size_t getNumUnlinkedLocation() const override
  {
    return pUnlinkedLocation.size();
  }

  //----------------------------------------------------------------------------
  //! Clear locations without notifying the listeners
  //----------------------------------------------------------------------------
  void clearLocations() override
  {
    pLocation.clear();
  }

  //----------------------------------------------------------------------------
  //! Test the location
  //----------------------------------------------------------------------------
  bool hasLocation(location_t location) override
  {
    for (unsigned int i = 0; i < pLocation.size(); i++) {
      if (pLocation[i] == location) {
        return true;
      }
    }

    return false;
  }

  //----------------------------------------------------------------------------
  //! Get number of location
  //----------------------------------------------------------------------------
  size_t getNumLocation() const override
  {
    return pLocation.size();
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
  //! Get layout
  //----------------------------------------------------------------------------
  layoutId_t getLayoutId() const override
  {
    return pLayoutId;
  }

  //----------------------------------------------------------------------------
  //! Set layout
  //----------------------------------------------------------------------------
  void setLayoutId(layoutId_t layoutId) override
  {
    pLayoutId = layoutId;
  }

  //----------------------------------------------------------------------------
  //! Get flags
  //----------------------------------------------------------------------------
  uint16_t getFlags() const override
  {
    return pFlags;
  }

  //----------------------------------------------------------------------------
  //! Get the n-th flag
  //----------------------------------------------------------------------------
  bool getFlag(uint8_t n) override
  {
    return pFlags & (0x0001 << n);
  }

  //----------------------------------------------------------------------------
  //! Set flags
  //----------------------------------------------------------------------------
  void setFlags(uint16_t flags) override
  {
    pFlags = flags;
  }

  //----------------------------------------------------------------------------
  //! Set the n-th flag
  //----------------------------------------------------------------------------
  void setFlag(uint8_t n, bool flag) override
  {
    if (flag) {
      pFlags |= (1 << n);
    } else {
      pFlags &= (~(1 << n));
    }
  }

  //----------------------------------------------------------------------------
  //! Env Representation
  //----------------------------------------------------------------------------
  void getEnv(std::string& env, bool escapeAnd = false) override;

  //----------------------------------------------------------------------------
  //! Set the FileMDSvc object
  //----------------------------------------------------------------------------
  void setFileMDSvc(IFileMDSvc* fileMDSvc) override
  {
    pFileMDSvc = fileMDSvc;
  }

  //----------------------------------------------------------------------------
  //! Get the FileMDSvc object
  //----------------------------------------------------------------------------
  virtual IFileMDSvc* getFileMDSvc() override
  {
    return pFileMDSvc;
  }

  //----------------------------------------------------------------------------
  //! Serialize the object to a buffer
  //----------------------------------------------------------------------------
  void serialize(Buffer& buffer) override;

  //----------------------------------------------------------------------------
  //! Deserialize the class to a buffer
  //----------------------------------------------------------------------------
  void deserialize(const Buffer& buffer) override;

  //----------------------------------------------------------------------------
  //! Get symbolic link
  //----------------------------------------------------------------------------
  std::string getLink() const override
  {
    return pLinkName;
  }

  //----------------------------------------------------------------------------
  //! Set symbolic link
  //----------------------------------------------------------------------------
  void setLink(std::string link_name) override
  {
    pLinkName = link_name;
  }

  //----------------------------------------------------------------------------
  //! Check if symbolic link
  //----------------------------------------------------------------------------
  bool isLink() const override
  {
    return pLinkName.length() ? true : false;
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
  //! Remove all attributes
  //----------------------------------------------------------------------------
  void clearAttributes() override
  {
    pXAttrs.clear();
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
  //! Get the attribute
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

protected:
  //----------------------------------------------------------------------------
  // Data members
  //----------------------------------------------------------------------------
  IFileMD::id_t       pId;
  ctime_t             pCTime;
  ctime_t             pMTime;
  uint64_t            pSize;
  IContainerMD::id_t  pContainerId;
  uid_t               pCUid;
  gid_t               pCGid;
  layoutId_t          pLayoutId;
  uint16_t            pFlags;
  std::string         pName;
  std::string         pLinkName;
  LocationVector      pLocation;
  LocationVector      pUnlinkedLocation;
  Buffer              pChecksum;
  XAttrMap            pXAttrs;
  IFileMDSvc*         pFileMDSvc;
};

EOSNSNAMESPACE_END

#endif // __EOS_NS_FILE_MD_HH__
