# tarfs

A lightweight, high-performance, read-only virtual file system parser for uncompressed POSIX ustar TAR archives. Written in clean C, `tarfs` is designed for embedded applications, game asset management, and resource bundling where low memory overhead is critical.

---

## Features

* **Zero-Copy In-Memory Parsing**: Parses archives directly out of pre-allocated buffers without duplication.
* **Single-Allocation File Loading**: Opens files using a single contiguous block of memory.
* **Opaque Architecture**: Maintains strict encapsulation to prevent user-space structural contamination.
* **Multi-Platform Support**: Built with export configurations for Linux, macOS, and Windows DLL platforms.
* **Fully Documented**: Extensively tagged using standard Doxygen semantics for automated manual generation.

---

## File Structure

* `tarfs.h`: Public API definition containing memory management guards, documentation handles, and DLL export macros.
* `tarfs.c`: Low-level architectural implementation, checksum validation, and octal-to-binary translation engines.

---

## Compilation Guide

This library can be compiled easily using Clang or GCC into whatever binary format your pipeline demands.

### 1. Static Library (.a)

Perfect for standard static linking into a single standalone executable file.

```bash
clang -Wall -Wextra -O2 -c tarfs.c -o tarfs.o
ar rcs libtarfs.a tarfs.o

```

### 2. Shared Library (.so / .dylib)

Ideal for dynamic dependency loading at runtime on Unix systems.

**Linux:**

```bash
clang -Wall -Wextra -O2 -fPIC -c tarfs.c -o tarfs.o
clang -shared -o libtarfs.so tarfs.o

```

**macOS:**

```bash
clang -Wall -Wextra -O2 -fPIC -c tarfs.c -o tarfs.o
clang -shared -o libtarfs.dylib tarfs.o

```

### 3. Windows Dynamic Link Library (.dll)

Windows requires explicit symbol exporting via compiler specifications. To build the DLL and force the creation of the import library (`tarfs.lib`), use the following commands based on your compiler flavor.

**Native MSVC / LLVM Toolchain:**

```bash
clang -shared -DTARFS_BUILD_DLL tarfs.c -o tarfs.dll -Wl,/implib:tarfs.lib

```

**MinGW / MSYS2 Environments:**

```bash
clang -shared -DTARFS_BUILD_DLL tarfs.c -o tarfs.dll -Wl,--out-implib,tarfs.lib

```

---

## Linking Against the Library

To consume the library within your own host application (e.g., `main.c`), place `tarfs.h` and the compiled library file inside your project directory.

### Linking the Static or Shared Library (Linux/macOS)

```bash
clang main.c -L. -ltarfs -o my_program

```

*Note for Shared Libraries: Ensure you append your execution runtime path using `LD_LIBRARY_PATH=. ./my_program` so the OS can locate the binary module.*

### Linking the Windows DLL

```bash
clang main.c tarfs.lib -o my_program.exe

```

*Note: Ensure `tarfs.dll` resides in the exact same directory as `my_program.exe` when executing.*

---

## Quick API Usage Example

```c
#include <stdio.h>
#include "tarfs.h"

int main(void) {
    // Open the archive file from disk
    tar_archive *arc = tar_open_file("assets.tar");
    if (!arc) {
        printf("Failed to open archive.\n");
        return 1;
    }

    // Inspect general file quantities
    size_t total_files = tar_get_file_count(arc);
    printf("Archive contains %zu files.\n", total_files);

    // Query file elements by path name
    const void *file_data = NULL;
    size_t file_size = 0;
    if (tar_get_file_by_name(arc, "images/hero.png", &file_data, &file_size) == 0) {
        printf("Found hero.png! Size: %zu bytes.\n", file_size);
        // file_data points directly to the raw bytes inside memory
    }

    // Clean up and release assets safely
    tar_close(arc);
    return 0;
}

```

---

## Documentation Generation

The code is formatted explicitly for Doxygen. To compile standard, locally hosted HTML help dashboards, execute the generator tool inside the project root directory:

```bash
doxygen Doxyfile

```

The resulting interactive web interface will be written to the `doc/html/` path automatically.


`The following readme was generated using a local gemini E4b model running on a private server.`

