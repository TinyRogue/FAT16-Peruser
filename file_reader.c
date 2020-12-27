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
    const uint8_t _aval_sectors_per_cluster[] = {1, 2, 4, 8, 16, 32, 64, 128};
    const uint8_t _length = sizeof(_aval_sectors_per_cluster) / sizeof(*_aval_sectors_per_cluster);
    for (; _counter < _length ; _counter++) {
        if (VBR->sectors_per_cluster == _aval_sectors_per_cluster[_counter]) break;
    }
    if (_counter == _length) return false;

    return VBR->sector_end_marker == SECTOR_END_MARKER_VALUE;
}


static bool load_FATs(struct volume_t* const volume) {

    //Allocate memory for FATs ptr

    uint8_t **FATs_handler = (uint8_t**)calloc(volume->disk->VBR->FATs, sizeof(uint8_t*));
    if (!FATs_handler) {
        errno = ENOMEM;
        LOG_ERROR("Not enough memory");
        return false;
    }

    const lba_t FAT_memory_size = volume->disk->VBR->sectors_per_FAT * volume->disk->VBR->bytes_per_sector;
    //Allocate memory for FATs
    for (int i = 0; i < volume->disk->VBR->FATs; i++) {
        FATs_handler[i] = (uint8_t*)calloc(1, FAT_memory_size);
        if (!FATs_handler[i]) {
            for (int j = 0; j < i; j++) {
                free(FATs_handler[i]);
            }
            errno = ENOMEM;
            LOG_ERROR("Not enough memory");
            return false;
        }
    }

    //Load data into FATs
    for (int i = 0; i < volume->disk->VBR->FATs; i++) {

        const lba_t fat_position = volume->volume_start + volume->disk->VBR->reserved_sectors + volume->disk->VBR->sectors_per_FAT * i;
        const int32_t sectors_read = disk_read(volume->disk, fat_position, FATs_handler[i], volume->disk->VBR->sectors_per_FAT);

        if (sectors_read != volume->disk->VBR->sectors_per_FAT) {
            errno = ERANGE;
            LOG_ERROR("Couldn't read FATs");
            for (int j = 0; j < volume->disk->VBR->FATs; j++) {
                free(FATs_handler[j]);
            }
            return false;
        }
    }

    //Validate FATs
    for (int i = 1; i < volume->disk->VBR->FATs; i++) {
        if (memcmp(FATs_handler[i - 1], FATs_handler[i], FAT_memory_size) != 0) {
            errno = EINVAL;
            LOG_ERROR("FATs damaged");
            for (int j = 0; j < volume->disk->VBR->FATs; j++) {
                free(FATs_handler[j]);
            }
            return false;
        }
    }

    //Load FAT16 into struct memory
    volume->FAT_mem = (uint16_t*)calloc(1, FAT_memory_size);
    if (!volume->FAT_mem) {
        for (int i = 0; i < volume->disk->VBR->FATs; i++) {
            free(FATs_handler[i]);
        }
        errno = ENOMEM;
        LOG_ERROR("Not enough memory");
        return false;
    }

    memcpy(volume->FAT_mem, FATs_handler[0], FAT_memory_size);

    for (int i = 0; i < volume->disk->VBR->FATs; i++) {
        free(FATs_handler[i]);
    }

    volume->eoc_marker = volume->FAT_mem[1];
    if (volume->eoc_marker < EOC_MARKER_LOW_BOUNDARY) {
        errno = EINVAL;
        LOG_ERROR("EOC damaged");
        free(volume->FAT_mem);
        return false;
    }

    return true;
}


static bool load_root_dir(struct volume_t* const volume) {
    const lba_t root_dir_pos = volume->volume_start + volume->disk->VBR->reserved_sectors + volume->disk->VBR->FATs * volume->disk->VBR->sectors_per_FAT;

    lba_t root_dir_size = (volume->disk->VBR->root_entries * sizeof(Entry_t)) / volume->disk->VBR->bytes_per_sector;
    root_dir_size += ((volume->disk->VBR->root_entries * sizeof(Entry_t)) % volume->disk->VBR->bytes_per_sector) != 0;

    const size_t root_dir_bytes = root_dir_size * volume->disk->VBR->bytes_per_sector;

    volume->root_dir_entries = (Entry_t *)calloc(1, root_dir_bytes);
    if (!volume->root_dir_entries) {
        errno = ENOMEM;
        LOG_ERROR("Not enough memory");
        return false;
    }

    const int32_t read_blocks = disk_read(volume->disk, root_dir_pos, volume->root_dir_entries, root_dir_size);
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

    const lba_t root_dir_pos = volume->volume_start + volume->disk->VBR->reserved_sectors + volume->disk->VBR->FATs * volume->disk->VBR->sectors_per_FAT;
    lba_t root_dir_size = (volume->disk->VBR->root_entries * sizeof(Entry_t)) / volume->disk->VBR->bytes_per_sector;
    root_dir_size += ((volume->disk->VBR->root_entries * sizeof(Entry_t)) % volume->disk->VBR->bytes_per_sector) != 0;

    volume->user_data_pos = root_dir_pos + root_dir_size;
    volume->entries_amount = 0;

    for (int i = 0; i < MAX_ENTRIES_AMOUNT; i++) {
        if ('\x0' == (*(volume->root_dir_entries + i)).filename[0]) {
            volume->entries_amount = i;
            break;
        }
    }
    volume->entries_amount = !volume->entries_amount ? MAX_ENTRIES_AMOUNT : volume->entries_amount;

    return volume;
}


int fat_close(struct volume_t* pvolume) {
    if (!pvolume || !pvolume->FAT_mem || !pvolume->root_dir_entries || !pvolume->disk) {
        errno = EFAULT;
        LOG_ERROR("Null pointer exception");
        return -1;
    }

    free(pvolume->FAT_mem);
    pvolume->FAT_mem = NULL;

    free(pvolume->root_dir_entries);
    pvolume->root_dir_entries = NULL;

    free(pvolume);
    pvolume = NULL;
    return 0;
}

//TODO: FILE SIZE TO CHECK
static bool is_dir(const Entry_t * const entry) {
    return (entry->file_size == 0 && (entry->attributes & DIRECTORY));
}


static void parse_filename(Entry_t const * const entry, char* to) {
    char filename[FILENAME_LEN + 1] = {0};
    memcpy(filename, entry->filename, FILENAME_LEN);

    if (!is_dir(entry)) {
        char extension[EXTENSION_LEN + 1] = {0};
        memcpy(extension, entry->extension, EXTENSION_LEN);
        sprintf(to, "%s.%s", strtok(filename, " "), strtok(extension, " "));
    } else {
        sprintf(to, "%s", strtok(filename, " "));
    }
}


static int find_file(const struct volume_t* const pvolume, const char* const file_name) {

    char buffer[FULL_FILENAME_LEN + 1] = {0};

    for (int i = 0; i < pvolume->entries_amount; i++) {
        if ((pvolume->root_dir_entries[i].attributes & VOLUME_LABEL) || (pvolume->root_dir_entries[i].filename[0] == DELETED)) {
            continue;
        }

        parse_filename(pvolume->root_dir_entries + i, buffer);
        if (strcmp(buffer, file_name) == 0) {
            return i;
        }
    }
    return -1;
}


struct file_t* file_open(struct volume_t* pvolume, const char* file_name) {

    if (!pvolume || !pvolume->disk || !pvolume->FAT_mem || !file_name) {
        errno = EFAULT;
        LOG_ERROR("Null pointer exception");
        return NULL;
    }


    int file_index = find_file(pvolume, file_name);
    if (-1 == file_index) {
        errno = ENOENT;
        LOG_ERROR("No such file")
        return NULL;
    }

    if (is_dir(pvolume->root_dir_entries + file_index)) {
        errno = EISDIR;
        LOG_ERROR("It's not a file, it's dir, my dear");
        return NULL;
    }

    struct file_t *file = (struct file_t *)calloc(sizeof(struct file_t), 1);
    if (!file) {
        errno = ENOMEM;
        LOG_ERROR("Not enough memory");
        return NULL;
    }

    file->entry = pvolume->root_dir_entries + file_index;
    file->offset = 0;
    file->is_open = true;
    file->size = pvolume->root_dir_entries[file_index].file_size;
    file->start_of_chain = ((cluster_t)pvolume->root_dir_entries[file_index].first_cluster_address_high_order << 16) | pvolume->root_dir_entries[file_index].first_cluster_address_low_order;
    file->in_volume = pvolume;
    return file;
}


int file_close(struct file_t* stream) {
    if (!stream) {
        errno = EFAULT;
        LOG_ERROR("Null pointer exception");
        return -1;
    }
    stream->is_open = false;
    free(stream);
    return 0;
}

static cluster_t get_next_cluster(const cluster_t after_cluster, const struct volume_t* const from) {
    if (!from->FAT_mem || after_cluster < 0) return -1;
    if (after_cluster >= EOC_MARKER_LOW_BOUNDARY) return after_cluster;
    return from->FAT_mem[after_cluster];
}


static cluster_t get_physical_address(cluster_t cluster, const struct volume_t * const volume) {
    return volume->user_data_pos + (cluster - 2) * volume->disk->VBR->sectors_per_cluster;
}


size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream) {

    if (!ptr || !stream) {
        errno = EFAULT;
        LOG_ERROR("Null pointer exception");
        return -1;
    }


    lba_t current_cluster = stream->start_of_chain;
    lba_t current_cluster_physical = get_physical_address(current_cluster, stream->in_volume);

    lba_t sector_offset = stream->offset / stream->in_volume->disk->VBR->bytes_per_sector;
    cluster_t cluster_offset = sector_offset / stream->in_volume->disk->VBR->sectors_per_cluster;
    sector_offset %= stream->in_volume->disk->VBR->sectors_per_cluster;

    lba_t byte_offset = stream->offset % stream->in_volume->disk->VBR->bytes_per_sector;

    long remain_to_read = size * nmemb > stream->size - stream->offset ? (long)(stream->size - stream->offset) : (long)(size * nmemb);
    long read_bytes = 0, length;
    char sector_data[SECTOR_SIZE + 1] = {0};

    while (cluster_offset-- != 0) {
        current_cluster = get_next_cluster(current_cluster, stream->in_volume);
        current_cluster_physical = get_physical_address(current_cluster, stream->in_volume);
    }

    while (true) {
        if (stream->size == stream->offset) return 0;

        int read_blocks = disk_read(stream->in_volume->disk, current_cluster_physical + sector_offset, sector_data, 1);
        if (read_blocks != 1) {
            errno = ERANGE;
            LOG_ERROR("Disk read failed");
            return -1;
        }

        bool new_sector = false;
        if (remain_to_read > SECTOR_SIZE) {
            length = SECTOR_SIZE;
            remain_to_read -= length;
            stream->offset += length;
            memcpy( (void*) ((uint8_t *)ptr + read_bytes), sector_data + byte_offset, length);
            sector_offset++;
            read_bytes += length;
        } else {
            int pos_in_sector = (int)stream->offset % SECTOR_SIZE;
            if (remain_to_read > (SECTOR_SIZE - pos_in_sector)) {
                length = SECTOR_SIZE - pos_in_sector;
                remain_to_read -= length;
                sector_offset++;
                new_sector = true;
            } else {
                length = remain_to_read;
                remain_to_read = 0;
            }
            stream->offset += length;
            memcpy( (void*) ((uint8_t *)ptr + read_bytes), sector_data + byte_offset, length);
            read_bytes += length;
        }

        if (new_sector) byte_offset = 0;
        if (remain_to_read > 0) {
            if (sector_offset >= stream->in_volume->disk->VBR->sectors_per_cluster) {
                current_cluster = get_next_cluster(current_cluster, stream->in_volume);
                current_cluster_physical = get_physical_address(current_cluster, stream->in_volume);
                sector_offset = 0;
            }
            continue;
        }

        return read_bytes / size;
    }
}



int32_t file_seek(struct file_t* stream, int32_t offset, int whence) {

    if (!stream) {
        errno = EFAULT;
        LOG_ERROR("Null pointer exception");
        return -1;
    }

    bool upper_bound_exceeded = false;
    bool lower_bound_exceeded = false;

    switch (whence) {
        case SEEK_SET:
            upper_bound_exceeded = offset > (int)stream->size;
            lower_bound_exceeded = offset < 0;
            break;
        case SEEK_CUR:
            upper_bound_exceeded = offset + (int)stream->offset > (int)stream->size;
            lower_bound_exceeded = offset + (int)stream->offset < 0;
            break;
        case SEEK_END:
            upper_bound_exceeded = offset > 0;
            lower_bound_exceeded = abs(offset) > (int)stream->size;
            break;
        default:
            errno = EINVAL;
            LOG_ERROR("Incorrect whence");
            return -1;
    }

    if (upper_bound_exceeded || lower_bound_exceeded) {
        errno = ENXIO;
        LOG_ERROR("Index out of bounds");
        return -1;
    }

    if (whence == SEEK_SET) {
        stream->offset = offset;
    } else if (whence == SEEK_CUR) {
        stream->offset += offset;
    } else {
        stream->offset = stream->size + offset;
    }

    return stream->offset;
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
