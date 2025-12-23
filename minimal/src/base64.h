// Minimal Base64 decoder
#pragma once

static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_decode_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static int base64_decode(const char* input, int input_len, unsigned char* output) {
    int i = 0, j = 0;
    unsigned char buffer[4];
    int buffer_count = 0;
    
    while (i < input_len) {
        char c = input[i++];
        if (c == '=') break;
        
        int val = base64_decode_value(c);
        if (val == -1) continue;
        
        buffer[buffer_count++] = val;
        
        if (buffer_count == 4) {
            output[j++] = (buffer[0] << 2) | (buffer[1] >> 4);
            output[j++] = (buffer[1] << 4) | (buffer[2] >> 2);
            output[j++] = (buffer[2] << 6) | buffer[3];
            buffer_count = 0;
        }
    }
    
    if (buffer_count == 2) {
        output[j++] = (buffer[0] << 2) | (buffer[1] >> 4);
    } else if (buffer_count == 3) {
        output[j++] = (buffer[0] << 2) | (buffer[1] >> 4);
        output[j++] = (buffer[1] << 4) | (buffer[2] >> 2);
    }
    
    return j;
}
