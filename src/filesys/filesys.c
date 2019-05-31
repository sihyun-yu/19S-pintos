#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  free_map_init ();
  cache_init();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  cache_close();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  disk_sector_t inode_sector = 0;
  char path[strlen(name) + 1];
  memset(path,0, strlen(name)+1);
  char* file_name = filename_from_path(name, path);
  struct dir *dir = dir_from_path (path);
  bool success = false;
  if (file_name == NULL) {
    dir_close(dir);
    free(file_name);
    return false;
  }

  if (is_dir == false) {
    success = (dir != NULL
                    && free_map_allocate (1, &inode_sector)
                    && inode_create (inode_sector, initial_size, is_dir)
                    && dir_add (dir, file_name, inode_sector));
    if (!success && inode_sector != 0) 
      free_map_release (inode_sector, 1);
    dir_close (dir);
    free(file_name);

  }
  
  else 
  {
    struct inode *inode = NULL;
    success = !dir_lookup(dir, file_name, &inode) 
                    && free_map_allocate (1, &inode_sector)
                    && dir_create(inode_sector, 16, inode_number(dir_get_inode(dir)))
                    && dir_add (dir, file_name, inode_sector);
      if (!success && inode_sector != 0) 
        free_map_release (inode_sector, 1);
      dir_close (dir);
      free(file_name);

  }

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char path[strlen(name) + 1];
  memset(path,0, strlen(name)+1);
  struct inode *inode = NULL;
  char *file_name = filename_from_path(name, path);
  struct dir *dir = dir_from_path (path);
  //mif () printf("file name : %s", file_name);
  if (file_name == NULL) {
    dir_close(dir);
    free(file_name);
    return NULL;
  }
  
  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  free(file_name);
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char path[strlen(name) + 1];
  memset(path,0, strlen(name)+1);

  char *file_name = filename_from_path(name, path);
  struct dir *dir = dir_from_path (path);
  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16, ROOT_DIR_SECTOR))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
