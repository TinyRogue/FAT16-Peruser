#include "file_reader.h"


//TODO: Func to implement
struct disk_t* disk_open_from_file(const char* volume_file_name) {
    return NULL;
}

//TODO: Func to implement
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read) {
    return 0;
}

//TODO: Func to implement
int disk_close(struct disk_t* pdisk) {
    return 0;
}

//TODO: Func to implement
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector) {
    return NULL;
}

//TODO: Func to implement
int fat_close(struct volume_t* pvolume) {
    return 0;
}

//TODO: Func to implement
struct file_t* file_open(struct volume_t* pvolume, const char* file_name) {
    return NULL;
}

//TODO: Func to implement
int file_close(struct file_t* stream) {
    return 0;
}

//TODO: Func to implement
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream) {
    return 0;
}

//TODO: Func to implement
int32_t file_seek(struct file_t* stream, int32_t offset, int whence) {
    return 0;
}

//TODO: Func to implement
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path) {
    return NULL;
}

//TODO: Func to implement
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry) {
    return 0;
}

//TODO: Func to implement
int dir_close(struct dir_t* pdir) {
    return 0;
}
