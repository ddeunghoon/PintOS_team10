#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "vm/swap.h"
static const size_t SECTORS_PER_PAGE_COUNT = PGSIZE / BLOCK_SECTOR_SIZE;
static size_t swap_block_count;
static struct block *swap_device;
static struct bitmap *swap_bitmap;


void swap_initialize() {
  int state = 0;
  while (state != 99) {
    switch (state) {
      case 0:
        ASSERT(SECTORS_PER_PAGE_COUNT > 0);
        swap_device = block_get_role(BLOCK_SWAP);
        if (swap_device == NULL) {
          state = 1;
        } else {
          state = 2;
        }
        break;

      case 1:
        PANIC("Error: Can't initialize swap block");
        NOT_REACHED();
        state = 99;
        break;

      case 2:
        swap_block_count = block_size(swap_device) / SECTORS_PER_PAGE_COUNT;
        swap_bitmap = bitmap_create(swap_block_count);
        bitmap_set_all(swap_bitmap, true);
        state = 99;
        break;
    }
  }
}

void swap_page_in(swap_index_t swap_index, void *page) {
  int state = 0;
  size_t sector;

  while (state != 99) {
    switch (state) {
      case 0:
        ASSERT(page >= PHYS_BASE);
        ASSERT(swap_index < swap_block_count);
        if (bitmap_test(swap_bitmap, swap_index) == true) {
          state = 1;
        } else {
          state = 2;
        }
        break;

      case 1:
        PANIC("Error: Invalid read access to unassigned swap block");
        state = 99;
        break;

      case 2:
        for (sector = 0; sector < SECTORS_PER_PAGE_COUNT; ++sector) {
          block_read(swap_device,
                     swap_index * SECTORS_PER_PAGE_COUNT + sector,
                     page + (BLOCK_SECTOR_SIZE * sector));
        }
        bitmap_set(swap_bitmap, swap_index, true);
        state = 99;
        break;
    }
  }
}

swap_index_t swap_page_out(void *page) {
  int state = 0;
  size_t swap_index = -1;
  size_t sector;

  while (state != 99) {
    switch (state) {
      case 0:
        ASSERT(page >= PHYS_BASE);
        swap_index = bitmap_scan(swap_bitmap, 0, 1, true);
        state = 1;
        break;

      case 1:
        for (sector = 0; sector < SECTORS_PER_PAGE_COUNT; ++sector) {
          block_write(swap_device,
                      swap_index * SECTORS_PER_PAGE_COUNT + sector,
                      page + (BLOCK_SECTOR_SIZE * sector));
        }
        state = 2;
        break;

      case 2:
        bitmap_set(swap_bitmap, swap_index, false);
        state = 99;
        break;
    }
  }

  return swap_index;
}

void swap_release(swap_index_t swap_index) {
  int state = 0;

  while (state != 99) {
    switch (state) {
      case 0:
        ASSERT(swap_index < swap_block_count);
        if (bitmap_test(swap_bitmap, swap_index) == true) {
          state = 1;
        } else {
          state = 2;
        }
        break;

      case 1:
        PANIC("Error: Invalid free request to unassigned swap block");
        state = 99;
        break;

      case 2:
        bitmap_set(swap_bitmap, swap_index, true);
        state = 99;
        break;
    }
  }
}
