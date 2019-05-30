#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    disk_sector_t inode_sector;         /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (disk_sector_t sector, size_t entry_cnt) 
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      thread_current()->dir = dir;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, disk_sector_t inode_sector) 
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

struct dir* dir_from_path(const char *imsi_path){
  char *token;
  char *next_ptr;
  //int cnt = 1;
  struct dir *dir;
  char *path = NULL;
  int n = strlen(imsi_path);
  memcpy (path, imsi_path, sizeof(char) * (n + 1));  
  if(path[0] == '/'){ // absolute path
    dir_close(thread_current()->dir);
    dir = dir_open_root();
    thread_current()->dir = dir;
    //path가 가리키는 부분을 / 이후로 만들어야 함.
  }
  else{               // relative path
    dir = thread_current()->dir;
  }

  token = strtok_r(path, "/", &next_ptr);
  //char **dir_tokens = (char**) palloc_get_page(0);
  //dir_tokens[0] = token;

  while(token){
    if (strcmp(token, "..") == 0){      //goto parent directory
      dir_close(thread_current()->dir); //dir_close 짤 때 root dir인지 확인하기
      dir = dir_open(parent_dir_inode(thread_current()->dir)); //dir_open할 때 thread_current()->dir 바꿔주기

    }
    else if (strcmp(token, ".") == 0){  //maintain current directory
      continue;
    }
    else if (token == NULL){ // dir로 아무것도 안 들어왔을 때
      return NULL;
    }
    else{ //name에 해당하는 dir_entry 찾아서 열어준다. dir일 때 dir에 넣어준다.
      struct inode **inode = NULL;
      if (dir_lookup(thread_current()->dir, token, inode)){
        if (inode_is_dir(*inode)){ //찾은 애가 dir이면 원래 dir 닫아주고 새로운 dir 연다.
          dir_close(thread_current()->dir);
          dir = dir_open(*inode);
        }
        else{ //file_name일 것이다.
          inode_close(*inode);
          break;
        }
      }
      else{ //dir 안에 name에 해당하는 애가 없으면 return null
        return NULL;
      }
    }
    token = strtok_r(NULL, "/", &next_ptr);
    //dir_tokens[cnt++] = token
    
  }
  return dir; //이 dir를 open한 상태로 dir를 return한다.

}

char *filename_from_path(const char *imsi_path){
  char *token;
  char *next_ptr;
  //int cnt = 1;
  char *file_name = NULL;
  char *last = NULL;

  char *path = NULL;
  int n = strlen(imsi_path);
  memcpy (path, imsi_path, sizeof(char) * (n + 1));  


  token = strtok_r(path, "/", &next_ptr);
  //printf("string : %s with index %d\n", cmdline_tokens[0], 0);

  while (token) {
     if (token == NULL)
      break;
    else {
      last = token;
    }
    token = strtok_r(NULL, "/", &next_ptr);
  }

  memcpy(file_name, last, strlen(last) + 1);

  if(strlen(file_name) > 14){ // file length must be lower than 14
    return NULL;
  }
  return file_name;
}

struct inode *parent_dir_inode(struct dir *dir){
  
  return NULL;
}

