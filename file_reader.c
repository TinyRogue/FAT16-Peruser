#include "file_reader.h"



//static lba_t calc_cluster_offset(lba_t data_start, uint8_t sectors_per_cluster, uint64_t n) {
//    return data_start + (n - 2) * sectors_per_cluster;
//}

//static lba_t available_clusters(struct disk_t *disk) {
//    uint32_t logical_sectors = disk->VBR->small_sectors == 0 ? disk->VBR->large_sectors : disk->VBR->small_sectors;
//    return (logical_sectors - disk->VBR->reserved_sectors - disk->VBR->FATs * disk->VBR->sectors_per_FAT - disk->sectors_per_root) / disk->VBR->sectors_per_cluster;
//}

static bool is_VBR_valid(VBR_t *VBR) {
    if (VBR->reserved_sectors > 0) return false;
    if (VBR->root_entries % VBR->bytes_per_sector != 0) return false;
    if (VBR->sectors_per_FAT < 1) return false;
    if (VBR->signature != SIGNATURE_VALUE) return false;
    if (VBR->small_sectors == 0 ^ VBR->large_sectors == 0) return false;
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

//TODO: Func to implement
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

//TODO: Func to implement
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector) {
    //FAT1
    //FAT2
    //calloc FATS* bytes per sector
    /*
     *     disk->FAT1_start = disk->volume_start + disk->VBR->reserved_sectors;
    disk->FAT2_start = disk->FAT1_start + disk->VBR->sectors_per_FAT;
    disk->root_start = disk->FAT1_start + disk->VBR->FATs * disk->VBR->sectors_per_FAT;
    disk->sectors_per_root = disk->VBR->root_entries * sizeof(disk->VBR->root_entries) / disk->VBR->bytes_per_sector;
    disk->data_start = disk->root_start + disk->sectors_per_root;
     */
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
