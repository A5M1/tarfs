/**
 * @file tarfs.h
 * @brief Public API definitions for the lightweight, read-only TAR file system parser.
 *
 * This module provides functions to safely load, query, and stream contents
 * out of standard POSIX ustar uncompressed TAR archives. It supports both 
 * file-system based streaming and direct, non-copying in-memory reads.
 *
 * @author ABNSOFT
 * @date 2026
 * @version 1.0.0
 */

#ifndef TARFS_H
#define TARFS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @def TARFS_API
 * @brief Handles explicit DLL symbol exporting/importing on Windows platforms.
 * Evaluates to an empty macro on non-Windows deployment environments.
 */
#ifdef _WIN32
    #ifdef TARFS_BUILD_DLL
        #define TARFS_API __declspec(dllexport)
    #else
        #define TARFS_API __declspec(dllimport)
    #endif
#else
    #define TARFS_API 
#endif

/**
 * @struct tar_archive
 * @brief Opaque handle representing a parsed and indexed TAR archive instance.
 *
 * The fields of this structure are hidden to maintain binary compatibility and
 * encapsulation. All interactions must occur via the public API functions.
 */
typedef struct tar_archive tar_archive;

/**
 * @brief Opens a TAR archive from the host file system.
 *
 * This function opens the designated file, determines its size, allocates a
 * single memory buffer matching that size, and reads the entire archive into 
 * RAM. It then constructs an internal lookup index of all regular files.
 *
 * @note The returned handle owns the allocated memory block. Closing the 
 * handle will cleanly release this memory.
 *
 * @pre \p filename must be a valid, null-terminated string pointing to an existing file.
 * @post On success, an initialized engine handle is returned. On failure, all local
 * file descriptors are safely re-pooled and NULL is returned.
 *
 * @param[in] filename Path to the TAR archive file on disk.
 * @return A pointer to an initialized `tar_archive` instance, or `NULL` if the 
 * file cannot be read, is empty, or contains corrupt headers.
 *
 * @see tar_close
 */
TARFS_API tar_archive* tar_open_file(const char *filename);

/**
 * @brief Opens and parses a TAR archive residing directly within a memory buffer.
 *
 * This function processes a pre-loaded or statically compiled archive raw memory block.
 * It builds an internal tracking index pointing directly to the payload sections inside
 * the original buffer without duplicating the data payloads.
 *
 * @warning The caller retains absolute ownership of \p data. The memory block bounded by
 * \p size **must** remain valid, immutable, and readable for the entire lifecycle
 * of the returned `tar_archive` handle.
 *
 * @pre \p data must not be NULL and \p size must be greater than 512 bytes.
 *
 * @param[in] data Pointer to the start of the raw TAR data array.
 * @param[in] size Total size of the memory block in bytes.
 * @return A pointer to an initialized `tar_archive` instance, or `NULL` if memory allocations 
 * fail or parsing invariants are violated.
 *
 * @see tar_close
 */
TARFS_API tar_archive* tar_open_memory(const void *data, size_t size);

/**
 * @brief Closes an open archive handle and deallocates its assets.
 *
 * Safely releases internal string mappings, structures, and index tables. If the archive
 * was created via tar_open_file(), the underlying raw data buffer is also freed.
 *
 * @param[in,out] arc The archive instance to destroy. If \p arc is `NULL`, this operation 
 * silently, safely terminates with no action.
 */
TARFS_API void tar_close(tar_archive *arc);

/**
 * @brief Obtains the total count of regular file entries indexed inside the archive.
 *
 * @note This count excludes directories, hard-links, soft-links, and device blocks.
 *
 * @param[in] arc Pointer to a valid archive instance.
 * @return The total number of files available for random access. Returns `0` if \p arc is `NULL`.
 */
TARFS_API size_t tar_get_file_count(const tar_archive *arc);

/**
 * @brief Retrieves the full name/path string of a specific file indexed by position.
 *
 * @note The returned string lifecycle matches the lifecycle of the parent archive object. 
 * Do not attempt to pass this pointer to free().
 *
 * @param[in] arc Pointer to a valid archive instance.
 * @param[in] idx Zero-based index of the target file entry. Must be less than tar_get_file_count().
 * @return A read-only pointer to the null-terminated file path string, or `NULL` if 
 * \p idx is out of bounds or \p arc is invalid.
 */
TARFS_API const char* tar_get_file_name(const tar_archive *arc, size_t idx);

/**
 * @brief Retrieves the actual payload size of a specific file indexed by position.
 *
 * @param[in] arc Pointer to a valid archive instance.
 * @param[in] idx Zero-based index of the target file entry. Must be less than tar_get_file_count().
 * @return The exact size of the target file payload in bytes, or `0` if arguments are invalid.
 */
TARFS_API size_t tar_get_file_size(const tar_archive *arc, size_t idx);

/**
 * @brief Obtains a direct memory pointer to the payload data of an indexed file.
 *
 * This provides zero-overhead access to raw files stored inside the TAR archive.
 *
 * @warning The payload is **not null-terminated**. If reading text data, you must bound 
 * your reads strictly using the size fetched via tar_get_file_size().
 *
 * @param[in] arc Pointer to a valid archive instance.
 * @param[in] idx Zero-based index of the target file entry.
 * @return A read-only pointer to the raw file data, or `NULL` if out of bounds.
 */
TARFS_API const void* tar_get_file_data(const tar_archive *arc, size_t idx);

/**
 * @brief Queries the archive to find a file by its name string.
 *
 * Performs a linear-time (`O(N)`) lookup through the internal index table matching names.
 *
 * @param[in]  arc      Pointer to a valid archive instance.
 * @param[in]  name     The null-terminated path name string to match against.
 * @param[out] out_data Optional pointer to capture the address of the raw data payload. Pass `NULL` if unneeded.
 * @param[out] out_size Optional pointer to capture the size of the payload. Pass `NULL` if unneeded.
 * @return `0` if the file is successfully resolved; `-1` if the file is not found or parameters are invalid.
 */
TARFS_API int tar_get_file_by_name(const tar_archive *arc, const char *name,
                                   const void **out_data, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* TARFS_H */