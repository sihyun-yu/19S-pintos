#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_init ();
int swap_in (void *addr, int index);
int swap_out (void *addr);
void read_from_disk (void *frame, int index);
void write_to_disk (void *frame, int index);

#endif /* vm/swap.h */
