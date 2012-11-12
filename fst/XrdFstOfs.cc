// ----------------------------------------------------------------------
// File: XrdFstOfs.cc
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
#include "fst/XrdFstOfs.hh"
#include "fst/XrdFstOss.hh"
#include "fst/checksum/ChecksumPlugins.hh"
#include "fst/FmdSqlite.hh"
#include "common/FileId.hh"
#include "common/FileSystem.hh"
#include "common/Path.hh"
#include "common/Statfs.hh"
#include "common/Attr.hh"
#include "common/SyncAll.hh"
/*----------------------------------------------------------------------------*/
#include "XrdNet/XrdNetOpts.hh"
#include "XrdOfs/XrdOfs.hh"
#include "XrdOfs/XrdOfsTrace.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSys/XrdSysTimer.hh"
/*----------------------------------------------------------------------------*/
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <attr/xattr.h>

#include <math.h>
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/

  
// The global OFS handle
eos::fst::XrdFstOfs eos::fst::gOFS;

extern XrdSysError OfsEroute;
extern XrdOssSys  *XrdOfsOss;
extern XrdOfs     *XrdOfsFS;
extern XrdOucTrace OfsTrace;

extern XrdOss* XrdOssGetSS( XrdSysLogger*, 
                            const char*, 
                            const char*, 
                            const char*, 
                            XrdVersionInfo& );

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
extern "C"
{
  XrdSfsFileSystem *XrdSfsGetFileSystem(XrdSfsFileSystem *native_fs, 
                                        XrdSysLogger     *lp,
                                        const char       *configfn)
  {
    // Do the herald thing
    //
    OfsEroute.SetPrefix("FstOfs_");
    OfsEroute.logger(lp);
    XrdOucString version = "FstOfs (Object Storage File System) ";
    version += VERSION;
    OfsEroute.Say("++++++ (c) 2010 CERN/IT-DSS ",
                  version.c_str());

    static XrdVERSIONINFODEF( info, XrdOss, XrdVNUMBER, XrdVERSION);

    // Initialize the subsystems
    //
    eos::fst::gOFS.ConfigFN = (configfn && *configfn ? strdup(configfn) : 0);

    if ( eos::fst::gOFS.Configure(OfsEroute) ) return 0;
    // Initialize the target storage system
    //

    if (!(XrdOfsOss = (eos::fst::XrdFstOss*) XrdOssGetSS(lp, configfn, 
                                                         eos::fst::gOFS.OssLib, 
                                                         0, info))) {
      return 0;
    } 

    XrdOfsFS = &eos::fst::gOFS;
    return &eos::fst::gOFS;
  }
}

EOSFSTNAMESPACE_BEGIN


//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
XrdFstOfs::XrdFstOfs() {
  eos::common::LogId(); Eroute = 0; Messaging = 0; Storage = 0; TransferScheduler = 0; 
  //-------------------------------------------
  // add Shutdown handler
  //-------------------------------------------
  (void) signal(SIGINT,xrdfstofs_shutdown);
  (void) signal(SIGTERM,xrdfstofs_shutdown);
  (void) signal(SIGQUIT,xrdfstofs_shutdown);
  //-------------------------------------------
  // add SEGV handler
  //-------------------------------------------
  (void) signal(SIGSEGV, xrdfstofs_stacktrace);
  (void) signal(SIGABRT, xrdfstofs_stacktrace);
  (void) signal(SIGBUS,  xrdfstofs_stacktrace);
}


//------------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
XrdFstOfs::~XrdFstOfs() {
  // empty
}


//------------------------------------------------------------------------------
// Function newDir
//-----------------------------------------------------------------------------
XrdSfsDirectory*
XrdFstOfs::newDir( char* user, int MonID ) {
  return static_cast<XrdSfsDirectory*>( new XrdFstOfsDirectory( user, MonID ) );
}


//------------------------------------------------------------------------------
// Function newFile
//-----------------------------------------------------------------------------
XrdSfsFile*
XrdFstOfs::newFile( char* user, int MonID ) {
  return static_cast<XrdSfsFile*>( new XrdFstOfsFile( user, MonID ) );
}


/*----------------------------------------------------------------------------*/
void
XrdFstOfs::xrdfstofs_stacktrace(int sig) {
  (void) signal(SIGINT,SIG_IGN);
  (void) signal(SIGTERM,SIG_IGN);
  (void) signal(SIGQUIT,SIG_IGN);
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, 2);
  exit(1);
}

/*----------------------------------------------------------------------------*/
void
XrdFstOfs::xrdfstofs_shutdown(int sig) {
  static XrdSysMutex ShutDownMutex;

  ShutDownMutex.Lock(); // this handler goes only one-shot .. sorry !

  // handler to shutdown the daemon for valgrinding and clean server stop (e.g. let's time to finish write operations

  if (gOFS.Messaging) {
    gOFS.Messaging->StopListener(); // stop any communication
  }

  XrdSysTimer sleeper;
  sleeper.Wait(1000);

  std::set<pthread_t>::const_iterator it;
  {
    XrdSysMutexHelper(gOFS.Storage->ThreadSetMutex);
    for (it= gOFS.Storage->ThreadSet.begin(); it != gOFS.Storage->ThreadSet.end(); it++) {
      eos_static_warning("op=shutdown threadid=%llx", (unsigned long long) *it);
      XrdSysThread::Cancel(*it);
      XrdSysThread::Join(*it,0);
    }
  }

  eos_static_warning("op=shutdown msg=\"stop messaging\""); 
  if (gOFS.Messaging) {
    delete gOFS.Messaging; // shutdown messaging thread
    gOFS.Messaging=0;
  }

  eos_static_warning("%s","op=shutdown msg=\"shutdown fmdsqlite handler\"");
  gFmdSqliteHandler.Shutdown();
  eos_static_warning("%s","op=shutdown status=completed");

  // sync & close all file descriptors
  eos::common::SyncAll::AllandClose();
  


  exit(0);
}

/*----------------------------------------------------------------------------*/
int XrdFstOfs::Configure(XrdSysError& Eroute) 
{
  char *var;
  const char *val;
  int  cfgFD;
  int NoGo=0;

  int rc = XrdOfs::Configure(Eroute);

  // enforcing 'sss' authentication for all communications

  setenv("XrdSecPROTOCOL","sss",1);
  Eroute.Say("=====> fstofs enforces SSS authentication for XROOT clients");

  if (rc)
    return rc;

  TransferScheduler = new XrdScheduler(&Eroute, &OfsTrace, 8, 128, 60);

  TransferScheduler->Start();

  eos::fst::Config::gConfig.autoBoot = false;

  eos::fst::Config::gConfig.FstOfsBrokerUrl = "root://localhost:1097//eos/";
  
  if (getenv("EOS_BROKER_URL")) {
    eos::fst::Config::gConfig.FstOfsBrokerUrl = getenv("EOS_BROKER_URL");
  }

  eos::fst::Config::gConfig.FstMetaLogDir = "/var/tmp/eos/md/";

  setenv("XrdClientEUSER", "daemon", 1);

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // extract the manager from the config file
  XrdOucStream Config(&Eroute, getenv("XRDINSTANCE"));

  if( !ConfigFN || !*ConfigFN) {
    // this error will be reported by XrdOfsFS.Configure
  } else {
    // Try to open the configuration file.
    //
    if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      return Eroute.Emsg("Config", errno, "open config file fn=", ConfigFN);

    Config.Attach(cfgFD);
    // Now start reading records until eof.
    //
    
    while((var = Config.GetMyFirstWord())) {
      if (!strncmp(var, "fstofs.",7)) {
        var += 7;
        // we parse config variables here 
        if (!strcmp("broker",var)) {
          if (!(val = Config.GetWord())) {
            Eroute.Emsg("Config","argument 2 for broker missing. Should be URL like root://<host>/<queue>/"); NoGo=1;
          } else {
            if (getenv("EOS_BROKER_URL")) {
              eos::fst::Config::gConfig.FstOfsBrokerUrl = getenv("EOS_BROKER_URL");
            } else {
              eos::fst::Config::gConfig.FstOfsBrokerUrl = val;
            }

          }
        }

        if (!strcmp("trace",var)) {
          if (!(val = Config.GetWord())) {
            Eroute.Emsg("Config","argument 2 for trace missing. Can be 'client'"); NoGo=1;
          } else {
            EnvPutInt( NAME_DEBUG, 3);
          }
        }
        
        if (!strcmp("autoboot",var)) {
          if ((!(val = Config.GetWord())) || (strcmp("true",val) && strcmp("false",val) && strcmp("1",val) && strcmp("0",val))) {
            Eroute.Emsg("Config","argument 2 for autobootillegal or missing. Must be <true>,<false>,<1> or <0>!"); NoGo=1;
          } else {
            if ((!strcmp("true",val) || (!strcmp("1",val)))) {
              eos::fst::Config::gConfig.autoBoot = true;
            }
          }
        }

        if (!strcmp("metalog",var)) {
          if (!(val = Config.GetWord())) {
            Eroute.Emsg("Config","argument 2 for metalog missing"); NoGo=1;
          } else {
            eos::fst::Config::gConfig.FstMetaLogDir = val;
          }
        }

      }
    }
    Config.Close();
  }

  if (eos::fst::Config::gConfig.autoBoot) {
    Eroute.Say("=====> fstofs.autoboot : true");
  } else {
    Eroute.Say("=====> fstofs.autoboot : false");
  }

  if (! eos::fst::Config::gConfig.FstOfsBrokerUrl.endswith("/")) {
    eos::fst::Config::gConfig.FstOfsBrokerUrl += "/";
  }

  eos::fst::Config::gConfig.FstDefaultReceiverQueue = eos::fst::Config::gConfig.FstOfsBrokerUrl;

  eos::fst::Config::gConfig.FstOfsBrokerUrl += HostName; 
  eos::fst::Config::gConfig.FstOfsBrokerUrl += ":";
  eos::fst::Config::gConfig.FstOfsBrokerUrl += myPort;
  eos::fst::Config::gConfig.FstOfsBrokerUrl += "/fst";


  eos::fst::Config::gConfig.FstHostPort = HostName; 
  eos::fst::Config::gConfig.FstHostPort += ":";
  eos::fst::Config::gConfig.FstHostPort += myPort;

  Eroute.Say("=====> fstofs.broker : ", eos::fst::Config::gConfig.FstOfsBrokerUrl.c_str(),"");

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // extract our queue name
  eos::fst::Config::gConfig.FstQueue = eos::fst::Config::gConfig.FstOfsBrokerUrl;
  {
    int pos1 = eos::fst::Config::gConfig.FstQueue.find("//");
    int pos2 = eos::fst::Config::gConfig.FstQueue.find("//",pos1+2);
    if (pos2 != STR_NPOS) {
      eos::fst::Config::gConfig.FstQueue.erase(0, pos2+1);
    } else {
      Eroute.Emsg("Config","cannot determin my queue name: ", eos::fst::Config::gConfig.FstQueue.c_str());
      return 1;
    }
  }

  // create our wildcard broadcast name
  eos::fst::Config::gConfig.FstQueueWildcard =  eos::fst::Config::gConfig.FstQueue;
  eos::fst::Config::gConfig.FstQueueWildcard+= "/*";

  // create our wildcard config broadcast name
  eos::fst::Config::gConfig.FstConfigQueueWildcard =  "*/";
  eos::fst::Config::gConfig.FstConfigQueueWildcard+= HostName; 
  eos::fst::Config::gConfig.FstConfigQueueWildcard+= ":";
  eos::fst::Config::gConfig.FstConfigQueueWildcard+= myPort;

  // Set Logging parameters
  XrdOucString unit = "fst@"; unit+= HostName; unit+=":"; unit+=myPort;

  // setup the circular in-memory log buffer
  eos::common::Logging::Init();
  eos::common::Logging::SetLogPriority(LOG_DEBUG);
  //eos::common::Logging::SetLogPriority(LOG_INFO);
  eos::common::Logging::SetUnit(unit.c_str());

  eos_info("info=\"logging configured\"");

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // create the messaging object(recv thread)
  
  eos::fst::Config::gConfig.FstDefaultReceiverQueue += "*/mgm";
  int pos1 = eos::fst::Config::gConfig.FstDefaultReceiverQueue.find("//");
  int pos2 = eos::fst::Config::gConfig.FstDefaultReceiverQueue.find("//",pos1+2);
  if (pos2 != STR_NPOS) {
    eos::fst::Config::gConfig.FstDefaultReceiverQueue.erase(0, pos2+1);
  }

  Eroute.Say("=====> fstofs.defaultreceiverqueue : ", eos::fst::Config::gConfig.FstDefaultReceiverQueue.c_str(),"");
  // set our Eroute for XrdMqMessage
  XrdMqMessage::Eroute = OfsEroute;

  // Enable the shared object notification queue
  ObjectManager.EnableQueue = true;
  ObjectManager.SetAutoReplyQueue("/eos/*/mgm");
  ObjectManager.SetDebug(false);
  eos::common::Logging::SetLogPriority(LOG_INFO);

  // setup notification subjects
  ObjectManager.SubjectsMutex.Lock();
  std::string watch_id = "id";
  std::string watch_bootsenttime = "bootsenttime";
  std::string watch_scaninterval = "scaninterval";
  std::string watch_symkey       = "symkey";
  std::string watch_manager      = "manager";
  std::string watch_publishinterval = "publish.interval";
  std::string watch_debuglevel   = "debug.level";
  std::string watch_gateway      = "txgw";
  std::string watch_gateway_rate = "gw.rate";
  std::string watch_gateway_ntx  = "gw.ntx";
  std::string watch_error_simulation  = "error.simulation";

  ObjectManager.ModificationWatchKeys.insert(watch_id);
  ObjectManager.ModificationWatchKeys.insert(watch_bootsenttime);
  ObjectManager.ModificationWatchKeys.insert(watch_scaninterval);
  ObjectManager.ModificationWatchKeys.insert(watch_symkey);
  ObjectManager.ModificationWatchKeys.insert(watch_manager);
  ObjectManager.ModificationWatchKeys.insert(watch_id);
  ObjectManager.ModificationWatchKeys.insert(watch_bootsenttime);
  ObjectManager.ModificationWatchKeys.insert(watch_scaninterval);
  ObjectManager.ModificationWatchKeys.insert(watch_symkey);
  ObjectManager.ModificationWatchKeys.insert(watch_manager);
  ObjectManager.ModificationWatchKeys.insert(watch_publishinterval);
  ObjectManager.ModificationWatchKeys.insert(watch_debuglevel);
  ObjectManager.ModificationWatchKeys.insert(watch_gateway);
  ObjectManager.ModificationWatchKeys.insert(watch_gateway_rate);
  ObjectManager.ModificationWatchKeys.insert(watch_gateway_ntx);
  ObjectManager.ModificationWatchKeys.insert(watch_error_simulation);
  ObjectManager.SubjectsMutex.UnLock();



  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // create the specific listener class
  Messaging = new eos::fst::Messaging(eos::fst::Config::gConfig.FstOfsBrokerUrl.c_str(),eos::fst::Config::gConfig.FstDefaultReceiverQueue.c_str(),false, false, &ObjectManager);
  Messaging->SetLogId("FstOfsMessaging");

  if( (!Messaging) || (!Messaging->StartListenerThread()) ) NoGo = 1;

  if ( (!Messaging) || (Messaging->IsZombie()) ) {
    Eroute.Emsg("Config","cannot create messaging object(thread)");
    NoGo = 1;
  }
  if (NoGo) 
    return NoGo;

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Attach Storage to the meta log dir

  Storage = eos::fst::Storage::Create(eos::fst::Config::gConfig.FstMetaLogDir.c_str());
  Eroute.Say("=====> fstofs.metalogdir : ", eos::fst::Config::gConfig.FstMetaLogDir.c_str());
  if (!Storage) {
    Eroute.Emsg("Config","cannot setup meta data storage using directory: ", eos::fst::Config::gConfig.FstMetaLogDir.c_str());
    return 1;
  } 


  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Create a wildcard broadcast 
  XrdMqSharedHash* hash = 0;

  // Create a node broadcast
  ObjectManager.CreateSharedHash(eos::fst::Config::gConfig.FstConfigQueueWildcard.c_str(),eos::fst::Config::gConfig.FstDefaultReceiverQueue.c_str());
  ObjectManager.HashMutex.LockRead();
  
  hash = ObjectManager.GetHash(eos::fst::Config::gConfig.FstConfigQueueWildcard.c_str());
  
  if (hash) {
    // ask for a broadcast
    hash->BroadCastRequest(eos::fst::Config::gConfig.FstDefaultReceiverQueue.c_str());
  }

  ObjectManager.HashMutex.UnLockRead();

  // Create a filesystem broadcast
  ObjectManager.CreateSharedHash(eos::fst::Config::gConfig.FstQueueWildcard.c_str(),eos::fst::Config::gConfig.FstDefaultReceiverQueue.c_str());
  ObjectManager.HashMutex.LockRead();
  hash = ObjectManager.GetHash(eos::fst::Config::gConfig.FstQueueWildcard.c_str());

  if (hash) {
    // ask for a broadcast
    hash->BroadCastRequest(eos::fst::Config::gConfig.FstDefaultReceiverQueue.c_str());
  }

  ObjectManager.HashMutex.UnLockRead();
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // start dumper thread
  XrdOucString dumperfile = eos::fst::Config::gConfig.FstMetaLogDir;
  dumperfile += "so.fst.dump";
  ObjectManager.StartDumper(dumperfile.c_str());

  XrdOucString keytabcks="unaccessible";

 
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // build the adler checksum of the default keytab file
  int fd = ::open("/etc/eos.keytab",O_RDONLY);
  if (fd>0) {
    char buffer[65535];
    size_t nread = ::read(fd, buffer, sizeof(buffer));
    if (nread>0) {
      CheckSum* KeyCKS = ChecksumPlugins::GetChecksumObject(eos::common::LayoutId::kAdler);
      if (KeyCKS) {
        KeyCKS->Add(buffer,nread,0);
        keytabcks= KeyCKS->GetHexChecksum();
        delete KeyCKS;
      }
    }
    close(fd);
  }

  eos_notice("FST_HOST=%s FST_PORT=%ld VERSION=%s RELEASE=%s KEYTABADLER=%s", HostName, myPort, VERSION,RELEASE, keytabcks.c_str());

  return 0;
}

/*----------------------------------------------------------------------------*/

void 
XrdFstOfs::SetSimulationError(const char* tag) {
  // -----------------------------------------------
  // define error bool variables to en-/disable error simulation in the OFS layer

  XrdOucString stag = tag;
  gOFS.Simulate_IO_read_error = gOFS.Simulate_IO_write_error = gOFS.Simulate_XS_read_error = gOFS.Simulate_XS_write_error = false;
  
  if (stag == "io_read") {
    gOFS.Simulate_IO_read_error = true;
  }
  if (stag == "io_write") {
    gOFS.Simulate_IO_write_error = true;
  } 
  if (stag == "xs_read") {
    gOFS.Simulate_XS_read_error = true;
  }
  if (stag == "xs_write") {
    gOFS.Simulate_XS_write_error = true;
  }
}

/*----------------------------------------------------------------------------*/
int
XrdFstOfs::stat(  const char             *path,
                  struct stat            *buf,
                  XrdOucErrInfo          &out_error,
                  const XrdSecEntity     *client,
                  const char             *opaque)
{
  EPNAME("stat");
  memset(buf,0,sizeof(struct stat));
  if (!XrdOfsOss->Stat(path, buf))
    return SFS_OK;
  else
    return gOFS.Emsg(epname,out_error,errno,"stat file",path);
}


//------------------------------------------------------------------------------
// CallManager function
//------------------------------------------------------------------------------
int
XrdFstOfs::CallManager( XrdOucErrInfo* error,
                        const char*    path,
                        const char*    manager,
                        XrdOucString&  capOpaqueFile,
                        XrdOucString*  return_result )
{
  EPNAME( "CallManager" );
  int rc = SFS_OK;
  int  result_size = 8192;
  char result[result_size];
  result[0] = 0;
  
  XrdOucString url = "root://";
  url += manager;
  url += "//dummy";
  XrdClientAdmin* admin = new XrdClientAdmin(url.c_str());
  XrdOucString msg = "";
      
  if (admin) {
    admin->Connect();
    admin->GetClientConn()->ClearLastServerError();
    admin->GetClientConn()->SetOpTimeLimit(10);
    admin->Query(kXR_Qopaquf,
                 (kXR_char *) capOpaqueFile.c_str(),
                 (kXR_char *) result, result_size);
    
    if (!admin->LastServerResp()) {
      if (error)
        gOFS.Emsg(epname, *error, ECOMM, "commit changed filesize to meta data cache for fn=", path);
      rc = SFS_ERROR;
    }
    switch (admin->LastServerResp()->status) {
    case kXR_ok:
      eos_debug("called MGM cache - %s", capOpaqueFile.c_str());
      rc = SFS_OK;
      break;
      
    case kXR_error:
      if (error) {
        gOFS.Emsg(epname, *error, ECOMM, "to call manager for fn=", path);
      }
      msg = (admin->LastServerError()->errmsg);
      rc = SFS_ERROR;

      if (msg.find("[EIDRM]") !=STR_NPOS)
        rc = -EIDRM;


      if (msg.find("[EBADE]") !=STR_NPOS)
        rc = -EBADE;


      if (msg.find("[EBADR]") !=STR_NPOS)
        rc = -EBADR;

      if (msg.find("[EINVAL]") != STR_NPOS)
	rc = -EINVAL;
      
      if (msg.find("[EADV]") != STR_NPOS)
	rc = -EADV;

      break;
      
    default:
      rc = SFS_OK;
      break;
    }
  } else {
    eos_crit("cannot get client admin to execute commit");
    if (error)
      gOFS.Emsg(epname, *error, ENOMEM, "allocate client admin object during close of fn=", path);
  }
  delete admin;

  if (return_result) {
    *return_result = result;
  }
  return rc;
}


/*----------------------------------------------------------------------------*/
void
XrdFstOfs::SetDebug(XrdOucEnv &env) 
{
  XrdOucString debugnode =  env.Get("mgm.nodename");
  XrdOucString debuglevel = env.Get("mgm.debuglevel");
  XrdOucString filterlist = env.Get("mgm.filter");
  int debugval = eos::common::Logging::GetPriorityByString(debuglevel.c_str());
  if (debugval<0) {
    eos_err("debug level %s is not known!", debuglevel.c_str());
  } else {
    // we set the shared hash debug for the lowest 'debug' level
    if (debuglevel == "debug") {
      ObjectManager.SetDebug(true);
    } else {
      ObjectManager.SetDebug(false);
    }

    eos::common::Logging::SetLogPriority(debugval);
    eos_notice("setting debug level to <%s>", debuglevel.c_str());
    if (filterlist.length()) {
      eos::common::Logging::SetFilter(filterlist.c_str());
      eos_notice("setting message logid filter to <%s>", filterlist.c_str());
    }
  }
  fprintf(stderr,"Setting debug to %s\n", debuglevel.c_str());
}


/*----------------------------------------------------------------------------*/
void
XrdFstOfs::SendRtLog(XrdMqMessage* message) 
{
  XrdOucEnv opaque(message->GetBody());
  XrdOucString queue = opaque.Get("mgm.rtlog.queue");
  XrdOucString lines = opaque.Get("mgm.rtlog.lines");
  XrdOucString tag   = opaque.Get("mgm.rtlog.tag");
  XrdOucString filter = opaque.Get("mgm.rtlog.filter");
  XrdOucString stdOut="";

  if (!filter.length()) filter = " ";

  if ( (!queue.length()) || (!lines.length()) || (!tag.length()) ) {
    eos_err("illegal parameter queue=%s lines=%s tag=%s", queue.c_str(), lines.c_str(), tag.c_str());
  }  else {
    if ( (eos::common::Logging::GetPriorityByString(tag.c_str())) == -1) {
      eos_err("mgm.rtlog.tag must be info,debug,err,emerg,alert,crit,warning or notice");
    } else {
      int logtagindex = eos::common::Logging::GetPriorityByString(tag.c_str());
      for (int j = 0; j<= logtagindex; j++) {
        for (int i=1; i<= atoi(lines.c_str()); i++) {
          eos::common::Logging::gMutex.Lock();
          XrdOucString logline = eos::common::Logging::gLogMemory[j][(eos::common::Logging::gLogCircularIndex[j]-i+eos::common::Logging::gCircularIndexSize)%eos::common::Logging::gCircularIndexSize].c_str();
          eos::common::Logging::gMutex.UnLock();
        
          if (logline.length() && ( (logline.find(filter.c_str())) != STR_NPOS)) {
            stdOut += logline;
            stdOut += "\n";
          }
          if (stdOut.length() > (4*1024)) {
            XrdMqMessage repmessage("rtlog reply message");
            repmessage.SetBody(stdOut.c_str());
            if (!XrdMqMessaging::gMessageClient.ReplyMessage(repmessage, *message)) {
              eos_err("unable to send rtlog reply message to %s", message->kMessageHeader.kSenderId.c_str());
            }
            stdOut = "";
          }
          
          if (!logline.length())
            break;
        }
      }
    }
  }
  if (stdOut.length()) {
    XrdMqMessage repmessage("rtlog reply message");
    repmessage.SetBody(stdOut.c_str());
    if (!XrdMqMessaging::gMessageClient.ReplyMessage(repmessage, *message)) {
      eos_err("unable to send rtlog reply message to %s", message->kMessageHeader.kSenderId.c_str());
    }
  }
}


/*----------------------------------------------------------------------------*/
void
XrdFstOfs::SendFsck(XrdMqMessage* message) 
{
  XrdOucEnv opaque(message->GetBody());
  XrdOucString stdOut="";
  XrdOucString tag   = opaque.Get("mgm.fsck.tags"); // the tag is either '*' for all, or a , seperated list of tag names
  if ( (!tag.length()) ) {
    eos_err("parameter tag missing");
  }  else {
    stdOut = "";
    // loop over filesystems
    eos::common::RWMutexReadLock(gOFS.Storage->fsMutex);
    std::vector <eos::fst::FileSystem*>::const_iterator it;
    for (unsigned int i=0; i< gOFS.Storage->fileSystemsVector.size(); i++) {
      std::map<std::string, std::set<eos::common::FileId::fileid_t> >* icset = gOFS.Storage->fileSystemsVector[i]->GetInconsistencySets();
      std::map<std::string, std::set<eos::common::FileId::fileid_t> >::const_iterator icit;
      for (icit = icset->begin(); icit != icset->end(); icit++) {
	// loop over all tags
	if ( ( (icit->first != "mem_n") && (icit->first != "d_sync_n") && (icit->first != "m_sync_n") ) &&  
	     ( (tag == "*") || ( (tag.find(icit->first.c_str()) != STR_NPOS)) ) ) {
	  char stag[4096];
	  eos::common::FileSystem::fsid_t fsid = gOFS.Storage->fileSystemsVector[i]->GetId();
	  snprintf(stag,sizeof(stag)-1,"%s@%lu", icit->first.c_str(), (unsigned long )fsid);
	  stdOut += stag;
	  std::set<eos::common::FileId::fileid_t>::const_iterator fit;

	  if (gOFS.Storage->fileSystemsVector[i]->GetStatus() != eos::common::FileSystem::kBooted) {
	    // we don't report filesystems which are not booted!
	    continue;
	  }
	  for (fit = icit->second.begin(); fit != icit->second.end(); fit ++) {
	    // don't report files which are currently write-open
	    XrdSysMutexHelper wLock(gOFS.OpenFidMutex);
	    if (gOFS.WOpenFid[fsid].count(*fit)) {
	      if (gOFS.WOpenFid[fsid][*fit]>0) {
		continue;
	      }
	    }
	    // loop over all fids
	    char sfid[4096];
	    snprintf(sfid,sizeof(sfid)-1,":%08llx", *fit);
	    stdOut += sfid;

	    if (stdOut.length() > (64*1024)) {
	      stdOut += "\n";
	      XrdMqMessage repmessage("fsck reply message");
	      repmessage.SetBody(stdOut.c_str());
	      fprintf(stderr,"Sending %s\n", stdOut.c_str());
	      if (!XrdMqMessaging::gMessageClient.ReplyMessage(repmessage, *message)) {
		eos_err("unable to send fsck reply message to %s", message->kMessageHeader.kSenderId.c_str());
	      }
	      stdOut = stag;
	    }
	  }
	  stdOut += "\n";
	}
      }
    }
  }
  if (stdOut.length()) {
    XrdMqMessage repmessage("fsck reply message");
    repmessage.SetBody(stdOut.c_str());
    fprintf(stderr,"Sending %s\n", stdOut.c_str());
    if (!XrdMqMessaging::gMessageClient.ReplyMessage(repmessage, *message)) {
      eos_err("unable to send fsck reply message to %s", message->kMessageHeader.kSenderId.c_str());
    }
  }
}

/*----------------------------------------------------------------------------*/
int            
XrdFstOfs::rem(const char             *path,
               XrdOucErrInfo          &error,
               const XrdSecEntity     *client,
               const char             *opaque) 
{
  EPNAME("rem");

  XrdOucString stringOpaque = opaque;
  stringOpaque.replace("?","&");
  stringOpaque.replace("&&","&");

  XrdOucEnv openOpaque(stringOpaque.c_str());
  XrdOucEnv* capOpaque;

  int caprc = 0;
  

  if ((caprc=gCapabilityEngine.Extract(&openOpaque, capOpaque))) {
    // no capability - go away!
    if (capOpaque) delete capOpaque;
    return gOFS.Emsg(epname,error,caprc,"open - capability illegal",path);
  }

  int envlen;

  if (capOpaque) {
    eos_info("path=%s info=%s capability=%s", path, opaque, capOpaque->Env(envlen));
  } else {
    eos_info("path=%s info=%s", path, opaque);
  }
  
  int rc =  _rem(path, error, client, capOpaque);
  if (capOpaque) {
    delete capOpaque;
    capOpaque = 0;
  }

  return rc;
}



/*----------------------------------------------------------------------------*/
int            
XrdFstOfs::_rem(const char             *path,
                XrdOucErrInfo          &error,
                const XrdSecEntity     *client,
                XrdOucEnv              *capOpaque, 
                const char*            fstpath, 
                unsigned long long     fid,
                unsigned long          fsid, 
		bool                   ignoreifnotexist)
{
  EPNAME("rem");
  int   retc = SFS_OK;
  XrdOucString fstPath="";

  const char* localprefix=0;
  const char* hexfid=0;
  const char* sfsid=0;

  eos_debug("");

  if ( (!fstpath) && (!fsid) && (!fid) ) {
    // standard deletion brings all information via the opaque info
    if (!(localprefix=capOpaque->Get("mgm.localprefix"))) {
      return gOFS.Emsg(epname,error,EINVAL,"open - no local prefix in capability",path);
    }
    
    if (!(hexfid=capOpaque->Get("mgm.fid"))) {
      return gOFS.Emsg(epname,error,EINVAL,"open - no file id in capability",path);
    }
    
    if (!(sfsid=capOpaque->Get("mgm.fsid"))) {
      return gOFS.Emsg(epname,error, EINVAL,"open - no file system id in capability",path);
    }
    eos::common::FileId::FidPrefix2FullPath(hexfid, localprefix,fstPath);

    fid = eos::common::FileId::Hex2Fid(hexfid);

    fsid   = atoi(sfsid);
  } else {
    // deletion during close provides the local storage path, fid & fsid
    fstPath = fstpath;
  }

  struct stat statinfo;
  if ((retc = XrdOfsOss->Stat(fstPath.c_str(), &statinfo))) {
    if (!ignoreifnotexist) {
      eos_notice("unable to delete file - file does not exist (anymore): %s fstpath=%s fsid=%lu id=%llu", path, fstPath.c_str(),fsid, fid);
      return gOFS.Emsg(epname,error,ENOENT,"delete file - file does not exist",fstPath.c_str());    
    }
  } 
  eos_info("fstpath=%s", fstPath.c_str());

  int rc=0;
  if (!retc) {
    // unlink file
    errno = 0;
    rc = XrdOfs::rem(fstPath.c_str(),error,client,0);
    if (rc) {
      eos_info("rc=%d errno=%d", rc,errno);
    }
  }

  if (ignoreifnotexist) {
    // hide error if a deleted file is deleted
    rc = 0;
  }

  // unlink block checksum files
  {
    // this is not the 'best' solution, but we don't have any info about block checksums
    Adler xs; // the type does not matter here
    const char* path = xs.MakeBlockXSPath(fstPath.c_str());
    if (!xs.UnlinkXSPath()) {
      eos_info("info=\"removed block-xs\" path=%s", path);
    }
  }

  // cleanup eventual transactions
  if (!gOFS.Storage->CloseTransaction(fsid, fid)) {
    // it should be the normal case that there is no open transaction for that file, in any case there is nothing to do here
  }
  
  if (rc) {
    return rc;
  }

  if (!gFmdSqliteHandler.DeleteFmd(fid, fsid)) {
    eos_notice("unable to delete fmd for fid %llu on filesystem %lu",fid,fsid);
    return gOFS.Emsg(epname,error,EIO,"delete file meta data ",fstPath.c_str());
  }

  return SFS_OK;
}

int       
XrdFstOfs::fsctl(const int               cmd,
                 const char             *args,
                 XrdOucErrInfo          &error,
                 const XrdSecEntity *client)

{
  static const char *epname = "fsctl";
  const char *tident = error.getErrUser();

  if ((cmd == SFS_FSCTL_LOCATE)) {
    char locResp[4096];
    char rType[3], *Resp[] = {rType, locResp};
    rType[0] = 'S';
    rType[1] = 'r';//(fstat.st_mode & S_IWUSR            ? 'w' : 'r');
    rType[2] = '\0';
    sprintf(locResp,"[::%s:%d] ",(char*)HostName,myPort);
    error.setErrInfo(strlen(locResp)+3, (const char **)Resp, 2);
    ZTRACE(fsctl,"located at headnode: " << locResp);
    return SFS_DATA;
  }
  return gOFS.Emsg(epname, error, EPERM, "execute fsctl function", "");
}

/*----------------------------------------------------------------------------*/
int
XrdFstOfs::FSctl(const int               cmd,
                 XrdSfsFSctl            &args,
                 XrdOucErrInfo          &error,
                 const XrdSecEntity     *client) 
{  
  char ipath[16384];
  char iopaque[16384];
  
  static const char *epname = "FSctl";
  const char *tident = error.getErrUser();

  if ((cmd == SFS_FSCTL_LOCATE)) {
    char locResp[4096];
    char rType[3], *Resp[] = {rType, locResp};
    rType[0] = 'S';
    rType[1] = 'r';//(fstat.st_mode & S_IWUSR            ? 'w' : 'r');
    rType[2] = '\0';
    sprintf(locResp,"[::%s:%d] ",(char*)HostName,myPort);
    error.setErrInfo(strlen(locResp)+3, (const char **)Resp, 2);
    ZTRACE(fsctl,"located at headnode: " << locResp);
    return SFS_DATA;
  }
  
  // accept only plugin calls!

  if (cmd != SFS_FSCTL_PLUGIN) {
    return gOFS.Emsg(epname, error, EPERM, "execute non-plugin function", "");
  }

  if (args.Arg1Len) {
    if (args.Arg1Len < 16384) {
      strncpy(ipath,args.Arg1,args.Arg1Len);
      ipath[args.Arg1Len] = 0;
    } else {
      return gOFS.Emsg(epname, error, EINVAL, "convert path argument - string too long", "");
    }
  } else {
    ipath[0] = 0;
  }
  
  if (args.Arg2Len) {
    if (args.Arg2Len < 16384) {
      strncpy(iopaque,args.Arg2,args.Arg2Len);
      iopaque[args.Arg2Len] = 0;
    } else {
      return gOFS.Emsg(epname, error, EINVAL, "convert opaque argument - string too long", "");
    }
  } else {
    iopaque[0] = 0;
  }
  
  // from here on we can deal with XrdOucString which is more 'comfortable'
  XrdOucString path    = ipath;
  XrdOucString opaque  = iopaque;
  XrdOucString result  = "";
  XrdOucEnv env(opaque.c_str());

  eos_debug("tident=%s path=%s opaque=%s",tident, path.c_str(), opaque.c_str());

  if (cmd!=SFS_FSCTL_PLUGIN) {
    return SFS_ERROR;
  }

  const char* scmd;

  if ((scmd = env.Get("fst.pcmd"))) {
    XrdOucString execmd = scmd;

    if (execmd == "getfmd") {
      char* afid   = env.Get("fst.getfmd.fid");
      char* afsid  = env.Get("fst.getfmd.fsid");

      if ((!afid) || (!afsid)) {
        return  Emsg(epname,error,EINVAL,"execute FSctl command",path.c_str());  
      }

      unsigned long long fileid = eos::common::FileId::Hex2Fid(afid);
      unsigned long fsid = atoi(afsid);

      FmdSqlite* fmd = gFmdSqliteHandler.GetFmd(fileid, fsid, 0, 0, 0, false, true);

      if (!fmd) {
        eos_static_err("no fmd for fileid %llu on filesystem %lu", fileid, fsid);
        const char* err = "ERROR";
        error.setErrInfo(strlen(err)+1,err);
        return SFS_DATA;
      }
      
      XrdOucEnv* fmdenv = fmd->FmdSqliteToEnv();
      int envlen;
      XrdOucString fmdenvstring = fmdenv->Env(envlen);
      delete fmdenv;
      delete fmd;
      error.setErrInfo(fmdenvstring.length()+1,fmdenvstring.c_str());
      return SFS_DATA;
    }

    if (execmd == "getxattr") {
      char* key    = env.Get("fst.getxattr.key");
      char* path   = env.Get("fst.getxattr.path");
      if (!key) {
        eos_static_err("no key specified as attribute name");
        const char* err = "ERROR";
        error.setErrInfo(strlen(err)+1,err);
        return SFS_DATA;
      }
      if (!path) {
        eos_static_err("no path specified to get the attribute from");
        const char* err = "ERROR";
        error.setErrInfo(strlen(err)+1,err);
        return SFS_DATA;
      }
      char value[1024];
      ssize_t attr_length = getxattr(path,key,value,sizeof(value));
      if (attr_length>0) {
        value[1023]=0;
        XrdOucString skey=key;
        XrdOucString attr = "";
        if (skey=="user.eos.checksum") {
          // checksum's are binary and need special reformatting ( we swap the byte order if they are 4 bytes long )
          if (attr_length==4) {
            for (ssize_t k=0; k<4; k++) {
              char hex[4];
              snprintf(hex,sizeof(hex)-1,"%02x", (unsigned char) value[3-k]);
              attr += hex;
            }
          } else {
            for (ssize_t k=0; k<attr_length; k++) {
              char hex[4];
              snprintf(hex,sizeof(hex)-1,"%02x", (unsigned char) value[k]);
              attr += hex;
            }
          }
        } else {
          attr = value;
        }
        error.setErrInfo(attr.length()+1,attr.c_str());
        return SFS_DATA;
      } else {
        eos_static_err("getxattr failed for path=%s",path);
        const char* err = "ERROR";
        error.setErrInfo(strlen(err)+1,err);
        return SFS_DATA;
      }
    }
  }

  return  Emsg(epname,error,EINVAL,"execute FSctl command",path.c_str());  
}


/*----------------------------------------------------------------------------*/
void 
XrdFstOfs::OpenFidString(unsigned long fsid, XrdOucString &outstring)
{
  outstring ="";
  OpenFidMutex.Lock();
  google::sparse_hash_map<unsigned long long, unsigned int>::const_iterator idit;
  int nopen = 0;

  for (idit = ROpenFid[fsid].begin(); idit != ROpenFid[fsid].end(); ++idit) {
    if (idit->second >0)
      nopen += idit->second;
  }
  outstring += "&statfs.ropen=";
  outstring += nopen;

  nopen = 0;
  for (idit = WOpenFid[fsid].begin(); idit != WOpenFid[fsid].end(); ++idit) {
    if (idit->second >0)
      nopen += idit->second;
  }
  outstring += "&statfs.wopen=";
  outstring += nopen;
  
  OpenFidMutex.UnLock();
}

int XrdFstOfs::Stall(XrdOucErrInfo   &error, // Error text & code
                     int              stime, // Seconds to stall
                     const char      *msg)   // Message to give
{
  XrdOucString smessage = msg;
  smessage += "; come back in ";
  smessage += stime;
  smessage += " seconds!";
  
  EPNAME("Stall");
  const char *tident = error.getErrUser();
  
  ZTRACE(delay, "Stall " <<stime <<": " << smessage.c_str());

  // Place the error message in the error object and return
  //
  error.setErrInfo(0, smessage.c_str());
  
  // All done
  //
  return stime;
}

/*----------------------------------------------------------------------------*/
int XrdFstOfs::Redirect(XrdOucErrInfo   &error, // Error text & code
                        const char* host,
                        int &port)
{
  EPNAME("Redirect");
  const char *tident = error.getErrUser();
  
  ZTRACE(delay, "Redirect " <<host <<":" << port);

  // Place the error message in the error object and return
  //
  error.setErrInfo(port,host);
  
  // All done
  //
  return SFS_REDIRECT;
}


/*----------------------------------------------------------------------------*/
int
XrdFstOfsDirectory::open(const char              *dirName,
                         const XrdSecClientName  *client,
                         const char              *opaque)
{
  /* --------------------------------------------------------------------------------- */
  /* We use opendir/readdir/closedir to send meta data information about EOS FST files */
  /* --------------------------------------------------------------------------------- */
  XrdOucEnv Opaque(opaque?opaque:"disk=1");

  eos_info("info=\"calling opendir\" dir=%s\n", dirName);
  dirname = dirName;
  if (!client || (strcmp(client->prot,"sss"))) {
    return gOFS.Emsg("opendir",error,EPERM, "open directory - you need to connect via sss",dirName);
  }

  if (Opaque.Get("disk")) {
    std::string dn = dirname.c_str();
    if (!gOFS.Storage->GetFsidFromPath(dn, fsid)) {
      return gOFS.Emsg("opendir",error, EINVAL,"open directory - filesystem has no fsid label ", dirName);
    }
    // here we traverse the tree of the path given by dirName
    fts_paths    = (char**) calloc(2, sizeof(char*));
    fts_paths[0] = (char*) dirName;
    fts_paths[1] = 0;
    fts_tree = fts_open(fts_paths, FTS_NOCHDIR, 0);
        
    if (fts_tree) {
      return SFS_OK;
    }
    return gOFS.Emsg("opendir",error,errno,"open directory - fts_open failed for ",dirName);
  }
  return SFS_OK;
}

/*----------------------------------------------------------------------------*/

const char*
XrdFstOfsDirectory::nextEntry()
{
  FTSENT *node;
  size_t nfound=0;
  entry="";
  // we send the directory contents in a packed format

  while ( (node = fts_read(fts_tree)) ) {
    if (node) {
      if (node->fts_level > 0 && node->fts_name[0] == '.') {
        fts_set(fts_tree, node, FTS_SKIP);
      } else {
        if (node->fts_info && FTS_F) {
          XrdOucString sizestring;
          XrdOucString filePath = node->fts_accpath;
          XrdOucString fileId   = node->fts_accpath;
          if (!filePath.matches("*.xsmap")) {
            struct stat st_buf;
            eos::common::Attr *attr = eos::common::Attr::OpenAttr(filePath.c_str());
            int spos = filePath.rfind("/");
            if (spos >0) {
              fileId.erase(0, spos+1);
            }
            if ((fileId.length() == 8) && (!stat(filePath.c_str(),&st_buf) && S_ISREG(st_buf.st_mode))) {
              // only scan closed files !!!!
              unsigned long long fileid = eos::common::FileId::Hex2Fid(fileId.c_str());
              bool isopenforwrite=false;

              gOFS.OpenFidMutex.Lock();
              if (gOFS.WOpenFid[fsid].count(fileid)) {
                if (gOFS.WOpenFid[fsid][fileid]>0) {
                  isopenforwrite=true;
                }
              }
              gOFS.OpenFidMutex.UnLock();

              std::string val="";
              // token[0]: fxid
              entry += fileId;
              entry += ":";
              // token[1] scandir timestap
              val = attr->Get("user.eos.timestamp").c_str();
              entry += val.length()?val.c_str():"x";
              entry += ":";
              // token[2] creation checksum
              val = "";
              char checksumVal[SHA_DIGEST_LENGTH];
              size_t checksumLen;
              memset(checksumVal,0,SHA_DIGEST_LENGTH);
              if (attr->Get("user.eos.checksum", checksumVal, checksumLen)) {
                for (unsigned int i=0; i< SHA_DIGEST_LENGTH; i++) {
                  char hb[3]; sprintf(hb,"%02x", (unsigned char) (checksumVal[i]));
                  val += hb;
                }
              }

              entry += val.length()?val.c_str():"x";
              entry += ":";
              // token[3] tag for file checksum error
              val = attr->Get("user.eos.filecxerror").c_str();
              entry += val.length()?val.c_str():"x";
              entry += ":";
              // token[4] tag for block checksum error
              val = attr->Get("user.eos.blockcxerror").c_str();
              entry += val.length()?val.c_str():"x";
              entry += ":";
              // token[5] tag for physical size
              entry += eos::common::StringConversion::GetSizeString(sizestring,(unsigned long long)st_buf.st_size);
              entry += ":";
              if (fsid) {
                FmdSqlite* fmd = gFmdSqliteHandler.GetFmd(eos::common::FileId::Hex2Fid(fileId.c_str()), fsid, 0,0,0,0, true);
                if (fmd) {
                  // token[6] size in changelog
                  entry += eos::common::StringConversion::GetSizeString(sizestring, fmd->fMd.size);
                  entry += ":";

		  entry += fmd->fMd.checksum.c_str();
                  delete fmd;
                } else {
                  entry += "x:x:";
                }
              } else {
                entry += "0:0:";
              }
              
              gOFS.OpenFidMutex.Lock();
              if (gOFS.WOpenFid[fsid].count(fileid)) {
                if (gOFS.WOpenFid[fsid][fileid]>0) {
                  isopenforwrite=true;
                }
              }
              gOFS.OpenFidMutex.UnLock();
              // token[8] :1 if it is write-open and :0 if not
              if (isopenforwrite) {
                entry += ":1";
              } else {
                entry += ":0";
              }
              entry += "\n";
              nfound++;
            }
            if (attr)
              delete attr;
          }
        }
      }
      if (nfound)
        break;
    }
  }

  if (nfound==0) 
    return 0;
  else
    return entry.c_str();
}

/*----------------------------------------------------------------------------*/

int
XrdFstOfsDirectory::close()
{
  if (fts_tree) {
    fts_close(fts_tree);
    fts_tree = 0;
  }
  if (fts_paths) {
    free(fts_paths);
    fts_paths=0;
  }
  return SFS_OK;
}
EOSFSTNAMESPACE_END

