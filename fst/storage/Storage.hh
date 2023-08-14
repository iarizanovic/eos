//------------------------------------------------------------------------------
// File: Storage.hh
// Author: Andreas-Joachim Peters - CERN
//------------------------------------------------------------------------------

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

#pragma once
#include "fst/Namespace.hh"
#include "fst/Load.hh"
#include "fst/Health.hh"
#include "common/Logging.hh"
#include "common/FileSystem.hh"
#include "common/RWMutex.hh"
#include "common/AssistedThread.hh"
#include "common/Fmd.hh"
#include "fst/filemd/FmdHandler.hh"
#include "namespace/ns_quarkdb/QdbContactDetails.hh"
#include <vector>
#include <list>
#include <queue>
#include <map>

namespace eos
{
namespace common
{
class ExecutorMgr;
}
}

namespace qclient
{
class SharedHashUpdate;
}

EOSFSTNAMESPACE_BEGIN

class Verify;
class Deletion;
class FileSystem;

//------------------------------------------------------------------------------
//! Class Storage
//------------------------------------------------------------------------------
class Storage: public eos::common::LogId
{
  friend class XrdFstOfsFile;
  friend class XrdFstOfs;
public:
  //----------------------------------------------------------------------------
  //! Create Storage object
  //!
  //! @param metadirectory path to meta dir
  //!
  //! @return pointer to newly created storage object
  //----------------------------------------------------------------------------
  static Storage* Create(const char* metadirectory);

  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  Storage(const char* metadirectory);

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  virtual ~Storage() = default;

  //----------------------------------------------------------------------------
  //! General shutdown including stopping the helper threads and also
  //! cleaning up the registered file systems
  //----------------------------------------------------------------------------
  void Shutdown();

  //----------------------------------------------------------------------------
  //! Add deletion object to the list of pending ones
  //!
  //! @param del deletion object
  //----------------------------------------------------------------------------
  void AddDeletion(std::unique_ptr<Deletion> del);

  //----------------------------------------------------------------------------
  //! Delete file by moving it to a special directory on the file system root
  //! mount location in the .eosdeletions directory
  //!
  //! @param del deletion object
  //----------------------------------------------------------------------------
  void DeleteByMove(std::unique_ptr<Deletion> del);

  //----------------------------------------------------------------------------
  //! Get deletion object removing it from the list
  //!
  //! @return get deletion object
  //----------------------------------------------------------------------------
  std::unique_ptr<Deletion> GetDeletion();

  //----------------------------------------------------------------------------
  //! Get number of pending deletions
  //!
  //! @return number of pending deletions
  //----------------------------------------------------------------------------
  size_t GetNumDeletions();

  //----------------------------------------------------------------------------
  //! Push new verification job to the queue if the maximum number of pending
  //! verifications is not exceeded.
  //!
  //! @param entry verification information about a file
  //----------------------------------------------------------------------------
  void PushVerification(eos::fst::Verify* entry);

  //----------------------------------------------------------------------------
  //! Check if files system is booting
  //!
  //! @param fsid file system id
  //!
  //! @return true if booting, otherwise false
  //----------------------------------------------------------------------------
  inline bool IsFsBooting(eos::common::FileSystem::fsid_t fsid) const
  {
    XrdSysMutexHelper lock(mBootingMutex);
    return (mBootingSet.find(fsid) != mBootingSet.end());
  }

  //----------------------------------------------------------------------------
  //! Get storage path for a particular file system id
  //!
  //! @param fsid file system id
  //!
  //! @return stoage path or empty string if unkown file system id
  //----------------------------------------------------------------------------
  std::string GetStoragePath(eos::common::FileSystem::fsid_t fsid) const;

  //----------------------------------------------------------------------------
  //! Get configuration associated with the given file system id
  //!
  //! @param fsid file system id
  //! @param key configuration key
  //!
  //! @return associated configuration value or empty string if nothing found
  //----------------------------------------------------------------------------
  std::string GetFileSystemConfig(eos::common::FileSystem::fsid_t fsid,
                                  const std::string& key) const;

  //----------------------------------------------------------------------------
  //! Update inconsistency info for the given file system id
  //!
  //! @param fsid file system id
  //!
  //! return true if successful, otherwise false
  //----------------------------------------------------------------------------
  bool UpdateInconsistencyInfo(eos::common::FileSystem::fsid_t fsid);

  //----------------------------------------------------------------------------
  //! Cleanup orphans
  //!
  //! @param fsid file system id or 0 if cleanup is to be performed for all
  //!             file systems
  //! @param err_msg error message
  //!
  //! @return true if successful, otherwise false
  //----------------------------------------------------------------------------
  bool CleanupOrphans(eos::common::FileSystem::fsid_t fsid,
                      std::ostringstream& err_msg);

  //----------------------------------------------------------------------------
  //! Cleanup orphans on disk
  //!
  //! @param mount file system mount path
  //! @param fids set of fids cleaned up
  //!
  //! @return true if successful, otherwise false
  //----------------------------------------------------------------------------
  bool CleanupOrphansDisk(const std::string& mount,
                          std::set<uint64_t>& fids);

  //----------------------------------------------------------------------------
  //! Cleanup orphans from local DB
  //!
  //! @param fsid file system id
  //!
  //! @return true if successful, otherwise false
  //----------------------------------------------------------------------------
  bool CleanupOrphansDb(eos::common::FileSystem::fsid_t fsid);

  //----------------------------------------------------------------------------
  //! Cleanup orphans from QDB
  //!
  //! @param fsid file system id
  //! @param fids set of fids to clean up
  //!
  //! @return true if successful, otherwise false
  //----------------------------------------------------------------------------
  bool CleanupOrphansQdb(eos::common::FileSystem::fsid_t fsid,
                         const std::set<uint64_t>& fids);

  //----------------------------------------------------------------------------
  //! Get Total FSes tracked in storage, ie. size of the FsMap
  //!
  //! @return total count of FSes
  //----------------------------------------------------------------------------
  size_t GetFSCount() const;

  //----------------------------------------------------------------------------
  //! Publish fsck error to QDB
  //!
  //! @param fid file identifier
  //! @param fsid file system identifier
  //! @param err_type fsck error type
  //----------------------------------------------------------------------------
  void PublishFsckError(eos::common::FileId::fileid_t fid,
                        eos::common::FileSystem::fsid_t fsid,
                        eos::common::FsckErr err_type);

  //----------------------------------------------------------------------------
  //! Push collected fsck errors to QDB
  //!
  //! @param fsid file system identifier
  //! @param fidset map of error types to set of fids which are affected
  //!
  //! @return true if push was successful, othewise false
  //----------------------------------------------------------------------------
  bool PushToQdb(eos::common::FileSystem::fsid_t fsid,
                 const std::map<std::string,
                 std::set<eos::common::FileId::fileid_t>>& fidset);

  //----------------------------------------------------------------------------
  //! Process file system configuration change
  //!
  //! @param targetFs target file system object
  //! @param key configuration key
  //! @param value configuration value
  //!
  //! @note This requires the mFsMutex to be write locked
  //----------------------------------------------------------------------------
  void ProcessFsConfigChange(fst::FileSystem* targetFs,
                             const std::string& key, const std::string& value);

protected:
  mutable eos::common::RWMutex mFsMutex; ///< Mutex protecting the fs map
  std::vector <fst::FileSystem*> mFsVect; ///< Vector of filesystems
  //! Map of filesystem id to filesystem object
  std::map<eos::common::FileSystem::fsid_t, fst::FileSystem*> mFsMap;

private:
  //! Publish inconsistency statistics once every two hours
  static constexpr std::chrono::minutes sConsistencyTimeout {120};
  //! Set of key updates to be tracked at the node level
  static std::set<std::string> sNodeUpdateKeys;
  //! Set of key updates to be tracked at the file system level
  static std::set<std::string> sFsUpdateKeys;

  bool mZombie; ///< State of the node
  XrdOucString mMetaDir; ///< Path to meta directory
  unsigned long long* mScrubPattern[2];
  unsigned long long* mScrubPatternVerify;
  mutable XrdSysMutex mBootingMutex; // Mutex protecting the boot set
  //! Set containing the filesystems currently booting
  std::set<eos::common::FileSystem::fsid_t> mBootingSet;
  eos::fst::Verify* mRunningVerify; ///< Currently running verification job
  XrdSysMutex mThreadsMutex; ///< Mutex protecting access to the set of threads
  std::set<pthread_t> mThreadSet; ///< Set of running helper threads
  XrdSysMutex mFsFullMapMutex; ///< Mutex protecting access to the fs full map
  //! Map indicating if a filesystem has less than  5 GB free
  std::map<eos::common::FileSystem::fsid_t, bool> mFsFullMap;
  //! Map indicating if a filesystem has less than (headroom) space free, which
  //! disables draining and balancing
  std::map<eos::common::FileSystem::fsid_t, bool> mFsFullWarnMap;
  XrdSysMutex mVerifyMutex; ///< Mutex protecting access to the verifications
  //! Queue of verification jobs pending
  std::queue <eos::fst::Verify*> mVerifications;
  XrdSysMutex mDeletionsMutex; ///< Mutex protecting the list of deletions
  std::list< std::unique_ptr<Deletion> > mListDeletions; ///< List of deletions
  Load mFstLoad; ///< Net/IO load monitor
  Health mFstHealth; ///< Local disk S.M.A.R.T monitor
  AssistedThread mCommunicatorThread;
  AssistedThread mQdbCommunicatorThread;
  std::set<std::string> mLastRoundFilesystems;
  AssistedThread mPublisherThread;

  //! Struct BootThreadInfo
  struct BootThreadInfo {
    Storage* storage;
    fst::FileSystem* filesystem;
  };

  //----------------------------------------------------------------------------
  //! Helper methods used for starting worker threads
  //----------------------------------------------------------------------------
  static void* StartVarPartitionMonitor(void* pp);
  static void* StartDaemonSupervisor(void* pp);
  static void* StartFsScrub(void* pp);
  static void* StartFsRemover(void* pp);
  static void* StartFsReport(void* pp);
  static void* StartFsErrorReport(void* pp);
  static void* StartFsVerify(void* pp);
  static void* StartMgmSyncer(void* pp);
  static void* StartBoot(void* pp);

  //----------------------------------------------------------------------------
  //! Get statistics about given file system used for publishing
  //!
  //! @param fs file system object
  //!
  //! @return map of statistics to be published
  //----------------------------------------------------------------------------
  std::map<std::string, std::string>
  GetFsStatistics(fst::FileSystem* fs);

  //----------------------------------------------------------------------------
  //! Get statistics about this FST node used for publishing
  //----------------------------------------------------------------------------
  std::map<std::string, std::string> GetFstStatistics(
    const std::string& tmpfile, unsigned long long netspeed);

  //----------------------------------------------------------------------------
  //! Publish statistics about the given file system
  //----------------------------------------------------------------------------
  bool PublishFsStatistics(fst::FileSystem* fs);

  //----------------------------------------------------------------------------
  //! Register file system for which we know we have file fsid info available
  //!
  //! @param queuepath file system queuepath identifier
  //----------------------------------------------------------------------------
  void RegisterFileSystem(const std::string& queuepath);

  //----------------------------------------------------------------------------
  //! Unregister file system given a queue path
  //!
  //! @param queuepath file system queuepath identifier
  //----------------------------------------------------------------------------
  void UnregisterFileSystem(const std::string& queuepath);

  //----------------------------------------------------------------------------
  //! Worker threads implementation
  //----------------------------------------------------------------------------
  void Supervisor();
  void Communicator(ThreadAssistant& assistant);
  void QdbCommunicator(ThreadAssistant& assistant);

  //----------------------------------------------------------------------------
  //! Get configuration value from global FST config
  //!
  //! @param key configuration key
  //! @param value output configuration value as string
  //!
  //! @return true if config key found, otherwise false
  //----------------------------------------------------------------------------
  bool GetFstConfigValue(const std::string& key, std::string& value) const;

  //----------------------------------------------------------------------------
  //! Get configuration value from global FST config
  //!
  //! @param key configuration key
  //! @param value output configuration value as ull
  //!
  //! @return true if config key found, otherwise false
  //----------------------------------------------------------------------------
  bool GetFstConfigValue(const std::string& key, unsigned long long& value) const;

  //----------------------------------------------------------------------------
  //! Process FST node configuration change
  //!
  //! @param key configuration key
  //! @param value configuration value
  //----------------------------------------------------------------------------
  void ProcessFstConfigChange(const std::string& key, const std::string& value);

  //----------------------------------------------------------------------------
  //! Process file system configuration change
  //!
  //! @param queue file system queue
  //! @param key configuration key
  //----------------------------------------------------------------------------
  void ProcessFsConfigChange(const std::string& queue, const std::string& key);

  void Scrub();
  void Remover();
  void Report();
  void ErrorReport();
  void Verify();
  void Publish(ThreadAssistant& assistant);
  void MgmSyncer();
  void Boot(fst::FileSystem* fs);


  //----------------------------------------------------------------------------
  //! Scrub filesystem
  //----------------------------------------------------------------------------
  int ScrubFs(const char* path, unsigned long long free,
              unsigned long long lbocks, unsigned long id, bool direct_io);

  //----------------------------------------------------------------------------
  //! Check if node is in zombie state i.e. true if any of the helper threads
  //! was not properly started.
  //----------------------------------------------------------------------------
  inline bool
  IsZombie()
  {
    return mZombie;
  }

  //----------------------------------------------------------------------------
  //! Run boot thread for specified filesystem
  //!
  //! @param fs filesystem object
  //!
  //! @return true if boot thread started successfully, otherwise false
  //----------------------------------------------------------------------------
  bool RunBootThread(fst::FileSystem* fs);

  //----------------------------------------------------------------------------
  //! Write file system label files (.eosid and .eosuuid) according to the
  //! configuration if they don't exist already.
  //!
  //! @param path mount point of the file system
  //! @param fsid file system id
  //! @param uuid file system uuid
  //!
  //! @return true if successful, otherwise false
  //----------------------------------------------------------------------------
  bool FsLabel(std::string path, eos::common::FileSystem::fsid_t fsid,
               std::string uuid);

  //----------------------------------------------------------------------------
  //! Check that the label on the file system matches the one in the
  //! configuration.
  //!
  //! @param path mount point of the file system
  //! @param fsid file system id
  //! @param uuid file system uuid
  //! @param fail_noid when true fail if there is no .eosfsid file present
  //! @param fail_nouuid when true fail if there is no .eosfsuuid file present
  //!
  //! @return true if labels match, otherwise false
  //----------------------------------------------------------------------------
  bool CheckLabel(std::string path, eos::common::FileSystem::fsid_t fsid,
                  std::string uuid, bool fail_noid = false,
                  bool fail_nouuid = false);

  //----------------------------------------------------------------------------
  //! Check if node is active i.e. the stat.active
  //----------------------------------------------------------------------------
  bool IsNodeActive() const;

  //----------------------------------------------------------------------------
  //! Check if the selected file system needs to be registered as "full" or
  //! "warning".
  //!
  //! @note  Needs to be called with at least a read lock on the mFsMutex.
  //----------------------------------------------------------------------------
  void CheckFilesystemFullness(fst::FileSystem* fs,
                               eos::common::FileSystem::fsid_t fsid);

  //----------------------------------------------------------------------------
  //! Get the filesystem associated with the given filesystem id
  //! or NULL if none could be found.
  //! @note  Needs to be called with at least a read lock on the mFsMutex.
  //!
  //! @param fsid filesystem id
  //!
  //! @return associated filesystem object or NULL
  //----------------------------------------------------------------------------
  fst::FileSystem* GetFileSystemById(eos::common::FileSystem::fsid_t fsid) const;

  //----------------------------------------------------------------------------
  //! Shutdown all helper threads
  //----------------------------------------------------------------------------
  void ShutdownThreads();

  //----------------------------------------------------------------------------
  //! Update file system list given the QDB shared hash configuration i.e. scan
  //! QDB for file systems belonging to the current node and update the
  //! internal list.
  //----------------------------------------------------------------------------
  void UpdateFilesystemDefinitions();

  //----------------------------------------------------------------------------
  //! FST node update callback - this is triggered whenever the underlying
  //! qclient::SharedHash corresponding to the node is modified.
  //!
  //! @param upd SharedHashUpdate object
  //----------------------------------------------------------------------------
  void NodeUpdateCb(qclient::SharedHashUpdate&& upd);
};

EOSFSTNAMESPACE_END
