// cache.h

#ifndef CACHE_H
#define CACHE_H

// Memory below 64MB is L1 and L2 cached (inner and outer)

// Mark the memory above 64MB to 128MB as L2 cached only (outer)
#define L2_CACHED_MEM_BASE 0x04000000

// Mark the memory above 128MB as uncachable 
#define UNCACHED_MEM_BASE 0x08000000

// Location of the high vectors (last page of L1 cached memory)
#define HIGH_VECTORS_BASE (L2_CACHED_MEM_BASE - 0x1000)

// The first 2MB of memory is mapped at 4K pages so the 6502 Co Pro
// can play tricks with banks selection
#define NUM_4K_PAGES 512

#ifndef __ASSEMBLER__

void map_4k_page(int logical, int physical);

void enable_MMU_and_IDCaches(void);

#endif

#endif
