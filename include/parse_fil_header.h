#ifndef PARSE_FIL_HEADER_H
#define PARSE_FIL_HEADER_H
#include "fil0fil.h"
#include <stdint.h>

struct FilHeader {
    uint32_t checksum;
    uint32_t page_no;
    uint32_t prev_page;
    uint32_t next_page;
    uint64_t lsn;
    uint16_t type;
    uint64_t flush_lsn;
};

void parse_fil_header(const byte* page, FilHeader& out);

#endif
