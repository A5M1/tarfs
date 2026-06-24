/**
 * @file tarfs.c
 * @brief Private implementation details and structural definitions for the TAR parser.
 */

#include "tarfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @struct tar_header
 * @brief Layout matching the POSIX 1003.1 ustar standard 512-byte header block.
 *
 * All numeric text fields (mode, size, mtime, chksum) are written as ASCII octal values,
 * terminated by space characters or nulls.
 */
typedef struct {
    char name[100];      /**< File path/name string. Null-terminated if less than 100 chars. */
    char mode[8];        /**< Permissions bitmask encoded as an octal string. */
    char uid[8];         /**< Numeric owner user ID string. */
    char gid[8];         /**< Numeric owner group ID string. */
    char size[12];       /**< File size in bytes, represented as an ASCII octal string. */
    char mtime[12];      /**< Modification timestamp represented as an ASCII octal string. */
    char chksum[8];      /**< Simple byte sum checksum string for header validation. */
    char typeflag[1];    /**< Type indicator: '0' or '\\0'=Normal, '2'=Symlink, '5'=Directory. */
    char linkname[100];  /**< Target path of symbolic/hard link reference. */
    char magic[6];       /**< Magic verification identifier; holds "ustar" followed by an explicit null. */
    char version[2];     /**< Version string; generally holds "00". */
    char uname[32];      /**< ASCII alphanumeric owner user name string. */
    char gname[32];      /**< ASCII alphanumeric owner group name string. */
    char devmajor[8];    /**< Major device number string. */
    char devminor[8];    /**< Minor device number string. */
    char prefix[155];    /**< Path prefix extension used if a filename exceeds 100 characters. */
    char padding[12];    /**< Structural padding to align the struct exactly to 512 bytes. */
} tar_header;

/**
 * @struct tar_entry
 * @brief Internal tracker linking an isolated file's identifier to its location inside the buffer.
 */
typedef struct {
    char *name;          /**< Dynamically allocated combined path name (prefix + name). */
    size_t size;         /**< Decoded binary file payload dimension in bytes. */
    size_t offset;       /**< Absolute byte index inside the global buffer where raw payload begins. */
} tar_entry;

/**
 * @struct tar_archive
 * @brief Master instance encapsulation data structure.
 */
struct tar_archive {
    uint8_t *buffer;     /**< Reference handle pointing to raw master memory stream. */
    size_t   size;       /**< Total linear size of raw master memory stream in bytes. */
    int      owns_buffer;/**< Flag signifying if the container is responsible for deallocating `buffer`. */
    tar_entry *entries;  /**< Dynamically allocated flat array tracking recorded file entries. */
    size_t   num_entries;/**< Active tally of records preserved in the `entries` array. */
    size_t   capacity;   /**< Total pointer allocation scale boundary of the current `entries` array. */
};

/**
 * @brief Utility function to translate an ASCII octal text segment into an unsigned size integer.
 *
 * Evaluation naturally breaks early if any character encountered is outside the octal radix '0'-'7' 
 * or if a null character terminator is discovered ahead of schedule.
 *
 * @param[in] s   Target string sequence containing the character array.
 * @param[in] len Maximum evaluation depth boundary in bytes.
 * @return The fully accumulated representation value as a `size_t` integer.
 */
static size_t parse_octal(const char *s, size_t len) {
    size_t val = 0;
    for (size_t i = 0; i < len && s[i]; i++) {
        if (s[i] < '0' || s[i] > '7') break;
        val = (val << 3) | (s[i] - '0');
    }
    return val;
}

/**
 * @brief Validates integrity of a TAR file header via checksum calculation.
 *
 * In accordance with POSIX regulations, the header checksum calculation treats the 8-byte 
 * `chksum` array field itself as if it were packed entirely with simple ASCII space characters (`' '`).
 *
 * @param[in] hdr Address of the target header record block to check.
 * @return `1` if the calculated sum perfectly correlates to the recorded checksum value; `0` otherwise.
 */
static int verify_checksum(const tar_header *hdr) {
    unsigned int sum = 0;
    const unsigned char *bytes = (const unsigned char*)hdr;
    
    for (size_t i = 0; i < sizeof(tar_header); i++) {
        if (i >= 148 && i < 156) {
            sum += ' ';   
        } else {
            sum += bytes[i];
        }
    }
    unsigned int stored = (unsigned int)parse_octal(hdr->chksum, sizeof(hdr->chksum));
    return (sum == stored);
}

/**
 * @brief Internal master routine designed to linearly sweep and map the TAR memory stream.
 *
 * Loops over the buffer in 512-byte structural increments, decodes file names, applies path extensions
 * if pre-populated, verifies structural limits, and populates the tracking index.
 *
 * @param[in] buffer Raw memory address containing target file data.
 * @param[in] size   Boundary scale of the target data buffer block.
 * @param[in] owns   Boolean toggle specifying if the manager system owns the buffer lifecycle.
 * @return Fully populated runtime archive handle object on success, or `NULL` if allocation fails.
 */
static tar_archive* parse_archive(void *buffer, size_t size, int owns) {
    tar_archive *arc = calloc(1, sizeof(tar_archive));
    if (!arc) return NULL;
    arc->buffer = buffer;
    arc->size = size;
    arc->owns_buffer = owns;
    arc->entries = NULL;
    arc->num_entries = 0;
    arc->capacity = 0;

    size_t pos = 0;
    while (pos + 512 <= size) {
        tar_header *hdr = (tar_header*)((uint8_t*)buffer + pos);
        if (hdr->name[0] == '\0') break;
        if (!verify_checksum(hdr)) {
            break; 
        }
        size_t filesize = parse_octal(hdr->size, sizeof(hdr->size));
        size_t data_blocks = (filesize + 511) / 512;
        if (pos + 512 + (data_blocks * 512) > size) {
            break; 
        }
        if (hdr->typeflag[0] == '0' || hdr->typeflag[0] == '\0') {
            char fullname[256];
            if (hdr->prefix[0] != '\0') {
                snprintf(fullname, sizeof(fullname), "%s/%s", hdr->prefix, hdr->name);
            } else {
                strncpy(fullname, hdr->name, sizeof(fullname)-1);
                fullname[sizeof(fullname)-1] = '\0';
            }
            if (arc->num_entries >= arc->capacity) {
                size_t newcap = arc->capacity ? arc->capacity * 2 : 8;
                tar_entry *newent = realloc(arc->entries, newcap * sizeof(tar_entry));
                if (!newent) goto error;
                arc->entries = newent;
                arc->capacity = newcap;
            }
            
            tar_entry *ent = &arc->entries[arc->num_entries];
            ent->name = strdup(fullname);
            if (!ent->name) goto error;
            ent->size = filesize;
            ent->offset = pos + 512;  
            arc->num_entries++;
        }
        pos += 512 + (data_blocks * 512);
    }

    return arc;

error:
    for (size_t i = 0; i < arc->num_entries; i++) {
        free(arc->entries[i].name);
    }
    free(arc->entries);
    if (arc->owns_buffer) free(arc->buffer);
    free(arc);
    return NULL;
}

tar_archive* tar_open_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    
    if (sz <= 0) { fclose(f); return NULL; }

    uint8_t *buf = malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    
    if (fread(buf, 1, sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    return parse_archive(buf, sz, 1);
}

tar_archive* tar_open_memory(const void *data, size_t size) {
    if (!data || size == 0) return NULL;
    return parse_archive((void*)data, size, 0);
}

void tar_close(tar_archive *arc) {
    if (!arc) return;
    for (size_t i = 0; i < arc->num_entries; i++) {
        free(arc->entries[i].name);
    }
    free(arc->entries);
    if (arc->owns_buffer) free(arc->buffer);
    free(arc);
}

size_t tar_get_file_count(const tar_archive *arc) {
    return arc ? arc->num_entries : 0;
}

const char* tar_get_file_name(const tar_archive *arc, size_t idx) {
    if (!arc || idx >= arc->num_entries) return NULL;
    return arc->entries[idx].name;
}

size_t tar_get_file_size(const tar_archive *arc, size_t idx) {
    if (!arc || idx >= arc->num_entries) return 0;
    return arc->entries[idx].size;
}

const void* tar_get_file_data(const tar_archive *arc, size_t idx) {
    if (!arc || idx >= arc->num_entries) return NULL;
    return arc->buffer + arc->entries[idx].offset;
}

int tar_get_file_by_name(const tar_archive *arc, const char *name,
                          const void **out_data, size_t *out_size) {
    if (!arc || !name) return -1;
    for (size_t i = 0; i < arc->num_entries; i++) {
        if (strcmp(arc->entries[i].name, name) == 0) {
            if (out_data) *out_data = arc->buffer + arc->entries[i].offset;
            if (out_size) *out_size = arc->entries[i].size;
            return 0;
        }
    }
    return -1;
}
