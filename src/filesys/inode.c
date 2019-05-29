#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include <stdio.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_BLOCK_CNT 124
#define DOUBLE_INDIRECT_CNT 128

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    disk_sector_t direct_block[DIRECT_BLOCK_CNT];    /* Direct Block sector */
    disk_sector_t double_indirect_block;
    bool is_dir;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
  };

/*For implementing indirect blocks*/
struct indirect_blocks {
  disk_sector_t sector_block[DOUBLE_INDIRECT_CNT];
};

/* Returns sector number from index */
static disk_sector_t convert_sector_from_index(off_t index, const struct inode_disk *idisk) {
  
  /* Direct block */
  if (index < DIRECT_BLOCK_CNT) {
    return idisk->direct_block[index];
  }
  
  /*If not, double indirect block */
  else {

    struct indirect_blocks *indirect_idisk;

    off_t first_index  = (index - DIRECT_BLOCK_CNT) / DOUBLE_INDIRECT_CNT;
    off_t second_index = (index - DIRECT_BLOCK_CNT) % DOUBLE_INDIRECT_CNT;    

    /*To get the second sector number, malloc indirect_idisk for a while*/

    indirect_idisk = calloc(1, sizeof(struct indirect_blocks));

    cache_read (idisk->double_indirect_block, indirect_idisk);
    cache_read (indirect_idisk->sector_block[first_index], indirect_idisk);
   
    disk_sector_t imsi = indirect_idisk->sector_block[second_index];
    
    free(indirect_idisk);

    return imsi; 
  }
}

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{  
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return convert_sector_from_index (pos / DISK_SECTOR_SIZE, &inode->data);
  else  
    return -1;
}



static bool inode_allocate (struct inode_disk* inode_disk) {

  size_t length = inode_disk->length;
  off_t sector_cnt = bytes_to_sectors(length);

  char for_init[DISK_SECTOR_SIZE];
  memset(for_init, 0, DISK_SECTOR_SIZE);

  int i;
  int imsi = 0;

  /*first, allocate direct blocks
    need to check if already allocated
    (if (inode_disk->direct_block[i] == 0)) */
  if (sector_cnt > 0) {
    imsi = sector_cnt > DIRECT_BLOCK_CNT ? DIRECT_BLOCK_CNT : sector_cnt;
    for (i=0; i<imsi; i++) {
      if (inode_disk->direct_block[i] == 0) {
        free_map_allocate (1, &inode_disk->direct_block[i]);
        cache_write (inode_disk->direct_block[i], for_init);
      }
    }
  }

  sector_cnt -= imsi;

  if (sector_cnt == 0) return true;

  /*Then, allocate second indirect blocks*/
  if (sector_cnt > 0) {
    imsi = sector_cnt > DOUBLE_INDIRECT_CNT * DOUBLE_INDIRECT_CNT ? DOUBLE_INDIRECT_CNT * DOUBLE_INDIRECT_CNT  : sector_cnt;
    
    int first_index = 0;
    int second_index = 0;
    struct indirect_blocks* indirect_idisk_first = calloc(1, sizeof(struct indirect_blocks));
    struct indirect_blocks* indirect_idisk_second = calloc(1, sizeof(struct indirect_blocks));

    /*Load previous allocations*/

    if (inode_disk->double_indirect_block != 0) {
      cache_read(inode_disk->double_indirect_block, indirect_idisk_first);
    }

    if (indirect_idisk_first->sector_block[first_index] != 0) {
      cache_read(indirect_idisk_first->sector_block[first_index], indirect_idisk_second);
    }

    /* Now, preivous load is done. 
       Need to do new allocation. */


    /*First, allocate the base block*/
    if (inode_disk->double_indirect_block == 0) {
      free_map_allocate(1, &inode_disk->double_indirect_block);
      cache_write (inode_disk->double_indirect_block, for_init);
    }

    /* Finally, allocate the second level block like recursive steps*/
    for (i=0; i<imsi; i++) {
      //printf("%d %d \n", first_index, second_index);
      /* If allocation at first level sector finished */
      if (second_index == DOUBLE_INDIRECT_CNT) {
        second_index = 0;

        /* Write to Cache */
        cache_write(indirect_idisk_first->sector_block[first_index], indirect_idisk_second);
        memset(indirect_idisk_second, 0, DISK_SECTOR_SIZE);
        first_index++;
      }

      /* Allocate new sector at first level*/
      if (second_index == 0) {
        if (indirect_idisk_first->sector_block[first_index] == 0) {
          free_map_allocate(1, &indirect_idisk_first->sector_block[first_index]); 
          cache_write(indirect_idisk_first->sector_block[first_index], for_init);
          //printf("first index : %d\n", indirect_idisk_first->sector_block[first_index]);
        }
      }

      /* Allocate at second level */
      if(indirect_idisk_second->sector_block[second_index] == 0) {
        free_map_allocate(1, &indirect_idisk_second->sector_block[second_index]);
        cache_write(indirect_idisk_second->sector_block[second_index], for_init);
      }
      //printf("second index : %d\n", indirect_idisk_second->sector_block[second_index]);

      second_index++;
    }
    /* write to disk (or cache) for the last */
    if (second_index != 0) {
      cache_write(indirect_idisk_first->sector_block[first_index], indirect_idisk_second);
    }

    cache_write(inode_disk->double_indirect_block, indirect_idisk_first);

    /*Now, free that we allocated */
    free(indirect_idisk_second);
    free(indirect_idisk_first);
    sector_cnt -= imsi;

  }
  if (sector_cnt == 0) {
    return true;
  }

  return false; 
}

static bool inode_disk_release(struct inode *inode) {
  size_t length = inode->data.length;
  off_t sector_cnt = bytes_to_sectors(length);

  int i;
  int imsi = 0;

  /*first, release direct blocks*/
  if (sector_cnt > 0) {
    imsi = sector_cnt > DIRECT_BLOCK_CNT ? DIRECT_BLOCK_CNT : sector_cnt;
    for (i=0; i<imsi; i++) {
      free_map_release (inode->data.direct_block[i], 1);
    }
  }
  sector_cnt -= imsi;

  if (sector_cnt == 0) return true;

  /*Then, allocate second indirect blocks*/
  if (sector_cnt > 0) {
    imsi = sector_cnt > DOUBLE_INDIRECT_CNT * DOUBLE_INDIRECT_CNT ? DOUBLE_INDIRECT_CNT * DOUBLE_INDIRECT_CNT : sector_cnt;
    
    int first_index = 0;
    int second_index = 0;

    struct indirect_blocks* indirect_idisk_first  = (struct indirect_blocks *) calloc(1, sizeof(struct indirect_blocks));
    struct indirect_blocks* indirect_idisk_second = (struct indirect_blocks *) calloc(1, sizeof(struct indirect_blocks));

    cache_read(inode->data.double_indirect_block, indirect_idisk_first);
    
    /* Finally, release the second level block */
    for (i=0; i<imsi; i++) {

      /* If allocation at first level sector finished */
      if (second_index == DOUBLE_INDIRECT_CNT) {
        free_map_release(indirect_idisk_first->sector_block[first_index], 1); 
        memset(indirect_idisk_second, 0, DISK_SECTOR_SIZE);
        first_index++;
        second_index = 0;
      }

      /* Allocate new sector at first level*/
      if (second_index == 0) {
        cache_read(indirect_idisk_first->sector_block[first_index], indirect_idisk_second);
      }

      /* Allocate at second level */
      free_map_release(indirect_idisk_second->sector_block[second_index], 1);
      second_index++;
    }

    if (second_index != 0) { 
      free_map_release(indirect_idisk_first->sector_block[first_index], 1);
    }

    free_map_release(inode->data.double_indirect_block, 1);

    free(indirect_idisk_first);
    free(indirect_idisk_second);
    sector_cnt -= imsi;
  }

  if (sector_cnt == 0) {
    return true;
  }

  return false; 
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *inode_disk = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *inode_disk == DISK_SECTOR_SIZE);

  inode_disk = calloc (1, sizeof *inode_disk);
  if (inode_disk != NULL)
    {
      //size_t sectors = bytes_to_sectors (length);
      inode_disk->length = length;
      inode_disk->magic = INODE_MAGIC;
      inode_disk->is_dir = is_dir;

      if (inode_allocate(inode_disk) == true) {
        cache_write(sector, inode_disk);
        success = true;
      }

      free (inode_disk);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  cache_read (inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          inode_disk_release (inode);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Read full sector directly into caller's buffer. */
          cache_read (sector_idx, buffer + bytes_read); 
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          cache_read (sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */

off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

if (byte_to_sector(inode, size + offset - 1) == (unsigned int) -1) {
  /* Then, need to extend the inode. */
  inode->data.length = size + offset;
  bool success = inode_allocate(&inode->data);

  if (success == true) {
    cache_write(inode->sector, &inode->data);
  } 
  else {
    inode->data.length = offset;
    return 0;
  }
}

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Write full sector directly to disk. */
          cache_write (sector_idx, buffer + bytes_written); 
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            cache_read (sector_idx, bounce);
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          cache_write (sector_idx, bounce); 
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool inode_is_dir(struct inode *inode) {
  return inode->data.is_dir;
}

disk_sector_t inode_number(struct inode *inode) {
  return inode->sector;
}
