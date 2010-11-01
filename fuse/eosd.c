/************************************************************************/
/* eosd.c                                                               */
/*                                                                      */
/* Author: Andreas-Joachim Peters (CERN,2008)                           */
/*                                                                      */
/* FUSE based xrootd multi-userfilesystem daemon                        */
/* Modified 2010 for EOS support                                        */
/************************************************************************/
/*
/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags --libs` hello_ll.c -o hello_ll
*/

#define FUSE_USE_VERSION 26

#include <fuse/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "xrdposix.hh"

int isdebug=0;
// mount hostport;
char mounthostport[1024];

// mount prefix 
char mountprefix[1024];
double entrycachetime = 5.0;
double attrcachetime = 5.0;
double readopentime = 5.0;

/*****************************************************************************/
/* be carefull - this structure was copied out of the fuse<XX>.c source code */
/* it might change in newer fuse version                                     */
/*****************************************************************************/
struct fuse_ll;
struct fuse_req {
  struct fuse_ll *f;
  uint64_t unique;
  int ctr;
  pthread_mutex_t lock;
  struct fuse_ctx ctx;
  struct fuse_chan *ch;
  int interrupted;
  union {
    struct {
      uint64_t unique;
    } i;
    struct {
      fuse_interrupt_func_t func;
      void *data;
    } ni;
  } u;
  struct fuse_req *next;
  struct fuse_req *prev;
};

struct fuse_ll {
  int debug;
  int allow_root;
  struct fuse_lowlevel_ops op;
  int got_init;
  void *userdata;
  uid_t owner;
  struct fuse_conn_info conn;
  struct fuse_req list;
  struct fuse_req interrupts;
  pthread_mutex_t lock;
  int got_destroy;
};

static void eosfs_ll_readlink(fuse_req_t req, fuse_ino_t ino)
{
  struct stat stbuf;
  char fullpath[16384];
  const char* name = xrd_get_name_for_inode((long long)ino);
  // the root is inode 1
  if (ino == 1) {
    name = "/";
  }
  if (!name) {
    fuse_reply_err(req, ENXIO);
    return;
  }
  
  sprintf(fullpath,"root://%s@%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,name);
  if (isdebug) printf("[%s]: inode=%lld path=%s\n", __FUNCTION__,(long long)ino,fullpath);  
  char linkbuffer[8912];
  
  int retc = xrd_readlink(fullpath, linkbuffer, sizeof(linkbuffer));
  if (!retc) {
    fuse_reply_readlink(req,linkbuffer);
    return;
  } else {
    fuse_reply_err(req, errno);
    return;
  }
}

static void eosfs_ll_getattr(fuse_req_t req, fuse_ino_t ino,
				struct fuse_file_info *fi)
{
  struct stat stbuf;
  memset(&stbuf,0,sizeof(struct stat));
  char fullpath[16384];
  const char* name = xrd_get_name_for_inode((long long)ino);
  // the root is inode 1
  if (ino == 1) {
    name = "/";
  }
  if (!name) {
    fuse_reply_err(req, ENXIO);
    return;
  }

  sprintf(fullpath,"root://%s@%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,name);
  if (isdebug) printf("[%s]: inode=%lld path=%s\n", __FUNCTION__,(long long)ino,fullpath);

  int retc = xrd_stat(fullpath,&stbuf);
 
  fuse_reply_attr(req, &stbuf, attrcachetime);
}

static void eosfs_ll_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
				int to_set, struct fuse_file_info *fi)
{
  int retc=0;
  char fullpath[16384];
  const char* name = xrd_get_name_for_inode((long long)ino);
  // the root is inode 1
  if (ino == 1) {
    name = "/";
  }
  if (!name) {
    fuse_reply_err(req, ENXIO);
    return;
  }

  sprintf(fullpath,"root://%s@%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,name);
  if (isdebug) printf("[%s]: inode=%lld path=%s\n", __FUNCTION__,(long long)ino,fullpath);  
  if (to_set & FUSE_SET_ATTR_MODE) {
    if (isdebug) printf("[%s]: set attr mode ino=%lld\n", __FUNCTION__,(long long)ino);
    // disable chmod
    //retc = xrd_chmod(fullpath, attr->st_mode);
    retc = 0;
  }
  if ( (to_set & FUSE_SET_ATTR_UID) && (to_set & FUSE_SET_ATTR_GID) ) {
    if (isdebug) printf("[%s]: set attr uid  ino=%lld\n", __FUNCTION__,(long long)ino);
    if (isdebug) printf("[%s]: set attr gid  ino=%lld\n", __FUNCTION__,(long long)ino);
    // f.t.m. we fake it works !
    //    fuse_reply_err(req,EPERM);
    //    return;
  }
  if (to_set & FUSE_SET_ATTR_SIZE) {
    if (isdebug) printf("[%s]: set attr size=%d ino=%lld\n", __FUNCTION__,attr->st_size, (long long)ino);
    int fd;
    if ((fd = xrd_open(fullpath, O_WRONLY , S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))>=0) {
      retc = xrd_truncate(fd,attr->st_size);
      xrd_close(fd);
    } else {
      retc = -1;
    }
  }
  if ( (to_set & FUSE_SET_ATTR_ATIME) && (to_set & FUSE_SET_ATTR_MTIME) ) {  
    struct timespec tvp[2];
    tvp[0].tv_sec = attr->st_atime;
    tvp[0].tv_nsec = 0;
    tvp[1].tv_sec = attr->st_mtime;
    tvp[1].tv_nsec = 0;
    if (isdebug) printf("[%s]: set attr atime ino=%lld time=%u\n", __FUNCTION__,(long long)ino, attr->st_atime);
    if (isdebug) printf("[%s]: set attr mtime ino=%lld time=%u\n", __FUNCTION__,(long long)ino, attr->st_mtime);
    retc = xrd_utimes(fullpath, tvp);
  }
  struct stat newattr;
  memset(&newattr,0,sizeof(struct stat));
  if (!retc) {
    int retc = xrd_stat(fullpath,&newattr);
    if (!retc) {
      fuse_reply_attr(req, &newattr,attrcachetime);
    } else {
      fuse_reply_err(req, errno);
    }
  } else {
    fuse_reply_err(req, errno);
  }
}

static void eosfs_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
  const char* parentpath=NULL;
  char fullpath[16384];

  parentpath = xrd_get_name_for_inode(parent);
  if (parent ==1) {
    parentpath = "/";
  }
  if (!parentpath) {
    fuse_reply_err(req, ENXIO);
    return;
  }

  sprintf(fullpath,"root://%s@%s%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix, parentpath,name);
  if (isdebug) printf("[%s]: parent=%lld path=%s uid=%d\n", __FUNCTION__,(long long)parent,fullpath,req->ctx.uid);
  struct fuse_entry_param e;
  memset(&e, 0, sizeof(e));
  e.attr_timeout = attrcachetime;
  e.entry_timeout = entrycachetime;
  int retc = xrd_stat(fullpath,&e.attr);
  if (!retc) {
    char ifullpath[16384];
    if (name[0] == '/') 
      sprintf(ifullpath,"%s%s",parentpath,name);
    else
      sprintf(ifullpath,"%s/%s",parentpath,name);
    if (isdebug) printf("[%s]: storeinode=%lld path=%s\n", __FUNCTION__,(long long) e.attr.st_ino,ifullpath);
    e.ino = e.attr.st_ino;
    xrd_store_inode(e.attr.st_ino,ifullpath);
    fuse_reply_entry(req, &e);
  } else {
    if (errno == EFAULT) {
      e.ino = 0;
      fuse_reply_entry(req, &e);
    } else {
      fuse_reply_err(req, errno);
    }
  }
}

#define min(x, y) ((x) < (y) ? (x) : (y))

static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
			     off_t off, size_t maxsize)
{
  if (off < bufsize)
    return fuse_reply_buf(req, buf + off,
			  min(bufsize - off, maxsize));
  else 
    return fuse_reply_buf(req, NULL, 0);
}

static void eosfs_ll_opendir(fuse_req_t req, fuse_ino_t ino,
			      struct fuse_file_info *fi)
{
  char fullpath[16384];
  const char* name = xrd_get_name_for_inode((long long)ino);
  // the root is inode 1
  if (ino == 1) {
    name = "/";
  }
  if (!name) {
    fuse_reply_err(req, ENXIO);
    return;
  }
  
  sprintf(fullpath,"root://%s@%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,name);
  if (isdebug) printf("[%s]: inode=%lld path=%s\n", __FUNCTION__,(long long)ino,fullpath);
  
  DIR* dir ;
  dir = xrd_opendir(fullpath);
  fi->fh = (uint64_t) dir;

  fuse_reply_open(req, fi);
}

static void dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name,
		fuse_ino_t ino)
{
  struct stat stbuf;
  size_t oldsize = b->size;
  b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
  b->p = (char *) realloc(b->p, b->size);
  memset(&stbuf, 0, sizeof(stbuf));
  stbuf.st_ino = ino;
  fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,
                    b->size);
}

static void eosfs_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
			      off_t off, struct fuse_file_info *fi)
{
  struct stat stbuf;
  char fullpath[16384];
  static int i=0;
  const char* name = xrd_get_name_for_inode((long long)ino);
  // the root is inode 1
  if (ino == 1) {
    name = "/";
  }
  if (!name) {
    fuse_reply_err(req, ENXIO);
    return;
  }
  
  sprintf(fullpath,"root://%s@%s//proc/user/?mgm.cmd=fuse&mgm.subcmd=inodirlist&mgm.path=%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,name);
  if (isdebug) printf("[%s]: inode=%lld path=%s size=%d off=%d\n", __FUNCTION__,(long long)ino,fullpath,size,off);

  int retc = 0;

  int cnt;
  char* namep;
  unsigned long long in;

  cnt=0;

  struct dirent* entry;
  struct dirbuf* b;

  if (xrd_inodirlist_entry(ino,0,&namep,&in)) {
    xrd_inodirlist(ino,fullpath);
    b = xrd_inodirlist_getbuffer(ino);
    if (!b) {
      fuse_reply_err(req, EPERM);
      return;
    }
    b->p=NULL;
    b->size=0;
    while ((!xrd_inodirlist_entry(ino, cnt, &namep, &in))) {
      char ifullpath[16384];
      sprintf(ifullpath,"%s/%s",name,namep);
      struct stat newbuf;
      if (isdebug) printf("[%s]: add entry name=%s\n", __FUNCTION__,namep);
      dirbuf_add(req, b, namep, (fuse_ino_t) in);
      
      cnt++;
    }
 
  } else {
    b = xrd_inodirlist_getbuffer(ino);
  }
  
  if (isdebug) printf("[%s]: return size=%d ptr=%d\n", __FUNCTION__,b->size,b->p);
  reply_buf_limited(req,b->p,b->size,off,size);

}

static void eosfs_ll_releasedir (fuse_req_t req, fuse_ino_t ino,
				  struct fuse_file_info *fi)
{
  if (fi->fh) {
    xrd_closedir((DIR*)fi->fh);
  }
  
  xrd_inodirlist_delete(ino);

  fuse_reply_err(req, 0);
}

static void eosfs_ll_statfs(fuse_req_t req, fuse_ino_t ino)
{
  struct statvfs svfs;
  svfs.f_bsize=128*1024;
  svfs.f_blocks=1000000000ll;
  svfs.f_bfree=1000000000ll;
  svfs.f_bavail=1000000000ll;
  svfs.f_files=1000000;
  svfs.f_ffree=1000000;
  fuse_reply_statfs(req, &svfs);
}


static void eosfs_ll_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode,
			    dev_t rdev)
{
  int res;

  if (S_ISREG(mode)) {
    const char* parentpath=NULL;
    char fullpath[16384];
    char fullparentpath[16384];

    parentpath = xrd_get_name_for_inode(parent);
    if (parent ==1) {
      parentpath = "/";
    }
    if (!parentpath) {
      fuse_reply_err(req, ENXIO);
      return;
    }

    sprintf(fullpath,"root://%s@%s/%s%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,parentpath,name);
    sprintf(fullparentpath,"root://%s@%s/%s%s",xrd_mapuser(req->ctx.uid),mounthostport, mountprefix,parentpath);
    if (isdebug) printf("[%s]: parent=%lld path=%s uid=%d\n", __FUNCTION__,(long long)parent,fullpath,req->ctx.uid);

    res = xrd_open(fullpath, O_CREAT | O_EXCL | O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (res == -1) {
      fuse_reply_err(req, errno);
      return;
    }

    //    xrd_close(res); this is done via the mknodopenfilelist hash lifetime or release

    char ifullpath[16384];
    sprintf(ifullpath,"%s/%s",parentpath,name);
    struct fuse_entry_param e;
    memset(&e, 0, sizeof(e));
    e.attr_timeout = attrcachetime;
    e.entry_timeout = entrycachetime;
    struct stat newbuf;

    // update the entry
    int retc = xrd_stat(fullpath,&e.attr);
    e.ino = e.attr.st_ino;

    if (retc == -1) {
      fuse_reply_err(req,errno);
      return;
    } else {
      xrd_mknodopenfilelist_add(res,e.ino);
      xrd_store_inode(e.ino,ifullpath);
      if (isdebug) printf("[%s]: storeinode=%lld path=%s\n", __FUNCTION__,(long long) e.ino,ifullpath);
      fuse_reply_entry(req,&e);
      return;
    }
  }    
  fuse_reply_err(req,EINVAL);
}

static void eosfs_ll_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode)
{
  const char* parentpath=NULL;
  char fullpath[16384];

  parentpath = xrd_get_name_for_inode(parent);
  if (parent ==1) {
    parentpath = "/";
  }
  if (!parentpath) {
    fuse_reply_err(req, ENXIO);
    return;
  }

  sprintf(fullpath,"root://%s@%s/%s%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,parentpath,name);
  if (isdebug) printf("[%s]: path=%s\n", __FUNCTION__,fullpath);
  int retc = xrd_mkdir(fullpath,mode);
  if (!retc) {
    char ifullpath[16384];
    sprintf(ifullpath,"%s/%s",parentpath,name);
    struct fuse_entry_param e;
    memset(&e, 0, sizeof(e));
    e.attr_timeout = attrcachetime;
    e.entry_timeout = entrycachetime;
    struct stat newbuf;
    int retc = xrd_stat(fullpath,&e.attr);

    e.ino = e.attr.st_ino;

    if (retc) {
      fuse_reply_err(req,errno);
      return;
    } else {
      xrd_store_inode(e.attr.st_ino,ifullpath);
      fuse_reply_entry(req,&e);
      return;
    }
  } else {
    fuse_reply_err(req, errno);
    return;
  }
}

static void eosfs_ll_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
  const char* parentpath=NULL;
  char fullpath[16384];

  parentpath = xrd_get_name_for_inode(parent);
  if (parent ==1) {
    parentpath = "/";
  }
  if (!parentpath) {
    fuse_reply_err(req, ENXIO);
    return;
  }

  sprintf(fullpath,"root://%s@%s/%s%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,parentpath,name);
  if (isdebug) printf("[%s]: path=%s\n", __FUNCTION__,fullpath);
  int retc = xrd_unlink(fullpath);
  if (!retc) {
    fuse_reply_buf(req, NULL, 0);
    return;
  } else {
    fuse_reply_err(req, errno);
    return;
  }
}

static void eosfs_ll_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
  const char* parentpath=NULL;
  char fullpath[16384];

  parentpath = xrd_get_name_for_inode(parent);
  if (parent ==1) {
    parentpath = "/";
  }
  if (!parentpath) {
    fuse_reply_err(req, ENXIO);
    return;
  }

  sprintf(fullpath,"root://%s@%s/%s%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,parentpath,name);
  if (isdebug) printf("[%s]: path=%s\n", __FUNCTION__,fullpath);
  int retc = xrd_rmdir(fullpath);
  if (!retc) {
    fuse_reply_err(req, 0);
    return;
  } else {
    if (errno == ENOSYS) {
      fuse_reply_err(req, ENOTEMPTY);
    } else {
      fuse_reply_err(req, errno);
    }
    return;
  }
}

static void eosfs_ll_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, 
			      const char *name)
{
  char linksource[16384];
  char linkdest[16384];
  const char* parentpath=NULL;
  char fullpath[16384];
  char fulllinkpath[16384];

  parentpath = xrd_get_name_for_inode(parent);
  if (parent ==1) {
    parentpath = "/";
  }
  if (!parentpath) {
    fuse_reply_err(req, ENXIO);
    return;
  }

  sprintf(fullpath,"root://%s@%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,parentpath,name);
  if (isdebug) printf("[%s]: path=%s\n", __FUNCTION__,fullpath);

  sprintf(linksource,"%s/%s",parentpath,name);
  sprintf(linkdest,"%s/%s",parentpath,link);

  sprintf(fulllinkpath,"root://%s@%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,parentpath,link);
  if (isdebug) printf("[%s]: sourcepath=%s link=%s\n", __FUNCTION__,linksource,link);
  int retc = xrd_symlink(fullpath,linksource,link);
  if (!retc) {
    struct fuse_entry_param e;
    memset(&e, 0, sizeof(e));
    e.attr_timeout = attrcachetime;
    e.entry_timeout = entrycachetime;
    int retc = xrd_stat(fullpath,&e.attr);
    if (!retc) {
      if (isdebug) printf("[%s]: storeinode=%lld path=%s\n", __FUNCTION__,(long long)e.attr.st_ino,linksource);
      e.ino = e.attr.st_ino;
      xrd_store_inode(e.attr.st_ino,linksource);
      fuse_reply_entry(req, &e);
      return;
    } else {
      fuse_reply_err(req, errno);
      return;
    }
  } else {
    fuse_reply_err(req, errno);
    return;
  }
}

static void eosfs_ll_rename(fuse_req_t req, fuse_ino_t parent, const char *name,
			     fuse_ino_t newparent, const char *newname)
{
  const char* parentpath=NULL;
  const char* newparentpath=NULL;
  char fullpath[16384];
  char newfullpath[16384];
  parentpath = xrd_get_name_for_inode(parent);
  if (parent ==1) {
    parentpath = "/";
  }
  if (!parentpath) {
    fuse_reply_err(req, ENXIO);
    return;
  }
  newparentpath = xrd_get_name_for_inode(newparent);
  if (newparent ==1) {
    newparentpath = "/";
  }
  if (!newparentpath) {
    fuse_reply_err(req, ENXIO);
    return;
  }

  sprintf(fullpath,"root://%s@%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,parentpath,name);
  sprintf(newfullpath,"root://%s@%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,newparentpath,newname);

  struct stat stbuf;
  int retcold = xrd_stat(fullpath, &stbuf);
  if (isdebug) printf("[%s]: path=%s inode=%lu [%d]\n", __FUNCTION__,fullpath,stbuf.st_ino,retcold);
  if (isdebug) printf("[%s]: path=%s newpath=%s\n", __FUNCTION__,fullpath,newfullpath);
  int retc = xrd_rename(fullpath,newfullpath);
  if (!retc) {
    // update the inode store
    if (!retcold) {
      char iparentpath[16384];
      sprintf(iparentpath,"%s/%s",newparentpath,newname);
      if (isdebug) printf("[%s]: forgetting inode=%lu \n",__FUNCTION__,stbuf.st_ino);
      xrd_forget_inode(stbuf.st_ino);
      xrd_store_inode(stbuf.st_ino,iparentpath);
    }
    
    fuse_reply_err(req,0);
    return;
  } else {
    fuse_reply_err(req,0);
    return;
  }
}


static void eosfs_ll_link(fuse_req_t req, fuse_ino_t ino, fuse_ino_t parent,
			   const char *name)
{
  char linkdest[16384];
  const char* parentpath=NULL;
  char fullpath[16384];
  const char* sourcepath=NULL;

  parentpath = xrd_get_name_for_inode(parent);
  if (parent ==1) {
    parentpath = "/";
  }
  if (!parentpath) {
    fuse_reply_err(req, ENXIO);
    return;
  }
  sourcepath = xrd_get_name_for_inode(ino);
  if (!sourcepath) {
    fuse_reply_err(req, ENXIO);
    return;
  }
  
  sprintf(fullpath,"root://%s@%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,parentpath,name);
  if (isdebug) printf("[%s]: path=%s\n", __FUNCTION__,fullpath);

  sprintf(linkdest,"%s/%s",parentpath,name);

  if (isdebug) printf("[%s]: sourcepath=%s link=%s\n", __FUNCTION__,linkdest,sourcepath);
  int retc = xrd_link(fullpath,linkdest,sourcepath);
  if (!retc) {
    struct fuse_entry_param e;
    memset(&e, 0, sizeof(e));
    e.attr_timeout = attrcachetime;
    e.entry_timeout = entrycachetime;
    int retc = xrd_stat(fullpath,&e.attr);
    if (!retc) {
      if (isdebug) printf("[%s]: storeinode=%lld path=%s\n", __FUNCTION__,(long long)e.attr.st_ino,linkdest);
      e.ino = e.attr.st_ino;
      xrd_store_inode(e.attr.st_ino,linkdest);
      fuse_reply_entry(req, &e);
      return;
    } else {
      fuse_reply_err(req, errno);
      return;
    }
  } else {
    fuse_reply_err(req, errno);
    return;
  }
}


static void eosfs_ll_access(fuse_req_t req, fuse_ino_t ino, int mask)
{
  char fullpath[16384];
  const char* name = xrd_get_name_for_inode((long long)ino);
  // the root is inode 1
  if (ino == 1) {
    name = "/";
  }
  if (!name) {
    fuse_reply_err(req, ENXIO);
    return;
  }
  
  sprintf(fullpath,"root://%s@%s/%s/%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,name);
  if (isdebug) printf("[%s]: inode=%lld path=%s\n", __FUNCTION__,(long long)ino,fullpath);
  int retc = xrd_access(fullpath,mask);
  if (!retc) {
    fuse_reply_err(req, 0);
  } else {
    fuse_reply_err(req, errno);
  }
}


static void eosfs_ll_open(fuse_req_t req, fuse_ino_t ino,
			     struct fuse_file_info *fi)
{
  struct stat stbuf;
  char fullpath[16384];
  const char* name = xrd_get_name_for_inode((long long)ino);
  // the root is inode 1
  if (ino == 1) {
    name = "/";
  }
  if (!name) {
    fuse_reply_err(req, ENXIO);
    return;
  }

  sprintf(fullpath,"root://%s@%s/%s%s",xrd_mapuser(req->ctx.uid),mounthostport,mountprefix,name);
  
  int res;
  if ( fi->flags | O_RDWR | O_WRONLY | O_CREAT ) {
    if ( (res = xrd_mknodopenfilelist_get(ino)) >0) {
      xrd_mknodopenfilelist_release(res, ino);
    } else {
      res = xrd_open(fullpath, fi->flags,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    }
  } else {
    // the read-only case
    //    if (! (res = xrd_readopenfilelist_get(ino,req->ctx.uid) )) {
      res = xrd_open(fullpath, fi->flags,0);
      //      if (res >=0) {
      //	xrd_readopenfilelist_add(res, ino, req->ctx.uid, readopentime);
      //      }
  }

  if (isdebug) printf("[%s]: inode=%lld path=%s res=%d\n", __FUNCTION__,(long long)ino,fullpath,res);
  if (res == -1) {
    fuse_reply_err(req, errno);
    return;
  }

  fi->fh = res;

  if (getenv("EOS_KERNELCACHE") && (!strcmp(getenv("EOS_KERNELCACHE"),"1"))) {
    // TODO: this should be improved
    if (strstr(fullpath,"/proc/")) {
      fi->keep_cache = 0;
    } else {
      fi->keep_cache = 1;
    }
  } else {
    fi->keep_cache = 0;
  }

  if (getenv("EOS_DIRECTIO") && (!strcmp(getenv("EOS_DIRECTIO"),"1"))) {
    fi->direct_io=1;
  } else {
    fi->direct_io=0;
  }

  if (!fdbuffermap[fi->fh])
    fdbuffermap[fi->fh] = (char*) malloc(PAGESIZE);

  fuse_reply_open(req, fi);
}

static void eosfs_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size,
			   off_t off, struct fuse_file_info *fi)
{
  if (fi->fh) {
    char* buf = fdbuffermap[fi->fh];
    if (isdebug) printf("[%s]: inode=%lld size=%d off=%lld buf=%d fh=%d\n", __FUNCTION__,(long long)ino,size,off,buf,fi->fh);
    int res = xrd_pread(fi->fh, buf, size, off);
    if (res == -1) {
      fuse_reply_err(req, errno);
      return;
    }
    
    fuse_reply_buf(req, buf, res);
    return;
  } else {
    fuse_reply_err(req, ENXIO);
  }
}

static void eosfs_ll_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size,
			    off_t off, struct fuse_file_info *fi)
{
  if (fi->fh) {
    if (isdebug) printf("[%s]: inode=%lld size=%d off=%lld buf=%d fh=%d\n", __FUNCTION__,(long long)ino,size,off,buf,fi->fh);
    int res = xrd_pwrite(fi->fh, buf, size, off);
    if (res == -1) {
      fuse_reply_err(req, errno);
    }
    
    fuse_reply_write(req, res);
  } else {
    fuse_reply_err(req, ENXIO);
  }
}

static void eosfs_ll_release(fuse_req_t req, fuse_ino_t ino,
			    struct fuse_file_info *fi)
{
  if (fi->fh) {
    if (isdebug) printf("[%s]: inode=%lld fh=%d\n", __FUNCTION__,(long long)ino,fi->fh);
    int fd = (int)fi->fh;

    if (fdbuffermap[fi->fh]) {
      free (fdbuffermap[fi->fh]);
      fdbuffermap[fi->fh] = 0;
    }
    
    int res=0;

    if ( (res = xrd_mknodopenfilelist_get(ino)) >0) {
      xrd_mknodopenfilelist_release(res, ino);
    }

    //    if ((xrd_readopenfilelist_lease(ino,req->ctx.uid)<0)) {
      res = xrd_close(fd);
      //    }

    fi->fh = 0;
    if (res == -1) {
      fuse_reply_err(req, errno);
      return;
    }
  }
  fuse_reply_err(req,0);
}

static void eosfs_ll_fsync(fuse_req_t req, fuse_ino_t ino, int datasync,
			    struct fuse_file_info *fi)
{
  if (fi->fh) {
    if (isdebug) printf("[%s]: inode=%lld fh=%d\n", __FUNCTION__,(long long)ino,fi->fh);
    int res = xrd_fsync(fi->fh);
    if (res == -1) {
      fuse_reply_err(req, errno);
    }
  }
  fuse_reply_err(req,0);
}

static void eosfs_ll_forget (fuse_req_t req, fuse_ino_t ino, unsigned long nlookup)
{
  xrd_forget_inode(ino);
  fuse_reply_none(req);
}

static void eosfs_ll_flush (fuse_req_t req, fuse_ino_t ino,
			     struct fuse_file_info *fi) 
{
  fuse_reply_err(req,0);
}

static void eosfs_ll_getxattr (fuse_req_t req, fuse_ino_t ino, const char *name,
		  size_t size) 
{

}

static void eosfs_ll_listxattr (fuse_req_t req, fuse_ino_t ino, size_t size)
{

}

static void eosfs_ll_removexattr (fuse_req_t req, fuse_ino_t ino, const char *name)
{

}

static void eosfs_ll_setxattr (fuse_req_t req, fuse_ino_t ino, const char *name,
				const char *value, size_t size, int flags)
{

}

static struct fuse_lowlevel_ops eosfs_ll_oper = {
  .getattr	= eosfs_ll_getattr,
  .lookup	= eosfs_ll_lookup,
  .setattr	= eosfs_ll_setattr,
  .access	= eosfs_ll_access,
  .readlink	= eosfs_ll_readlink,
  .readdir	= eosfs_ll_readdir,
  //  .opendir      = eosfs_ll_opendir,
  .mknod	= eosfs_ll_mknod,
  .mkdir	= eosfs_ll_mkdir,
  .symlink	= eosfs_ll_symlink,
  .unlink	= eosfs_ll_unlink,
  .rmdir	= eosfs_ll_rmdir,
  .rename	= eosfs_ll_rename,
  .link	        = eosfs_ll_link,
  .open	        = eosfs_ll_open,
  .read	        = eosfs_ll_read,
  .write	= eosfs_ll_write,
  .statfs	= eosfs_ll_statfs,
  .release	= eosfs_ll_release,
  .releasedir   = eosfs_ll_releasedir,
  .fsync	= eosfs_ll_fsync,
  .forget       = eosfs_ll_forget,
  .flush        = eosfs_ll_flush,
  /*  .setxattr     = eosfs_ll_setxattr,
  .getxattr     = eosfs_ll_getxattr,
  .listxattr    = eosfs_ll_listxattr,
  .removexattr  = eosfs_ll_removexattr,*/
};

int main(int argc, char *argv[])
{
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct fuse_chan *ch;
  time_t xcfsatime;

  char *mountpoint;
  int err = -1;
  int i;

  char *epos, *spos, *rdr;

  for (i=0; i< argc; i++) {
    if (!strcmp(argv[i],"-d")) {
      isdebug = 1;
    }
  }

  xcfsatime=time(NULL);
  for (i=0; i< argc; i++) {
    if ( (spos=strstr(argv[i],"url=root://"))) {
      if ( (epos=strstr(spos+11,"//" )) ) {
      	//*(epos+2) = 0;
	*(spos) = 0;
	if (*(spos-1) == ',') {
	  *(spos-1) = 0;
	}
	setenv("EOS_RDRURL",spos+4,1);
	setenv("LD_LIBRARY_PATH","/opt/xrootd/lib",1);
      }
    }
  }

  rdr = getenv("EOS_RDRURL");


  fprintf(stdout,"EOS_RDRURL = %s\n", getenv("EOS_RDRURL"));

  if (! rdr) {
    fprintf(stderr,"error: EOS_RDRURL is not defined or add root://<host>// to the options argument\n");
    exit(-1);
  }

  if (strchr(rdr,'@') ) {
    fprintf(stderr,"error: EOS_RDRURL or url option contains user specification '@' - forbidden\n");
    exit(-1);
  }

  xrd_init();

  // move the mounthostport starting with the host name
  char* pmounthostport=0;
  char* smountprefix=0;

  pmounthostport = strstr(rdr,"root://");
  if (!pmounthostport) {
    fprintf(stderr,"error: EOS_RDRURL or url option is not valid\n");
    exit(-1);
  }

  pmounthostport += 7;
  strcpy(mounthostport,pmounthostport);

  if (!(smountprefix = strstr(mounthostport,"//"))) {
    fprintf(stderr,"error: EOS_RDRURL or url option is not valid\n");
    exit(-1);
  } else {
    smountprefix++;
    strcpy(mountprefix,smountprefix);
    *smountprefix=0;

    if (mountprefix[strlen(mountprefix)-1] == '/') {
      mountprefix[strlen(mountprefix)-1] = 0;
    }

    if (mountprefix[strlen(mountprefix)-1] == '/') {
      mountprefix[strlen(mountprefix)-1] = 0;
    }
  }
  
  fprintf(stdout,"mounthost=%s mountmountprefix=%s\n", mounthostport, mountprefix);

  if (!isdebug) {
    pid_t m_pid=fork();
    if(m_pid<0) {
      fprintf(stderr,"ERROR: Failed to fork daemon process\n");
      exit(-1);
    } 

    // kill the parent
    if(m_pid>0) {
      exit(0);
    }
    
    umask(0); 

    pid_t sid;
    if((sid=setsid()) < 0) {
      fprintf(stderr,"ERROR: failed to create new session (setsid())\n");
      exit(-1);
    }

    if ((chdir("/")) < 0) {
      /* Log any failure here */
      exit(-1);
    }
    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  }

  if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
      (ch = fuse_mount(mountpoint, &args)) != NULL) {
    struct fuse_session *se;
    
    se = fuse_lowlevel_new(&args, &eosfs_ll_oper,
			   sizeof(eosfs_ll_oper), NULL);
    if (se != NULL) {
      if (fuse_set_signal_handlers(se) != -1) {
	fuse_session_add_chan(se, ch);
	err = fuse_session_loop(se);
	fuse_remove_signal_handlers(se);
	fuse_session_remove_chan(ch);
      }
      fuse_session_destroy(se);
    }
    fuse_unmount(mountpoint, ch);
  }
  fuse_opt_free_args(&args);
  
  return err ? 1 : 0;
}
