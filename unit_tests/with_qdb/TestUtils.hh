//------------------------------------------------------------------------------
// File: TestUtils.hh
// Author: Georgios Bitzes - CERN
//------------------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2020 CERN/Switzerland                                  *
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

#include "namespace/ns_quarkdb/QdbContactDetails.hh"
#include <gtest/gtest.h>
#include <memory>

namespace qclient {
  class QClient;
  class SharedManager;
}

namespace eos
{

namespace mq {
  class MessagingRealm;
}

//------------------------------------------------------------------------------
//! Class FlushAllOnConstruction
//------------------------------------------------------------------------------
class FlushAllOnConstruction
{
public:
  FlushAllOnConstruction(const QdbContactDetails& cd);

private:
  QdbContactDetails contactDetails;
};

//------------------------------------------------------------------------------
//! Test fixture providing generic utilities and initialization / destruction
//! boilerplate code
//------------------------------------------------------------------------------
class UnitTestsWithQDBFixture : public ::testing::Test
{
public:
  //----------------------------------------------------------------------------
  //! Constructor
  //----------------------------------------------------------------------------
  UnitTestsWithQDBFixture();

  //----------------------------------------------------------------------------
  //! Destructor
  //----------------------------------------------------------------------------
  ~UnitTestsWithQDBFixture();

  //----------------------------------------------------------------------------
  //! Make QClient object
  //----------------------------------------------------------------------------
  std::unique_ptr<qclient::QClient> makeQClient() const;

  //----------------------------------------------------------------------------
  //! Get MessagingRealm object, lazy init
  //----------------------------------------------------------------------------
  mq::MessagingRealm* getMessagingRealm();

  //----------------------------------------------------------------------------
  //! Get SharedManager object, lazy init
  //----------------------------------------------------------------------------
  qclient::SharedManager* getSharedManager();

  //----------------------------------------------------------------------------
  //! Retrieve contact details
  //----------------------------------------------------------------------------
  QdbContactDetails getContactDetails() const;


private:
  QdbContactDetails mContactDetails;
  std::unique_ptr<FlushAllOnConstruction> mFlushGuard;

  std::unique_ptr<mq::MessagingRealm> mMessagingRealm;
  std::unique_ptr<qclient::SharedManager> mSharedManager;
};

}

