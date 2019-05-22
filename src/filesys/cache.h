#include "filesys/filesys.h"
#include <list.h>

void cache_init(void);
void cache_read(disk_sector_t sec_no, void *buffer);
void cache_write(disk_sector_t sec_no, const void *buffer);
void cache_close(void);
void cache_evict(void);


struct cache {
	struct list_elem cache_elem;
	bool dirty;
	bool accessed;
	disk_sector_t sec_no;
	void *data;
};