#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "vm/swap.h"
#include <hash.h>
#include "filesys/off_t.h"

enum page_status {
  ALL_ZERO,         
  ON_FRAME,        
  ON_SWAP,        
  FROM_FILESYS      
};

struct page_table
  {
    struct hash page_map;
  };

struct page_entry
  {
    void *upage;              
    void *kpage;            
    struct hash_elem elem;
    enum page_status status;
    bool dirty;           
    swap_index_t swap_index;  
    struct file *file;
    off_t file_offset;
    uint32_t read_bytes, zero_bytes;
    bool writable;
  };

struct page_table*supplemental_table_create (void);
void supplemental_table_destroy (struct page_table *);
bool supplemental_frame_install (struct page_table *supt, void *upage, void *kpage);
bool supplemental_zeropage_install (struct page_table *supt, void *);
bool supplemental_swap_configure (struct page_table *supt, void *, swap_index_t);
bool supplemental_filesys_install (struct page_table *supt, void *page,
    struct file * file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
struct page_entry* supplemental_page_lookup (struct page_table *supt, void *);
bool supplemental_entry_exist (struct page_table *, void *page);
bool supplemental_dirty_set (struct page_table *supt, void *, bool);
bool vm_page_load(struct page_table *supt, uint32_t *pagedir, void *upage);
bool supplemental_page_unmap(struct page_table *supt, uint32_t *pagedir,
    void *page, struct file *f, off_t offset, size_t bytes);
void vm_page_pin(struct page_table *supt, void *page);
void vm_page_unpin(struct page_table *supt, void *page);

#endif

