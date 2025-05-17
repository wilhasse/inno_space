#ifndef ZIPDECOMPRESS_STUB_H
#define ZIPDECOMPRESS_STUB_H

#include <stdint.h>   // for uint32_t, etc.
#include <stdbool.h>  // for bool
#include <string.h>   // for memcpy, memset
#include <zlib.h>

/** Simulate an InnoDB page size (16KiB). */
#ifndef UNIV_PAGE_SIZE
#define UNIV_PAGE_SIZE 16384
#endif

/** Common InnoDB offsets. In real code, these come from `page0types.h`. */
#ifndef FIL_PAGE_TYPE
#define FIL_PAGE_TYPE 24
#endif
static const int PAGE_HEADER       = 38;
static const int PAGE_DATA         = 38;
static const int PAGE_ZIP_START    = 99;  // Typically 99 in MySQL 8
static const int PAGE_NEW_INFIMUM  = 99;
static const int PAGE_NEW_SUPREMUM = 113;

/** Some placeholders. */
#ifdef byte
typedef byte page_byte_t;
#else
typedef unsigned char byte;
typedef byte page_byte_t;
#endif
typedef page_byte_t* page_t;

/** Minimal "compressed page descriptor." */
typedef struct page_zip_des {
  byte* data;      /* pointer to compressed page bytes */
  uint32_t ssize;  /* a "shift" representing size, or just store real size */
  /* Some extra state from real MySQL: 
     We'll skip them for brevity. 
     e.g. unsigned m_nonempty, m_end, m_start, n_blobs */
} page_zip_des_t;

/** In MySQL, this macro calculates an actual size from `page_zip->ssize`. 
    We'll assume we store the real page size directly in `ssize`. */
static inline uint32_t page_zip_get_size(const page_zip_des_t* p) {
  return p->ssize; 
}

/** Stub: validate that `page_zip` is non-null etc. */
static inline bool page_zip_simple_validate(const page_zip_des_t* p) {
  return (p != nullptr && p->data != nullptr && p->ssize <= UNIV_PAGE_SIZE);
}

/** Minimal stubs for memory-heap. Real InnoDB uses `mem0mem.h`. */
typedef struct mem_heap_t {
  // dummy
  int placeholder;
} mem_heap_t;

static inline mem_heap_t* mem_heap_create(size_t size) {
  (void)size;
  // In real InnoDB, we allocate a custom heap. For stubs, just return an object.
  return new mem_heap_t;
}

static inline void* mem_heap_alloc(mem_heap_t* /*heap*/, size_t n) {
  return new char[n]; // naive new
}

static inline void mem_heap_free(mem_heap_t* heap) {
  delete heap;
}

/** Minimal z_stream allocators (In MySQL, we map them to mem_heap_zalloc). */
static void* page_zip_zalloc(void* opaque, uInt items, uInt size) {
  mem_heap_t* h = static_cast<mem_heap_t*>(opaque);
  (void)h;
  // Just do a naive new. 
  return new char[items * size];
}
static void page_zip_free(void* opaque, void* address) {
  (void)opaque;
  delete[] static_cast<char*>(address);
}
static inline void page_zip_set_alloc(z_stream* strm, mem_heap_t* heap) {
  strm->zalloc = page_zip_zalloc;
  strm->zfree  = page_zip_free;
  strm->opaque = heap;
}

/** Minimal macros for the "dense directory." 
    In real code, you'd see page_zip_dir_elems(), etc. 
    We'll store the "n_dense" user records in the last 2*n_dense bytes. */
static inline uint32_t page_zip_dir_size(uint32_t n_dense) {
  return n_dense * 2; // each slot is 2 bytes
}

/** Read 2 bytes from little-endian memory. 
    Real MySQL code uses mach_read_from_2. */
static inline uint16_t read_u16(const byte* p) {
  return (p[0] << 8) | p[1];
}
static inline void write_u16(byte* p, uint16_t val) {
  p[0] = (val >> 8) & 0xFF;
  p[1] = val & 0xFF;
}

/** This function returns the offset from the compressed "dense dir." 
    We assume the last 2*n_dense bytes store them in ascending order. 
    Real MySQL uses page_zip_dir_get(). */
static inline uint16_t page_zip_dir_get(const page_zip_des_t* zip, 
                                        uint32_t n_dense, uint32_t slot) {
  // The start of the directory = zip->data + (zip->ssize - 2*n_dense)
  const byte* dir_start = zip->data + (zip->ssize - page_zip_dir_size(n_dense));
  // in MySQL, it's reversed order, but let's pretend it's forward
  // just for demonstration:
  const byte* entry = dir_start + 2*slot;
  return read_u16(entry);
}

/** We'll define a "page_zip_decompress_low()" signature. */
bool page_zip_decompress_low(page_zip_des_t* zip, page_t page, bool all);

#endif // ZIPDECOMPRESS_STUB_H
