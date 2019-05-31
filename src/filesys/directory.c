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
dir_create (disk_sector_t sector, size_t entry_cnt, disk_sector_t parent_disk_sector) 
{
  bool success = inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
  struct inode* inode = inode_open(sector);
  inode_equip_parent(parent_disk_sector, inode);
  inode_close(inode);
  return success; 
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  //printf("dir open\n");
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      //thread_current()->dir = dir;
      //printf("dir open success");
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
  //printf("open root \n");
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
  off_t ofs, offset=0;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  bool flag = true;

  /*여기가 rm-parent (확인)*/
  while ((sizeof e) != inode_read_at (dir->inode, &e, sizeof e, offset)) {
    if (e.in_use) {
      flag = false;
    }
    offset += (sizeof e);
  }

  if (flag == false) {
    goto done;
  }

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
  struct dir *dir;

  unsigned int n = strlen(imsi_path);
  char *path = malloc(n + 1);
  memcpy (path, imsi_path, (n + 1));  

  /*path 잘 들어갔는지 확인*/
  /*if (strlen(path) > 0){
    printf("path : %s\n", path);
  }*/
  /*path는 잘 들어감*/

  /*path is relative or not?*/
  if (path[0] == '/') {
    //printf("here1\n");
    dir = dir_open_root();
  }

  else {
    if (thread_current()->dir == NULL) {
      //printf("here2\n");
      dir = dir_open_root();
    }
    else {
      //printf("here3\n");
      //printf("%p\n", thread_current()->dir);
      dir = dir_reopen(thread_current()->dir);
    }
  }
  //dir = dir_open_root();
  //printf("%p\n", dir);
  
  token = strtok_r(path, "/", &next_ptr);

  while (token != NULL) {
    struct inode *inode = NULL;
    if (strcmp(token, ".") == 0) continue;
    else if (strcmp(token, "..")) {
      inode = inode_open(inode_parent_sector(dir_get_inode(dir)));
      struct dir* next_dir = dir_open(inode);
      if (next_dir == NULL) {
        dir_close(dir);
        free(path);
        return NULL;
      }
      dir_close(dir);
      dir = next_dir;
    }
    else{ 
      if (dir_lookup((const struct dir *) dir, (const char *) token, &inode)) {
        struct dir* next_dir = dir_open(inode);
        if (next_dir == NULL) {
          dir_close(dir);
          free(path);
          return NULL;
        }
        else {
          dir_close(dir);
          dir = next_dir;
        }
      }

      else {
        dir_close(dir);
        free(path);
        return NULL;
      }
    }
    token = strtok_r(NULL, "/", &next_ptr);
  }
  if (inode_is_removed(dir->inode)) {
    dir_close(dir);
    free(path);
    return NULL;
  }
  free(path);
  return dir; 
}

char *filename_from_path(const char *imsi_path, char* path_wo_fn){

  char *token;
  char *next_ptr;
  //int cnt = 1;
  unsigned int n = strlen(imsi_path);

  char *path = malloc(n + 1);
  char *file_name = malloc(n + 1);
  char *last = "";

  memcpy (path, imsi_path, (n + 1)); 

  if (path[0] == '/') {
    path_wo_fn[0] = '/';
    path_wo_fn++;
  } 

  token = strtok_r(path, "/", &next_ptr);

  if (token == NULL) {
    memcpy(file_name, imsi_path, n + 1);
    return file_name;
  }

  while (token != NULL) {
    if (strlen(last) > 0){
      memcpy(path_wo_fn, last, strlen(last));
      path_wo_fn += strlen(last);
      path_wo_fn[0] = '/';
      path_wo_fn++;
    }
    if (token == NULL)
      break;
    else {
      last = token;
    }
    token = strtok_r(NULL, "/", &next_ptr);
  }

  if (last == NULL) {
    *file_name = '\0';
    free(path);
    return file_name;
  }

  memcpy(file_name, last, strlen(last) + 1);

  if(strlen(file_name) > 14){ // file length must be lower than 14
    free(path);
    return NULL;
  }

  free(path);
  return file_name;
}

struct inode *parent_dir_inode(struct dir *dir){
  
  return NULL;
}

