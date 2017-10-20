//------------------------------------------------------------------------------
//! file DrainFS.hh
//! @uthor Andrea Manzi - CERN
//------------------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2017 CERN/Switzerland                                  *
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
#include <pthread.h>
#include "mgm/Namespace.hh"
#include "mgm/FileSystem.hh"
#include "common/Logging.hh"

EOSMGMNAMESPACE_BEGIN

//! Forward declaration
class DrainTransferJob;

//------------------------------------------------------------------------------
//! @brief Class implementing the draining of a filesystem
//------------------------------------------------------------------------------
class DrainFS: public eos::common::LogId
{
public:
  //----------------------------------------------------------------------------
  //! Static thread startup function
  //----------------------------------------------------------------------------
  static void* StaticThreadProc(void*);

  //----------------------------------------------------------------------------
  //! Constructor
  //!
  //! @param fs_id filesystem id
  //----------------------------------------------------------------------------
  DrainFS(eos::common::FileSystem::fsid_t fs_id):
    mFsId(fs_id), mThread(0)
  {
    // @todo (amanzi) move out to StartDrain func, the constructor is not a
    //                good place for this
    XrdSysThread::Run(&mThread,
                      DrainFS::StaticThreadProc,
                      static_cast<void*>(this),
                      XRDSYSTHREAD_HOLD,
                      "DrainFS Thread");
  }

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  virtual ~DrainFS();

  //----------------------------------------------------------------------------
  //! Stop draining attached file system
  //---------------------------------------------------------------------------
  void DrainStop();

  //----------------------------------------------------------------------------
  // get the list of  Failed  Jobs
  //----------------------------------------------------------------------------
  inline const std::vector<shared_ptr<DrainTransferJob>>& GetFailedJobs() const
  {
    return mJobsFailed;
  }

  //---------------------------------------------------------------------------
  //! Get drain status
  //---------------------------------------------------------------------------
  inline eos::common::FileSystem::eDrainStatus GetDrainStatus() const
  {
    return mDrainStatus;
  }

private:

  //----------------------------------------------------------------------------
  // Thread loop implementing the drain job
  //----------------------------------------------------------------------------
  void* Drain();

  //----------------------------------------------------------------------------
  //! Select target file system using the GeoTreeEngine
  //!
  //! @param job drain job object
  //!
  ///! @return if successful then target file system, othewise 0
  //----------------------------------------------------------------------------
  eos::common::FileSystem::fsid_t SelectTargetFS(DrainTransferJob* job);

  //----------------------------------------------------------------------------
  //! Set initial drain counters and status
  //----------------------------------------------------------------------------
  void SetInitialCounters();

  //----------------------------------------------------------------------------
  //! Get space defined drain variables i.e. number of retires, number of
  //! transfers per fs, etc.
  //----------------------------------------------------------------------------
  void GetSpaceConfiguration();

  //---------------------------------------------------------------------------
  //! Clean up when draining is completed
  //---------------------------------------------------------------------------
  void CompleteDrain();

  eos::common::FileSystem::fsid_t mFsId; ///< Id of the draining file system
  // @todo (amanzi): try using std::thread just like in drain job
  pthread_t mThread; ///< Thead supervising the draining
  eos::common::FileSystem::eDrainStatus mDrainStatus;
  std::string mSpace; ///< Space where fs resides
  std::string mGroup; ///< Group where fs resided
  //! Collection of drain jobs to run
  std::vector<shared_ptr<DrainTransferJob>> mJobsPending;
  //! Collection of failed drain jobs
  std::vector<shared_ptr<DrainTransferJob>> mJobsFailed;
  //! Collection of running drain jobs
  std::vector<shared_ptr<DrainTransferJob>> mJobsRunning;
  bool mDrainStop = false; ///< Flag to cancel an ongoing draining
  int mMaxRetries = 1; ///< Max number of retries
  int maxParallelJobs = 5; ///< Max number of parallel drain jobs
};

EOSMGMNAMESPACE_END
