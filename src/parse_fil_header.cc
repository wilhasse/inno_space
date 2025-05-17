#include "include/parse_fil_header.h"
#include "include/mach_data.h"

void parse_fil_header(const byte* page, FilHeader& out) {
    out.checksum = mach_read_from_4(page + FIL_PAGE_SPACE_OR_CHKSUM);
    out.page_no = mach_read_from_4(page + FIL_PAGE_OFFSET);
    out.prev_page = mach_read_from_4(page + FIL_PAGE_PREV);
    out.next_page = mach_read_from_4(page + FIL_PAGE_NEXT);
    out.lsn = mach_read_from_8(page + FIL_PAGE_LSN);
    out.type = mach_read_from_2(page + FIL_PAGE_TYPE);
    out.flush_lsn = mach_read_from_8(page + FIL_PAGE_FILE_FLUSH_LSN);
}
