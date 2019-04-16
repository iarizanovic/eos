/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2016 CERN/Switzerland                                  *
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

#include "namespace/ns_quarkdb/persistency/FileMDSvc.hh"
#include "namespace/ns_quarkdb/Constants.hh"
#include "namespace/ns_quarkdb/FileMD.hh"
#include "namespace/ns_quarkdb/BackendClient.hh"
#include "namespace/ns_quarkdb/accounting/QuotaStats.hh"
#include "namespace/ns_quarkdb/persistency/ContainerMDSvc.hh"
#include "namespace/ns_quarkdb/flusher/MetadataFlusher.hh"
#include "namespace/ns_quarkdb/persistency/MetadataFetcher.hh"
#include "namespace/ns_quarkdb/persistency/MetadataProvider.hh"
#include "namespace/ns_quarkdb/persistency/RequestBuilder.hh"
#include "namespace/ns_quarkdb/ConfigurationParser.hh"
#include "namespace/ns_quarkdb/QdbContactDetails.hh"
#include "namespace/utils/StringConvertion.hh"
#include <numeric>

EOSNSNAMESPACE_BEGIN

std::chrono::seconds QuarkFileMDSvc::sFlushInterval(5);

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
QuarkFileMDSvc::QuarkFileMDSvc(qclient::QClient *qcl, MetadataFlusher *flusher)
  : pQuotaStats(nullptr), pContSvc(nullptr), pFlusher(flusher), pQcl(qcl),
    mMetaMap(), mNumFiles(0ull) {}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
QuarkFileMDSvc::~QuarkFileMDSvc()
{
  if (pFlusher) {
    pFlusher->synchronize();
  }
}

//------------------------------------------------------------------------------
// Configure the file service
//------------------------------------------------------------------------------
void
QuarkFileMDSvc::configure(const std::map<std::string, std::string>& config)
{
  std::string qdb_cluster;
  std::string qdb_flusher_id;
  const std::string key_flusher = "qdb_flusher_md";

  if (config.find(key_flusher) != config.end()) {
    // This should only be called once during booting but then the rest of the
    // config values can be updated also while running
    QdbContactDetails contactDetails = ConfigurationParser::parse(config);
    mMetaMap.setKey(constants::sMapMetaInfoKey);
    mMetaMap.setClient(*pQcl);
    mUnifiedInodeProvider.configure(mMetaMap);
    mMetadataProvider.reset(new MetadataProvider(contactDetails, pContSvc, this));
    static_cast<QuarkContainerMDSvc*>(pContSvc)->setMetadataProvider
    (mMetadataProvider.get());
    static_cast<QuarkContainerMDSvc*>(pContSvc)->setInodeProvider
    (&mUnifiedInodeProvider);
  }

  if (config.find(constants::sMaxNumCacheFiles) != config.end()) {
    std::string val = config.at(constants::sMaxNumCacheFiles);
    mMetadataProvider->setFileMDCacheNum(std::stoull(val));
  }
}

//------------------------------------------------------------------------------
// Initialize the file service
//------------------------------------------------------------------------------
void
QuarkFileMDSvc::initialize()
{
  if (pContSvc == nullptr) {
    MDException e(EINVAL);
    e.getMessage()  << __FUNCTION__ << " FileMDSvc: container service not set";
    throw e;
  }

  if ((pQcl == nullptr) || (pFlusher == nullptr)) {
    MDException e(EINVAL);
    e.getMessage()  << __FUNCTION__ << " No qclient/flusher initialized for "
                    << "the container metadata service";
    throw e;
  }

  SafetyCheck();
  mNumFiles.store(pQcl->execute(
                    RequestBuilder::getNumberOfFiles()).get()->integer);
}

//------------------------------------------------------------------------------
// Safety check to make sure there are no file entries in the backend with
// ids bigger than the max file id.
//------------------------------------------------------------------------------
void
QuarkFileMDSvc::SafetyCheck()
{
  std::string blob;
  IFileMD::id_t free_id = getFirstFreeId();
  std::list<uint64_t> offsets {1, 10, 50, 100, 501, 1001, 11000, 50000,
                               100000, 150199, 200001, 1000002, 2000123 };
  std::vector<folly::Future<eos::ns::FileMdProto>> futs;

  for (auto incr : offsets) {
    IFileMD::id_t check_id = free_id + incr;
    futs.emplace_back(MetadataFetcher::getFileFromId(*pQcl,
                      FileIdentifier(check_id)));
  }

  for (size_t i = 0; i < futs.size(); i++) {
    try {
      futs[i].get();
    } catch (eos::MDException& qdb_err) {
      // All is good, we didn't find any file, as expected
      continue;
    }

    // Uh-oh, this is bad.
    MDException e(EEXIST);
    e.getMessage()  << __FUNCTION__ << " FATAL: Risk of data loss, found "
                    << "file with id bigger than max file id";
    throw e;
  }
}

//------------------------------------------------------------------------------
// Get the file metadata information for the given file id - asynchronous API.
//------------------------------------------------------------------------------
folly::Future<IFileMDPtr>
QuarkFileMDSvc::getFileMDFut(IFileMD::id_t id)
{
  return mMetadataProvider->retrieveFileMD(FileIdentifier(id));
}

//------------------------------------------------------------------------------
// Get the file metadata information for the given file id
//------------------------------------------------------------------------------
std::shared_ptr<IFileMD>
QuarkFileMDSvc::getFileMD(IFileMD::id_t id, uint64_t* clock)
{
  IFileMDPtr file = mMetadataProvider->retrieveFileMD(FileIdentifier(id)).get();

  if (file && clock) {
    *clock = file->getClock();
  }

  return file;
}

//------------------------------------------------------------------------------
//! Check if a FileMD with a given identifier exists
//------------------------------------------------------------------------------
folly::Future<bool>
QuarkFileMDSvc::hasFileMD(const eos::FileIdentifier id)
{
  return mMetadataProvider->hasFileMD(id);
}

//------------------------------------------------------------------------------
// Create new file metadata object
//------------------------------------------------------------------------------
std::shared_ptr<IFileMD>
QuarkFileMDSvc::createFile()
{
  uint64_t free_id = mUnifiedInodeProvider.reserveFileId();
  std::shared_ptr<IFileMD> file{new QuarkFileMD(free_id, this)};
  mMetadataProvider->insertFileMD(file->getIdentifier(), file);
  IFileMDChangeListener::Event e(file.get(), IFileMDChangeListener::Created);
  notifyListeners(&e);
  ++mNumFiles;
  return file;
}

//------------------------------------------------------------------------------
// Update backend store and notify all the listeners
//------------------------------------------------------------------------------
void
QuarkFileMDSvc::updateStore(IFileMD* obj)
{
  pFlusher->execute(RequestBuilder::writeFileProto(obj));

  // If file is detached then add it to the list of orphans
  if (obj->getContainerId() == 0) {
    pFlusher->sadd(constants::sOrphanFiles, stringify(obj->getId()));
  }
}

//------------------------------------------------------------------------------
// Remove object from the store
//------------------------------------------------------------------------------
void
QuarkFileMDSvc::removeFile(IFileMD* obj)
{
  std::string sid = stringify(obj->getId());
  pFlusher->execute(RequestBuilder::deleteFileProto(FileIdentifier(
                      obj->getId())));
  pFlusher->srem(constants::sOrphanFiles, sid);
  IFileMDChangeListener::Event e(obj, IFileMDChangeListener::Deleted);
  notifyListeners(&e);
  obj->setDeleted();

  if (mNumFiles) {
    --mNumFiles;
  }
}

//------------------------------------------------------------------------------
// Get number of files
//------------------------------------------------------------------------------
uint64_t
QuarkFileMDSvc::getNumFiles()
{
  return mNumFiles.load();
}

//------------------------------------------------------------------------------
// Attach a broken file to lost+found
//------------------------------------------------------------------------------
void
QuarkFileMDSvc::attachBroken(const std::string& parent, IFileMD* file)
{
  std::ostringstream s1, s2;
  std::shared_ptr<IContainerMD> parentCont =
    pContSvc->getLostFoundContainer(parent);
  s1 << file->getContainerId();
  std::shared_ptr<IContainerMD> cont = parentCont->findContainer(s1.str());

  if (!cont) {
    cont = pContSvc->createInParent(s1.str(), parentCont.get());
  }

  s2 << file->getName() << "." << file->getId();
  file->setName(s2.str());
  cont->addFile(file);
}

//------------------------------------------------------------------------------
// Add file listener
//------------------------------------------------------------------------------
void
QuarkFileMDSvc::addChangeListener(IFileMDChangeListener* listener)
{
  pListeners.push_back(listener);
}

//------------------------------------------------------------------------------
// Notify the listeners about the change
//------------------------------------------------------------------------------
void
QuarkFileMDSvc::notifyListeners(IFileMDChangeListener::Event* event)
{
  for (const auto& elem : pListeners) {
    elem->fileMDChanged(event);
  }
}

//------------------------------------------------------------------------------
// Set container service
//------------------------------------------------------------------------------
void
QuarkFileMDSvc::setContMDService(IContainerMDSvc* cont_svc)
{
  QuarkContainerMDSvc* impl_cont_svc = dynamic_cast<eos::QuarkContainerMDSvc*>
                                       (cont_svc);

  if (!impl_cont_svc) {
    MDException e(EFAULT);
    e.getMessage() << __FUNCTION__ << " ContainerMDSvc dynamic cast failed";
    throw e;
  }

  pContSvc = impl_cont_svc;
}

//------------------------------------------------------------------------------
// Get first free file id
//------------------------------------------------------------------------------
IFileMD::id_t
QuarkFileMDSvc::getFirstFreeId()
{
  return mUnifiedInodeProvider.getFirstFreeFileId();
}

//------------------------------------------------------------------------------
//! Retrieve MD cache statistics.
//------------------------------------------------------------------------------
CacheStatistics QuarkFileMDSvc::getCacheStatistics()
{
  return mMetadataProvider->getFileMDCacheStats();
}

EOSNSNAMESPACE_END
