void cache_init();
void cache_read(struct disk *d, disk_sector_t sec_no, void *buffer);
void cache_write(struct disk *d, disk_sector_t sec_no, const void *buffer);
void cache_close();

struct cache {
	struct list_elem cache_elem;
};