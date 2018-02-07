// Copyright 2017-2018 Leland Lucius
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if !defined(_LITTLEFLASH_H_)
#define _LITTLEFLASH_H_ 1

#include <sys/lock.h>

#include "esp_err.h"
#include "esp_vfs.h"
#include "esp_partition.h"

#include "extflash.h"

extern "C"
{
#include "lfs.h"
}

typedef struct
{
    ExtFlash *flash;            // initialized ExtFlash, or NULL for internal flash
    const char *part_label;     // partition label if using internal flash
    const char *base_path;      // mount point
    int open_files;             // number of open files to support
    bool auto_format;           // true=format if not valid
    lfs_size_t lookahead;       // number of LFS lookahead blocks
} little_flash_config_t;

class LittleFlash
{
public:
    LittleFlash();
    virtual ~LittleFlash();

    esp_err_t init(const little_flash_config_t *config);
    void term();

private:
    //
    // VFS interface
    //
    int get_free_fd();

    static int map_lfs_error(int err);

    static ssize_t write_p(void *ctx, int fd, const void *data, size_t size);
    static off_t lseek_p(void *ctx, int fd, off_t size, int mode);
    static ssize_t read_p(void *ctx, int fd, void *dst, size_t size);
    static int open_p(void *ctx, const char *path, int flags, int mode);
    static int close_p(void *ctx, int fd);
    static int fstat_p(void *ctx, int fd, struct stat *st);
    static int stat_p(void *ctx, const char *path, struct stat *st);
    static int link_p(void *ctx, const char *n1, const char *n2);
    static int unlink_p(void *ctx, const char *path);
    static int rename_p(void *ctx, const char *src, const char *dst);
    static DIR *opendir_p(void *ctx, const char *name);
    static struct dirent *readdir_p(void *ctx, DIR *pdir);
    static int readdir_r_p(void *ctx, DIR *pdir, struct dirent *entry, struct dirent **out_dirent);
    static long telldir_p(void *ctx, DIR *pdir);
    static void seekdir_p(void *ctx, DIR *pdir, long offset);
    static int closedir_p(void *ctx, DIR *pdir);
    static int mkdir_p(void *ctx, const char *name, mode_t mode);
    static int rmdir_p(void *ctx, const char *name);
    static int fcntl_p(void *ctx, int fd, int cmd, va_list args);
    static int ioctl_p(void *ctx, int fd, int cmd, va_list args);
    static int fsync_p(void *ctx, int fd);

    //
    // LFS disk interface for external flash
    //
    static int external_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
    static int external_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
    static int external_erase(const struct lfs_config *c, lfs_block_t block);
    static int external_sync(const struct lfs_config *c);

    //
    // LFS disk interface for internal flash
    //
    static int internal_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
    static int internal_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
    static int internal_erase(const struct lfs_config *c, lfs_block_t block);
    static int internal_sync(const struct lfs_config *c);

private:
    struct lfs_config lfs_cfg;

    little_flash_config_t cfg;
    const esp_partition_t *part;

    bool mounted;
    bool registered;

    size_t sector_sz;
    size_t block_cnt;

    _lock_t lock;
    lfs_t lfs;

    typedef struct vfs_fd
    {
        lfs_file *file;
        char *name;
    } vfs_fd_t;

    vfs_fd_t *fds;
};

#endif
