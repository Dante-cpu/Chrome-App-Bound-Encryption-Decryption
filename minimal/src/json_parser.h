// Minimal JSON parser - only extracts "encrypted_key" field
#pragma once
#include <windows.h>

// Find a string value in JSON by key
// Returns pointer to value start and length, or NULL if not found
static const char* json_find_string(const char* json, const char* key, int* value_len) {
    // Search for "key":"value" pattern
    int key_len = 0;
    while (key[key_len]) key_len++;
    
    const char* p = json;
    while (*p) {
        // Look for opening quote of key
        if (*p == '"') {
            p++;
            const char* key_start = p;
            
            // Check if this is our key
            int i = 0;
            while (key[i] && *p == key[i]) {
                p++;
                i++;
            }
            
            if (i == key_len && *p == '"') {
                // Found the key, now look for the value
                p++; // skip closing quote
                
                // Skip whitespace and colon
                while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':'))
                    p++;
                
                // Value should start with quote
                if (*p == '"') {
                    p++; // skip opening quote
                    const char* value_start = p;
                    
                    // Find closing quote (unescaped)
                    while (*p && *p != '"') {
                        if (*p == '\\' && *(p+1)) p++; // skip escaped char
                        p++;
                    }
                    
                    *value_len = (int)(p - value_start);
                    return value_start;
                }
            }
        }
        p++;
    }
    
    return NULL;
}
