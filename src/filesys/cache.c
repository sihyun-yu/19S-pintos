#include "devices/disk.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct list cache_list;
struct lock cache_lock;

void cache_init() {
	list_init(&cache_list);
	lock_init(&cache_lock);

}

void cache_read(struct disk *d, disk_sector_t sec_no, void *buffer) {

}

void cache_write(struct disk *d, disk_sector_t sec_no, const void *buffer) {

}

void cache_close() {
	
}
