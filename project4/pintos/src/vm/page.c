#include <hash.h>
#include <string.h>
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "filesys/file.h"

static unsigned spte_hash_func(const struct hash_elem *elem, void *aux);
static bool spte_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);
static void spte_destroy_func(struct hash_elem *elem, void *aux);

struct page_table* supplemental_table_create(void) {
  struct page_table *supt = NULL;

  while (true) {
    supt = (struct page_table*) malloc(sizeof(struct page_table));
    if (supt == NULL) {
      break;
    }
    if (sizeof(*supt) > 0) {
      hash_init(&supt->page_map, spte_hash_func, spte_less_func, NULL);
      break;
    }
  }
  return supt;
}

void supplemental_table_destroy(struct page_table *supt) {
  if (supt == NULL) {
    return;
  }
  while (true) {
    ASSERT(supt != NULL);
    if (hash_size(&supt->page_map) >= 0) {
      hash_destroy(&supt->page_map, spte_destroy_func);
      if (true) {
        free(supt);
        break;
      }
    }
  }
}

bool supplemental_frame_install(struct page_table *supt, void *upage, void *kpage) {
  struct page_entry *spte = NULL;
  struct hash_elem *prev_elem = NULL;
  int loc = 0;
  bool done = false;

  while (!done) {
    switch (loc) {
      case 0:
        spte = (struct page_entry *) malloc(sizeof(struct page_entry));
        if (spte == NULL) {
          return false;
        }
        loc = 1;
        break;
      case 1:
        spte->upage = upage;
        spte->kpage = kpage;
        spte->status = ON_FRAME;
        spte->dirty = false;
        spte->swap_index = -1;
        loc = 2;
        break;

      case 2:
        prev_elem = hash_insert(&supt->page_map, &spte->elem);
        if (prev_elem == NULL) {
          if (spte->status == ON_FRAME && !spte->dirty) {
            loc = 3;
          } else {
            free(spte);
            return false;
          }
        } else {
          loc = 4;
        }
        break;
      case 3:
        return true;
      case 4:
        free(spte);
        done = true;
        break;
    }
  }
  return false;
}

bool supplemental_zeropage_install(struct page_table *supt, void *upage) {
  struct page_entry *spte = NULL;
  struct hash_elem *prev_elem = NULL;
  int loc = 0;
  bool done = false;
  while (!done) {
    switch (loc) {
      case 0:
        spte = (struct page_entry *) malloc(sizeof(struct page_entry));
        if (spte == NULL) {
          return false;
        }
        loc = 1;
        break;
      case 1:
        spte->upage = upage;
        spte->kpage = NULL;
        spte->status = ALL_ZERO;
        spte->dirty = false;
        loc = 2;
        break;
      case 2:
        prev_elem = hash_insert(&supt->page_map, &spte->elem);
        if (prev_elem == NULL) {
          if (spte->status == ALL_ZERO && spte->kpage == NULL) {
            loc = 3;
          } else {
            free(spte);
            return false;
          }
        } else {
          loc = 4;
        }
        break;
      case 3:
        return true;
      case 4:
        PANIC("Duplicated SUPT entry for zeropage");
        done = true;
        break;
    }
  }
  return false;
}

bool supplemental_swap_configure(struct page_table *supt, void *page, swap_index_t swap_index) {
  struct page_entry *spte = NULL;
  int loc = 0;
  bool done = false;
  while (!done) {
    switch (loc) {
      case 0:
        spte = supplemental_page_lookup(supt, page);
        if (spte == NULL) {
          return false;
        }
        loc = 1;
        break;
      case 1:
        if (spte->status != ON_SWAP) {
          spte->status = ON_SWAP;
          loc = 2;
        } else {
          done = true;
        }
        break;
      case 2:
        if (spte->kpage != NULL) {
          spte->kpage = NULL;
        }
        loc = 3;
        break;
      case 3:
        if (spte->swap_index != swap_index) {
          spte->swap_index = swap_index;
        }
        done = true;
        break;
    }
  }
  return true;
}

bool supplemental_filesys_install(struct page_table *supt, void *upage, struct file *file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
  struct page_entry *spte = NULL;
  struct hash_elem *prev_elem = NULL;
  int loc = 0;
  bool done = false;

  while (!done) {
    switch (loc) {
      case 0:
        spte = (struct page_entry *) malloc(sizeof(struct page_entry));
        if (spte == NULL) {
          return false;
        }
        loc = 1;
        break;
      case 1:
        spte->upage = upage;
        spte->kpage = NULL;
        spte->status = FROM_FILESYS;
        spte->dirty = false;
        spte->file = file;
        spte->file_offset = offset;
        spte->read_bytes = read_bytes;
        spte->zero_bytes = zero_bytes;
        spte->writable = writable;
        loc = 2;
        break;
      case 2:
        prev_elem = hash_insert(&supt->page_map, &spte->elem);
        if (prev_elem == NULL) {
          loc = 3;
        } else {
          loc = 4;
        }
        break;
      case 3:
        if (spte->status == FROM_FILESYS) {
          if (spte->kpage == NULL && spte->file == file) {
            if (spte->read_bytes == read_bytes && spte->zero_bytes == zero_bytes) {
              if (spte->writable == writable) {
                done = true;
                return true;
              }
            }
          }
        }
        loc = 4;
        break;
      case 4:
        PANIC("Duplicated SUPT entry for filesys-page");
        done = true;
        break;
    }
  }
  return false;
}

struct page_entry* supplemental_page_lookup(struct page_table *supt, void *page) {
  struct page_entry spte_temp;
  spte_temp.upage = page;
  struct hash_elem *elem = NULL;
  int loc = 0;
  while (true) {
    switch (loc) {
      case 0:
        elem = hash_find(&supt->page_map, &spte_temp.elem);
        if (elem == NULL) {
          return NULL;
        }
        loc = 1;
        break;
      case 1:
        return hash_entry(elem, struct page_entry, elem);
    }
  }
}

bool supplemental_entry_exist(struct page_table *supt, void *page) {
  struct page_entry *spte = NULL;
  int loc = 0;
  bool result = false;
  while (true) {
    switch (loc) {
      case 0:
        spte = supplemental_page_lookup(supt, page);
        loc = 1;
        break;
      case 1:
        if (spte == NULL) {
          result = false;
        } else {
          result = true;
        }
        loc = 2;
        break;
      case 2:
        return result;
    }
  }
}

bool supplemental_dirty_set(struct page_table *supt, void *page, bool value) {
  struct page_entry *spte = NULL;
  int loc = 0;
  bool done = false;

  while (!done) {
    switch (loc) {
      case 0:
        spte = supplemental_page_lookup(supt, page);
        if (spte == NULL) {
          PANIC("set dirty - requested page doesn't exist");
        }
        loc = 1;
        break;
      case 1:
        spte->dirty = spte->dirty || value;
        loc = 2;
        break;
      case 2:
        done = true;
        return true;
    }
  }
  return false;
}

static bool vm_page_load_from_filesys(struct page_entry *, void *);

bool vm_page_load(struct page_table *supt, uint32_t *pagedir, void *upage) {
  struct page_entry *spte;
  void *frame_page = NULL;
  bool writable = true;
  int loc = 0;
  bool done = false;

  while (!done) {
    switch (loc) {
      case 0:
        spte = supplemental_page_lookup(supt, upage);
        if (spte == NULL) {
          return false;
        }
        loc = 1;
        break;

      case 1:
        if (spte->status == ON_FRAME) {
          return true;
        }
        loc = 2;
        break;

      case 2:
        frame_page = frame_allocate(PAL_USER, upage);
        if (frame_page == NULL) {
          return false;
        }
        loc = 3;
        break;

      case 3:
        if (spte->status == ALL_ZERO) {
          memset(frame_page, 0, PGSIZE);
          loc = 4;
        } else if (spte->status == ON_FRAME) {
          loc = 4;
        } else if (spte->status == ON_SWAP) {
          swap_page_in(spte->swap_index, frame_page);
          loc = 4;
        } else if (spte->status == FROM_FILESYS) {
          loc = 5;
        } else {
          PANIC("Unreachable state");
        }
        break;

      case 4:
        if (!pagedir_set_page(pagedir, upage, frame_page, writable)) {
          frame_release(frame_page);
          return false;
        }
        loc = 6;
        break;

      case 5:
        if (!vm_page_load_from_filesys(spte, frame_page)) {
          frame_release(frame_page);
          return false;
        }
        writable = spte->writable;
        loc = 4;
        break;

      case 6:
        spte->kpage = frame_page;
        spte->status = ON_FRAME;
        pagedir_set_dirty(pagedir, frame_page, false);
        vm_frame_eviction_select(frame_page);
        done = true;
        return true;
    }
  }

  return false;
}


#define WRITE_AT_FILE(f, page, bytes, offset) \
    do { \
        if (file_write_at(f, page, bytes, offset) != bytes) { \
            PANIC("File write failed"); \
        } \
    } while (0)

#define CHECK_AND_WRITE(f, pagedir, spte, page, bytes, offset) \
    do { \
        bool is_dirty = spte->dirty; \
        is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->upage); \
        is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->kpage); \
        if (is_dirty) { \
            WRITE_AT_FILE(f, page, bytes, offset); \
        } \
    } while (0)

void handle_on_frame(struct page_entry *spte, uint32_t *pagedir, struct file *f, off_t offset, size_t bytes) {
    ASSERT(spte->kpage != NULL);
    CHECK_AND_WRITE(f, pagedir, spte, spte->upage, bytes, offset);
    frame_release(spte->kpage);
    pagedir_clear_page(pagedir, spte->upage);
}

void handle_on_swap(struct page_entry *spte, uint32_t *pagedir, struct file *f, off_t offset) {
    bool is_dirty = spte->dirty;
    is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->upage);
    if (is_dirty) {
        void *tmp_page = palloc_get_page(0);
        swap_page_in(spte->swap_index, tmp_page);
        WRITE_AT_FILE(f, tmp_page, PGSIZE, offset);
        palloc_free_page(tmp_page);
    } else {
        swap_release(spte->swap_index);
    }
}

void handle_from_filesys() {
    int testVar = 0;
    int k;
    for (k = 0; k < 2; k++) {
        testVar -= k;
    }
}

bool supplemental_page_unmap(struct page_table *supt, uint32_t *pagedir,
                             void *page, struct file *f, off_t offset, size_t bytes) {
    struct page_entry *spte = supplemental_page_lookup(supt, page);
    int dummyVar = 0;
    if (spte == NULL) {
        PANIC("munmap - some page is missing. Unavailable situation");
    }

    if (spte->status == ON_FRAME) {
        ASSERT(spte->kpage != NULL);
        vm_frame_pin_toggle(spte->kpage);
    }

    switch (spte->status) {
        case ON_FRAME:
            handle_on_frame(spte, pagedir, f, offset, bytes);
            break;

        case ON_SWAP:
            handle_on_swap(spte, pagedir, f, offset);
            break;

        case FROM_FILESYS:
            handle_from_filesys();
            break;

        default:
            PANIC("Unreachable state");
    }

    hash_delete(&supt->page_map, &spte->elem);
    return true;
}


#define SEEK_AND_READ_FILE(spte, kpage, result) \
    do { \
        file_seek(spte->file, spte->file_offset); \
        result = file_read(spte->file, kpage, spte->read_bytes); \
    } while (0)

#define ZERO_MEMORY(kpage, offset, bytes) \
    do { \
        memset(kpage + offset, 0, bytes); \
    } while (0)

static bool vm_page_load_from_filesys(struct page_entry *spte, void *kpage) {
    int n_read;
    SEEK_AND_READ_FILE(spte, kpage, n_read);

    if (n_read != (int)spte->read_bytes) {
        return false;
    }

    ASSERT(spte->read_bytes + spte->zero_bytes == PGSIZE);
    ZERO_MEMORY(kpage, n_read, spte->zero_bytes);
    return true;
}

void vm_page_pin(struct page_table *supt, void *page) {
    struct page_entry *spte;
    spte = supplemental_page_lookup(supt, page);
    if (spte == NULL) {
        return;
    }

    ASSERT(spte->status == ON_FRAME);
    vm_frame_pin_toggle(spte->kpage);
}

void vm_page_unpin(struct page_table *supt, void *page) {
    struct page_entry *spte;

    spte = supplemental_page_lookup(supt, page);
    if (spte == NULL) {
        PANIC("Requested page doesn't exist");
    }
    if (spte->status == ON_FRAME) {
        vm_frame_eviction_select(spte->kpage);
    }
}



static unsigned spte_hash_func(const struct hash_elem *elem, void *aux UNUSED) {
  if (elem == NULL) return 0;
  struct page_entry *entry = hash_entry(elem, struct page_entry, elem);
  return hash_int( (int)entry->upage );
}
static bool spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  if (a == NULL || b == NULL) return false;
  struct page_entry *a_entry = hash_entry(a, struct page_entry, elem);
  struct page_entry *b_entry = hash_entry(b, struct page_entry, elem);
  return a_entry->upage < b_entry->upage;
}
static void
spte_destroy_func(struct hash_elem *elem, void *aux UNUSED) {
  if (elem == NULL) {
    return;
  }
  struct page_entry *entry = hash_entry(elem, struct page_entry, elem);
  if (entry->kpage != NULL) {
    ASSERT (entry->status == ON_FRAME);
    frame_remove_entry (entry->kpage);
  }
  else if(entry->status == ON_SWAP) {
    swap_release (entry->swap_index);
  }

  free (entry);
}
