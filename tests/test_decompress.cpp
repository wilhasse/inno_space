#define CATCH_CONFIG_MAIN
#include "../third_party/catch.hpp"
#include "include/fil0fil.h"
#include "src/zipdecompress_stub.h"
#include "include/mach_data.h"
#include <vector>
#include <cstring>
#include <zlib.h>

static void write_u8(byte* b, uint64_t v) {
    mach_write_to_4(b, (uint32_t)(v >> 32));
    mach_write_to_4(b + 4, (uint32_t)(v & 0xffffffff));
}

TEST_CASE(test_page_zip_decompress_low) {
    byte original[UNIV_PAGE_SIZE];
    memset(original, 0, sizeof(original));
    mach_write_to_4(original + FIL_PAGE_SPACE_OR_CHKSUM, 0xabcddcba);
    mach_write_to_4(original + FIL_PAGE_OFFSET, 100);
    mach_write_to_4(original + FIL_PAGE_PREV, 99);
    mach_write_to_4(original + FIL_PAGE_NEXT, 101);
    write_u8(original + FIL_PAGE_LSN, 0x0102030405060708ULL);
    mach_write_to_2(original + FIL_PAGE_TYPE, FIL_PAGE_INDEX);
    write_u8(original + FIL_PAGE_FILE_FLUSH_LSN, 0x0a0b0c0d0e0f1011ULL);

    const size_t data_len = 100;
    for (size_t i = 0; i < data_len; ++i) {
        original[PAGE_DATA + i] = static_cast<byte>(i);
    }

    std::vector<byte> comp(PAGE_DATA); // header part
    comp.insert(comp.end(), data_len * 2, 0); // allocate

    z_stream strm; memset(&strm, 0, sizeof(strm));
    deflateInit(&strm, Z_BEST_COMPRESSION);
    std::vector<byte> buf(data_len * 2);
    strm.next_in = original + PAGE_DATA;
    strm.avail_in = data_len;
    strm.next_out = buf.data();
    strm.avail_out = buf.size();
    int ret = deflate(&strm, Z_FINISH);
    REQUIRE(ret == Z_STREAM_END);
    size_t out_len = strm.total_out;
    deflateEnd(&strm);

    comp.resize(PAGE_DATA + out_len + 4);
    memcpy(comp.data(), original, PAGE_DATA);
    memcpy(comp.data() + PAGE_DATA, buf.data(), out_len);
    mach_write_to_2(comp.data() + PAGE_DATA + out_len, 50); // dir slot
    mach_write_to_2(comp.data() + PAGE_DATA + out_len + 2, 1); // n_dense

    page_zip_des_t zip{comp.data(), static_cast<uint32_t>(comp.size())};
    byte out[UNIV_PAGE_SIZE];
    memset(out, 0, sizeof(out));

    bool ok = page_zip_decompress_low(&zip, out, true);
    REQUIRE(ok);

    REQUIRE(mach_read_from_4(out + FIL_PAGE_SPACE_OR_CHKSUM) == 0xabcddcba);
    REQUIRE(mach_read_from_4(out + FIL_PAGE_OFFSET) == 100);
    REQUIRE(mach_read_from_4(out + FIL_PAGE_PREV) == 99);
    REQUIRE(mach_read_from_4(out + FIL_PAGE_NEXT) == 101);
    REQUIRE(mach_read_from_8(out + FIL_PAGE_LSN) == 0x0102030405060708ULL);
    REQUIRE(mach_read_from_2(out + FIL_PAGE_TYPE) == FIL_PAGE_INDEX);
    REQUIRE(mach_read_from_8(out + FIL_PAGE_FILE_FLUSH_LSN) == 0x0a0b0c0d0e0f1011ULL);
    REQUIRE(memcmp(out + PAGE_DATA, original + PAGE_DATA, data_len) == 0);
}
