#include "zipdecompress_stub.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

/** 
 * Example "page_zip_decompress_low" that:
 *   1) Copies PAGE_DATA (first 38 bytes) from `zip->data` to `page`.
 *   2) Calls inflate() from offset=38 up to `zip->ssize`.
 *   3) Builds a trivial "dense directory" from the last 2*n_dense bytes 
 *      and stubs out the rest.
 * 
 * This is a toy example. Real MySQL code is ~700 lines. 
 */

bool page_zip_decompress_low(page_zip_des_t* zip, page_t page, bool all) {
  // 1) Basic validations
  if (!zip || !zip->data || !page) {
    fprintf(stderr, "[Stub decompress] Invalid args.\n");
    return false;
  }
  if (!page_zip_simple_validate(zip)) {
    fprintf(stderr, "[Stub decompress] page_zip_simple_validate failed.\n");
    return false;
  }

  // We'll assume 16 KB page (or whatever zip->ssize says).
  uint32_t comp_size = page_zip_get_size(zip);
  if (comp_size < PAGE_DATA + 1) {
    fprintf(stderr, "[Stub decompress] Not enough data.\n");
    return false;
  }

  // 2) Copy the first PAGE_DATA (38) bytes uncompressed
  //    In real code, MySQL copies partial headers, checks them, etc.
  if (all) {
    memcpy(page, zip->data, PAGE_DATA);
  } else {
    // partial copying or verifying
    memcpy(page, zip->data, PAGE_DATA);
  }

  // 3) Let's figure out how many "user records" are in the compressed form
  //    Typically, you'd do `page_dir_get_n_heap(zip->data) - PAGE_HEAP_NO_USER_LOW`
  //    We'll do a naive approach: 
  //    Suppose the last 2 bytes store "n_dense".
  if (comp_size < 2) {
    fprintf(stderr, "[Stub decompress] Not enough data for n_dense.\n");
    return false;
  }
  // read n_dense from the first 2 bytes of the trailer for demonstration
  // (this is obviously not what MySQL does, but we need *something*).
  uint16_t n_dense = read_u16(zip->data + comp_size - 2);

  // We want to do zlib inflate from offset=PAGE_DATA..(comp_size - 2*n_dense)
  // ignoring the last 2*n_dense bytes which store directory.
  uint32_t in_len = comp_size - PAGE_DATA - 2*n_dense;
  if (in_len <= 0) {
    fprintf(stderr, "[Stub decompress] No space to inflate.\n");
    return false;
  }

  // 4) Set up zlib to inflate into `page+PAGE_DATA` up to 16KB - PAGE_DATA
  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  mem_heap_t* heap = mem_heap_create(1024); // dummy
  page_zip_set_alloc(&strm, heap);

  // feed input
  strm.next_in  = zip->data + PAGE_DATA;
  strm.avail_in = (uInt)in_len;

  // output
  strm.next_out  = page + PAGE_DATA;
  strm.avail_out = UNIV_PAGE_SIZE - PAGE_DATA;

  int ret = inflateInit2(&strm, 15); // 15 or maybe 15+16 if there's gzip
  if (ret != Z_OK) {
    fprintf(stderr, "[Stub decompress] inflateInit2 error.\n");
    mem_heap_free(heap);
    return false;
  }

  ret = inflate(&strm, Z_FINISH);
  if (ret != Z_STREAM_END) {
    fprintf(stderr, "[Stub decompress] inflate() did not finish properly, ret=%d.\n", ret);
    inflateEnd(&strm);
    mem_heap_free(heap);
    return false;
  }

  inflateEnd(&strm);

  // 5) "Construct" the dense directory. Real code does more checks.
  //    We'll just read the last 2*n_dense bytes (besides the final 2 for n_dense).
  //    Then we could store them somewhere on `page`.
  uint32_t dir_size = 2 * n_dense;
  if (dir_size > comp_size) {
    fprintf(stderr, "[Stub decompress] invalid n_dense.\n");
    mem_heap_free(heap);
    return false;
  }

  // The directory is presumably stored from [comp_size - 2 - dir_size .. comp_size - 2).
  const byte* dir_start = zip->data + comp_size - 2 - dir_size;
  // Real MySQL would place these offsets at the end of `page` in the "sparse" dir.
  // We'll just copy them into the last part of `page`.
  memcpy(page + (UNIV_PAGE_SIZE - dir_size), dir_start, dir_size);

  // Done with stubs.
  // In real code, you'd parse offsets, build a "free list," handle infimum/supremum, etc.
  mem_heap_free(heap);
  return true;
}
