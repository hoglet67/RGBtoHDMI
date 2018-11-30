#include <stdio.h>
#include <string.h>
#include "cache.h"
#include "logging.h"
#include "rpi-base.h"
#include "startup.h"

// Historical Note:
// Were seeing core 3 crashes if inner *and* outer both set to some flavour of WB (i.e. 1 or 3)
// The point of crashing is when the data cache is enabled
// At that point, the stack appears to vanish and the data read back is 0x55555555
// Reason turned out to be failure to correctly invalidate the entire data cache

const static unsigned l1_cached_threshold = L2_CACHED_MEM_BASE >> 20;
const static unsigned l2_cached_threshold = UNCACHED_MEM_BASE >> 20;
const static unsigned uncached_threshold = PERIPHERAL_BASE >> 20;

volatile __attribute__ ((aligned (0x4000))) unsigned int PageTable[4096];
volatile __attribute__ ((aligned (0x4000))) unsigned int PageTable2[NUM_4K_PAGES];

const static int aa = 1;
const static int bb = 1;
const static int shareable = 1;

#if defined(RPI2) || defined (RPI3)

#define SETWAY_LEVEL_SHIFT          1

// 4 ways x 128 sets x 64 bytes per line 32KB
#define L1_DATA_CACHE_SETS        128
#define L1_DATA_CACHE_WAYS          4
#define L1_SETWAY_WAY_SHIFT        30   // 32-Log2(L1_DATA_CACHE_WAYS)
#define L1_SETWAY_SET_SHIFT         6   // Log2(L1_DATA_CACHE_LINE_LENGTH)

#if defined(RPI2)
// 8 ways x 1024 sets x 64 bytes per line = 512KB
#define L2_CACHE_SETS            1024
#define L2_CACHE_WAYS               8
#define L2_SETWAY_WAY_SHIFT        29   // 32-Log2(L2_CACHE_WAYS)
#else
// 16 ways x 512 sets x 64 bytes per line = 512KB
#define L2_CACHE_SETS             512
#define L2_CACHE_WAYS              16
#define L2_SETWAY_WAY_SHIFT        28   // 32-Log2(L2_CACHE_WAYS)
#endif

#define L2_SETWAY_SET_SHIFT         6   // Log2(L2_CACHE_LINE_LENGTH)

// The origin of this function is:
// https://github.com/rsta2/uspi/blob/master/env/lib/synchronize.c

void InvalidateDataCache (void)
{
   unsigned nSet;
   unsigned nWay;
   uint32_t nSetWayLevel;
   // invalidate L1 data cache
   for (nSet = 0; nSet < L1_DATA_CACHE_SETS; nSet++) {
      for (nWay = 0; nWay < L1_DATA_CACHE_WAYS; nWay++) {
         nSetWayLevel = nWay << L1_SETWAY_WAY_SHIFT
            | nSet << L1_SETWAY_SET_SHIFT
            | 0 << SETWAY_LEVEL_SHIFT;
         asm volatile ("mcr p15, 0, %0, c7, c6,  2" : : "r" (nSetWayLevel) : "memory");   // DCISW
      }
   }

   // invalidate L2 unified cache
   for (nSet = 0; nSet < L2_CACHE_SETS; nSet++) {
      for (nWay = 0; nWay < L2_CACHE_WAYS; nWay++) {
         nSetWayLevel = nWay << L2_SETWAY_WAY_SHIFT
            | nSet << L2_SETWAY_SET_SHIFT
            | 1 << SETWAY_LEVEL_SHIFT;
         asm volatile ("mcr p15, 0, %0, c7, c6,  2" : : "r" (nSetWayLevel) : "memory");   // DCISW
      }
   }
}

void CleanDataCache (void)
{
   unsigned nSet;
   unsigned nWay;
   uint32_t nSetWayLevel;
   // clean L1 data cache
   for (nSet = 0; nSet < L1_DATA_CACHE_SETS; nSet++) {
      for (nWay = 0; nWay < L1_DATA_CACHE_WAYS; nWay++) {
         nSetWayLevel = nWay << L1_SETWAY_WAY_SHIFT
            | nSet << L1_SETWAY_SET_SHIFT
            | 0 << SETWAY_LEVEL_SHIFT;
         asm volatile ("mcr p15, 0, %0, c7, c10,  2" : : "r" (nSetWayLevel) : "memory");
      }
   }

   // clean L2 unified cache
   for (nSet = 0; nSet < L2_CACHE_SETS; nSet++) {
      for (nWay = 0; nWay < L2_CACHE_WAYS; nWay++) {
         nSetWayLevel = nWay << L2_SETWAY_WAY_SHIFT
            | nSet << L2_SETWAY_SET_SHIFT
            | 1 << SETWAY_LEVEL_SHIFT;
         asm volatile ("mcr p15, 0, %0, c7, c10,  2" : : "r" (nSetWayLevel) : "memory");
      }
   }
}
#endif

// TLB 4KB Section Descriptor format
// 31..12 Section Base Address
// 11..9        - unused, set to zero
// 8..6   TEX   - type extension- TEX, C, B used together, see below
// 5..4   AP    - access ctrl   - set to 11 for full access from user and super modes
// 3      C     - cacheable     - TEX, C, B used together, see below
// 2      B     - bufferable    - TEX, C, B used together, see below
// 1      1
// 0      1

void map_4k_page(int logical, int physical) {
   // Invalidate the data TLB before changing mapping
   _invalidate_dtlb_mva((void *)(logical << 12));
   // Setup the 4K page table entry
   // Second level descriptors use extended small page format so inner/outer cacheing can be controlled
   // Pi 0/1:
   //   XP (bit 23) in SCTRL is 0 so descriptors use ARMv4/5 backwards compatible format
   // Pi 2/3:
   //   XP (bit 23) in SCTRL no longer exists, and we see to be using ARMv6 table formats
   //   this means bit 0 of the page table is actually XN and must be clear to allow native ARM code to execute
   //   (this was the cause of issue #27)
#if defined(RPI2) || defined (RPI3)
   PageTable2[logical] = (physical<<12) | 0x132 | (bb << 6) | (aa << 2);
#else
   PageTable2[logical] = (physical<<12) | 0x133 | (bb << 6) | (aa << 2);
#endif
}

void enable_MMU_and_IDCaches(void)
{

   log_debug("enable_MMU_and_IDCaches");
   //log_debug("cpsr    = %08x", _get_cpsr());

   unsigned i;
   unsigned base;

   // TLB 1MB Sector Descriptor format
   // 31..20 Section Base Address
   // 19     NS    - ?             - set to 0
   // 18     0     -               - set to 0
   // 17     nG    - ?             - set to 0
   // 16     S     - ?             - set to 0
   // 15     APX   - access ctrl   - set to 0 for full access from user and super modes
   // 14..12 TEX   - type extension- TEX, C, B used together, see below
   // 11..10 AP    - access ctrl   - set to 11 for full access from user and super modes
   // 9      P     -               - set to 0
   // 8..5   Domain- access domain - set to 0000 as nor using access ctrl
   // 4      XN    - eXecute Never - set to 1 for I/O devices
   // 3      C     - cacheable     - set to 1 for cachable RAM i
   // 2      B     - bufferable    - set to 1 for cachable RAM
   // 1      1                     - TEX, C, B used together, see below
   // 0      0                     - TEX, C, B used together, see below

   // For I/O devices
   // TEX = 000; C=0; B=1 (Shared device)

   // For cacheable RAM
   // TEX = 001; C=1; B=1 (Outer and inner write back, write allocate)

   // For non-cachable RAM
   // TEX = 001; C=0; B=0 (Outer and inner non-cacheable)

   // For individual control
   // TEX = 1BB CB=AA
   // AA = inner policy
   // BB = outer policy
   // 00 = NC    (non-cacheable)
   // 01 = WBWA  (write-back, write allocate)
   // 10 = WT    (write-through
   // 11 = WBNWA (write-back, no write allocate)
   /// TEX = 100; C=0; B=1 (outer non cacheable, inner write-back, write allocate)

   for (base = 0; base < l1_cached_threshold; base++)
   {
      // Value from my original RPI code = 11C0E (outer and inner write back, write allocate, shareable)
      // bits 11..10 are the AP bits, and setting them to 11 enables user mode access as well
      // Values from RPI2 = 11C0E (outer and inner write back, write allocate, shareable (fast but unsafe)); works on RPI
      // Values from RPI2 = 10C0A (outer and inner write through, no write allocate, shareable)
      // Values from RPI2 = 15C0A (outer write back, write allocate, inner write through, no write allocate, shareable)
      PageTable[base] = base << 20 | 0x04C02 | (shareable << 16) | (bb << 12) | (aa << 2);
   }
   for (; base < l2_cached_threshold; base++)
   {
      PageTable[base] = base << 20 | 0x04C02 | (shareable << 16) | (bb << 12);
   }
   for (; base < uncached_threshold; base++)
   {
      PageTable[base] = base << 20 | 0x01C02;
   }
   for (; base < 4096; base++)
   {
      // shared device, never execute
      PageTable[base] = base << 20 | 0x10C16;
   }

   // suppress a warning as we really do want to copy from src address 0!
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"
   // copy vectors from virtual address zero to a higher unused location
   memcpy((void *)HIGH_VECTORS_BASE, (void *)0, 0x1000);
#pragma GCC diagnostic pop

   // replace the first N 1MB entries with second level page tables, giving N x 256 4K pages
   for (i = 0; i < NUM_4K_PAGES >> 8; i++)
   {
      PageTable[i] = (unsigned int) (&PageTable2[i << 8]);
      PageTable[i] +=1;
   }

   // populate the second level page tables
   for (base = 0; base < NUM_4K_PAGES; base++)
   {
      map_4k_page(base, base);
   }

   // relocate the vector pointer to the moved page
   asm volatile("mcr p15, 0, %[addr], c12, c0, 0" : : [addr] "r" (HIGH_VECTORS_BASE));

#if defined(RPI3)
   unsigned cpuextctrl0, cpuextctrl1;
   asm volatile ("mrrc p15, 1, %0, %1, c15" : "=r" (cpuextctrl0), "=r" (cpuextctrl1));
   //log_debug("extctrl = %08x %08x", cpuextctrl1, cpuextctrl0);
#else
   // RPI:  bit 6 of auxctrl is restrict cache size to 16K (no page coloring)
   // RPI2: bit 6 of auxctrl is set SMP bit, otherwise all caching disabled
   unsigned auxctrl;
   asm volatile ("mrc p15, 0, %0, c1, c0,  1" : "=r" (auxctrl));
   auxctrl |= 1 << 6;
   asm volatile ("mcr p15, 0, %0, c1, c0,  1" :: "r" (auxctrl));
   asm volatile ("mrc p15, 0, %0, c1, c0,  1" : "=r" (auxctrl));
   //log_debug("auxctrl = %08x", auxctrl);
#endif

   // set domain 0 to client
   asm volatile ("mcr p15, 0, %0, c3, c0, 0" :: "r" (1));

   // always use TTBR0
   asm volatile ("mcr p15, 0, %0, c2, c0, 2" :: "r" (0));

   unsigned ttbcr;
   asm volatile ("mrc p15, 0, %0, c2, c0, 2" : "=r" (ttbcr));
   //log_debug("ttbcr   = %08x", ttbcr);

#if defined(RPI2) || defined(RPI3)
   // set TTBR0 - page table walk memory cacheability/shareable
   // [Bit 0, Bit 6] indicates inner cachability: 01 = normal memory, inner write-back write-allocate cacheable
   // [Bit 4, Bit 3] indicates outer cachability: 01 = normal memory, outer write-back write-allocate cacheable
   // Bit 1 indicates sharable
   // 4A = 0100 1010
   int attr = ((aa & 1) << 6) | (bb << 3) | (shareable << 1) | ((aa & 2) >> 1);
   asm volatile ("mcr p15, 0, %0, c2, c0, 0" :: "r" (attr | (unsigned) &PageTable));
#else
   // set TTBR0 (page table walk inner cacheable, outer non-cacheable, shareable memory)
   asm volatile ("mcr p15, 0, %0, c2, c0, 0" :: "r" (0x03 | (unsigned) &PageTable));
#endif
   unsigned ttbr0;
   asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r" (ttbr0));
   //log_debug("ttbr0   = %08x", ttbr0);


   // Invalidate entire data cache
#if defined(RPI2) || defined(RPI3)
   asm volatile ("isb" ::: "memory");
   InvalidateDataCache();
#else
   // invalidate data cache and flush prefetch buffer
   // NOTE: The below code seems to cause a Pi 2 to crash
   asm volatile ("mcr p15, 0, %0, c7, c5,  4" :: "r" (0) : "memory");
   asm volatile ("mcr p15, 0, %0, c7, c6,  0" :: "r" (0) : "memory");
#endif

   // enable MMU, L1 cache and instruction cache, L2 cache, write buffer,
   //   branch prediction and extended page table on
   unsigned sctrl;
   asm volatile ("mrc p15,0,%0,c1,c0,0" : "=r" (sctrl));
   // Bit 13 enable vector relocation
   // Bit 12 enables the L1 instruction cache
   // Bit 11 enables branch pre-fetching
   // Bit  2 enables the L1 data cache
   // Bit  0 enabled the MMU
   // The L1 instruction cache can be used independently of the MMU
   // The L1 data cache will one be enabled if the MMU is enabled

   sctrl |= 0x00001805;
   asm volatile ("mcr p15,0,%0,c1,c0,0" :: "r" (sctrl) : "memory");
   asm volatile ("mrc p15,0,%0,c1,c0,0" : "=r" (sctrl));
   //log_debug("sctrl   = %08x", sctrl);

   // For information, show the cache type register
   // From this you can tell what type of cache is implemented
   unsigned ctype;
   asm volatile ("mrc p15,0,%0,c0,c0,1" : "=r" (ctype));
   //log_debug("ctype   = %08x", ctype);
}
