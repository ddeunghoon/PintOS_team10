#ifndef VM_FRAME_H
#define VM_FRAME_H
#include "threads/synch.h"
#include "threads/palloc.h"
#include <hash.h>
#include "lib/kernel/hash.h"

void frame_management_init (void);
void* frame_allocate (enum palloc_flags flags, void *upage);
void frame_release (void*);
void frame_remove_entry (void*);
void frame_pin_toggle (void* kpage);
void frame_eviction_select (void* kpage);

#endif 

