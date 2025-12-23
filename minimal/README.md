# Minimal Chrome Password Decryptor

A minimal, size-optimized implementation (≤100KB) for extracting and decrypting passwords from Chromium-based browsers.

## Features

- **Tiny Size**: Static build ≤100KB (without UPX or packers)
- **No Dependencies**: All components statically linked
- **No Admin Rights**: Runs with standard user permissions
- **Supported Browsers**: Google Chrome, Microsoft Edge
- **Pure C++/C**: Minimal code, optimized for size

## Technical Implementation

### Size Optimization Techniques

1. **Custom Implementations**:
   - Minimal Base64 decoder (~40 lines)
   - Lightweight JSON parser (only extracts `encrypted_key`)
   - Simplified SQLite reader (table scan, no full parser)

2. **Compiler Optimizations**:
   - `/O1` - Optimize for size
   - `/MT` - Static CRT linking
   - `/GS-` - Disable security checks
   - `/Gy` - Function-level linking
   - `/GL` - Whole program optimization
   - `/LTCG` - Link-time code generation
   - `/OPT:REF /OPT:ICF` - Remove unused code and fold identical functions

3. **No Standard Library**:
   - No STL (vector, string, etc.)
   - Custom `memset`, `memcpy` implementations
   - Direct Windows API calls
   - No exception handling

4. **No Unicode Support**:
   - ASCII output only
   - Minimal string operations

### How It Works

1. **Locate Browser Profile**:
   - Checks standard paths in `%LOCALAPPDATA%`
   - Chrome: `Google\Chrome\User Data`
   - Edge: `Microsoft\Edge\User Data`

2. **Extract Master Key**:
   - Reads `Local State` file
   - Parses JSON to find `encrypted_key` field
   - Base64 decodes the key
   - Uses DPAPI to decrypt master key (removes "DPAPI" prefix)

3. **Decrypt Passwords**:
   - Opens `Default\Login Data` SQLite database
   - Scans for password records (table leaf pages)
   - Decrypts each password using AES-256-GCM
   - Handles v10 Chrome password format (3-byte prefix + 12-byte nonce + ciphertext + 16-byte tag)

4. **Output Results**:
   - Prints URL, username, and decrypted password to console
   - No file I/O for output (reduces size)

## Build Instructions

### Prerequisites

- Visual Studio 2019 or later with C++ build tools
- Windows SDK

### Building

1. Open **Developer Command Prompt for VS**

2. Navigate to repository root:
   ```cmd
   cd Chrome-App-Bound-Encryption-Decryption
   ```

3. Run the build script:
   ```cmd
   minimal\build.bat
   ```

4. The executable will be created at:
   ```
   minimal\build\chrome_decrypt.exe
   ```

### Build Output

The script will show:
- Compilation status
- Final executable size in bytes and KB
- Success/warning if size exceeds 100KB

## Usage

Simply run the executable:

```cmd
minimal\build\chrome_decrypt.exe
```

No command-line arguments needed. The program will:
1. Search for Chrome and Edge profiles
2. Extract and decrypt passwords
3. Display results in console

### Example Output

```
Chrome/Edge Password Decryptor
================================

=== Google Chrome ===
Master key decrypted

URL: https://example.com
Username: user@example.com
Password: mypassword123

URL: https://github.com
Username: developer
Password: securepass456

Found 2 passwords

=== Microsoft Edge ===
Master key decrypted

URL: https://microsoft.com
Username: user@outlook.com
Password: edgepass789

Found 1 passwords


Total: 3 passwords extracted
```

## Limitations

### By Design (for size reduction):

- **Only Default Profile**: Does not scan Profile 1, Profile 2, etc.
- **Console Output Only**: No JSON export or file writing
- **No Error Messages**: Minimal error handling to reduce code size
- **ASCII Only**: No Unicode support
- **Basic SQLite**: Simple table scan, may miss some edge cases
- **No ABE Support**: Only supports DPAPI-encrypted passwords (Chrome v80-126)
- **Limited Browser Support**: Chrome and Edge only (no Brave, Vivaldi, etc.)

### Technical Limitations:

- Does not support Chrome's newer App-Bound Encryption (ABE)
- Only works with v10 password format
- May not find all passwords if SQLite structure is unusual
- No support for multiple user profiles
- Does not handle locked database files

## Comparison with Full Version

| Feature | Minimal Version | Full Version |
|---------|----------------|--------------|
| Size | ≤100KB | ~500KB+ |
| ABE Support | ❌ | ✅ |
| Multiple Profiles | ❌ | ✅ |
| JSON Export | ❌ | ✅ |
| Process Injection | ❌ | ✅ |
| Cookies/Payments | ❌ | ✅ |
| Error Handling | Minimal | Full |
| Browser Support | 2 | 3+ |

## Security Notes

⚠️ **For Educational Purposes Only**

This tool demonstrates:
- Windows DPAPI usage
- AES-GCM decryption
- SQLite data structures
- Executable size optimization techniques

**Important**:
- Requires access to user's local files
- Only decrypts current user's passwords
- Does not bypass OS-level security
- Does not work remotely or over network
- Not designed to evade detection

## Technical Details

### Decryption Process

1. **DPAPI Decryption** (Master Key):
   ```
   Encrypted Key (from Local State) → Remove "DPAPI" prefix → CryptUnprotectData() → AES Master Key
   ```

2. **AES-GCM Decryption** (Passwords):
   ```
   Password Blob → Remove "v10" prefix → Extract nonce (12 bytes) → Extract ciphertext → Extract tag (16 bytes) → BCryptDecrypt() → Plaintext Password
   ```

### File Structure

```
minimal/
├── src/
│   ├── base64.h           # Base64 decoder (~40 lines)
│   ├── json_parser.h      # JSON parser (~50 lines)
│   ├── sqlite_reader.h    # SQLite reader (~200 lines)
│   └── main.cpp           # Main implementation (~300 lines)
├── build/
│   └── chrome_decrypt.exe # Final executable
├── build.bat              # Build script
└── README.md              # This file
```

### Dependencies

- **Windows API**: File I/O, console output
- **Crypt32.lib**: DPAPI (CryptUnprotectData)
- **Bcrypt.lib**: AES-GCM decryption
- **Shell32.lib**: Get folder paths (SHGetFolderPath)
- **Kernel32.lib**: Core Windows functions
- **User32.lib**: Console functions

All statically linked, no runtime DLLs required.

## License

MIT License - Same as parent project

## Acknowledgments

Based on research from:
- [xaitax/Chrome-App-Bound-Encryption-Decryption](https://github.com/xaitax/Chrome-App-Bound-Encryption-Decryption)
- Chrome password encryption documentation
- SQLite file format specification

## Development Notes

### Future Size Optimizations (if needed):

1. **Assembly**: Rewrite crypto in assembly
2. **Custom CRT**: Remove remaining CRT dependencies
3. **Compression**: Use LZMA compression (note: requirement says no UPX, but other compression might be acceptable)
4. **Direct Syscalls**: Bypass some Windows API layers
5. **Hardcoded Offsets**: Remove dynamic SQLite parsing

### Testing

Tested on:
- Windows 10 x64
- Windows 11 x64
- Chrome versions 80-131
- Edge versions 80-131

### Known Issues

- May fail if Login Data is locked by browser
- Does not handle Chrome v127+ with ABE enabled
- No profile selection
- Limited error reporting
