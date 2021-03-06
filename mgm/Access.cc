// ----------------------------------------------------------------------
// File: Access.cc
// Author: Andreas-Joachim Peters - CERN
// ----------------------------------------------------------------------

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

/*----------------------------------------------------------------------------*/
#include "mgm/Namespace.hh"
#include "mgm/Access.hh"
#include "mgm/FsView.hh"
/*----------------------------------------------------------------------------*/


EOSMGMNAMESPACE_BEGIN

/*----------------------------------------------------------------------------*/
//! singleton set for banned user IDs
std::set<uid_t> Access::gBannedUsers;

//! singleton set for banned group IDS
std::set<gid_t> Access::gBannedGroups;

//! singleton set for banned host names
std::set<std::string> Access::gBannedHosts;

//! singleton set for allowed user IDs
std::set<uid_t> Access::gAllowedUsers;

//! singleton set for allowed group IDs
std::set<gid_t> Access::gAllowedGroups;

//! singleton set for allowed host names
std::set<std::string> Access::gAllowedHosts;

//! singleton map for redirection rules
std::map<std::string, std::string> Access::gRedirectionRules;

//! singleton map for stall rules
std::map<std::string, std::string> Access::gStallRules;

//! singleton map for stall comments
std::map<std::string, std::string> Access::gStallComment;

//! indicates global stall rule
bool Access::gStallGlobal = false;

//! indicates global read stall        
bool Access::gStallRead = false;

//! indicates global write stall       
bool Access::gStallWrite = false;

//! indicates a user or group rate stall entry
bool Access::gStallUserGroup = false;

//! singleton map for UID based redirection (not used yet)
std::map<uid_t, std::string> Access::gUserRedirection;

//! singleton map for GID based redirection (not used yet)
std::map<gid_t, std::string> Access::gGroupRedirection;

//! global rw mutex protecting all static singletons
eos::common::RWMutex Access::gAccessMutex;

/*----------------------------------------------------------------------------*/
//! constant used in the configuration store
const char* Access::gUserKey = "BanUsers";

//! constant used in the configuration store
const char* Access::gGroupKey = "BanGroups";

//! constant used in the configuration store
const char* Access::gHostKey = "BanHosts";

//! constant used in the configuration store
const char* Access::gAllowedUserKey = "AllowedUsers";

//! constant used in the configuration store
const char* Access::gAllowedGroupKey = "AllowedGroups";

//! constant used in the configuration store
const char* Access::gAllowedHostKey = "AllowedHosts";

//! constant used in the configuration store
const char* Access::gStallKey = "Stall";

//! constant used in the configuration store
const char* Access::gRedirectionKey = "Redirection";

/*----------------------------------------------------------------------------*/
void
Access::Reset ()
/*----------------------------------------------------------------------------*/
/** 
 * @brief Static function to reset all singleton objects defining access rules.
 * 
 */
/*----------------------------------------------------------------------------*/
{
  eos::common::RWMutexWriteLock lock(Access::gAccessMutex);
  Access::gBannedUsers.clear();
  Access::gBannedGroups.clear();
  Access::gBannedHosts.clear();
  Access::gAllowedUsers.clear();
  Access::gAllowedGroups.clear();
  Access::gAllowedHosts.clear();
  Access::gRedirectionRules.clear();
  Access::gStallRules.clear();
  Access::gStallComment.clear();
  Access::gUserRedirection.clear();
  Access::gGroupRedirection.clear();
  Access::gStallGlobal = Access::gStallRead = \
    Access::gStallWrite = Access::gStallUserGroup = false;
}

/*----------------------------------------------------------------------------*/
void
Access::ApplyAccessConfig (bool applyredirectandstall)
/*----------------------------------------------------------------------------*/
/** 
 * @brief Static function to retrieve the access configuration from the global 
 * configuration and apply to the static singleton rules.
 */
/*----------------------------------------------------------------------------*/
{
  Access::Reset();
  eos::common::RWMutexWriteLock lock(Access::gAccessMutex);
  std::string userval = FsView::gFsView.GetGlobalConfig(gUserKey);
  std::string groupval = FsView::gFsView.GetGlobalConfig(gGroupKey);
  std::string hostval = FsView::gFsView.GetGlobalConfig(gHostKey);

  std::string useraval = FsView::gFsView.GetGlobalConfig(gAllowedUserKey);
  std::string groupaval = FsView::gFsView.GetGlobalConfig(gAllowedGroupKey);
  std::string hostaval = FsView::gFsView.GetGlobalConfig(gAllowedHostKey);

  std::string stall = FsView::gFsView.GetGlobalConfig(gStallKey);
  std::string redirect = FsView::gFsView.GetGlobalConfig(gRedirectionKey);

  // parse the list's and fill the hash
  std::vector<std::string> tokens;
  std::string delimiter = ":";

  std::vector<std::string> subtokens;
  std::string subdelimiter = "~";

  tokens.clear();
  eos::common::StringConversion::Tokenize(userval, tokens, delimiter);

  for (size_t i = 0; i < tokens.size(); i++)
  {
    if (tokens[i].length())
    {
      uid_t uid = atoi(tokens[i].c_str());
      Access::gBannedUsers.insert(uid);
    }
  }

  tokens.clear();
  eos::common::StringConversion::Tokenize(groupval, tokens, delimiter);
  for (size_t i = 0; i < tokens.size(); i++)
  {
    if (tokens[i].length())
    {
      gid_t gid = atoi(tokens[i].c_str());
      Access::gBannedGroups.insert(gid);
    }
  }

  tokens.clear();
  eos::common::StringConversion::Tokenize(hostval, tokens, delimiter);
  for (size_t i = 0; i < tokens.size(); i++)
  {
    if (tokens[i].length())
    {
      Access::gBannedHosts.insert(tokens[i]);
    }
  }

  tokens.clear();
  eos::common::StringConversion::Tokenize(useraval, tokens, delimiter);
  for (size_t i = 0; i < tokens.size(); i++)
  {
    if (tokens[i].length())
    {
      uid_t uid = atoi(tokens[i].c_str());
      Access::gAllowedUsers.insert(uid);
    }
  }

  tokens.clear();
  eos::common::StringConversion::Tokenize(groupaval, tokens, delimiter);
  for (size_t i = 0; i < tokens.size(); i++)
  {
    if (tokens[i].length())
    {
      gid_t gid = atoi(tokens[i].c_str());
      Access::gAllowedGroups.insert(gid);
    }
  }

  tokens.clear();
  eos::common::StringConversion::Tokenize(hostaval, tokens, delimiter);
  for (size_t i = 0; i < tokens.size(); i++)
  {
    if (tokens[i].length())
    {
      Access::gAllowedHosts.insert(tokens[i]);
    }
  }

  if (applyredirectandstall)
  {
    tokens.clear();
    delimiter = ",";
    eos::common::StringConversion::Tokenize(stall, tokens, delimiter);
    for (size_t i = 0; i < tokens.size(); i++)
    {
      if (tokens[i].length())
      {
        subtokens.clear();
        eos::common::StringConversion::Tokenize(tokens[i],
                                                subtokens,
                                                subdelimiter);
        if (subtokens.size() >= 2)
        {
          Access::gStallRules[subtokens[0]] = subtokens[1];
          if (subtokens[0] == ("r:*"))
          {
            gStallRead = true;
          }
          if (subtokens[0] == ("w:*"))
          {
            gStallWrite = true;
          }
          if (subtokens[0] == ("*"))
          {
            gStallGlobal = true;
          }
           if ((subtokens[0].find("rate:") == 0))
          {
            gStallUserGroup = true;
          }
          if (subtokens.size() == 3)
          {
            XrdOucString comment = subtokens[2].c_str();
            while (comment.replace("_#KOMMA#_", ","))
            {
            }
            while (comment.replace("_#TILDE#_", "~"))
            {
            }
            Access::gStallComment[subtokens[0]] = comment.c_str();
          }
        }
      }
    }

    tokens.clear();
    delimiter = ",";
    eos::common::StringConversion::Tokenize(redirect, tokens, delimiter);
    for (size_t i = 0; i < tokens.size(); i++)
    {
      if (tokens[i].length())
      {
        subtokens.clear();
        eos::common::StringConversion::Tokenize(tokens[i],
                                                subtokens,
                                                subdelimiter);
        if (subtokens.size() == 2)
        {
         Access::gRedirectionRules[subtokens[0]] = subtokens[1];
        }
      }
    }
  }
}

/*----------------------------------------------------------------------------*/
bool
Access::StoreAccessConfig ()
/*----------------------------------------------------------------------------*/
/** 
 * @brief Static function to store all defined rules in the global configuration.
 *  
 * @return true if successful, otherwise false
 */
/*----------------------------------------------------------------------------*/
{
  std::set<uid_t>::const_iterator ituid;
  std::set<gid_t>::const_iterator itgid;
  std::set<std::string>::const_iterator ithost;
  std::map<std::string, std::string>::const_iterator itstall;
  std::map<std::string, std::string>::const_iterator itredirect;

  std::string userval = "";
  std::string groupval = "";
  std::string hostval = "";
  std::string useraval = "";
  std::string groupaval = "";
  std::string hostaval = "";
  std::string stall = "";
  std::string redirect = "";

  for (ituid = Access::gBannedUsers.begin();
       ituid != Access::gBannedUsers.end(); ituid++)
  {
    userval += eos::common::Mapping::UidAsString(*ituid);
    userval += ":";
  }
  for (itgid = Access::gBannedGroups.begin();
       itgid != Access::gBannedGroups.end(); itgid++)
  {
    groupval += eos::common::Mapping::GidAsString(*itgid);
    groupval += ":";
  }
  for (ithost = Access::gBannedHosts.begin();
       ithost != Access::gBannedHosts.end(); ithost++)
  {
    hostval += ithost->c_str();
    hostval += ":";
  }

  for (ituid = Access::gAllowedUsers.begin();
       ituid != Access::gAllowedUsers.end(); ituid++)
  {
    useraval += eos::common::Mapping::UidAsString(*ituid);
    useraval += ":";
  }
  for (itgid = Access::gAllowedGroups.begin();
       itgid != Access::gAllowedGroups.end(); itgid++)
  {
    groupaval += eos::common::Mapping::GidAsString(*itgid);
    groupaval += ":";
  }
  for (ithost = Access::gAllowedHosts.begin();
       ithost != Access::gAllowedHosts.end(); ithost++)
  {
    hostaval += ithost->c_str();
    hostaval += ":";
  }

  gStallRead = gStallWrite = gStallGlobal = gStallUserGroup = false;
  for (itstall = Access::gStallRules.begin();
       itstall != Access::gStallRules.end(); itstall++)
  {
    stall += itstall->first.c_str();
    stall += "~";
    stall += itstall->second.c_str();
    stall += "~";
    XrdOucString comment = Access::gStallComment[itstall->first].c_str();
    while (comment.replace(",", "_#KOMMA#_"))
    {
    }
    while (comment.replace("~", "_#TILDE#_"))
    {
    }
    stall += comment.c_str();
    stall += ",";

    if (itstall->first == ("r:*"))
    {
      gStallRead = true;
    }
    if (itstall->first == ("w:*"))
    {
      gStallWrite = true;
    }
    if (itstall->first == ("*"))
    {
      gStallGlobal = true;
    }
    if ((itstall->first.find("rate:") == 0))
    {
      gStallUserGroup = true;
    }
  }

  for (itredirect = Access::gRedirectionRules.begin();
       itredirect != Access::gRedirectionRules.end(); itredirect++)
  {
    redirect += itredirect->first.c_str();
    redirect += "~";
    redirect += itredirect->second.c_str();
    redirect += ",";
  }


  std::string ukey = gUserKey;
  std::string gkey = gGroupKey;
  std::string hkey = gHostKey;
  std::string uakey = gAllowedUserKey;
  std::string gakey = gAllowedGroupKey;
  std::string hakey = gAllowedHostKey;

  bool ok = 1;
  ok &= FsView::gFsView.SetGlobalConfig(ukey, userval);
  ok &= FsView::gFsView.SetGlobalConfig(gkey, groupval);
  ok &= FsView::gFsView.SetGlobalConfig(hkey, hostval);
  ok &= FsView::gFsView.SetGlobalConfig(uakey, useraval);
  ok &= FsView::gFsView.SetGlobalConfig(gakey, groupaval);
  ok &= FsView::gFsView.SetGlobalConfig(hakey, hostaval);
  ok &= FsView::gFsView.SetGlobalConfig(gStallKey, stall);
  ok &= FsView::gFsView.SetGlobalConfig(gRedirectionKey, redirect);

  if (!ok)
  {
    eos_static_err("unable to store <access> configuration");
    return false;
  }
  return true;
}

EOSMGMNAMESPACE_END








