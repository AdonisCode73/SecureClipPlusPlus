# SecureClip++

A Windows clipboard manager that encrypts your clipboard history with a password. Every piece of text you copy is automatically saved to an encrypted file on disk, and you can retrieve any past item back into your clipboard at any time.

## Features

- **Automatic clipboard monitoring** — passively captures every text copy via the Windows clipboard listener
- **Encrypted storage** — clipboard history is encrypted with XChaCha20-Poly1305 (via libsodium) and saved to `data.enc`
- **Password protection** — set a password on first run; required to unlock on subsequent launches
- **Hotkey support** — press `Ctrl+Shift+D` to manually store the current clipboard content
- **Retrieve past items** — copy any stored snippet back to your clipboard from the interactive menu
- **Duplicate prevention** — identical entries are not stored twice
- **Secure memory handling** — passwords and keys are zeroed from memory after use

## How It Works

SecureClip++ runs as a console application with a hidden window that listens for clipboard changes and hotkey events. A separate thread provides an interactive menu:

1. **List** all stored clipboard items
2. **Copy** a stored item back to your clipboard
3. **Clear** all history
4. **Toggle** verbose logging
5. **Exit**

All data is persisted to `data.enc` using authenticated encryption. Each save generates a fresh random salt and nonce, and derives the encryption key from your password using Argon2id.

## Building

**Requirements:** CMake 3.14+, MSVC (or Clang-CL/MinGW), Windows

```bash
cmake -B build -S .
cmake --build build --config Release
```

CMake will automatically download and build [libsodium](https://github.com/jedisct1/libsodium) from source. The resulting executable is `SecureClipPlusPlus.exe`.

## Dependencies

- [libsodium](https://github.com/jedisct1/libsodium) — cryptography (XChaCha20-Poly1305, Argon2id, secure memory)
- [nlohmann/json](https://github.com/nlohmann/json) — JSON serialization (header-only, included in repo)

## License

MIT — see [LICENSE](LICENSE) for details.
