#include "XrdCommon/XrdCommonMapping.hh"
#include "XrdCommon/XrdCommonStringStore.hh"

/*----------------------------------------------------------------------------*/
XrdSysMutex                       XrdCommonMapping::gMapMutex;
XrdCommonMapping::UserRoleMap     XrdCommonMapping::gUserRoleVector;
XrdCommonMapping::GroupRoleMap    XrdCommonMapping::gGroupRoleVector;
XrdCommonMapping::VirtualUserMap  XrdCommonMapping::gVirtualUidMap;
XrdCommonMapping::VirtualGroupMap XrdCommonMapping::gVirtualGidMap;
XrdCommonMapping::SudoerMap       XrdCommonMapping::gSudoerMap;
XrdOucHash<XrdCommonMapping::id_pair>    XrdCommonMapping::gPhysicalUidCache;
XrdOucHash<XrdCommonMapping::gid_vector> XrdCommonMapping::gPhysicalGidCache;
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
void 
XrdCommonMapping::IdMap(const XrdSecEntity* client,const char* env, const char* tident, XrdCommonMapping::VirtualIdentity &vid)
{
  // you first are 'nobody'
  Nobody(vid);
  
  if (!client) 
    return;

  // first map by alias
  XrdOucString useralias = client->prot;
  useralias += ":";
  useralias += "\"";
  useralias += client->name;
  useralias += "\"";
  useralias += ":";
  XrdOucString groupalias=useralias;
  useralias += "uid";
  groupalias+= "gid";


  gMapMutex.Lock();

  if ((gVirtualUidMap.count("krb:<pwd>:uid")) || (gVirtualUidMap.count("ssl:<pwd>:uid"))) {
    eos_static_debug("physical mapping");

    // use physical mapping for kerberos names
    XrdCommonMapping::getPhysicalIds(client->name, vid);
    vid.gid=99;
    vid.gid_list.clear();
  }
  
  if ((gVirtualGidMap.count("krb:<pwd>:gid")) || (gVirtualGidMap.count("ssl:<pwd>:gid"))) {
    eos_static_debug("physical mapping");
    // use physical mapping for kerberos names
    uid_t uid = vid.uid;
    XrdCommonMapping::getPhysicalIds(client->name, vid);
    vid.uid = uid;
    vid.uid_list.clear();
    vid.uid_list.push_back(uid);
  }

  // explicit virtual mapping overrules physical mappings
  vid.uid = (gVirtualUidMap.count(useralias.c_str())) ?gVirtualUidMap[useralias.c_str() ]:99;
  if (!HasUid(vid.uid,vid.uid_list)) vid.uid_list.insert(vid.uid_list.begin(),vid.uid);
  vid.gid = (gVirtualGidMap.count(groupalias.c_str()))?gVirtualGidMap[groupalias.c_str()]:99;

  eos_static_debug("mapped %d %d", vid.uid,vid.gid);
  if (!HasGid(vid.gid,vid.gid_list)) vid.gid_list.insert(vid.gid_list.begin(),vid.gid);

  // add virtual user and group roles - if any 
  if (gUserRoleVector.count(vid.uid)) {
    uid_vector::const_iterator it;
    for (it = gUserRoleVector[vid.uid].begin(); it != gUserRoleVector[vid.uid].end(); ++it)
      if (!HasUid((*it),vid.uid_list)) vid.uid_list.push_back((*it));
  }

  if (gUserRoleVector.count(vid.gid)) {
    gid_vector::const_iterator it;
    for (it = gGroupRoleVector[vid.gid].begin(); it != gGroupRoleVector[vid.gid].end(); ++it)
      if (!HasGid((*it),vid.gid_list)) vid.gid_list.push_back((*it));
  }


  // select roles


  // set sudoer flag
  if (gSudoerMap.count(vid.uid)) {
    vid.sudoer = true;
  }
  gMapMutex.UnLock();
}

/*----------------------------------------------------------------------------*/
void
XrdCommonMapping::Print(XrdOucString &stdOut, XrdOucString option)
{
  if ((!option.length()) || ( (option.find("u"))!=STR_NPOS)) {
    UserRoleMap::const_iterator it;
    for ( it = gUserRoleVector.begin(); it != gUserRoleVector.end(); ++it) {
      char iuid[4096];
      sprintf(iuid,"%d", it->first);
      char suid[4096];
      sprintf(suid,"%-6s",iuid);
      stdOut += "membership uid: ";stdOut += suid;
      stdOut += " => uids(";
      for ( unsigned int i=0; i< (it->second).size(); i++) {
	stdOut += (int) (it->second)[i];
	if (i < ((it->second).size()-1))
	  stdOut += ",";
      }
      stdOut += ")\n";
    }
  }

  if ((!option.length()) || ( (option.find("g"))!=STR_NPOS)) {
    UserRoleMap::const_iterator it;
    for ( it = gGroupRoleVector.begin(); it != gGroupRoleVector.end(); ++it) {
      char iuid[4096];
      sprintf(iuid,"%d", it->first);
      char suid[4096];
      sprintf(suid,"%-6s",iuid);
      stdOut += "membership uid: ";stdOut += suid;
      stdOut += " => gids(";
      for ( unsigned int i=0; i< (it->second).size(); i++) {
	stdOut += (int) (it->second)[i];
	if (i < ((it->second).size()-1))
	  stdOut += ",";
      }
      stdOut += ")\n";
    }
  }

  if ((!option.length()) || ( (option.find("s"))!=STR_NPOS)) {
    SudoerMap::const_iterator it;
    // print sudoer line
    stdOut += "sudoer                 => uids(";
    for ( it = gSudoerMap.begin() ;it != gSudoerMap.end(); ++it) {  
      if (it->second) {
	stdOut += (int) (it->first);
	stdOut += ",";
      }
    }
    if (stdOut.endswith(",")) {
      stdOut.erase(stdOut.length()-1);
    }
    stdOut += ")\n";
  }

  if ((!option.length()) || ( (option.find("U"))!=STR_NPOS)) {
    VirtualUserMap::const_iterator it;
    for ( it = gVirtualUidMap.begin(); it != gVirtualUidMap.end(); ++it) {
      stdOut += it->first.c_str(); stdOut += " => "; stdOut += (int)it->second; stdOut += "\n";
    }
  }

  if ((!option.length()) || ( (option.find("G"))!=STR_NPOS)) {
    VirtualGroupMap::const_iterator it;
    for ( it = gVirtualGidMap.begin(); it != gVirtualGidMap.end(); ++it) {
      stdOut += it->first.c_str(); stdOut += " => "; stdOut += (int)it->second; stdOut += "\n";
    }
  }
}

/*----------------------------------------------------------------------------*/
void
XrdCommonMapping::getPhysicalIds(const char* name, VirtualIdentity &vid)
{
  struct group* gr;
  struct passwd passwdinfo;
  char buffer[1024];

  memset(&passwdinfo,0, sizeof(passwdinfo))
;
  gid_vector* gv;
  id_pair* id;

  // cache short cut's
  if (!(id = gPhysicalUidCache.Find(name))) {
    struct passwd **pwbufp=0;
    
    if (!getpwnam_r(name, &passwdinfo, buffer, 1024, pwbufp)) 
      return;
    id = new id_pair(passwdinfo.pw_uid, passwdinfo.pw_gid);
    gPhysicalUidCache.Add(name, id, 60);
  };

  if ((gv = gPhysicalGidCache.Find(name))) {
    vid.uid_list.push_back(id->uid);
    vid.gid_list = *gv;
    vid.uid = id->uid;
    vid.gid = id->gid;
    return; 
  }

  gid_t gid = id->gid;

  setgrent();

  while( (gr = getgrent() ) ) {
    int cnt;
    cnt=0;
    if (gr->gr_gid == gid) {
      if (!vid.gid_list.size()) {
	vid.gid_list.push_back(gid);
	vid.gid = gid;
      }
    }

    while (gr->gr_mem[cnt]) {
      if (!strcmp(gr->gr_mem[cnt],name)) {
	vid.gid_list.push_back(gr->gr_gid);
      }
      cnt++;
    }
  }
  endgrent();

  // add to the cache
  gid_vector* vec = new uid_vector;
  *vec = vid.gid_list;
  gPhysicalGidCache.Add(name,vec, 60);

  return ;
}
