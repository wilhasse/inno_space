#include "../third_party/catch.hpp"
#include "include/parse_fil_header.h"
#include "include/mach_data.h"
#include "include/fil0fil.h"
#define UNIV_PAGE_SIZE 16384
#include <cstring>

static void write_u8(byte* b, uint64_t v) {
    mach_write_to_4(b, (uint32_t)(v >> 32));
    mach_write_to_4(b + 4, (uint32_t)(v & 0xffffffff));
}

TEST_CASE(test_fil_header_parsing) {
    byte page[UNIV_PAGE_SIZE];
    memset(page, 0, sizeof(page));
    mach_write_to_4(page + FIL_PAGE_SPACE_OR_CHKSUM, 0xdeadbeef);
    mach_write_to_4(page + FIL_PAGE_OFFSET, 55);
    mach_write_to_4(page + FIL_PAGE_PREV, 54);
    mach_write_to_4(page + FIL_PAGE_NEXT, 56);
    write_u8(page + FIL_PAGE_LSN, 0x1122334455667788ULL);
    mach_write_to_2(page + FIL_PAGE_TYPE, FIL_PAGE_INDEX);
    write_u8(page + FIL_PAGE_FILE_FLUSH_LSN, 0x1234567890abcdefULL);

    FilHeader h{};
    parse_fil_header(page, h);

    REQUIRE(h.checksum == 0xdeadbeef);
    REQUIRE(h.page_no == 55);
    REQUIRE(h.prev_page == 54);
    REQUIRE(h.next_page == 56);
    REQUIRE(h.lsn == 0x1122334455667788ULL);
    REQUIRE(h.type == FIL_PAGE_INDEX);
    REQUIRE(h.flush_lsn == 0x1234567890abcdefULL);
}
