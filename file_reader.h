#ifndef FILE_READER
#define FILE_READER

#include <stdio.h>      /* For size_t, perror(), printf(), fprintf()    */
#include <stdlib.h>     /* For malloc() and its family                  */
#include <stdint.h>     /* For (u)int(*)_t types                        */
#include <stdbool.h>    /* For bool type                                */

#include <errno.h>      /* For ERRNO and constants                      */
#include <string.h>     /* For strerror()                               */

#include <fcntl.h>      /* For open()                                   */
#include <sys/stat.h>   /* For struct stat                              */
#include <sys/mman.h>   /* For mmap()                                   */
#include <unistd.h>     /* For close()                                  */


#define SECTOR_SIZE 0x200
#define SECTOR_END_MARKER_VALUE 0xAA55
#define SIGNATURE_VALUE 0x29

#define LOG_ERROR(str)                                                                        \
    fprintf(stderr, "%s, error code %d ('%s'), in line '%d', in func '%s', in file: '%s'\n",  \
        str, errno, strerror(errno), __LINE__, __func__, __FILE__);                           \


typedef uint32_t lba_t;
typedef uint32_t cluster_t;

typedef enum {READ_ONLY = 0x01, HIDDEN_FILE = 0x02, SYSTEM_FILE = 0x04,
              VOLUME_LABEL = 0x08, LONG_FILE_NAME = 0x0f, DIRECTORY = 0x10,
              ARCHIVE = 0x20, LFN = READ_ONLY | HIDDEN_FILE | SYSTEM_FILE | VOLUME_LABEL} dir_attributes;


typedef struct VBR_t {
    uint8_t jump_instructions[3];
    uint8_t OEM[8];

    struct {
        uint16_t bytes_per_sector;
        uint8_t sectors_per_cluster;
        uint16_t reserved_sectors;
        uint8_t FATs;
        uint16_t root_entries;
        uint16_t small_sectors;
        uint8_t media_type;
        uint16_t sectors_per_FAT;
        uint16_t sectors_per_track;
        uint16_t head_sides_num;
        uint32_t hidden_sectors;
        uint32_t large_sectors;
        uint8_t drive_number;
        uint8_t : 8;
        uint8_t signature;
        uint32_t serial_number;
    } __attribute__((packed));

    struct {
        uint8_t label[11];
        uint8_t system_type_level[8];
    } __attribute__((packed));

    uint8_t boot_code[448];
    uint16_t sector_end_marker;
} __attribute__((packed)) VBR_t;


typedef struct FAT_file_t {
    union {
        struct {
            uint8_t filename[8];
            uint8_t extension[3];
        };
        uint8_t allocation_status;
    };

    uint8_t attributes;
    uint8_t reserved;

    struct creation_time {
        uint8_t seconds_tens;
        uint16_t hh_mm_ss;
        uint16_t yy_mm_dd;
    } __attribute__((packed)) creation_time;

    uint16_t access_date;
    uint16_t first_cluster_address_high_order;

    struct modify_time {
        uint16_t hh_mm_ss;
        uint16_t yy_mm_dd;
    } __attribute__((packed)) modify_time;

    uint16_t first_cluster_address_low_order;
    uint32_t file_size;
} __attribute__((packed)) FAT_file_t;


struct disk_t {
    VBR_t *VBR;
    FILE *disk_file;
} __attribute__((packed));

struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);


struct volume_t {
    lba_t volume_start;
    lba_t FAT1_start;
    lba_t FAT2_start;
    lba_t root_start;
    lba_t sectors_per_root;
    lba_t data_start;
    uint32_t fat_entry_count; //available_clusters + 2
} __attribute__((packed));


struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);


struct file_t {
    //TODO: TO DELETE
    int a;
    //END OF DELETE
} __attribute__((packed));

struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);


struct dir_t {
//TODO: TO DELETE
    int a;
    //END OF DELETE
} __attribute__((packed));


struct dir_entry_t {
    char* name;
    size_t size;
    bool is_archived;
    bool is_readonly;
    bool is_system;
    bool is_hidden;
    bool is_directory;
} __attribute__((packed));

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);

#endif
