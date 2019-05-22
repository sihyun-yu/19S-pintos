#include "devices/disk.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threads/malloc.h"
#include <list.h>

#define MAX_CACHE_NUM 64

struct list cache_list;
struct lock cache_lock;

void cache_init() {
	list_init(&cache_list);
	lock_init(&cache_lock);
}

void cache_read(disk_sector_t sec_no, void *buffer) {
	//printf("cache read\n");

	/*for sync, first, acquire the lock */
	struct list_elem *e;
	struct cache *cache = NULL;
	struct cache *imsi;

	/*search the cache list and check the sec_no is equal*/
	for (e=list_begin(&cache_list); e!=list_end(&cache_list); e=list_next(e)) {	
		imsi = list_entry(e, struct cache, cache_elem);

		/*if the sec_no is equal the number in the sec_no of the list_elem,
		  this implies we've stored this data in cache.*/
		if (imsi->sec_no == sec_no) {
			cache = imsi;
			break;
		}
	}

	/*nothing matched in cache list */
	if (cache == NULL) {

		/*check cache list is already full (equal to 64) */
		if (list_size(&cache_list) == MAX_CACHE_NUM) {
			cache_evict();
		}

		/*we need to read from (filesys) disk and add this part 
		into cache list.*/

		cache = (struct cache *)malloc(sizeof(struct cache));
    	cache->dirty = false;
    	cache->sec_no = sec_no;
    	cache->data = malloc(DISK_SECTOR_SIZE);

    	lock_acquire(&cache_lock);
    	/*Read the data from filesys_disk, since no match in cache_list*/
    	disk_read(filesys_disk, cache->sec_no, cache->data);
	  	lock_release(&cache_lock);
  

    	/*Now, add this created cache into cache list. */
    	list_push_back(&cache_list, &cache->cache_elem);
	}

	/*Copy the data at buffer from cache we find*/
  	memcpy(buffer, cache->data, DISK_SECTOR_SIZE);

  	/*Now, cache is accessed (for eviction)*/
  	cache->accessed = true; 

	/*Now, release the lock*/
}

void cache_write(disk_sector_t sec_no, const void *buffer) {

	//printf("cache write\n");
	/*for sync, first, acquire the lock */
	struct list_elem *e;
	struct cache *cache = NULL;
	struct cache *imsi;

	/*search the cache list and check the sec_no is equal*/
	for (e=list_begin(&cache_list); e!=list_end(&cache_list); e=list_next(e)) {	
		imsi = list_entry(e, struct cache, cache_elem);

		/*if the sec_no is equal the number in the sec_no of the list_elem,
		  this implies we've stored this data in cache.*/
		if (imsi->sec_no == sec_no) {
			cache = imsi;
			break;
		}
	}

	/*nothing matched in cache list */
	if (cache == NULL) {

		/*check cache list is already full (equal to 64) */
		if (list_size(&cache_list) == MAX_CACHE_NUM ) {
			cache_evict();
		}

		/*We need to add new cache, for saving datas in buffer*/

		cache = (struct cache *) malloc(sizeof(struct cache));
    	cache->dirty = false;
    	cache->sec_no = sec_no;
    	cache->data = malloc(DISK_SECTOR_SIZE);

 
 	  	/*Now, add this created cache into cache list. */
    	list_push_back(&cache_list, &cache->cache_elem);
	}

	/*Copy the data at cache from buffer*/
  	memcpy(cache->data, buffer, DISK_SECTOR_SIZE);

  	/*to pin whether it has been written or not*/
  	cache->dirty = true;

  	/*Now, cache is accessed (for eviction)*/
  	cache->accessed = true; 

	/*Now, release the lock*/
}

void cache_close() {
	//printf("cache close\n");
	if (list_empty(&cache_list)) return;

	/*Delete all elements from cache list*/
	while(!list_empty(&cache_list)) {
		struct list_elem *e = list_pop_front(&cache_list);
		struct cache *cache = list_entry(e, struct cache, cache_elem);

		/*If this bit is true, then we need to update this new
		  written date into filesys_disk*/
		if (cache->dirty == true) {
			 lock_acquire(&cache_lock);
		     disk_write(filesys_disk, cache->sec_no, cache->data);
		     lock_release(&cache_lock);
		}

		/*Free that we malloced*/
		free(cache->data);
		free(cache);
	}
}

void cache_evict() {
	//printf("eviction start\n");
	if (list_empty(&cache_list)) return;

	/*initial setting for clock algo.*/
	struct list_elem *e;
	e = list_begin(&cache_list);

	int i;
	int n = list_size(&cache_list);

	for (i=0; i<=2*n; i++) {
		struct cache *tmp = list_entry(e, struct cache, cache_elem);
		if (tmp->accessed == true) {
			tmp->accessed = false;
		}

		else {
			list_remove(e);
			
			if (tmp->dirty == true) {
				lock_acquire(&cache_lock);
				disk_write(filesys_disk, tmp->sec_no, tmp->data);
				lock_release(&cache_lock);
			}

			free(tmp->data);
			free(tmp);
			//printf("eviction finished\n");
			return;
		}
		/*clcock*/
		if (e->next != list_end(&cache_list)) {
			e = list_next(e);
		}

		else {
			e = list_begin(&cache_list);
		}

	}

	return;
}
