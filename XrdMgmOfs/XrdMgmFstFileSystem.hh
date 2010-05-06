#ifndef __XRDMGMOFS_FSTFILESYSTEM__HH__
#define __XRDMGMOFS_FSTFILESYSTEM__HH__

/*----------------------------------------------------------------------------*/
#include "XrdCommon/XrdCommonFileSystem.hh"
/*----------------------------------------------------------------------------*/
#include "XrdOuc/XrdOucString.hh"
/*----------------------------------------------------------------------------*/
#include <sys/vfs.h>
/*----------------------------------------------------------------------------*/

class XrdMgmFstFileSystem : public XrdCommonFileSystem {
private:
  char infoString[1024];
  unsigned int Id;
  XrdOucString Path;
  XrdOucString queueName;
  XrdOucString schedulingGroup;
  time_t bootSentTime;
  time_t bootDoneTime;
  XrdOucString bootFailureMsg;
  XrdOucString bootString;

  int bootStatus;
  int configStatus;

  int errc;
  XrdOucString errmsg;

  struct statfs statFs;

public:
  unsigned int GetId()    {return Id;}
  const char*  GetPath()  {return Path.c_str();}
  const char*  GetQueue() {return queueName.c_str();}
  int GetBootStatus()     {return bootStatus;}
  int GetConfigStatus()   {return configStatus;}
  time_t GetBootSentTime() { return bootSentTime;}
  time_t GetBootDoneTime() { return bootDoneTime;}
  int GetErrc()            {return errc;}
  const char* GetErrMsg()  {return errmsg.c_str();}


  const char*  GetBootString() { bootString = "mgm.nodename="; bootString += GetQueue(); bootString += "&mgm.fsname="; bootString += GetQueue(); bootString += GetPath(); bootString += "&mgm.fspath="; bootString += GetPath();bootString += "&mgm.fsid="; bootString += (int)GetId(); bootString += "&mgm.fsschedgroup="; bootString += GetSchedulingGroup(); bootString += "&mgm.cfgstatus=";bootString += GetConfigStatusString(); return bootString.c_str();}

  const char*  GetBootFailureMsg() {return bootFailureMsg.c_str();}

  const char*  GetSchedulingGroup() {return schedulingGroup.c_str();}

  const char* GetBootStatusString()   {if (bootStatus==kBootFailure) return "failed"; if (bootStatus==kDown) return "down"; if (bootStatus==kBootSent) return "sent"; if (bootStatus==kBooting) return "booting"; if (bootStatus==kBooted) return "booted"; if (bootStatus==kOpsError) return "opserror";  return "";}
  const char* GetConfigStatusString() {if (configStatus==kOff) return "off"; if (configStatus==kRO) return "ro"; if (configStatus==kRW) return "rw"; return "unknown";}
  static const char* GetInfoHeader() {static char infoHeader[1024];sprintf(infoHeader,"%-36s %-4s %-24s %-16s %-10s %-4s %-10s %-8s %-8s %-8s %-3s %s\n","QUEUE","FSID","PATH","SCHEDGROUP","BOOTSTAT","BT", "CONFIGSTAT","BLOCKS", "FREE", "FILES", "EC ", "EMSG"); return infoHeader;}
  const char* GetInfoString()         {XrdOucString sizestring,freestring,filesstring; sprintf(infoString,"%-36s %04u %-24s %-16s %-10s %04lu %-10s %-8s %-8s %-8s %03d %s\n",GetQueue(),GetId(),GetPath(),GetSchedulingGroup(),GetBootStatusString(),GetBootDoneTime()?(GetBootDoneTime()-GetBootSentTime()):(GetBootSentTime()?(time(0)-GetBootSentTime()):0) , GetConfigStatusString(), XrdCommonFileSystem::GetReadableSizeString(sizestring,statFs.f_blocks * 4096ll,"B"), XrdCommonFileSystem::GetReadableSizeString(freestring, statFs.f_bfree * 4096ll,"B"), XrdCommonFileSystem::GetReadableSizeString(filesstring, (statFs.f_files-statFs.f_ffree) *1ll),errc, errmsg.c_str());fprintf(stderr,"CONTENTS %lu %lu\n", statFs.f_files, statFs.f_ffree);return infoString;}

  void SetDown()    {bootStatus   = kDown;}   
  void SetBootSent(){bootStatus   = kBootSent;bootSentTime = time(0);bootDoneTime = 0;}
  void SetBooting() {bootStatus   = kBooting;}
  void SetBooted()  {bootStatus   = kBooted; bootDoneTime = time(0);}
  void SetBootStatus(int status) {bootStatus = status; if (status == kBooted) bootDoneTime = time(0); if (status == kBootSent) bootSentTime = time(0);}
  void SetBootFailure(const char* txt) {bootStatus = kBootFailure;bootFailureMsg = txt;}
  void SetRO()      {configStatus = kRO;}
  void SetRW()      {configStatus = kRW;}

  void SetId(unsigned int inid)  {Id   = inid;}
  void SetPath(const char* path) {Path = path;}
  void SetSchedulingGroup(const char* group="default") { schedulingGroup = group; }
  void SetError(int inerrc, const char* inerrmsg) {errc = inerrc; if (inerrmsg) errmsg = inerrmsg; else errmsg="";}
  void SetStatfsEnv(XrdOucEnv* env) {
    const char* val;
    if (!env) return;
    if (( val = env->Get("statfs.type")))  {statFs.f_type = strtol(val,0,10);}
    if (( val = env->Get("statfs.bsize"))) {statFs.f_bsize = strtol(val,0,10);}
    if (( val = env->Get("statfs.blocks"))){statFs.f_blocks = strtol(val,0,10);}
    if (( val = env->Get("statfs.bfree"))) {statFs.f_bfree = strtol(val,0,10);}
    if (( val = env->Get("statfs.bavail"))){statFs.f_bavail = strtol(val,0,10);}
    if (( val = env->Get("statfs.files"))) {statFs.f_files = strtol(val,0,10);}
    if (( val = env->Get("statfs.ffree"))) {statFs.f_ffree = strtol(val,0,10);}
    if (( val = env->Get("statfs.namelen"))){statFs.f_namelen = strtol(val,0,10);}
  }

  struct statfs* GetStatfs();

  XrdMgmFstFileSystem(int id, const char* path, const char* queue, const char* schedulinggroup = "default") {
    Id = id; Path = path; queueName = queue; bootStatus=kDown;configStatus = kOff; schedulingGroup = schedulinggroup; bootSentTime=0; bootFailureMsg=""; bootDoneTime=0; errc=0; errmsg=""; memset(&statFs,0,sizeof(statFs));
  };
  
  ~XrdMgmFstFileSystem() {};
  
}; 


#endif

