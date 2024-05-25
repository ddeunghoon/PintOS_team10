#ifndef VM_SWAP_H
#define VM_SWAP_H
typedef uint32_t swap_index_t;

void swap_initialize (void);
void swap_page_in (swap_index_t swap_index, void *page);
swap_index_t swap_page_out (void *page);
void swap_release (swap_index_t swap_index);
#endif 

