# 🎯 Minimal Chrome Password Decryptor - Quick Start Guide

## What Is This?

A **lightweight Chrome/Edge password decryptor** in a single executable **≤100KB** - no dependencies, no admin rights, no packers.

## Quick Facts

- 📦 **Size**: ≤100KB (without UPX)
- 💻 **Platforms**: Windows x64, ARM64
- 🔓 **Browsers**: Google Chrome, Microsoft Edge
- 🛡️ **Permissions**: No admin required
- 🔒 **Encryption**: DPAPI + AES-256-GCM
- 📝 **Output**: Console (text)
- 🚀 **Dependencies**: None (static build)

## How to Build

### On Windows (Developer Command Prompt):

```cmd
cd minimal
build.bat
```

Output: `minimal/build/chrome_decrypt.exe`

### Via GitHub Actions:

Automatically builds on push:
- `chrome_decrypt_minimal_x64.exe`
- `chrome_decrypt_minimal_arm64.exe`

Available in [Releases](https://github.com/Dante-cpu/Chrome-App-Bound-Encryption-Decryption/releases)

## How to Use

Simply run the executable:

```cmd
chrome_decrypt.exe
```

No arguments needed. It will:
1. Find Chrome/Edge installations
2. Extract master keys
3. Decrypt passwords
4. Display results

Example output:
```
Chrome/Edge Password Decryptor
================================

=== Google Chrome ===
Master key decrypted

URL: https://example.com
Username: user@example.com
Password: mypassword123

Found 2 passwords

Total: 2 passwords extracted
```

## What It Does

```
┌─────────────────────────────────────────────────────────────┐
│  1. Find Browser Paths                                       │
│     ↓ %LOCALAPPDATA%\Google\Chrome\User Data                │
│     ↓ %LOCALAPPDATA%\Microsoft\Edge\User Data               │
├─────────────────────────────────────────────────────────────┤
│  2. Read Local State (JSON)                                  │
│     ↓ Extract "encrypted_key" field                          │
│     ↓ Base64 decode → encrypted master key                   │
├─────────────────────────────────────────────────────────────┤
│  3. Decrypt Master Key (DPAPI)                               │
│     ↓ Remove "DPAPI" prefix                                  │
│     ↓ CryptUnprotectData() → AES-256 key                     │
├─────────────────────────────────────────────────────────────┤
│  4. Read Login Data (SQLite)                                 │
│     ↓ Scan table leaf pages                                  │
│     ↓ Extract password records                               │
├─────────────────────────────────────────────────────────────┤
│  5. Decrypt Passwords (AES-GCM)                              │
│     ↓ Parse "v10" format                                     │
│     ↓ BCryptDecrypt() → plaintext passwords                  │
├─────────────────────────────────────────────────────────────┤
│  6. Display Results                                          │
│     ↓ Console output                                         │
└─────────────────────────────────────────────────────────────┘
```

## What It Doesn't Do

❌ **Not Supported** (to keep size ≤100KB):
- App-Bound Encryption (ABE) - Chrome 127+
- Multiple profiles (Profile 1, 2, etc.)
- Brave browser
- Cookies, payments, IBANs
- JSON file export
- Unicode output
- File locking handling

✅ **Supported** (core functionality):
- Chrome & Edge (DPAPI passwords)
- Default profile
- Password extraction
- Console output
- x64 & ARM64

## Technical Details

### Size Breakdown (~100KB):

```
Component                    Size
────────────────────────────────
Windows APIs & CRT          ~40KB
DPAPI (crypt32.lib)         ~15KB
BCrypt (bcrypt.lib)         ~20KB
Custom code (Base64/JSON)   ~15KB
Main application logic      ~10KB
────────────────────────────────
Total                       ~100KB
```

### Custom Implementations:

| Component | Lines | Replaces | Savings |
|-----------|-------|----------|---------|
| Base64 decoder | 45 | CRT/external | ~15KB |
| JSON parser | 54 | rapidjson/nlohmann | ~100KB |
| SQLite reader | 197 | sqlite3.lib | ~250KB |

### Compiler Optimizations:

```
/O1      - Optimize for size
/MT      - Static CRT
/GS-     - No security checks
/Gy      - Function-level linking
/GL      - Whole program optimization
/LTCG    - Link-time code generation
/OPT:REF - Remove unused code
/OPT:ICF - Fold identical functions
```

## Files Included

```
minimal/
├── src/
│   ├── base64.h         # Custom Base64 decoder
│   ├── json_parser.h    # Minimal JSON parser
│   ├── sqlite_reader.h  # Simplified SQLite reader
│   └── main.cpp         # Main implementation
├── build.bat            # Build script
├── README.md            # User guide (this file)
├── TECHNICAL.md         # Technical documentation
└── IMPLEMENTATION.md    # Implementation summary
```

## Comparison

| Feature | Minimal | Full Version |
|---------|---------|--------------|
| **Size** | ≤100KB | ~500KB |
| **Build Time** | ~5 sec | ~30 sec |
| **ABE Support** | ❌ | ✅ |
| **Browsers** | 2 | 3+ |
| **Profiles** | 1 | All |
| **Data Types** | Passwords | All |
| **Output** | Console | JSON |
| **Injection** | ❌ | ✅ |

## When to Use Each Version

### Use **Minimal** when:
- ✅ Size constraint (≤100KB)
- ✅ Simple password extraction
- ✅ Chrome v80-126 (DPAPI era)
- ✅ Quick console output
- ✅ Educational/research purposes

### Use **Full** when:
- ✅ Need ABE support (Chrome 127+)
- ✅ Want cookies, payments, IBANs
- ✅ Need JSON export
- ✅ Multiple profiles
- ✅ Production use

## Security Notes

⚠️ **Educational Purposes Only**

**What this tool does:**
- Accesses current user's files
- Uses legitimate Windows APIs
- Decrypts user's own passwords

**What this tool does NOT do:**
- Access other users' files
- Require admin privileges
- Bypass OS security
- Work remotely
- Inject code
- Hook APIs

## Requirements

- **OS**: Windows 10/11
- **Arch**: x64 or ARM64
- **Browsers**: Chrome 80-126 or Edge 80-126
- **Rights**: Standard user (no admin)
- **Encryption**: DPAPI (not ABE)

## Troubleshooting

### "Cannot open Local State"
- Browser not installed or path wrong
- Run from user account that has browser profile

### "Cannot open Login Data"
- Database locked by browser
- Close browser and try again
- Minimal version doesn't handle file locks

### "Cannot decrypt master key"
- DPAPI unavailable
- Wrong Windows user account

### No passwords found
- Database empty
- ABE enabled (Chrome 127+)
- Only Default profile scanned

## Further Reading

- **README.md** - Full user documentation
- **TECHNICAL.md** - Technical deep dive
- **IMPLEMENTATION.md** - Implementation details
- **Main README** - Full version documentation

## License

MIT License - Same as parent project

## Credits

Based on research from:
- [Chrome-App-Bound-Encryption-Decryption](https://github.com/xaitax/Chrome-App-Bound-Encryption-Decryption)

Minimal implementation by: GitHub Copilot
