#ifndef INNO_SPACE_H
#define INNO_SPACE_H

#include <vector>
#include <string>
#include <stdint.h>
#include <cstdio>
#include "fil0fil.h"
#include "page0page.h"
#include "ut0crc32.h"
#include "fsp0fsp.h"
#include "fsp0types.h"
#include "page0types.h"
#include "rem0types.h"
#include "rec.h"

class InnoSpace {
public:
    static const uint32_t kPageSize = 16384;

    explicit InnoSpace(const char* path);
    ~InnoSpace();

    void SetSdiPath(const char* sdi);

    void ShowSpaceHeader();
    void ShowSpacePageType();
    void ShowIndexSummary();
    void ShowUndoFile();
    void DumpAllRecords();

    void ShowFILHeader(uint32_t page_num, uint16_t* type);
    void ShowIndexHeader(uint32_t page_num, bool show_records);
    void ShowBlobHeader(uint32_t page_num);
    void ShowBlobFirstPage(uint32_t page_num);
    void ShowBlobIndexPage(uint32_t page_num);
    void ShowBlobDataPage(uint32_t page_num);
    void ShowUndoPageHeader(uint32_t page_num);
    void ShowRsegArray(uint32_t page_num, uint32_t* rseg_array = nullptr);
    void DeletePage(uint32_t page_num);
    void UpdateCheckSum(uint32_t page_num);

private:
    struct dict_col {
        std::string col_name;
        std::string column_type_utf8;
        int char_length{};
    };

    int rec_init_offsets();
    void ShowRecord(rec_t* rec);
    void ShowUndoLogHdr(uint32_t page_num, uint32_t page_offset);
    void ShowUndoRseg(uint32_t rseg_id, uint32_t page_num);

public:
    static char path_[1024];
    static char sdi_path_[1024];
    static int fd_;
    static byte* read_buf_;
    static byte* inode_page_buf_;
    static ulint offsets_[REC_OFFS_NORMAL_SIZE];
    static std::vector<dict_col> dict_cols_;
};

#endif // INNO_SPACE_H
