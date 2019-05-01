#include "vm/swap.h"
#include "devices/disk.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <bitmap.h>
#include <stdio.h>
#define FREE 0
#define ALLOC 1
#define FOR_EACH_SECTOR PGSIZE / DISK_SECTOR_SIZE

/* The swap device */
static struct disk *swap_device;

/* Tracks in-use and free swap slots */
static struct bitmap *swap_table;

/* Protects swap_table */
static struct lock swap_lock;

/* 
 * Initialize swap_device, swap_table, and swap_lock.
 */
void 
swap_init (void)
{
	swap_device = disk_get(1,1);
	swap_table = bitmap_create(disk_size(swap_device));
	lock_init(&swap_lock);
	bitmap_set_all(swap_table, true);
}

/*
 * Reclaim a frame from swap device.
 * 1. Check that the page has been already evicted. 
 * 2. You will want to evict an already existing frame
 * to make space to read from the disk to cache. 
 * 3. Re-link the new frame with the corresponding supplementary
 * page table entry. 
 * 4. Do NOT create a new supplementray page table entry. Use the 
 * already existing one. 
 * 5. Use helper function read_from_disk in order to read the contents
 * of the disk into the frame. 
 */ 
int 
swap_in (void *addr, int index)
{
	lock_acquire(&swap_lock);
	read_from_disk(addr, index);
	bitmap_set_multiple(swap_table, index, FOR_EACH_SECTOR, 0);
	lock_release(&swap_lock);
	return true; 
}

/* 
 * Evict a frame to swap device. 
 * 1. Choose the frame you want to evict. 
 * (Ex. Least Recently Used policy -> Compare the timestamps when each 
 * frame is last accessed)
 * 2. Evict the frame. Unlink the frame from the supplementray page table entry
 * Remove the frame from the frame table after freeing the frame with
 * pagedir_clear_page. 
 * 3. Do NOT delete the supplementary page table entry. The process
 * should have the illusion that they still have the page allocated to
 * them. 
 * 4. Find a free block to write you data. Use swap table to get track
 * of in-use and free swap slots.
 */
int
swap_out (void *addr)
{
	lock_acquire(&swap_lock);
	int index = bitmap_scan_and_flip(swap_table, 0, FOR_EACH_SECTOR, 0);
	if (index == BITMAP_ERROR) {
		lock_release(&swap_lock);
		return 0; 
	}
	write_to_disk(addr, index);
	lock_release(&swap_lock);
	return index; 
}

void swap_free(int index){
  lock_acquire(&swap_lock);
  bitmap_set_multiple(swap_table, index, FOR_EACH_SECTOR, 0);
  lock_release(&swap_lock);
}
/* 
 * Read data from swap device to frame. 
 * Look at device/disk.c
 */
void read_from_disk (void *frame, int index)
{
	int i;
	for (i=0; i<FOR_EACH_SECTOR; i++) {
		disk_read(swap_device, index+i, frame + i * DISK_SECTOR_SIZE);
	}

}

/* Write data to swap device from frame */
void write_to_disk (void *frame, int index)
{
	int i;
	for (i=0; i<FOR_EACH_SECTOR; i++) {
		disk_write(swap_device, index+i, frame + i * DISK_SECTOR_SIZE);
	}
}

