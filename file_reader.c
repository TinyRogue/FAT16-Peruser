#include "file_reader.h"



struct disk_t* disk_open_from_file(const char* volume_file_name) {

    if (!volume_file_name) {
        errno = EFAULT;
        LOG_ERROR("Pointer is NULL")
        return NULL;
    }

    struct disk_t *disk = (struct disk_t*)calloc(sizeof(struct disk_t), 1);
    if (!disk) {
        errno = ENOMEM;
        LOG_ERROR("Not enough memory")
        return NULL;
    }

    disk->VBR = (struct VBR_t*)calloc(1, sizeof(struct VBR_t));
    if (!disk->VBR) {
        free(disk);
        errno = ENOMEM;
        LOG_ERROR("Not enough memory")
        return NULL;
    }

    disk->disk_file = fopen(volume_file_name, "rb");
    if (!disk->disk_file) {
        free(disk->VBR);
        free(disk);
        errno = ENOENT;
        LOG_ERROR("No such file");
        return NULL;
    }

    if (disk_read(disk, 0, disk->VBR, 1) == -1) {
        free(disk->VBR);
        fclose(disk->disk_file);
        free(disk);
        return NULL;
    }

    return disk;
}


int disk_read(struct disk_t* from, int32_t first_sector, void* to, int32_t sectors_to_read) {

    if (!from || !to) {
        errno = EFAULT;
        LOG_ERROR("Null pointer exception");
        return -1;
    }

    if (first_sector < 0 || sectors_to_read < 0) {
        errno = ERANGE;
        LOG_ERROR("Range cannot be lower than zero");
        return -1;
    }

    fseek(from->disk_file, first_sector * SECTOR_SIZE, SEEK_SET);

    int32_t read_blocks = (int32_t)fread(to, SECTOR_SIZE, sectors_to_read, from->disk_file);
    if (read_blocks != sectors_to_read) {
        errno = ERANGE;
        LOG_ERROR("Could not read set number of sectors");
        return -1;
    }

    return read_blocks;
}


int disk_close(struct disk_t* pdisk) {
    if (!pdisk || !pdisk->disk_file || !pdisk->VBR) {
        errno = EFAULT;
        LOG_ERROR("Pointer is NULL")
        return -1;
    }

    fclose(pdisk->disk_file);
    free(pdisk->VBR);
    free(pdisk);
    return 0;
}


static bool is_VBR_valid(const VBR_t* const VBR) {
    if (VBR->reserved_sectors == 0) return false;
    if (VBR->bytes_per_sector == 0 || ((VBR->root_entries * sizeof(Entry_t)) % VBR->bytes_per_sector) != 0) return false;
    if (VBR->sectors_per_FAT < 1) return false;
    if (VBR->signature != SIGNATURE_VALUE) return false;
    if (!(VBR->small_sectors == 0 ^ VBR->large_sectors == 0)) return false;
    if (VBR->small_sectors == 0 && VBR->large_sectors < 65536) return false;

    uint8_t _counter = 0;
    uint8_t _aval_sectors_per_cluster[] = {1, 2, 4, 8, 16, 32, 64, 128};
    uint8_t _length = sizeof(_aval_sectors_per_cluster) / sizeof(*_aval_sectors_per_cluster);
    for (; _counter < _length ; _counter++) {
        if (VBR->sectors_per_cluster == _aval_sectors_per_cluster[_counter]) break;
    }
    if (_counter == _length) return false;

    return VBR->sector_end_marker == SECTOR_END_MARKER_VALUE;
}


static bool load_FATs(struct volume_t* const volume) {

    //Allocate memory for FATs ptr
    volume->FATs_mem = (uint8_t**)calloc(volume->disk->VBR->FATs, sizeof(uint8_t*));
    if (!volume->FATs_mem) {
        errno = ENOMEM;
        LOG_ERROR("Not enough memory");
        return false;
    }

    lba_t FAT_memory_size = volume->disk->VBR->sectors_per_FAT * volume->disk->VBR->bytes_per_sector;
    //Allocate memory for FATs
    for (int i = 0; i < volume->disk->VBR->FATs; i++) {
        volume->FATs_mem[i] = (uint8_t*)calloc(1, FAT_memory_size);
        if (!volume->FATs_mem[i]) {
            for (int j = 0; j < i; j++) {
                free(volume->FATs_mem[i]);
            }
            errno = ENOMEM;
            LOG_ERROR("Not enough memory");
            return false;
        }
    }

    //Load memory into FATs
    for (int i = 0; i < volume->disk->VBR->FATs; i++) {

        lba_t fat_position = volume->volume_start + volume->disk->VBR->reserved_sectors + volume->disk->VBR->sectors_per_FAT * i;
        int32_t sectors_read = disk_read(volume->disk, fat_position, volume->FATs_mem[i], volume->disk->VBR->sectors_per_FAT);

        if (sectors_read != volume->disk->VBR->sectors_per_FAT) {
            errno = ERANGE;
            LOG_ERROR("Couldn't read FATs");
            for (int j = 0; j < volume->disk->VBR->FATs; j++) {
                free(volume->FATs_mem[j]);
            }
            return false;
        }
    }

    //Validate FATs
    for (int i = 1; i < volume->disk->VBR->FATs; i++) {
        if (memcmp(volume->FATs_mem[i - 1], volume->FATs_mem[i], FAT_memory_size) != 0) {
            errno = EINVAL;
            LOG_ERROR("FATs damaged");
            for (int j = 0; j < volume->disk->VBR->FATs; j++) {
                free(volume->FATs_mem[j]);
            }
            return false;
        }
    }

    return true;
}


static bool load_root_dir(struct volume_t* const volume) {
    lba_t root_dir_pos = volume->volume_start + volume->disk->VBR->reserved_sectors + volume->disk->VBR->FATs * volume->disk->VBR->sectors_per_FAT;

    lba_t root_dir_size = (volume->disk->VBR->root_entries * sizeof(Entry_t)) / volume->disk->VBR->bytes_per_sector;
    root_dir_size += ((volume->disk->VBR->root_entries * sizeof(Entry_t)) % volume->disk->VBR->bytes_per_sector) != 0;

    size_t root_dir_bytes = root_dir_size * volume->disk->VBR->bytes_per_sector;

    volume->root_dir_entries = (Entry_t *)calloc(1, root_dir_bytes);
    if (!volume->root_dir_entries) {
        errno = ENOMEM;
        LOG_ERROR("Not enough memory");
        return false;
    }

    int32_t read_blocks = disk_read(volume->disk, root_dir_pos, volume->root_dir_entries, root_dir_size);
    if (read_blocks != (int64_t)root_dir_size) {
        free(volume->root_dir_entries);
        LOG_ERROR("Could not read root directory entries");
        return false;
    }

    return true;
}


/* Boot Sector | FAT1 | FAT2 | FAT... | ROOT DIRECTORY | DATA REGION */
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector) {
    if (!pdisk) {
        errno = EFAULT;
        LOG_ERROR("Pointer is NULL")
        return NULL;
    }

    if (!is_VBR_valid(pdisk->VBR)) {
        errno = EINVAL;
        LOG_ERROR("VBR_t is invalid")
        return NULL;
    }

    struct volume_t *volume = (struct volume_t*)calloc(1, sizeof(struct volume_t));
    if (!volume) {
        errno = ENOMEM;
        LOG_ERROR("Not enough memory");
        return NULL;
    }

    volume->disk = pdisk;
    volume->volume_start = first_sector;

    if (!load_FATs(volume)) {
        free(volume);
        return NULL;
    }

    if (!load_root_dir(volume)) {
        free(volume);
        return NULL;
    }

    return volume;
}


int fat_close(struct volume_t* pvolume) {
    if (!pvolume || !pvolume->FATs_mem || !pvolume->root_dir_entries || !pvolume->disk) {
        errno = EFAULT;
        LOG_ERROR("Null pointer exception");
        return -1;
    }

    for (int i = 0; i < pvolume->disk->VBR->FATs; i++) {
        free(pvolume->FATs_mem[i]);
        pvolume->FATs_mem[i] = NULL;
    }
    free(pvolume->FATs_mem);
    pvolume->FATs_mem = NULL;

    free(pvolume->root_dir_entries);
    pvolume->root_dir_entries = NULL;

    free(pvolume);
    pvolume = NULL;
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
