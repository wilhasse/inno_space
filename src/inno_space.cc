
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <fstream>

#include <sys/stat.h>

#include <cstdlib>
#include <iostream>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

#include "include/fil0fil.h"
#include "include/page0page.h"
#include "include/ut0crc32.h"
#include "include/page_crc32.h"
#include "include/fsp0fsp.h"
#include "include/fsp0types.h"
#include "include/page0types.h"
#include "include/rem0types.h"
#include "include/rec.h"



static const uint32_t kPageSize = 16384;

// global variables
char path[1024];
char sdi_path[1024];
int fd;

byte* read_buf;
byte* inode_page_buf;

// init offsets here
ulint offsets_[REC_OFFS_NORMAL_SIZE];

struct dict_col {
  std::string col_name;
  std::string column_type_utf8;
  int char_length;
};

std::vector<dict_col> dict_cols;

static void usage()
{
  fprintf(stderr,
      "Inno space\n"
      "usage: inno [-h] [-f test/t.ibd] [-p page_num]\n"
      "\t-h                -- show this help\n"
      "\t-f test/t.ibd     -- ibd file \n"
      "\t\t-c list-page-type      -- show all page type\n"
      "\t\t-c index-summary       -- show indexes information\n"
      "\t\t-c show-undo-file       -- show undo log file detail\n"
      "\t-p page_num       -- show page information\n"
      "\t\t-c show-records        -- show all records information\n"
      "\t-u page_num       -- update page checksum\n"
      "\t-d page_num       -- delete page \n"
      "Example: \n"
      "====================================================\n"
      "Show sbtest1.ibd all page type\n"
      "./inno -f ~/git/primary/dbs2250/sbtest/sbtest1.ibd -c list-page-type\n"
      "Show sbtest1.ibd all indexes information\n"
      "./inno -f ~/git/primary/dbs2250/sbtest/sbtest1.ibd -c index-summary\n"
      "Show undo_001 all rseg information\n"
      "./inno -f ~/git/primary/dbs2250/log/undo_001 -c show-undo-file\n"
      "Show specify page information\n"
      "./inno -f ~/git/primary/dbs2250/sbtest/sbtest1.ibd -p 10\n"
      "Delete specify page\n"
      "./inno -f ~/git/primary/dbs2250/test/t1.ibd -d 2\n"
      "Update specify page checksum\n"
      "./inno -f ~/git/primary/dbs2250/test/t1.ibd -u 2\n"
      );
}

ulint get_next_rec_offset(byte* rec, byte* page) {
    ulint next_off = mach_read_from_2(rec - REC_NEXT);
    ulint rec_offset = ((rec - page) + next_off) & (UNIV_PAGE_SIZE - 1);
    return rec_offset;
}

void ShowFILHeader(uint32_t page_num, uint16_t* type) {
  printf("=========================%u's block==========================\n", page_num);
  printf("FIL Header:\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;

  int ret = pread(fd, read_buf, kPageSize, offset);
  if (ret == -1) {
    printf("ShowFILHeader read error %d\n", ret);
    return;
  }

  printf("CheckSum: %u\n", mach_read_from_4(read_buf));

  // uint32_t cc = buf_calc_page_crc32(read_buf, 0);
  // printf("crc %u\n", cc);

  printf("Page number: %u\n", mach_read_from_4(read_buf + FIL_PAGE_OFFSET));
  printf("Previous Page: %u\n", mach_read_from_4(read_buf + FIL_PAGE_PREV));
  printf("Next Page: %u\n", mach_read_from_4(read_buf + FIL_PAGE_NEXT));
  printf("Page LSN: %lu\n", mach_read_from_8(read_buf + FIL_PAGE_LSN));
  *type = mach_read_from_2(read_buf + FIL_PAGE_TYPE);
  printf("Page Type: %hu\n", *type);
  printf("Flush LSN: %lu\n", mach_read_from_8(read_buf + FIL_PAGE_FILE_FLUSH_LSN));
}

void hexDump(void *ptr, size_t size) {
  uint8_t *p = reinterpret_cast<uint8_t *>(ptr);
  uint8_t bytesNum = 0;
  while (bytesNum < size) {
    std::cout << std::hex << "0x" << static_cast<uint32_t>(p[bytesNum++]) << " ";

  }
  std::cout << std::endl;
} 

/** TRUE if the record is the supremum record on a page.
 @return true if the supremum record */
static inline bool page_rec_is_supremum_low(
    ulint offset) /*!< in: record offset on page */
{
  return (offset == PAGE_NEW_SUPREMUM || offset == PAGE_OLD_SUPREMUM);
}

/** TRUE if the record is the infimum record on a page.
 @return true if the infimum record */
static inline bool page_rec_is_infimum_low(
    ulint offset) /*!< in: record offset on page */
{
  return (offset == PAGE_NEW_INFIMUM || offset == PAGE_OLD_INFIMUM);
}

int rec_init_offsets() {
  // Open the file for reading
  std::ifstream file(sdi_path);

  // Check if the file is open
  if (!file.is_open()) {
    std::cerr << "Failed to open json the file." << std::endl;
    return 1;
  }

  // Create a string to store the file contents
  std::string file_contents;

  // Read the file and append its contents to the string
  std::string line;
  while (std::getline(file, line)) {
    file_contents += line + '\n';
  }

  // std::cout << file_contents;
  // Close the file
  file.close();

  rapidjson::Document d;
  d.Parse(file_contents.c_str());

  // the sysbench offsets array
  // offsets[0] 100   // 这个值默认初始化成100, 数组的元素个数, 没用
  // offsets[1] 6  // 除了默认offsets[0, 1, 2] 三列以后, 剩下的其他列的个数, 是table 里面列的个数+ 2(trx_id 和 rollptr), 从offset[3] 开始
  // offsets[2] 2147483653  //extra size
  // offsets[3] 4  // id 这个列
  // offsets[4] 10  // 根据record format 可以看出, 这个是trx_id
  // offsets[5] 17  // 根据record format 可以看出, 这个是rollptr
  // offsets[6] 21 // k 这个列 
  // offsets[7] 141 // c 这个列
  // offsets[8] 201 // pad 这个列,   201 + offset[2](如果有extra size) 就是整个rec 的大小

  
  // init offsets array from here
  // previous code parse the json file
  memset(offsets_, 0, sizeof(offsets_));
  offsets_[0] = REC_OFFS_NORMAL_SIZE;

  offsets_[1] = d[1]["object"]["dd_object"]["columns"].Size();
  // TODO: init extra size
  offsets_[2] = 0;

  dict_cols.resize(REC_OFFS_NORMAL_SIZE);
  dict_cols[3].col_name = d[1]["object"]["dd_object"]["columns"][0]["name"].GetString();
  dict_cols[3].column_type_utf8 = d[1]["object"]["dd_object"]["columns"][0]["column_type_utf8"].GetString();
  if (dict_cols[3].column_type_utf8 == "int") {
    offsets_[3] = 4; 
  } else if (dict_cols[3].column_type_utf8.substr(0, 4) == "char") {
    offsets_[3] = d[1]["object"]["dd_object"]["columns"][0]["char_length"].GetInt();
    dict_cols[3].char_length = d[1]["object"]["dd_object"]["columns"][0]["char_length"].GetInt();
  } else {
    return -1;
  }
  offsets_[4] = offsets_[3] + 6;
  offsets_[5] = offsets_[4] + 7;

  for (uint32_t i = 1; i < offsets_[1] - 2; i++) {
    dict_cols[i + 5].col_name = d[1]["object"]["dd_object"]["columns"][i]["name"].GetString();
    dict_cols[i + 5].column_type_utf8
      = d[1]["object"]["dd_object"]["columns"][i]["column_type_utf8"].GetString();
    if (dict_cols[i + 5].column_type_utf8 == "int") {
      offsets_[i + 5] = offsets_[i + 4] + 4;
      dict_cols[i + 5].char_length = 4;
    } else if (dict_cols[i + 5].column_type_utf8.substr(0, 4) == "char") {
      dict_cols[i + 5].char_length = d[1]["object"]["dd_object"]["columns"][i]["char_length"].GetInt();
      offsets_[i + 5] = offsets_[i + 4] + d[1]["object"]["dd_object"]["columns"][i]["char_length"].GetInt();
    } else {
      return -1;
    }
  }
  return 0;
}

ulint rec_get_heap_no(rec_t* rec) {
    // In the compact page format, the heap number is stored in the record's header
    // Retrieve the heap number from the record's header
    return rec_get_bit_field_2(rec, REC_NEW_HEAP_NO, REC_HEAP_NO_MASK, REC_HEAP_NO_SHIFT);
}

#include <inttypes.h>  // Include for PRIu64

void ShowRecord(rec_t* rec) {
    ulint heap_no = rec_get_heap_no(rec);

    // Define REC_HEAP_NO_INFIMUM and REC_HEAP_NO_SUPREMUM if not already defined
    #define REC_HEAP_NO_INFIMUM 0   // Heap number for infimum record
    #define REC_HEAP_NO_SUPREMUM 1  // Heap number for supremum record

    if (rec_get_status(rec) >= REC_STATUS_NODE_PTR || heap_no == REC_HEAP_NO_INFIMUM || heap_no == REC_HEAP_NO_SUPREMUM) {
        // Skip infimum, supremum, and node pointer records
        return;
    }

    byte* field_ptr = rec + REC_N_NEW_EXTRA_BYTES;

    printf("Record:\n");

    // Loop over the columns in the record
    for (size_t i = 0; i < dict_cols.size(); ++i) {
        printf("  %s: ", dict_cols[i].col_name.c_str());

        if (dict_cols[i].column_type_utf8 == "int") {
            uint32_t value = mach_read_from_4(field_ptr) ^ 0x80000000;
            printf("%d", (int32_t)value); // Cast to signed int if necessary
            field_ptr += 4;
        } else if (dict_cols[i].column_type_utf8 == "smallint") {
            uint16_t value = mach_read_from_2(field_ptr) ^ 0x8000;
            printf("%d", (int16_t)value); // Cast to signed short if necessary
            field_ptr += 2;
        } else if (dict_cols[i].column_type_utf8 == "char") {
            printf("%.*s", dict_cols[i].char_length, field_ptr);
            field_ptr += dict_cols[i].char_length;
        } else if (dict_cols[i].column_type_utf8 == "datetime") {
            // Handle datetime conversion if necessary
            uint64_t datetime_value = mach_read_from_8(field_ptr);
            // Convert to readable datetime format
            printf("%" PRIu64, datetime_value); // Use PRIu64 for portable printing
            field_ptr += 8;
        } else {
            // Handle other data types as needed
        }
        printf("\n");
    }
}

// void ShowCompressInfo(uint32_t page_num) {

// }

void ShowIndexHeader(byte* page_buf, uint32_t page_num, bool is_show_records) {
  printf("Index Header for page %u:\n", page_num);

  // Read page header information
  uint16_t n_dir_slots = mach_read_from_2(page_buf + PAGE_HEADER + PAGE_N_DIR_SLOTS);
  uint16_t garbage_space = mach_read_from_2(page_buf + PAGE_HEADER + PAGE_GARBAGE);
  uint16_t n_heap = page_header_get_field(page_buf, PAGE_N_HEAP);
  uint16_t n_recs = mach_read_from_2(page_buf + PAGE_HEADER + PAGE_N_RECS);
  uint64_t max_trx_id = mach_read_from_8(page_buf + PAGE_HEADER + PAGE_MAX_TRX_ID);
  uint16_t page_level = mach_read_from_2(page_buf + PAGE_HEADER + PAGE_LEVEL);
  uint64_t index_id = mach_read_from_8(page_buf + PAGE_HEADER + PAGE_INDEX_ID);

  printf("Number of Directory Slots: %hu\n", n_dir_slots);
  printf("Garbage Space: %hu\n", garbage_space);
  printf("Number of Heap Records: %hu\n", n_heap);
  printf("Number of Records: %hu\n", n_recs);
  printf("Max Trx id: %lu\n", max_trx_id);
  printf("Page level: %hu\n", page_level);
  printf("Index ID: %lu\n", index_id);

  // Optionally, handle symbol table if necessary
  // ...
  rec_init_offsets();

  uint16_t page_type = mach_read_from_2(page_buf + FIL_PAGE_TYPE);
  if (page_type != FIL_PAGE_INDEX || !is_show_records) {
    return;
  }

  // Now iterate over the records in the page
  byte* rec_ptr = page_buf + PAGE_NEW_INFIMUM;

  // Get the offset to the first user record after infimum
  ulint rec_offset = get_next_rec_offset(rec_ptr, page_buf);

  // Iterate over all records until we reach the supremum record
  while (rec_offset != PAGE_NEW_SUPREMUM) {
    rec_ptr = page_buf + rec_offset;

    // Process the record
    ShowRecord(rec_ptr);

    // Get the offset to the next record
    rec_offset = get_next_rec_offset(rec_ptr, page_buf);
  }
}

void ShowBlobHeader(uint32_t page_num) {
  printf("BLOB Header:\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;

  int ret = pread(fd, read_buf, kPageSize, offset);

  if (ret == -1) {
    printf("ShowBlobHeader read error %d\n", ret);
    return;
  }
  printf("BLOB part len on this page: %u\n", mach_read_from_4(read_buf + PAGE_HEADER));
  printf("BLOB next part page no: %u\n", mach_read_from_4(read_buf + PAGE_HEADER + BTR_BLOB_HDR_NEXT_PAGE_NO));

}

void ShowBlobFirstPage(uint32_t page_num) {
  printf("BLOB First Page:\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;

  int ret = pread(fd, read_buf, kPageSize, offset);

  if (ret == -1) {
    printf("ShowBlobFirstPage read error %d\n", ret);
    return;
  }
  printf("BLOB FLAGS: %u\n", mach_read_from_1(read_buf + (ulint)BlobFirstPage::OFFSET_FLAGS));
  printf("BLOB LOB VERSION: %u\n", mach_read_from_1(read_buf + (ulint)BlobFirstPage::OFFSET_LOB_VERSION));
  printf("BLOB LAST_TRX_ID: %lu\n", mach_read_from_6(read_buf + (ulint)BlobFirstPage::OFFSET_LAST_TRX_ID));
  printf("BLOB LAST_UNDO_NO: %u\n", mach_read_from_4(read_buf + (ulint)BlobFirstPage::OFFSET_LAST_UNDO_NO));
  printf("BLOB DATA_LEN: %u\n", mach_read_from_4(read_buf + (ulint)BlobFirstPage::OFFSET_DATA_LEN));
  printf("BLOB TRX_ID: %lu\n", mach_read_from_6(read_buf + (ulint)BlobFirstPage::OFFSET_TRX_ID));
}

void ShowBlobIndexPage(uint32_t page_num) {
  printf("BLOB Index Page:\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;

  int ret = pread(fd, read_buf, kPageSize, offset);

  if (ret == -1) {
    printf("ShowBlobIndexPage read error %d\n", ret);
    return;
  }

  printf("BLOB LOB VERSION: %u\n", mach_read_from_1(read_buf + (ulint)BlobDataPage::OFFSET_VERSION));
}

void ShowBlobDataPage(uint32_t page_num) {
  printf("BLOB Data Page:\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;

  int ret = pread(fd, read_buf, kPageSize, offset);

  if (ret == -1) {
    printf("ShowBlobDataPage read error %d\n", ret);
    return;
  }

  printf("BLOB LOB VERSION: %u\n", mach_read_from_1(read_buf + (ulint)BlobDataPage::OFFSET_VERSION));
  printf("BLOB OFFSET_DATA_LEN: %u\n", mach_read_from_4(read_buf + (ulint)BlobDataPage::OFFSET_DATA_LEN));
  printf("BLOB OFFSET_TRX_ID: %lu\n", mach_read_from_6(read_buf + (ulint)BlobDataPage::OFFSET_TRX_ID));
}

void ShowUndoPageHeader(uint32_t page_num) {
  printf("Undo Page Header:\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;

  int ret = pread(fd, read_buf, kPageSize, offset);

  if (ret == -1) {
    printf("ShowUndoPageHeader read error %d\n", ret);
    return;
  }

  /** Print undo page Header. */
  printf("## Page Type enum:[1:insert; 2:update]\n");
  printf("Undo page type: %u\n", mach_read_from_2(read_buf + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_TYPE));
  printf("Latest undo log rec offset on this undo page: %u\n", mach_read_from_2(read_buf + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_START));
  printf("First free byte offset on this undo page: %u\n", mach_read_from_2(read_buf + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_FREE));

  fil_addr_t prev_undo_page_node = flst_get_prev_addr(read_buf + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_NODE);
  fil_addr_t next_undo_page_node = flst_get_next_addr(read_buf + TRX_UNDO_PAGE_HDR + TRX_UNDO_PAGE_NODE);
  printf("Prev undo page node[%u, %u]\n", prev_undo_page_node.boffset, prev_undo_page_node.page);
  printf("Next undo page node[%u, %u]\n\n", next_undo_page_node.boffset, next_undo_page_node.page);

  /** Print undo segment header. */
  printf("## Undo state enum:[1:active; 2:cached; 3:free; 4:to purge; 5:preapred]\n");
  printf("Undo state on this undo page: %u\n", mach_read_from_2(read_buf + TRX_UNDO_SEG_HDR + TRX_UNDO_STATE));
  printf("Offset of last undo log header on this undo page: %u\n", mach_read_from_2(read_buf + TRX_UNDO_SEG_HDR + TRX_UNDO_LAST_LOG));

}

void ShowRsegArray(uint32_t page_num, uint32_t* rseg_array = nullptr) {
  ut_a(page_num == FSP_RSEG_ARRAY_PAGE_NO);

  printf("Rsegs Array:\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;

  int ret = pread(fd, read_buf, kPageSize, offset);

  if (ret == -1) {
    printf("ShowRsegArray read error %d\n", ret);
    return;
  }

  ut_a(RSEG_ARRAY_VERSION ==
        mach_read_from_4(read_buf + RSEG_ARRAY_HEADER + RSEG_ARRAY_VERSION_OFFSET));

  printf("Rsegs dict size: %u\n", mach_read_from_4(read_buf + RSEG_ARRAY_HEADER + RSEG_ARRAY_SIZE_OFFSET));

  byte *rseg_array_buf = read_buf + RSEG_ARRAY_HEADER + RSEG_ARRAY_PAGES_OFFSET;
  for (ulint slot = 0; slot < TRX_SYS_N_RSEGS; slot++) {
    page_no_t page_no = mach_read_from_4(rseg_array_buf + slot * RSEG_ARRAY_SLOT_SIZE);
    printf("Rseg %u's page no: %u\n", slot, page_no);
    if (rseg_array != nullptr)
    {
      rseg_array[slot] = page_no;
    }
  }
}

void ShowFile() {
  struct stat stat_buf;
  int ret = fstat(fd, &stat_buf);
  if (ret == -1) {
    printf("ShowFile read error %d\n", ret);
    return;
  }
  printf("File size %lu\n", stat_buf.st_size);

  int block_num = stat_buf.st_size / kPageSize;
  uint16_t type = 0;
  for (int i = 0; i < block_num; i++) {
    ShowFILHeader(i, &type);
    if (type == FIL_PAGE_TYPE_BLOB) {
      ShowBlobHeader(i);
    } else if (type == FIL_PAGE_TYPE_LOB_FIRST) {
      ShowBlobFirstPage(i);
    } else if (type == FIL_PAGE_TYPE_LOB_INDEX) {
      ShowBlobIndexPage(i);
    } else if (type == FIL_PAGE_TYPE_LOB_DATA) {
      ShowBlobDataPage(i);
    } else if (type == FIL_PAGE_UNDO_LOG) {
      ShowUndoPageHeader(i);
    } else if (type == FIL_PAGE_TYPE_RSEG_ARRAY) {
      ShowRsegArray(i);
    } else {
//      ShowIndexHeader(i, 0);
    }
  }
}

void ShowUndoLogHdr(uint32_t page_num, uint32_t page_offset)
{
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;

  int ret = pread(fd, read_buf, kPageSize, offset);

  if (ret == -1)
  {
    printf("ShowUndoLogHdr read error %d\n", ret);
    return;
  }

  byte *undo_log_hdr = read_buf + page_offset;

  printf("trx id: %lu\n", mach_read_from_8(undo_log_hdr + TRX_UNDO_TRX_ID));
  printf("trx no: %lu\n", mach_read_from_8(undo_log_hdr + TRX_UNDO_TRX_NO));
  printf("del marks: %hu\n", mach_read_from_2(undo_log_hdr + TRX_UNDO_DEL_MARKS));
  printf("undo log start: %hu\n", mach_read_from_2(undo_log_hdr + TRX_UNDO_LOG_START));
  printf("next undo log header: %hu\n", mach_read_from_2(undo_log_hdr + TRX_UNDO_NEXT_LOG));
  printf("prev undo log header: %hu\n", mach_read_from_2(undo_log_hdr + TRX_UNDO_PREV_LOG));
}

void ShowUndoRseg(uint32_t rseg_id, uint32_t page_num)
{
  printf("==========================Rollback Segment==========================\n");

  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;

  int ret = pread(fd, read_buf, kPageSize, offset);

  if (ret == -1)
  {
    printf("ShowUndoRseg read error %d\n", ret);
    return;
  }

  byte *rseg_header = TRX_RSEG + read_buf;

  printf("Rseg %u's max size: %u\n", rseg_id,
         mach_read_from_4(rseg_header + TRX_RSEG_MAX_SIZE));
  printf("Rseg %u's history list page size: %u\n", rseg_id,
         mach_read_from_4(rseg_header + TRX_RSEG_HISTORY_SIZE));
  printf("Rseg %u's history list size: %u\n", rseg_id,
         flst_get_len(rseg_header + TRX_RSEG_HISTORY));

  fil_addr_t last_trx = flst_get_last(rseg_header + TRX_RSEG_HISTORY);
  last_trx.boffset -= TRX_UNDO_HISTORY_NODE;

  printf("Rseg %u's last page no: %u\n", rseg_id,
         last_trx.page);
  printf("Rseg %u's last offset: %u\n", rseg_id,
         last_trx.boffset);

  if (last_trx.page == FIL_NULL)
  {
    return;
  }

  uint16_t type = 0;
  ShowFILHeader(last_trx.page, &type);
  ShowUndoPageHeader(last_trx.page);
  printf("-------------------last undo log header---------------------\n");
  ShowUndoLogHdr(last_trx.page, last_trx.boffset);
}

void ShowUndoFile() {
  struct stat stat_buf;
  int ret = fstat(fd, &stat_buf);
  if (ret == -1)
  {
    printf("ShowUndoFile read error %d\n", ret);
    return;
  }
  int block_num = stat_buf.st_size / kPageSize;
  printf("Undo File size %ld, blocks %d\n", stat_buf.st_size, block_num);

  uint32_t rseg_array[TRX_SYS_N_RSEGS];
  uint16_t type = 0;
  ShowFILHeader(FSP_RSEG_ARRAY_PAGE_NO, &type);
  ShowRsegArray(FSP_RSEG_ARRAY_PAGE_NO, rseg_array);

  for (size_t slot = 0; slot < TRX_SYS_N_RSEGS; slot++)
  {
    printf("\n-------------------rseg %lu's info-----------------------\n", slot);
    ShowFILHeader(rseg_array[slot], &type);
    ShowUndoRseg(slot, rseg_array[slot]);
  }
}

void UpdateCheckSum(uint32_t page_num) {
  printf("==========================DeletePage==========================\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;
  int ret = pread(fd, read_buf, kPageSize, offset);
  if (ret == -1) {
    printf("UpdateCheckSum read error %d\n", ret);
    return;
  }
  printf("CheckSum: %u\n", mach_read_from_4(read_buf));

  uint32_t cc = buf_calc_page_crc32(read_buf, 0);
  printf("crc %u\n", cc);
  mach_write_to_4(read_buf, cc);
  mach_write_to_4(read_buf + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM, cc);
  ret = pwrite(fd, read_buf, kPageSize, offset);
  printf("UpdateCheckSum %u\n", ret);
}

static uint32_t find_prev_page(uint32_t page_num) {
  struct stat stat_buf;
  int ret = fstat(fd, &stat_buf);
  if (ret == -1) {
    printf("ShowFile read error %d\n", ret);
    return 0;
  }
  int block_num = stat_buf.st_size / kPageSize;

  uint64_t offset = 0;
  uint32_t next_page;
  for (int i = 0; i < block_num; i++) {
    offset = (uint64_t)kPageSize * (uint64_t)i;
    ret = pread(fd, read_buf, kPageSize, offset);
    next_page = mach_read_from_4(read_buf + FIL_PAGE_NEXT);
    if (next_page == page_num) {
      return i;
    }
  }
  return 0;
}

static uint32_t find_next_page(uint32_t page_num) {
  struct stat stat_buf;
  int ret = fstat(fd, &stat_buf);
  if (ret == -1) {
    printf("ShowFile read error %d\n", ret);
    return 0;
  }
  int block_num = stat_buf.st_size / kPageSize;

  uint64_t offset = 0;
  uint32_t prev_page;
  for (int i = 0; i < block_num; i++) {
    offset = (uint64_t)kPageSize * (uint64_t)i;
    ret = pread(fd, read_buf, kPageSize, offset);
    prev_page = mach_read_from_4(read_buf + FIL_PAGE_PREV);
    if (prev_page == page_num) {
      return i;
    }
  }
  return 0;
}

void DeletePage(uint32_t page_num) {
  printf("==========================DeletePage==========================\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)page_num;

  int ret = pread(fd, read_buf, kPageSize, offset);
  if (ret == -1) {
    printf("DeletePage read error %d\n", ret);
    return;
  }

  printf("CheckSum: %u\n", mach_read_from_4(read_buf));

  uint32_t cc = buf_calc_page_crc32(read_buf, 0);
  printf("crc %u\n", cc);
  byte prev_buf[16 * 1024];
  byte next_buf[16 * 1024];
  uint32_t prev_page = 0, next_page = 0;
  // prev_page = mach_read_from_4(read_buf + FIL_PAGE_PREV);
  // next_page = mach_read_from_4(read_buf + FIL_PAGE_NEXT);
  prev_page = find_prev_page(page_num);
  next_page = find_next_page(page_num);
  if (prev_page == 0 || next_page == 0) {
    printf("Delete Page can't next or prev page, prev_page %u, next_page %u\n", prev_page, next_page);
    return;
  }

  uint64_t prev_offset = (uint64_t)kPageSize * (uint64_t)prev_page;
  uint64_t next_offset = (uint64_t)kPageSize * (uint64_t)next_page;
  pread(fd, prev_buf, kPageSize, prev_offset);
  pread(fd, next_buf, kPageSize, next_offset);


  printf("prev_page %u next_page %u\n", prev_page, next_page);
  
  mach_write_to_4(prev_buf + FIL_PAGE_NEXT, next_page);
  mach_write_to_4(next_buf + FIL_PAGE_PREV, prev_page);

  uint32_t prev_cc = buf_calc_page_crc32(prev_buf, 0);
  uint32_t next_cc = buf_calc_page_crc32(next_buf, 0);

  mach_write_to_4(prev_buf, prev_cc);
  mach_write_to_4(next_buf, next_cc);

  mach_write_to_4(prev_buf + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
      prev_cc);

  mach_write_to_4(next_buf + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM,
      next_cc);

  ret = pwrite(fd, prev_buf, kPageSize, prev_offset);
  printf("Delete prev page ret %u\n", ret);

  ret = pwrite(fd, next_buf, kPageSize, next_offset);
  printf("Delete next page ret %u\n", ret);

}

void ShowExtent()
{
  printf("==========================extents==========================\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)0;

  int ret = pread(fd, read_buf, kPageSize, offset);
  if (ret == -1) {
    printf("ShowExtent read error %d\n", ret);
  }

  uint32_t xdes_state;
  const char *str_state;
  for (int i = 0; i < 255; i++) {
    xdes_state = mach_read_from_4(read_buf + FIL_PAGE_DATA + FSP_HEADER_SIZE + XDES_STATE + (i * 40));
    if (xdes_state == 0) {
      str_state = "not initialized";
    } else if (xdes_state == 1) {
      str_state = "free list";
    } else if (xdes_state == 2) {
      str_state = "free fragment list";
    } else if (xdes_state == 3) {
      str_state = "full fragment list";
    } else if (xdes_state == 4) {
      str_state = "belongs to a segment";
    } else if (xdes_state == 5) {
      str_state = "leased to segment";
    } else {
      str_state = "error";
    }
    printf("Extent i %d status: %s\n", i, str_state); 
    
  }
}

void PrintPageType(page_type_t page_type) {
  const char *str_type;
  if (page_type == FIL_PAGE_INDEX) {
    str_type = "INDEX PAGE";
  } else if (page_type == FIL_PAGE_RTREE) {
    str_type = "RTREE PAGE";
  } else if (page_type == FIL_PAGE_SDI) {
    str_type = "SDI INDEX PAGE";
  } else if (page_type == FIL_PAGE_UNDO_LOG) {
    str_type = "UNDO LOG PAGE";
  } else if (page_type == FIL_PAGE_INODE) {
    str_type = "INDEX NODE PAGE";
  } else if (page_type == FIL_PAGE_IBUF_FREE_LIST) {
    str_type = "INSERT BUFFER FREE LIST";
  } else if (page_type == FIL_PAGE_TYPE_ALLOCATED) {
    str_type = "FRESHLY ALLOCATED PAGE";
  } else if (page_type == FIL_PAGE_IBUF_BITMAP ) {
    str_type = "INSERT BUFFER BITMAP";
  } else if (page_type == FIL_PAGE_TYPE_SYS) {
    str_type = "SYSTEM PAGE";
  } else if (page_type == FIL_PAGE_TYPE_TRX_SYS) {
    str_type = "TRX SYSTEM PAGE";
  } else if (page_type == FIL_PAGE_TYPE_FSP_HDR) {
    str_type = "FSP HDR";
  } else if (page_type == FIL_PAGE_TYPE_XDES) {
    str_type = "XDES";
  } else if (page_type == FIL_PAGE_TYPE_BLOB) {
    str_type = "UNCOMPRESSED BLOB PAGE";
  } else if (page_type == FIL_PAGE_TYPE_ZBLOB) {
    str_type = "FIRST COMPRESSED BLOB PAGE";
  } else if (page_type == FIL_PAGE_TYPE_ZBLOB2) {
    str_type = "SUBSEQUENT FRESHLY ALLOCATED PAGE";
  } else if (page_type == FIL_PAGE_TYPE_UNKNOWN) {
    str_type = "UNDO TYPE PAGE";
  } else if (page_type == FIL_PAGE_TYPE_LOB_FIRST) {
    str_type = "FIRST PAGE OF UNCOMPRESSED BLOB PAGE";
  } else if (page_type == FIL_PAGE_TYPE_LOB_INDEX) {
    str_type = "INDEX PAGE OF UNCOMPRESSED BLOB PAGE";
  } else if (page_type == FIL_PAGE_TYPE_LOB_DATA) {
    str_type = "DATA PAGE OF UNCOMPRESSED BLOB PAGE";
  } else {
    str_type = "ERROR";
  }
  printf("%s", str_type);
  
}

void ShowSpacePageType() {
  printf("==========================space page type==========================\n");
  struct stat stat_buf;
  int ret = fstat(fd, &stat_buf);
  if (ret == -1) {
    printf("ShowFile read error %d\n", ret);
    return ;
  }
  printf("File size %lu\n", stat_buf.st_size);

  int block_num = stat_buf.st_size / kPageSize;

  int st = 0, ed = 0, cnt = 0;

  printf("start\t\tend\t\tcount\t\ttype\n");
  uint64_t offset = 0;
  page_type_t page_type = 0, prev_page_type = 0;
  for (int i = 0; i < block_num; i++) {
    cnt++;
    offset = (uint64_t)kPageSize * (uint64_t)i;
    ret = pread(fd, read_buf, kPageSize, offset);
    page_type = fil_page_get_type(read_buf);
    if (i == 0) {
      prev_page_type = page_type;
    } else if (page_type != prev_page_type) {
      ed = i - 1;
      printf("%d\t\t%d\t\t%d\t\t", st, ed, ed - st + 1);
      PrintPageType(prev_page_type);
      printf("\n");
      prev_page_type = page_type;
      st = i;
      cnt = 0;
    }
  }
  // printf last page blocks
  ed = block_num - 1;
  printf("%d\t\t%d\t\t%d\t\t", st, ed, cnt);
  PrintPageType(prev_page_type);
  printf("\n");
}

void ShowSpaceHeader() {
  printf("==========================Space Header==========================\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)0;

  int ret = pread(fd, read_buf, kPageSize, offset);
  if (ret == -1) {
    printf("ShowSpaceHeader read error %d\n", ret);
    return;
  }

  fsp_header_t *header;
  header = FSP_HEADER_OFFSET + read_buf;

  printf("Space ID: %u\n", mach_read_from_4(header + FSP_SPACE_ID));
  printf("Highest Page number: %u\n", mach_read_from_4(header + FSP_SIZE));
  printf("Free limit Page Number: %u\n", mach_read_from_4(header + FSP_FREE_LIMIT));
  printf("FREE_FRAG page number: %u\n", mach_read_from_4(header + FSP_FRAG_N_USED));
  printf("Next Seg ID: %lu\n", mach_read_from_8(header + FSP_SEG_ID));

}
/** Checks a file segment header within a B-tree root page.
 *  @return true if valid */
static bool btr_root_fseg_validate(
    const fseg_header_t *seg_header, /*!< in: segment header */
    space_id_t space)                /*!< in: tablespace identifier */
{
  ulint offset = mach_read_from_2(seg_header + FSEG_HDR_OFFSET);

  if (mach_read_from_4(seg_header + FSEG_HDR_SPACE) == space && 
      offset >= FIL_PAGE_DATA && (offset <= UNIV_PAGE_SIZE - FIL_PAGE_DATA_END)) {
    return true;
  }
  return false;
}


/** Calculates reserved fragment page slots.
 @return number of fragment pages */
static ulint fseg_get_n_frag_pages(
    fseg_inode_t *inode) /*!< in: segment inode */
{
  ulint i;
  ulint count = 0;


  for (i = 0; i < FSEG_FRAG_ARR_N_SLOTS; i++) {
    if (FIL_NULL != mach_read_from_4(inode + FSEG_FRAG_ARR + i * FSEG_FRAG_SLOT_SIZE)) {
      count++;
    }
  }

  return (count);
}

/** Calculates the number of pages reserved by a segment, and how many
pages are currently used.
@param[in]      space_id    Unique tablespace identifier
@param[in]      inode       File segment inode pointer
@param[out]     used        Number of pages used (not more than reserved)
@return number of reserved pages */
static ulint fseg_n_reserved_pages_low(space_id_t space_id,
                                       fseg_inode_t *inode, ulint *used) {
  ulint ret;

  File_segment_inode fseg_inode(space_id, inode);

  /* number of used segment pages in the FSEG_NOT_FULL list */
  uint32_t n_used_not_full = fseg_inode.read_not_full_n_used();

  /* total number of segment pages in the FSEG_NOT_FULL list */
  ulint n_total_not_full =
      FSP_EXTENT_SIZE * mach_read_from_4(inode + FSEG_NOT_FULL);

  /* n_used can be zero only if n_total is zero. */
  ut_ad(n_used_not_full > 0 || n_total_not_full == 0);
  ut_ad((n_used_not_full < n_total_not_full) ||
        ((n_used_not_full == 0) && (n_total_not_full == 0)));

  /* total number of pages in FSEG_FULL list. */
  ulint n_total_full = FSP_EXTENT_SIZE * mach_read_from_4(inode + FSEG_FULL + FLST_LEN);

  /* total number of pages in FSEG_FREE list. */
  ulint n_total_free = FSP_EXTENT_SIZE * flst_get_len(inode + FSEG_FREE);

  /* Number of fragment pages in the segment. */
  ulint n_frags = fseg_get_n_frag_pages(inode);

  *used = n_frags + n_total_full + n_used_not_full;
  ret = n_frags + n_total_full + n_total_free + n_total_not_full;

  ut_ad(*used <= ret);
  ut_ad((*used < ret) || ((n_used_not_full == 0) && (n_total_not_full == 0) &&
                          (n_total_free == 0)));

  return (ret);
}

/** Writes info of a segment. */
static void fseg_print_low(space_id_t space_id,
                           fseg_inode_t *inode, uint32_t &free_page) /*!< in: segment inode */
{
  space_id_t space;
  // ulint n_used;
  // ulint n_frag;
  ulint n_free;
  ulint n_not_full;
  ulint n_full;
  ulint reserved;
  ulint used;
  // page_no_t page_no;
  uint64_t seg_id;
  File_segment_inode fseg_inode(space_id, inode);

  space = page_get_space_id(align_page(inode));
  // page_no = page_get_page_no(align_page(inode));

  reserved = fseg_n_reserved_pages_low(space_id, inode, &used);

  seg_id = mach_read_from_8(inode + FSEG_ID);

  // n_used = fseg_inode.read_not_full_n_used();
  // n_frag = fseg_get_n_frag_pages(inode);
  n_free = flst_get_len(inode + FSEG_FREE);
  n_not_full = flst_get_len(inode + FSEG_NOT_FULL);
  n_full = flst_get_len(inode + FSEG_FULL);

  printf("SEGMENT id %lu, space id %u\n", seg_id, space);
  printf("Extents information:\n");
  printf("FULL extent list size %u\n", n_full);
  printf("FREE extent list size %u\n", n_free);
  printf("PARTIALLY FREE extent list size %u\n", n_not_full);

  printf("Pages information:\n");
  printf("Reserved page num: %u\n", reserved);
  printf("Used page num: %u\n", used);
  printf("Free page num: %u\n", reserved - used);
  free_page = reserved - used;

  return;
}

void ShowIndexSummary() {
  struct stat stat_buf;
  int ret = fstat(fd, &stat_buf);
  if (ret == -1) {
    printf("ShowIndexSummary read error %d\n", ret);
    return;
  }

  int block_num = stat_buf.st_size / kPageSize;

  uint32_t total_free_page = 0;
  uint32_t free_page = 0;
  page_type_t page_type = 0;
  uint64_t offset;
  
  space_id_t space_id = UINT32_MAX;
  bool is_primary = 0;
  for (int i = 0; i < block_num; i++) {
    offset = (uint64_t)kPageSize * (uint64_t)i;
    ret = pread(fd, read_buf, kPageSize, offset);
    // fsp header page
    // get the space id
    if (i == 0) {
      space_id = mach_read_from_4(FSP_HEADER_OFFSET + read_buf + FSP_SPACE_ID);
    }
    page_type = fil_page_get_type(read_buf);
    if (page_type == FIL_PAGE_INDEX) {
      if (btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_LEAF + read_buf, space_id)
          && btr_root_fseg_validate(FIL_PAGE_DATA + PAGE_BTR_SEG_TOP + read_buf, space_id)) {
        printf("iiiiiiiiiiiiiiiiiiiiiiiiiiii %d\n", i);
        if (is_primary == 0) {
          printf("========Primary index========\n");
          printf("Primary index root page space_id %u page_no %d\n", space_id, i);
          printf("Btree hight: %hu\n", mach_read_from_2(read_buf + PAGE_HEADER + PAGE_LEVEL));
          is_primary = 1;
        } else {
          printf("========Secondary index========\n");
          printf("Secondary index root page space_id %u page_no %d\n", space_id, i);
          printf("Btree hight: %hu\n", mach_read_from_2(read_buf + PAGE_HEADER + PAGE_LEVEL));
        }

        printf("<<<Leaf page segment>>>\n");
        fseg_header_t *seg_header;
        seg_header = read_buf + PAGE_HEADER + PAGE_BTR_SEG_LEAF;
        fil_addr_t inode_addr;
        inode_addr.page = mach_read_from_4(seg_header + FSEG_HDR_PAGE_NO);
        inode_addr.boffset = mach_read_from_2(seg_header + FSEG_HDR_OFFSET);

        posix_memalign((void**)&inode_page_buf, kPageSize, kPageSize);
        offset = (uint64_t)kPageSize * (uint64_t)inode_addr.page;
        ret = pread(fd, inode_page_buf, kPageSize, offset);
        fseg_inode_t *inode = inode_page_buf + inode_addr.boffset;
        fseg_print_low(space_id, inode, free_page);
        total_free_page += free_page;

        inode_addr.page = mach_read_from_4(seg_header + FSEG_HDR_PAGE_NO + FSEG_HEADER_SIZE);
        inode_addr.boffset = mach_read_from_2(seg_header + FSEG_HDR_OFFSET + FSEG_HEADER_SIZE);

        printf("\n<<<Non-Leaf page segment>>>\n");
        offset = (uint64_t)kPageSize * (uint64_t)inode_addr.page;
        ret = pread(fd, inode_page_buf, kPageSize, offset);
        inode = inode_page_buf + inode_addr.boffset;
        fseg_print_low(space_id, inode, free_page);
        total_free_page += free_page;

        free(inode_page_buf);
        printf("\n");
      }
    }
  }

  printf("**Suggestion**\n");
  printf("File size %lu, reserved but not used space %lu, percentage %.2lf%%\n", 
      stat_buf.st_size, (uint64_t)total_free_page * (uint64_t)kPageSize,
      (double)total_free_page * (double)kPageSize * 100.00 / stat_buf.st_size);
  printf("Optimize table will get new fie size %lu\n", stat_buf.st_size - (uint64_t)total_free_page * (uint64_t)kPageSize);

  return;
}

void ProcessPage(byte* page_buf, uint32_t page_num) {
    uint16_t page_type = mach_read_from_2(page_buf + FIL_PAGE_TYPE);

    // Only process index pages
    if (page_type == FIL_PAGE_INDEX) {
        uint16_t page_level = mach_read_from_2(page_buf + PAGE_HEADER + PAGE_LEVEL);

        // Only process leaf pages (level 0)
        if (page_level == 0) {
            // Extract records from the leaf page
            ShowIndexHeader(page_buf, page_num, true);
        }
    }
}

void DumpAllRecords() {
    // Get the file size
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    // Calculate the total number of pages
    uint32_t total_pages = file_size / kPageSize;

    for (uint32_t page_num = 0; page_num < total_pages; ++page_num) {
        uint64_t offset = static_cast<uint64_t>(kPageSize) * page_num;

        int ret = pread(fd, read_buf, kPageSize, offset);
        if (ret == -1) {
            printf("DumpAllRecords read error %d\n", errno);
            return;
        }

        // Process the page
        ProcessPage(read_buf, page_num);
    }
}

void ShowSpaceIndexs() {
  printf("==========================block==========================\n");
  printf("Space Indexs:\n");
  uint64_t offset = (uint64_t)kPageSize * (uint64_t)FIL_PAGE_INODE;

  int ret = pread(fd, read_buf, kPageSize, offset);
  if (ret == -1) {
    printf("ShowSpaceIndexs read error %d\n", ret);
    return;
  }

  // seg_id = mach_read_from_8(space_header + FSP_SEG_ID);
}

int main(int argc, char *argv[]) {
  if (argc <= 2) {
    usage();
    exit(-1);
  }

  uint32_t user_page = 0;
  bool path_opt = false;
  bool sdi_path_opt = false;
  char c;
  bool show_file = true;
  bool delete_page = false;
  bool update_checksum = false;
  bool is_show_records = false;
  char command[128];
  while (-1 != (c = getopt(argc, argv, "hf:s:p:d:u:c:"))) {
    switch (c) {
      case 'f':
        snprintf(path, 1024, "%s", optarg);
        path_opt = true;
        break;
      case 's':
        snprintf(sdi_path, 1024, "%s", optarg);
        sdi_path_opt = true;
        break;
      case 'p':
        show_file = false;
        user_page = std::atol(optarg);
        break;
      case 'd':
        show_file = false;
        delete_page = true;
        user_page = std::atol(optarg);
        break;
      case 'u':
        show_file = false;
        update_checksum = true;
        user_page = std::atol(optarg);
        break;
      case 'c':
        snprintf(command, 128, "%s", optarg);
        break;
      case 'h':
        usage();
        return 0;
      default:
        usage();
        return 0;
    }
  }

  if (path_opt == false) {
    fprintf(stderr, "Please specify the ibd file path\n");
    usage();
    exit(-1);
  }

  printf("File path %s path, page num %u\n", path, user_page);

  fd = open(path, O_RDWR, 0644); 
  if (fd == -1) {
    fprintf(stderr, "[ERROR] Open %s failed: %s\n", path, strerror(errno));
    exit(1);
  }

  ut_crc32_init();

  posix_memalign((void**)&read_buf, kPageSize, kPageSize);

  if (show_file == true) {
    ShowSpaceHeader();
    if (strcmp(command, "list-page-type") == 0) {
      ShowSpacePageType();
    } else if (strcmp(command, "index-summary") == 0) {
      ShowIndexSummary();
      // ShowSpaceIndexs();
    } else if (strcmp(command, "show-undo-file") == 0) {
      ShowUndoFile();
    } else if (strcmp(command, "dump-all-records") == 0) {
      DumpAllRecords();
    }
  } else {
    uint16_t type = 0;
    ShowFILHeader(user_page, &type);
    // PrintUserRecord(user_page, &type);
    printf("\n");
    if (strcmp(command, "show-records") == 0) {
      if (sdi_path_opt == false) {
        fprintf(stderr, "Please specify the sdi file path\n");
        // usage();
        // exit(-1);
      }
      is_show_records = true;
    }
    if (type == FIL_PAGE_TYPE_BLOB) {
      ShowBlobHeader(user_page);
    } else if (type == FIL_PAGE_TYPE_LOB_FIRST) {
      ShowBlobFirstPage(user_page);
    } else if (type == FIL_PAGE_TYPE_LOB_INDEX) {
      ShowBlobIndexPage(user_page);
    } else if (type == FIL_PAGE_TYPE_LOB_DATA) {
      ShowBlobDataPage(user_page);
    } else if (type == FIL_PAGE_UNDO_LOG) {
      ShowUndoPageHeader(user_page);
    } else if (type == FIL_PAGE_TYPE_RSEG_ARRAY) {
      ShowRsegArray(user_page);
    } else {
     // ShowIndexHeader(user_page, is_show_records);
    }
  }

  if (delete_page) {
    DeletePage(user_page);
  }

  if (update_checksum) {
    UpdateCheckSum(user_page);
  }

  free(read_buf);

  return 0;
}
