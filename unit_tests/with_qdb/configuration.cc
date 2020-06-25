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

//------------------------------------------------------------------------------
//! @author Georgios Bitzes - CERN
//! @brief Tests related to the configuration
//------------------------------------------------------------------------------

#include "unit_tests/with_qdb/TestUtils.hh"
#include "mgm/config/QuarkConfigHandler.hh"

#include <qclient/QClient.hh>

using namespace eos;
class VariousTests : public eos::UnitTestsWithQDBFixture {};
class ConfigurationTests : public eos::UnitTestsWithQDBFixture {};

TEST_F(VariousTests, Ping) {
  std::unique_ptr<qclient::QClient> qcl = makeQClient();

  qclient::redisReplyPtr reply = qcl->exec("PING").get();
  ASSERT_EQ(qclient::describeRedisReply(reply),
    "PONG");
}

TEST_F(ConfigurationTests, BasicFetch) {
  std::unique_ptr<qclient::QClient> qcl = makeQClient();

  qclient::redisReplyPtr reply = qcl->exec("HSET", "eos-config:default", "a", "b").get();
  ASSERT_EQ(qclient::describeRedisReply(reply), "(integer) 1");

  eos::mgm::QuarkConfigHandler ch(getContactDetails());

  std::map<std::string, std::string> cfmap;
  ASSERT_TRUE(ch.fetchConfiguration("default", cfmap));

  ASSERT_EQ(cfmap.size(), 1u);
  ASSERT_EQ(cfmap["a"], "b");

  bool exists = false;
  ASSERT_TRUE(ch.checkExistence("default", exists));
  ASSERT_TRUE(exists);

  ASSERT_TRUE(ch.checkExistence("default-2", exists));
  ASSERT_FALSE(exists);

  qclient::redisReplyPtr reply2 = qcl->exec("SADD", "eos-config:default-3", "a", "b").get();
  ASSERT_EQ(qclient::describeRedisReply(reply2), "(integer) 2");

  common::Status st = ch.checkExistence("default-3", exists);
  ASSERT_FALSE(st);
  ASSERT_EQ(st.toString(), "(22): Received unexpected response in HLEN existence check: Unexpected reply type; was expecting INTEGER, received (error) ERR Invalid argument: WRONGTYPE Operation against a key holding the wrong kind of value");
}

TEST_F(ConfigurationTests, Listing) {
  std::unique_ptr<qclient::QClient> qcl = makeQClient();

  qclient::redisReplyPtr reply = qcl->exec("HSET", "eos-config:default", "a", "b").get();
  ASSERT_EQ(qclient::describeRedisReply(reply), "(integer) 1");

  qclient::redisReplyPtr reply2 = qcl->exec("HSET", "eos-config:default-2", "a", "b").get();
  ASSERT_EQ(qclient::describeRedisReply(reply2), "(integer) 1");

  qclient::redisReplyPtr reply3 = qcl->exec("HSET", "eos-config-backup:default-1", "a", "b").get();
  ASSERT_EQ(qclient::describeRedisReply(reply3), "(integer) 1");

  eos::mgm::QuarkConfigHandler ch(getContactDetails());

  std::vector<std::string> configs, backups;
  ASSERT_TRUE(ch.listConfigurations(configs, backups));

  ASSERT_EQ(configs.size(), 2u);
  ASSERT_EQ(configs[0], "eos-config:default");
  ASSERT_EQ(configs[1], "eos-config:default-2");

  ASSERT_EQ(backups.size(), 1u);
  ASSERT_EQ(backups[0], "eos-config-backup:default-1");
}

TEST_F(ConfigurationTests, WriteRead) {
  eos::mgm::QuarkConfigHandler ch(getContactDetails());
  ASSERT_TRUE(ch.checkConnection(std::chrono::seconds(1)));

  std::map<std::string, std::string> configuration, configuration2;
  configuration["a"] = "b";
  configuration["c"] = "d";

  ASSERT_TRUE(ch.writeConfiguration("default", configuration, false).get());
  ASSERT_TRUE(ch.fetchConfiguration("default", configuration2));
  ASSERT_EQ(configuration, configuration2);

  ASSERT_FALSE(ch.writeConfiguration("default", configuration, false).get());
  configuration["d"] = "e";
  ASSERT_TRUE(ch.writeConfiguration("default", configuration, true).get());

  ASSERT_FALSE(configuration == configuration2);
  ASSERT_TRUE(ch.fetchConfiguration("default", configuration2));
  ASSERT_EQ(configuration, configuration2);
}

TEST_F(ConfigurationTests, HashKeys) {
  ASSERT_EQ(eos::mgm::QuarkConfigHandler::formHashKey("default"), "eos-config:default");
  ASSERT_EQ(eos::mgm::QuarkConfigHandler::formBackupHashKey("default", 1588936606), "eos-config-backup:default-20200508111646");
}

TEST_F(ConfigurationTests, TailLog) {
  std::unique_ptr<qclient::QClient> qcl = makeQClient();

  qclient::redisReplyPtr reply = qcl->exec("deque-push-back", "eos-config-changelog", "aaa", "bbb", "ccc", "ddd", "eee").get();
  ASSERT_EQ(qclient::describeRedisReply(reply), "(integer) 5");

  std::vector<std::string> changelog;
  eos::mgm::QuarkConfigHandler ch(getContactDetails());

  ASSERT_TRUE(ch.tailChangelog(100, changelog));
  ASSERT_EQ(changelog.size(), 5u);
  ASSERT_EQ(changelog[0], "aaa");
  ASSERT_EQ(changelog[1], "bbb");
  ASSERT_EQ(changelog[2], "ccc");
  ASSERT_EQ(changelog[3], "ddd");
  ASSERT_EQ(changelog[4], "eee");

  ASSERT_TRUE(ch.tailChangelog(2, changelog));
  ASSERT_EQ(changelog.size(), 2u);
  ASSERT_EQ(changelog[0], "ddd");
  ASSERT_EQ(changelog[1], "eee");
}

TEST_F(ConfigurationTests, AppendChangelog) {
  eos::mgm::ConfigChangelogEntry entry;

  eos::mgm::ConfigModification *modif = entry.add_modifications();
  modif->set_key("aa");
  modif->set_previous_value("b");
  modif->set_new_value("c");

  eos::mgm::QuarkConfigHandler ch(getContactDetails());

  common::Status st = ch.appendChangelog(entry).get();
  ASSERT_TRUE(st) << st.toString();
}
