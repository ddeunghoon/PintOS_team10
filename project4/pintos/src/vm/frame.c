#include <hash.h>
#include <list.h>
#include <stdio.h>
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

static struct lock frame_lock;
static struct hash frame_map;
static struct list frame_list;     
static struct list_elem *clock_ptr;
static unsigned frame_hash_func(const struct hash_elem *elem, void *aux);
static bool     frame_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);

struct frame_table_entry{
    void *kpage;              
    struct hash_elem helem;  
    struct list_elem lelem;  
    void *upage;               
    struct thread *t;        
    bool pinned;     
};

static struct frame_table_entry* pick_frame_to_evict(uint32_t* pagedir);
static void frame_do_free (void *kpage, bool free_page);

void frame_management_init() {
  lock_init (&frame_lock);
  hash_init (&frame_map, frame_hash_func, frame_less_func, NULL);
  list_init (&frame_list);
  clock_ptr = NULL;
}

void* frame_allocate(enum palloc_flags flags, void *upage) {
  int state = 0, lock_check1 = 0, lock_check2 = 0;
  void *frame_page = NULL;
  struct frame_table_entry *f_evicted = NULL;
  struct frame_table_entry *frame = NULL;
  bool done = false;

  while (!done) {
    switch (state) {
      case 0:
        lock_check1 = 1;
        lock_check2 = 1;
        if (lock_check1 + lock_check2 == 2) {
          lock_acquire(&frame_lock);
          state = 1;
        } else {
          state = 99;
        }
        break;

      case 1:
        frame_page = palloc_get_page(PAL_USER | flags);
        if (frame_page != NULL) {
          state = 6;
        } else {
          state = 2;
        }
        break;

      case 2:
        f_evicted = pick_frame_to_evict(thread_current()->pagedir);

#if DEBUG
        printf("f_evicted: %x th=%x, pagedir = %x, up = %x, kp = %x, hash_size=%d\n", f_evicted, f_evicted->t,
            f_evicted->t->pagedir, f_evicted->upage, f_evicted->kpage, hash_size(&frame_map));
#endif
        if (f_evicted != NULL && f_evicted->t != NULL) {
          state = 3;
        } else {
          state = 99;
        }
        break;

      case 3:
        if (f_evicted->t->pagedir != (void*)0xcccccccc) {
          pagedir_clear_page(f_evicted->t->pagedir, f_evicted->upage);
          bool is_dirty = false;
          is_dirty = is_dirty || pagedir_is_dirty(f_evicted->t->pagedir, f_evicted->upage);
          is_dirty = is_dirty || pagedir_is_dirty(f_evicted->t->pagedir, f_evicted->kpage);

          swap_index_t swap_idx = swap_page_out(f_evicted->kpage);
          supplemental_swap_configure(f_evicted->t->supt, f_evicted->upage, swap_idx);
          supplemental_dirty_set(f_evicted->t->supt, f_evicted->upage, is_dirty);
          frame_do_free(f_evicted->kpage, true);
          state = 4;
        } else {
          state = 99;
        }
        break;

      case 4:
        frame_page = palloc_get_page(PAL_USER | flags);
        if (frame_page != NULL) {
          state = 5;
        } else {
          state = 99;
        }
        break;

      case 5:
        frame = malloc(sizeof(struct frame_table_entry));
        if (frame != NULL) {
          state = 7;
        } else {
          state = 99;
        }
        break;

      case 6:
        frame = malloc(sizeof(struct frame_table_entry));
        if (frame != NULL) {
          state = 7;
        } else {
          lock_release(&frame_lock);
          return NULL;
        }
        break;

      case 7:
        frame->t = thread_current();
        frame->upage = upage;
        frame->kpage = frame_page;
        frame->pinned = true;
        hash_insert(&frame_map, &frame->helem);
        list_push_back(&frame_list, &frame->lelem);
        lock_release(&frame_lock);
        return frame_page;

      case 99:
        lock_release(&frame_lock);
        return NULL;
    }
  }
}

void frame_release (void *kpage) {
  lock_acquire (&frame_lock);
  frame_do_free (kpage, true);
  lock_release (&frame_lock);
}

void frame_remove_entry (void *kpage) {
  lock_acquire (&frame_lock);
  frame_do_free (kpage, false);
  lock_release (&frame_lock);
}

void frame_do_free (void *kpage, bool free_page) {
  int loc0 = 0;
  while (1) {
    switch (loc0) {
      case 0:
        ASSERT (lock_held_by_current_thread(&frame_lock) == true);
        ASSERT (is_kernel_vaddr(kpage));
        ASSERT (pg_ofs (kpage) == 0);
        loc0 = 1;
        break;
      case 1: {
        struct frame_table_entry f_tmp;
        f_tmp.kpage = kpage;
        struct hash_elem *h = hash_find (&frame_map, &(f_tmp.helem));
        if (h == NULL) {
          PANIC ("There is no such page to be feed in the table");
        }
        struct frame_table_entry *f;
        f = hash_entry(h, struct frame_table_entry, helem);
        hash_delete (&frame_map, &f->helem);
        list_remove (&f->lelem);
        if (free_page) {
          palloc_free_page(kpage);
        }
        free(f);
        loc0 = 2;
        break;
      }
      case 2:
        return;
    }
  }
}

struct frame_table_entry* clock_frame_next(void) {
    if (list_empty(&frame_list))
        PANIC("Empty Frame table. Note a leak in somewhere");

    clock_ptr = (clock_ptr == NULL || clock_ptr == list_end(&frame_list)) ? list_begin(&frame_list) : list_next(clock_ptr);
    return list_entry(clock_ptr, struct frame_table_entry, lelem);
}

struct frame_table_entry* pick_frame_to_evict(uint32_t *pagedir) {
  int loc0 = 0;
  size_t n;
  size_t it = 0;
  struct frame_table_entry *e = NULL;

  while (1) {
    switch (loc0) {
      case 0:
        n = hash_size(&frame_map);
        if (n == 0) {
          PANIC("Empty Frame table. Note a leak in somewhere");
        }
        loc0 = 1;
        break;
        
      case 1:
        if (it > n + n) {
          loc0 = 3;
        } else {
          loc0 = 2;
        }
        break;
        
      case 2:
        e = clock_frame_next();
        if (e->pinned) {
          it++;
          loc0 = 1;
        } else if (pagedir_is_accessed(pagedir, e->upage)) {
          pagedir_set_accessed(pagedir, e->upage, false);
          it++;
          loc0 = 1;
        } else {
          return e;
        }
        break;
        
      case 3:
        PANIC("Unable fram eviction. Not enough memory\n");
        break;
    }
  }
}

static void vm_frame_set_pinned(void *kpage, bool new_value) {
  int loc0 = 0;
  struct frame_table_entry f_tmp;
  struct hash_elem *h = NULL;
  struct frame_table_entry *f = NULL;

  while (1) {
    switch (loc0) {
      case 0:
        lock_acquire(&frame_lock);
        loc0 = 1;
        break;       
      case 1:
        f_tmp.kpage = kpage;
        h = hash_find(&frame_map, &(f_tmp.helem));
        if (h == NULL) {
          loc0 = 3;
        } else {
          loc0 = 2;
        }
        break;      
      case 2:
        f = hash_entry(h, struct frame_table_entry, helem);
        f->pinned = new_value;
        loc0 = 4;
        break;   
      case 3:
        PANIC("No such frame to be pinned/unpinned");
        break;   
      case 4:
        lock_release(&frame_lock);
        return;
    }
  }
}


void vm_frame_eviction_select (void* kpage) {
  vm_frame_set_pinned (kpage, false);
}

void vm_frame_pin_toggle (void* kpage) {
  vm_frame_set_pinned (kpage, true);
}

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED) {
  struct frame_table_entry *entry;
  entry = hash_entry(elem, struct frame_table_entry, helem);
  // we use a variation of the DJB2 hash algorithm to minimize hash collisions
  // DJB2 algorithm generates hash values by multiplying by 33 and adding the values together
  // algorithm is known to be simple and effective at generating hash values
  // this implementation uses a variation of DJB2 by using 31
  // to avoid hash collisions, the hash func considers all bits of the input to generate the hash value
  // we process the kpage pointer value byte by byte to generate the hash value
  // loc0 variable is used as an index in the iteration, accessing each byte of the ptr array
  // while loop traverses all bytes of kpage and updates the hash value.
  // byte is added to the hash value via an XOR operation, and the hash value is updated by multiplying it by 31
  unsigned int hash = 0;
  unsigned char *ptr = (unsigned char *)&entry->kpage;
  int loc0 = 0;
  while (loc0 < sizeof entry->kpage) {
    hash = (hash * 31) ^ ptr[loc0];
    loc0++;
  }
  return hash;
}

static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct frame_table_entry *a_entry;
  struct frame_table_entry *b_entry;

  int loc0 = 0;
  while (1) {
    switch (loc0) {
      case 0:
        a_entry = hash_entry(a, struct frame_table_entry, helem);
        b_entry = hash_entry(b, struct frame_table_entry, helem);
        loc0 = 1;
        break;
      case 1:
        return a_entry->kpage < b_entry->kpage;
    }
  }
}