<br />
<p align="center">
  <a href="https://github.com/TinyRogue/FAT16-Peruser">
    <img src="fat.png" alt="Logo" width="80" height="80">
  </a>

  <h3 align="center">FAT16-Peruser</h3>

  <p align="center">
  Parses for the file conatiners
    <br />
    <a href="#api-depiction"><strong>Explore the API»</strong></a>
    <br />
    <br />
    ·
    <a href="https://github.com/TinyRogue/FAT16-Peruser/issues">Report Bug</a>
    ·
    <a href="https://github.com/TinyRogue/FAT16-Peruser/issues">Request Feature</a>
  </p>
</p>

<details open="open">
  <summary><h2 style="display: inline-block">Table of Contents</h2></summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
      </ul>
    </li>
    <li><a href="#api-depiction">API Depiction</a></li>
    <li><a href="#contributing">Contributing</a></li>
  </ol>
</details>


## About The Project
The purpose of this project was to built tool for exploring disc with FAT16 file system. Projects consist of read-only features, writing FAT16 files is not supported.


## Getting Started

To get a local copy up and running follow these simple steps.

#### Prerequisites

This is an example of how to list things you need to use the tool.
If You don't have [gcc](http://gcc.gnu.org) installed, get it! =)

1. Clone the repo
   ```sh
   git clone https://github.com/TinyRogue/FAT16-Peruser.git
   ```
2. Compile the project


## API Depiction
```C
struct disk_t* disk_open_from_file(const char* volume_file_name);
```

This function takes filename of file with disc image and opens it.<br/>
__ReturnValue:__ pointer to `struct disk_t`, which is device descriptor or NULL in case of error and sets errno:

EFAULT - volume_file_name is pointer to NULL, ENOENT - no such file, ENOMEM - not enough memory

```C
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
```

This function reads `sectors_to_read` blocks.<br/>
__ReturnValue:__ the amount of read blocks equals to `sectors_to_read` on success. In case of error returns -1 and sets errno to:

EFAULT - invalid buffer/structure pointer, ERANGE - cannot read more blocks from this buffer

```C
int disk_close(struct disk_t* pdisk);
```

This functions closes the disk_t;<br/>
__ReturnValue:__ 0 on success. In case of error returns -1 and sets errno to:

EFAULT - invalid buffer/structure pointer

```C
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
```
This functions opens and checks FAT volume on device beginning at `first_sector`. <br/>
__ReturnValue:__ pointer to `struct volume_t`, which is volume descriptor or NULL in case of error and sets errno:

EFAULT - pdisk is invalid pointer, ENOMEN - not enough memory, EINVAL - volume is corrupted 
```C
int fat_close(struct volume_t* pvolume);
```
This functions closes volume given by pointer and frees structe memory. <br/>
__ReturnValue:__ 0 on success. In case of error returns -1 and sets errno to:

EFAULT - invalid buffer/structure pointer
```C
struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
```
This function does pretty much the same as [fopen()](https://man7.org/linux/man-pages/man3/fopen.3.html) with `rb` flag. <br/>
__ReturnValue:__ pointer to `struct file_t`, which is file descriptor or NULL in case of error and sets errno:

EFAULT - invalid pointer is invalid pointer, ENOMEN - not enough memory, ENOENT - no such file, EISDIR - file_name is not a file (for example: volume or directory)
```C
int file_close(struct file_t* stream);
```
This function, as well as above, does pretty much the same as [fclose()](https://man7.org/linux/man-pages/man3/fclose.3.html).<br/>
__ReturnValue:__ 0 on success. In case of error returns -1 and sets errno to:

EFAULT - invalid buffer/structure pointer
```C
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
```
This function imitates behaviour of [fread()](https://man7.org/linux/man-pages/man3/fread.3.html).<br/>
__ReturnValue:__ 0 on success. In case of error returns -1 and sets errno to:

EFAULT - invalid buffer/structure pointer, ERANGE - attempt to read out of device space, ENXIO - attempt to read out of volume space
```C
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);
```
Implementation of [fseek()](https://man7.org/linux/man-pages/man3/fseek.3.html) function.
__ReturnValue:__ Absolute position of cursor in file or -1 on error and sets errno to:


EFAULT - invalid buffer/structure pointer, EINVAL - wrong whence, ENXIO - this position cannot be set
```C
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
```
Equivalent of `file_open`, yet to use on directories.

```C
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
```
Equivalent of `file_read`, yet to use on directories.
```C
int dir_close(struct dir_t* pdir);
```
Equivalent of `file_close`, yet to use on directories.

## Contributing

Contributions are what make the open source community such an amazing place to be learn, inspire, and create. Any contributions you make are **greatly appreciated**.

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request
