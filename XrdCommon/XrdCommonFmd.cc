/*----------------------------------------------------------------------------*/
#include "XrdCommon/XrdCommonFmd.hh"
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
#include <stdio.h>
#include <sys/mman.h>
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
XrdCommonFmdHandler gFmdHandler;
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
bool 
XrdCommonFmdHeader::Read(int fd) 
{
  // the header is already in the beginning
  lseek(fd,0,SEEK_SET);
  int nread = read(fd,&fmdHeader, sizeof(struct FMDHEADER));
  if (nread != sizeof(struct FMDHEADER)) {
    eos_crit("unable to read fmd header");
    return false;
  }

  eos_info("fmd header version %s creation time is %u filesystem id %04d", fmdHeader.version, fmdHeader.ctime, fmdHeader.fsid);
  if (strcmp(fmdHeader.version, VERSION)) {
    eos_crit("fmd header contains version %s but this is version %s", fmdHeader.version, VERSION);
    return false;
  }
  if (fmdHeader.magic != XRDCOMMONFMDHEADER_MAGIC) {
    eos_crit("fmd header magic is wrong - found %x", fmdHeader.magic);
    return false;
  }
  return true;
}
 
/*----------------------------------------------------------------------------*/
bool 
XrdCommonFmdHeader::Write(int fd) 
{
  // the header is always in the beginning
  lseek(fd,0,SEEK_SET);
  int nwrite = write(fd,&fmdHeader, sizeof(struct FMDHEADER));
  if (nwrite != sizeof(struct FMDHEADER)) {
    eos_crit("unable to write fmd header");
    return false;
  }
  eos_debug("wrote fmd header version %s creation time %u filesystem id %04d", fmdHeader.version, fmdHeader.ctime, fmdHeader.fsid);
  return true;
}

/*----------------------------------------------------------------------------*/
bool 
XrdCommonFmd::Write(int fd) 
{
  // compute crc32
  fMd.crc32 = ComputeCrc32((char*)(&(fMd.fid)), sizeof(struct FMD) - sizeof(fMd.magic) - (2*sizeof(fMd.sequencetrailer)) - sizeof(fMd.crc32));
  eos_debug("computed CRC for fileid %d to %x", fMd.fid, fMd.crc32);
  if ( (write(fd, &fMd, sizeof(fMd))) != sizeof(fMd)) {
    eos_crit("failed to write fmd struct");
    return false;
  }
  return true;
}

/*----------------------------------------------------------------------------*/
bool 
XrdCommonFmd::Read(int fd, off_t offset) 
{
  if ( (pread(fd, &fMd, sizeof(fMd),offset)) != sizeof(fMd)) {
    eos_crit("failed to read fmd struct");
    return false;
  }

  return true;
}


/*----------------------------------------------------------------------------*/
bool
XrdCommonFmdHandler::SetChangeLogFile(const char* changelogfilename, int fsid) 
{
  Mutex.Lock();
  bool isNew=false;

  char fsChangeLogFileName[1024];
  sprintf(fsChangeLogFileName,"%s.%04d.mdlog", ChangeLogFileName.c_str(),fsid);
  
  // clear the hash
  Fmd.clear();
  if (fdChangeLogRead[fsid]>0) {
    eos_info("closing changelog read file %s", ChangeLogFileName.c_str());
    close(fdChangeLogRead[fsid]);
  }
  if (fdChangeLogWrite[fsid]>0) {
    eos_info("closing changelog read file %s", ChangeLogFileName.c_str());
    close(fdChangeLogWrite[fsid]);
  }

  sprintf(fsChangeLogFileName,"%s.%04d.mdlog", changelogfilename,fsid);
  ChangeLogFileName = changelogfilename;
  eos_info("changelog file is now %s\n", ChangeLogFileName.c_str());

  // check if this is a new changelog file
  if ((access(fsChangeLogFileName,R_OK))) {
    isNew = true;
  }

  if ( (fdChangeLogWrite[fsid] = open(fsChangeLogFileName,O_CREAT| O_RDWR, 0600 )) <0) {
    eos_err("unable to open changelog file for writing %s",fsChangeLogFileName);
    fdChangeLogWrite[fsid] = fdChangeLogRead[fsid] = -1;
    Mutex.UnLock();
    return false;
  }

  // spool to the end
  lseek(fdChangeLogWrite[fsid], 0, SEEK_END);
 
  if ( (fdChangeLogRead[fsid] = open(fsChangeLogFileName,O_RDONLY)) < 0) {
    eos_err("unable to open changelog file for writing %s",fsChangeLogFileName);
    close(fdChangeLogWrite[fsid]);
    fdChangeLogWrite[fsid] = fdChangeLogRead[fsid] = -1;
    Mutex.UnLock();
    return false;
  }
  eos_info("opened changelog file %s for filesystem %04d", fsChangeLogFileName, fsid);

  if (isNew) {
    // try to write the header
    fmdHeader.SetId(fsid);
    fmdHeader.SetLogId("FmdHeader");
    if (!fmdHeader.Write(fdChangeLogWrite[fsid])) {
      // cannot write the header 
      isOpen=false;
      Mutex.UnLock();
      return isOpen;
    }
  }

  isOpen = ReadChangeLogHash(fsid);

  Mutex.UnLock();
  return isOpen;
}


int 
XrdCommonFmdHandler::CompareMtime(const void* a, const void *b) {
  struct filestat {
    struct stat buf;
    char filename[256];
  };
  return ( (((struct filestat*)a)->buf.st_mtime) - ((struct filestat*)b)->buf.st_mtime);
}

/*----------------------------------------------------------------------------*/
bool XrdCommonFmdHandler::AttachLatestChangeLogFile(const char* changelogdir, int fsid) 
{
  DIR *dir;
  struct dirent *dp;
  struct filestat {
    struct stat buf;
    char filename[1024];
  };

  int nobjects=0;
  long tdp=0;

  struct filestat* allstat = 0;
  XrdOucString Directory = changelogdir;
  XrdOucString FileName ="";
  char fileend[1024];
  sprintf(fileend,".%04d.mdlog",fsid);
  if ((dir = opendir(Directory.c_str()))) {
    tdp = telldir(dir);
    while ((dp = readdir (dir)) != 0) {
      FileName = dp->d_name;
      if ( (!strcmp(dp->d_name,".")) || (!strcmp(dp->d_name,".."))
           || (strlen(dp->d_name) != strlen("fmd.1272892439.0000.mdlog")) || (strncmp(dp->d_name,"fmd.",4)) || (!FileName.endswith(fileend)))
        continue;
      
      nobjects++;
    }
    allstat = (struct filestat*) malloc(sizeof(struct filestat) * nobjects);
    if (!allstat) {
      eos_err("cannot allocate sorting array");
      return false;
    }
   
    eos_debug("found %d old changelog files\n", nobjects);
    // go back
    seekdir(dir,tdp);
    int i=0;
    while ((dp = readdir (dir)) != 0) {
      FileName = dp->d_name;
      if ( (!strcmp(dp->d_name,".")) || (!strcmp(dp->d_name,".."))
           || (strlen(dp->d_name) != strlen("fmd.1272892439.0000.mdlog")) || (strncmp(dp->d_name,"fmd.",4)) || (!FileName.endswith(fileend)))
        continue;
      char fullpath[8192];
      sprintf(fullpath,"%s/%s",Directory.c_str(),dp->d_name);

      sprintf(allstat[i].filename,"%s",dp->d_name);
      eos_debug("stat on %s\n", dp->d_name);
      if (stat(fullpath, &(allstat[i].buf))) {
        eos_err("cannot stat after readdir file %s", fullpath);
      }
      i++;
    }
    closedir(dir);
    // do the sorting
    qsort(allstat,nobjects,sizeof(struct filestat),XrdCommonFmdHandler::CompareMtime);
  } else {
    eos_err("cannot open changelog directory",Directory.c_str());
    return false;
  }
  
  XrdOucString changelogfilename = changelogdir;

  if (allstat && (nobjects>0)) {
    // attach an existing one
    changelogfilename += "/";
    while (changelogfilename.replace("//","/")) {};
    changelogfilename += allstat[0].filename;
    changelogfilename.replace(fileend,"");
    eos_info("attaching existing changelog file %s", changelogfilename.c_str());
    free(allstat);
  } else {
    // attach a new one
    changelogfilename += "fmd."; char now[1024]; sprintf(now,"%u",(unsigned int) time(0)); changelogfilename += now; 
    eos_info("creating new changelog file %s", changelogfilename.c_str());
  }
  // initialize sequence number
  fdChangeLogSequenceNumber[fsid]=0;
  return SetChangeLogFile(changelogfilename.c_str(),fsid);
}

/*----------------------------------------------------------------------------*/
bool XrdCommonFmdHandler::ReadChangeLogHash(int fsid) 
{
  if (!fmdHeader.Read(fdChangeLogRead[fsid])) {
    // failed to read header
    return false;
  }
  struct stat stbuf;

  if (::fstat(fdChangeLogRead[fsid], &stbuf)) {
    eos_crit("unable to stat file size of changelog file - errc%d", errno);
    return false;
  }

  if (stbuf.st_size > (4 * 1000l*1000l*1000l)) {
    // we don't map more than 4 GB ... should first trim here
    eos_crit("changelog file exceeds memory limit of 4 GB for boot procedure");
    return false;
  }
  
  // mmap the complete changelog ... wow!

  if ( (unsigned long)stbuf.st_size <= sizeof(struct XrdCommonFmdHeader::FMDHEADER)) {
    eos_info("changelog is empty - nothing to check");
    return true;
  }

  char* changelogmap   =  (char*) mmap(0, stbuf.st_size, PROT_READ, MAP_SHARED, fdChangeLogRead[fsid],0);

  if (!changelogmap) {
    eos_crit("unable to mmap changelog file - errc=%d",errno);
    return false;
  }

  char* changelogstart =  changelogmap + sizeof(struct XrdCommonFmdHeader::FMDHEADER);;
  char* changelogstop  =  changelogmap + stbuf.st_size;
  struct XrdCommonFmd::FMD* pMd;
  bool success = true;
  unsigned long sequencenumber=0;
  int retc=0;
  int nchecked=0;

  eos_debug("memory mapped changelog file at %lu", changelogstart);

  while ( (changelogstart+sizeof(struct XrdCommonFmd::FMD)) <= changelogstop) {
    nchecked++;
    eos_debug("checking SEQ# %d # %d", sequencenumber, nchecked);
    pMd = (struct XrdCommonFmd::FMD*) changelogstart;
    eos_debug("%llx %llx %ld %llx %lu %llu %x", pMd, &(pMd->fid), sizeof(*pMd), pMd->magic, pMd->sequenceheader, pMd->fid,pMd->crc32);
    if (!(retc = XrdCommonFmd::IsValid(pMd, sequencenumber))) {
      // good meta data block
    } else {
      // illegal meta data block
      if (retc == EINVAL) {
	eos_crit("Block is neither creation/update or deletion block %u offset %llu", sequencenumber, ((char*)pMd) - changelogmap);
      }
      if (retc == EILSEQ) {
	eos_crit("CRC32 error in meta data block sequencenumber %u offset %llu", sequencenumber, ((char*)pMd) - changelogmap);
      }
      if (retc == EOVERFLOW) {
	eos_crit("SEQ# error in meta data block sequencenumber %u offset %llu", sequencenumber, ((char*)pMd) - changelogmap);
      }
      if (retc == EFAULT) {
	eos_crit("SEQ header/trailer mismatch in meta data block sequencenumber %u/%u offset %llu", pMd->sequenceheader,pMd->sequencetrailer, ((char*)pMd) - changelogmap);
      }
      success = false;
    }

    if (pMd->sequenceheader > (unsigned long long) fdChangeLogSequenceNumber[fsid]) {
      fdChangeLogSequenceNumber[fsid] = pMd->sequenceheader;
    }

    // setup the hash entries
    Fmd.insert(make_pair(pMd->fid, (unsigned long long) (changelogstart-changelogmap)));
    // do quota hashs
    if (XrdCommonFmd::IsCreate(pMd)) {
      unsigned long long exsize = FmdSize[pMd->fid];
      if (exsize) {
	// substract old size
	UserBytes[pMd->uid]  -= exsize;
	GroupBytes[pMd->gid] -= exsize;
	UserFiles[pMd->uid]--;
	GroupFiles[pMd->gid]--;
      }
      // store new size
      FmdSize[pMd->fid] = pMd->size;

      // add new size
      UserBytes[pMd->uid]  += pMd->size;
      GroupBytes[pMd->gid] += pMd->size;
      UserFiles[pMd->uid]++;
      GroupFiles[pMd->gid]++;
    }
    if (XrdCommonFmd::IsDelete(pMd)) {
      FmdSize[pMd->fid] = 0;
      UserBytes[pMd->uid]  -= pMd->size;
      GroupBytes[pMd->gid] -= pMd->size;
      UserFiles[pMd->uid]--;
      GroupFiles[pMd->gid]--;
    }
    pMd++;
    changelogstart += sizeof(struct XrdCommonFmd::FMD);
  }
  munmap(changelogmap, stbuf.st_size);
  eos_debug("checked %d FMD entries",nchecked);
  return success;
}

/*----------------------------------------------------------------------------*/
XrdCommonFmd*
XrdCommonFmdHandler::GetFmd(unsigned long long fid, unsigned int fsid, uid_t uid, gid_t gid, unsigned int layoutid, bool isRW) 
{
  Mutex.Lock();
  if (fdChangeLogRead[fsid]>0) {
    if (Fmd[fid] != 0) {
      // this is to read an existing entry
      XrdCommonFmd* fmd = new XrdCommonFmd();
      if (!fmd->Read(fdChangeLogRead[fsid],Fmd[fid])) {
	eos_crit("unable to read block for fid %d on fs %d", fid, fsid);
	Mutex.UnLock();
	return 0;
      }
      if ( fmd->fMd.fid != fid) {
	// fatal this is somehow a wrong record!
	eos_crit("unable to get fmd for fid %d on fs %d - file id mismatch in meta data block", fid, fsid);
	Mutex.UnLock();
	return 0;
      }
      if ( fmd->fMd.fsid != fsid) {
	// fatal this is somehow a wrong record!
	eos_crit("unable to get fmd for fid %d on fs %d - filesystem id mismatch in meta data block", fid, fsid);
	Mutex.UnLock();
	return 0;
      }
      // return the new entry
      Mutex.UnLock();	
      return fmd;
    }
    if (isRW) {
      // make a new record
      XrdCommonFmd* fmd = new XrdCommonFmd(fid, fsid);
      fmd->MakeCreationBlock();
      
      if (fdChangeLogWrite[fsid]>0) {
	off_t position = lseek(fdChangeLogWrite[fsid],0,SEEK_CUR);

	fdChangeLogSequenceNumber[fsid]++;
	// set sequence number
	fmd->fMd.uid = uid;
	fmd->fMd.gid = gid;
	fmd->fMd.lid = layoutid;
	fmd->fMd.sequenceheader=fmd->fMd.sequencetrailer = fdChangeLogSequenceNumber[fsid];
	fmd->fMd.ctime = fmd->fMd.mtime = time(0);
	// get micro seconds
	struct timeval tv;
	struct timezone tz;
	
	gettimeofday(&tv, &tz);
	fmd->fMd.ctime_ns = fmd->fMd.mtime_ns = tv.tv_usec * 1000;
	
	// write this block
	if (!fmd->Write(fdChangeLogWrite[fsid])) {
	  // failed to write
	  eos_crit("failed to write new block for fid %d on fs %d", fid, fsid);
	  Mutex.UnLock();
	  return 0;
	}
	// add to the in-memory hashes
	Fmd[fid]     = position;
	FmdSize[fid] = 0;

	// add new file counter
	UserFiles[fmd->fMd.uid] ++;
	GroupFiles[fmd->fMd.gid] ++;
  
	
	eos_debug("returning meta data block for fid %d on fs %d", fid, fsid);
	// return the mmaped meta data block
	Mutex.UnLock();
	return fmd;
      } else {
	eos_crit("unable to write new block for fid %d on fs %d - no changelog file open for writing", fid, fsid);
	Mutex.UnLock();
	return 0;
      }
    } else {
      eos_err("unable to get fmd for fid %d on fs %d - record not found", fid, fsid);
      Mutex.UnLock();
      return 0;
    }
  } else {
    eos_crit("unable to get fmd for fid %d on fs %d - there is no changelog file open for that file system id", fid, fsid);
    Mutex.UnLock();
    return 0;
  }
}


/*----------------------------------------------------------------------------*/
bool
XrdCommonFmdHandler::DeleteFmd(unsigned long long fid, unsigned int fsid) 
{
  Mutex.Lock();
   
  Mutex.UnLock();
  return false;
}


/*----------------------------------------------------------------------------*/
bool
XrdCommonFmdHandler::Commit(XrdCommonFmd* fmd)
{
  if (!fmd)
    return false;

  int fsid = fmd->fMd.fsid;
  int fid  = fmd->fMd.fid;

  Mutex.Lock();
  
  // get file position
  off_t position = lseek(fdChangeLogWrite[fsid],0,SEEK_CUR);
  fdChangeLogSequenceNumber[fsid]++;
  // set sequence number
  fmd->fMd.sequenceheader=fmd->fMd.sequencetrailer = fdChangeLogSequenceNumber[fsid];

  // put modification time
  fmd->fMd.mtime = time(0);
  // get micro seconds
  struct timeval tv;
  struct timezone tz;
  
  gettimeofday(&tv, &tz);
  fmd->fMd.mtime_ns = tv.tv_usec * 1000;
  
  // write this block
  if (!fmd->Write(fdChangeLogWrite[fsid])) {
    // failed to write
    eos_crit("failed to write commit block for fid %d on fs %d", fid, fsid);
    Mutex.UnLock();
    return false;
  }

  // store present size
  unsigned long long oldsize = FmdSize[fid];
  // add to the in-memory hashes
  Fmd[fid]     = position;
  FmdSize[fid] = fmd->fMd.size;


  // adjust the quota accounting of the update
  eos_debug("booking %d new bytes on quota %d/%d", (fmd->fMd.size-oldsize), fmd->fMd.uid, fmd->fMd.gid);
  UserBytes[fmd->fMd.uid] += (fmd->fMd.size-oldsize);
  GroupBytes[fmd->fMd.gid] += (fmd->fMd.size-oldsize);

  Mutex.UnLock();
  return true;
}

