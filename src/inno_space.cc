#include "inno_space.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <cstdio>

// Forward declarations of functions implemented in inno_impl.cc
void ShowSpaceHeader();
void ShowSpacePageType();
void ShowIndexSummary();
void ShowUndoFile();
void DumpAllRecords();
void ShowFILHeader(uint32_t, uint16_t*);
void ShowIndexHeader(uint32_t, bool);
void ShowBlobHeader(uint32_t);
void ShowBlobFirstPage(uint32_t);
void ShowBlobIndexPage(uint32_t);
void ShowBlobDataPage(uint32_t);
void ShowUndoPageHeader(uint32_t);
void ShowRsegArray(uint32_t, uint32_t*);
void DeletePage(uint32_t);
void UpdateCheckSum(uint32_t);

// Static member definitions
char InnoSpace::path_[1024] = {0};
char InnoSpace::sdi_path_[1024] = {0};
int InnoSpace::fd_ = -1;
byte* InnoSpace::read_buf_ = nullptr;
byte* InnoSpace::inode_page_buf_ = nullptr;
ulint InnoSpace::offsets_[REC_OFFS_NORMAL_SIZE];
std::vector<InnoSpace::dict_col> InnoSpace::dict_cols_;

InnoSpace::InnoSpace(const char* path) {
    std::snprintf(path_, sizeof(path_), "%s", path);
    fd_ = open(path, O_RDWR, 0644);
    if (fd_ == -1) {
        fprintf(stderr, "[ERROR] Open %s failed: %s\n", path, strerror(errno));
        std::exit(1);
    }
    posix_memalign((void**)&read_buf_, kPageSize, kPageSize);
    ut_crc32_init();
}

InnoSpace::~InnoSpace() {
    if (read_buf_) free(read_buf_);
    if (inode_page_buf_) free(inode_page_buf_);
    if (fd_ != -1) close(fd_);
    fd_ = -1;
}

void InnoSpace::SetSdiPath(const char* sdi) {
    std::snprintf(sdi_path_, sizeof(sdi_path_), "%s", sdi);
}

// Wrapper methods
void InnoSpace::ShowSpaceHeader() { ::ShowSpaceHeader(); }
void InnoSpace::ShowSpacePageType() { ::ShowSpacePageType(); }
void InnoSpace::ShowIndexSummary() { ::ShowIndexSummary(); }
void InnoSpace::ShowUndoFile() { ::ShowUndoFile(); }
void InnoSpace::DumpAllRecords() { ::DumpAllRecords(); }
void InnoSpace::ShowFILHeader(uint32_t p, uint16_t* t) { ::ShowFILHeader(p,t); }
void InnoSpace::ShowIndexHeader(uint32_t p, bool s) { ::ShowIndexHeader(p,s); }
void InnoSpace::ShowBlobHeader(uint32_t p){ ::ShowBlobHeader(p); }
void InnoSpace::ShowBlobFirstPage(uint32_t p){ ::ShowBlobFirstPage(p); }
void InnoSpace::ShowBlobIndexPage(uint32_t p){ ::ShowBlobIndexPage(p); }
void InnoSpace::ShowBlobDataPage(uint32_t p){ ::ShowBlobDataPage(p); }
void InnoSpace::ShowUndoPageHeader(uint32_t p){ ::ShowUndoPageHeader(p); }
void InnoSpace::ShowRsegArray(uint32_t p, uint32_t* a){ ::ShowRsegArray(p,a); }
void InnoSpace::DeletePage(uint32_t p){ ::DeletePage(p); }
void InnoSpace::UpdateCheckSum(uint32_t p){ ::UpdateCheckSum(p); }

static void usage() {
    fprintf(stderr,
        "Inno space\n"
        "usage: inno [-h] [-f test/t.ibd] [-p page_num]\n"
        "\t-h                -- show this help\n"
        "\t-f test/t.ibd     -- ibd file \n"
        "\t\t-c list-page-type      -- show all page type\n"
        "\t\t-c index-summary       -- show indexes information\n"
        "\t\t-c show-undo-file      -- show undo log file detail\n"
        "\t\t-c dump-all-records    -- parse all records from a known root\n"
        "\t-p page_num       -- show page information\n"
        "\t\t-c show-records        -- show all records from that page\n"
        "\t-u page_num       -- update page checksum\n"
        "\t-d page_num       -- delete page \n");
}

int main(int argc, char *argv[]) {
    if (argc <= 2) {
        usage();
        return -1;
    }
    uint32_t user_page = 0;
    bool path_opt = false;
    bool sdi_path_opt = false;
    char c;
    bool show_file = true;
    bool delete_page = false;
    bool update_checksum = false;
    bool is_show_records = false;
    char command[128] = {0};
    char filepath[1024] = {0};
    char sdi_path[1024] = {0};
    while (-1 != (c = getopt(argc, argv, "hf:s:p:d:u:c:"))) {
        switch (c) {
            case 'f':
                snprintf(filepath, sizeof(filepath), "%s", optarg);
                path_opt = true;
                break;
            case 's':
                snprintf(sdi_path, sizeof(sdi_path), "%s", optarg);
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
                snprintf(command, sizeof(command), "%s", optarg);
                break;
            case 'h':
                usage();
                return 0;
            default:
                usage();
                return 0;
        }
    }
    if (!path_opt) {
        fprintf(stderr, "Please specify the ibd file path\n");
        usage();
        return -1;
    }
    InnoSpace space(filepath);
    if (sdi_path_opt) {
        space.SetSdiPath(sdi_path);
    }
    printf("File path %s path, page num %u\n", filepath, user_page);
    if (show_file) {
        space.ShowSpaceHeader();
        if (strcmp(command, "list-page-type") == 0) {
            space.ShowSpacePageType();
        } else if (strcmp(command, "index-summary") == 0) {
            space.ShowIndexSummary();
        } else if (strcmp(command, "show-undo-file") == 0) {
            space.ShowUndoFile();
        } else if (strcmp(command, "dump-all-records") == 0) {
            space.DumpAllRecords();
        }
    } else {
        uint16_t type = 0;
        space.ShowFILHeader(user_page, &type);
        if (strcmp(command, "show-records") == 0) {
            if (!sdi_path_opt) {
                fprintf(stderr, "Please specify the sdi file path\n");
            }
            is_show_records = true;
        }
        if (type == FIL_PAGE_TYPE_BLOB) {
            space.ShowBlobHeader(user_page);
        } else if (type == FIL_PAGE_TYPE_LOB_FIRST) {
            space.ShowBlobFirstPage(user_page);
        } else if (type == FIL_PAGE_TYPE_LOB_INDEX) {
            space.ShowBlobIndexPage(user_page);
        } else if (type == FIL_PAGE_TYPE_LOB_DATA) {
            space.ShowBlobDataPage(user_page);
        } else if (type == FIL_PAGE_UNDO_LOG) {
            space.ShowUndoPageHeader(user_page);
        } else if (type == FIL_PAGE_TYPE_RSEG_ARRAY) {
            space.ShowRsegArray(user_page);
        } else {
            space.ShowIndexHeader(user_page, is_show_records);
        }
    }
    if (delete_page) {
        space.DeletePage(user_page);
    }
    if (update_checksum) {
        space.UpdateCheckSum(user_page);
    }
    return 0;
}
