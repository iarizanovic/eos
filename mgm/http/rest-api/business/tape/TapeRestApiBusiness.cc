// ----------------------------------------------------------------------
// File: TapeRestApiBusiness.cc
// Author: Cedric Caffy - CERN
// ----------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2013 CERN/Switzerland                                  *
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

#include "mgm/http/rest-api/exception/ObjectNotFoundException.hh"
#include "TapeRestApiBusiness.hh"
#include "mgm/http/rest-api/model/tape/common/FilesContainer.hh"
#include "mgm/bulk-request/utils/PrepareArgumentsWrapper.hh"
#include "mgm/bulk-request/interface/RealMgmFileSystemInterface.hh"
#include "mgm/bulk-request/prepare/manager/BulkRequestPrepareManager.hh"
#include "mgm/bulk-request/business/BulkRequestBusiness.hh"
#include "mgm/bulk-request/dao/factories/ProcDirectoryDAOFactory.hh"
#include "mgm/http/rest-api/exception/tape/TapeRestApiBusinessException.hh"
#include "mgm/http/rest-api/exception/tape/FileDoesNotBelongToBulkRequestException.hh"
#include "mgm/bulk-request/exception/PersistencyException.hh"

EOSMGMRESTNAMESPACE_BEGIN

std::shared_ptr<bulk::BulkRequest> TapeRestApiBusiness::createStageBulkRequest(const CreateStageBulkRequestModel* model, const common::VirtualIdentity * vid) {
  const FilesContainer & files = model->getFiles();
  bulk::PrepareArgumentsWrapper pargsWrapper(
      "fake_id", Prep_STAGE, files.getPaths(), files.getOpaqueInfos());
  auto prepareManager = createBulkRequestPrepareManager();
  XrdOucErrInfo error;
  int prepareRetCode = prepareManager->prepare(*pargsWrapper.getPrepareArguments(),error,vid);
  if(prepareRetCode != SFS_DATA){
    throw TapeRestApiBusinessException(error.getErrText());
  }
  return prepareManager->getBulkRequest();
}

void TapeRestApiBusiness::cancelStageBulkRequest(const std::string & requestId, const PathsModel* model, const common::VirtualIdentity * vid) {
  std::shared_ptr<bulk::BulkRequestBusiness> bulkRequestBusiness = createBulkRequestBusiness();
  auto bulkRequest = bulkRequestBusiness->getBulkRequest(requestId,bulk::BulkRequest::Type::PREPARE_STAGE);
  if(bulkRequest == nullptr) {
    std::stringstream ss;
    ss << "Unable to find the STAGE bulk-request ID = " << requestId;
    throw ObjectNotFoundException(ss.str());
  }
  //Create the prepare arguments, we will only cancel the files that were given by the user
  const FilesContainer & filesFromClient = model->getFiles();
  auto filesFromBulkRequest = bulkRequest->getFiles();
  FilesContainer filesToCancel;
  for(auto & fileFromClient: filesFromClient.getPaths()) {
    if(filesFromBulkRequest->find(fileFromClient) != filesFromBulkRequest->end()){
      filesToCancel.addFile(fileFromClient);
    } else {
      std::stringstream ss;
      ss << "The file " << fileFromClient << " does not belong to the STAGE request " << bulkRequest->getId() << ". No modification has been made to this request.";
      throw FileDoesNotBelongToBulkRequestException(ss.str());
    }
  }
  //Do the cancellation
  bulk::PrepareArgumentsWrapper pargsWrapper(requestId, Prep_CANCEL,
                                             filesToCancel.getPaths(),
                                             filesToCancel.getOpaqueInfos());
  auto pm = createBulkRequestPrepareManager();
  XrdOucErrInfo error;
  int retCancellation = pm->prepare(*pargsWrapper.getPrepareArguments(),error,vid);
  if(retCancellation != SFS_OK) {
    std::stringstream ss;
    ss << "Unable to cancel the files provided. errMsg=\"" << error.getErrText() << "\"";
    throw TapeRestApiBusinessException(ss.str());
  }
}

std::shared_ptr<bulk::QueryPrepareResponse> TapeRestApiBusiness::getStageBulkRequest(const std::string& requestId,const common::VirtualIdentity * vid) {
  auto bulkRequestBusiness = createBulkRequestBusiness();
  try {
    if (!bulkRequestBusiness->exists(requestId,
                                     bulk::BulkRequest::Type::PREPARE_STAGE)) {
      std::stringstream ss;
      ss << "Unable to find the STAGE bulk-request ID =" << requestId;
      throw ObjectNotFoundException(ss.str());
    }
  } catch(bulk::PersistencyException & ex){
    throw TapeRestApiBusinessException(ex.what());
  }
  //Instanciate prepare manager
  bulk::PrepareArgumentsWrapper pargsWrapper(requestId,Prep_QUERY);
  auto pm = createBulkRequestPrepareManager();
  XrdOucErrInfo error;
  auto queryPrepareResult = pm->queryPrepare(*pargsWrapper.getPrepareArguments(),error,vid);
  if(!queryPrepareResult->hasQueryPrepareFinished()){
    std::stringstream ss;
    ss << "Unable to get information about the request " << requestId <<". errMsg=\"" << error.getErrText() << "\"";
    throw TapeRestApiBusinessException(ss.str());
  }
  return queryPrepareResult->getResponse();
}

void TapeRestApiBusiness::deleteStageBulkRequest(const std::string& requestId, const common::VirtualIdentity* vid) {
  //Get the prepare request from the persistency
  std::shared_ptr<bulk::BulkRequestBusiness> bulkRequestBusiness = createBulkRequestBusiness();
  auto bulkRequest = bulkRequestBusiness->getBulkRequest(requestId,bulk::BulkRequest::Type::PREPARE_STAGE);
  if(bulkRequest == nullptr) {
    std::stringstream ss;
    ss << "Unable to find the STAGE bulk-request ID = " << requestId;
    throw ObjectNotFoundException(ss.str());
  }
  //Create the prepare arguments, we will cancel all the files from this bulk-request
  auto filesFromBulkRequest = bulkRequest->getFiles();
  FilesContainer filesToCancel;
  for(auto & fileFromBulkRequest: *filesFromBulkRequest){
    filesToCancel.addFile(fileFromBulkRequest.first);
  }
  bulk::PrepareArgumentsWrapper pargsWrapper(requestId, Prep_CANCEL,
                                             filesToCancel.getPaths(),
                                             filesToCancel.getOpaqueInfos());
  auto pm = createBulkRequestPrepareManager();
  XrdOucErrInfo error;
  int retCancellation = pm->prepare(*pargsWrapper.getPrepareArguments(),error,vid);
  if(retCancellation != SFS_OK) {
    std::stringstream ss;
    ss << "Unable to cancel the files provided. errMsg=\"" << error.getErrText() << "\"";
    throw TapeRestApiBusinessException(ss.str());
  }
  //Now that the request got cancelled, let's delete it from the persistency
  try {
    bulkRequestBusiness->deleteBulkRequest(bulkRequest.get());
  } catch (bulk::PersistencyException &ex) {
    throw TapeRestApiBusinessException(ex.what());
  }
}

std::shared_ptr<bulk::QueryPrepareResponse> TapeRestApiBusiness::getFileInfo(const PathsModel * model, const common::VirtualIdentity* vid) {
  auto & filesContainer = model->getFiles();
  bulk::PrepareArgumentsWrapper pargsWrapper("fake_id", Prep_QUERY,
                                             filesContainer.getPaths(),
                                             filesContainer.getOpaqueInfos());
  auto pm = createBulkRequestPrepareManager();
  XrdOucErrInfo error;
  auto queryPrepareResult = pm->queryPrepare(*pargsWrapper.getPrepareArguments(),error,vid);
  if(!queryPrepareResult->hasQueryPrepareFinished()){
    std::stringstream ss;
    ss << "Unable to get information about the files provided. errMsg=\"" << error.getErrText() << "\"";
    throw TapeRestApiBusinessException(ss.str());
  }
  return queryPrepareResult->getResponse();
}

void TapeRestApiBusiness::unpinPaths(const PathsModel* model, const common::VirtualIdentity* vid) {
  auto & filesContainer = model->getFiles();
  bulk::PrepareArgumentsWrapper pargsWrapper("fake_id", Prep_EVICT,
                                             filesContainer.getPaths(),
                                             filesContainer.getOpaqueInfos());
  auto pm = createBulkRequestPrepareManager();
  XrdOucErrInfo error;
  int retEvict = pm->prepare(*pargsWrapper.getPrepareArguments(),error,vid);
  if(retEvict != SFS_OK) {
    std::stringstream ss;
    ss << "Unable to unpin the files provided. errMsg=\"" << error.getErrText() << "\"";
    throw TapeRestApiBusinessException(ss.str());
  }
}

std::unique_ptr<bulk::BulkRequestPrepareManager> TapeRestApiBusiness::createBulkRequestPrepareManager() {
  std::unique_ptr<bulk::RealMgmFileSystemInterface> mgmOfs = std::make_unique<bulk::RealMgmFileSystemInterface>(gOFS);
  std::unique_ptr<bulk::BulkRequestPrepareManager> prepareManager = std::make_unique<bulk::BulkRequestPrepareManager>(std::move(mgmOfs));
  std::shared_ptr<bulk::BulkRequestBusiness> bulkRequestBusiness = createBulkRequestBusiness();
  prepareManager->setBulkRequestBusiness(bulkRequestBusiness);
  return prepareManager;
}

std::shared_ptr<bulk::BulkRequestBusiness> TapeRestApiBusiness::createBulkRequestBusiness() {
  std::unique_ptr<bulk::AbstractDAOFactory> daoFactory(new bulk::ProcDirectoryDAOFactory(gOFS,*gOFS->mProcDirectoryBulkRequestTapeRestApiLocations));
  return std::make_shared<bulk::BulkRequestBusiness>(std::move(daoFactory));
}

EOSMGMRESTNAMESPACE_END