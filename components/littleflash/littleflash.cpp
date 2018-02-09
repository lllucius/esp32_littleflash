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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/lock.h>

#include "esp_err.h"
#include "esp_log.h"

#include "littleflash.h"

static const char *TAG = "littleflash";

LittleFlash::LittleFlash()
{
    fds = NULL;
    mounted = false;
    registered = false;
}

LittleFlash::~LittleFlash()
{
    term();
}

esp_err_t LittleFlash::init(const little_flash_config_t *config)
{
    ESP_LOGD(TAG, "%s", __func__);

    _lock_init(&lock);

    lfs_cfg = {};

    cfg = *config;

    if (cfg.flash)
    {
        sector_sz = cfg.flash->sector_size();
        block_cnt = cfg.flash->chip_size() / sector_sz;

        lfs_cfg.read  = &external_read;
        lfs_cfg.prog  = &external_prog;
        lfs_cfg.erase = &external_erase;
        lfs_cfg.sync  = &external_sync;
    }
    else
    {
        part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                        ESP_PARTITION_SUBTYPE_ANY,
                                        cfg.part_label);
        if (part == NULL)
        {
            ESP_LOGE(TAG, "Partition '%s' not found", cfg.part_label);
            return ESP_ERR_NOT_FOUND;
        }

        sector_sz = SPI_FLASH_SEC_SIZE;
        block_cnt = part->size / sector_sz;

        lfs_cfg.read  = &internal_read;
        lfs_cfg.prog  = &internal_prog;
        lfs_cfg.erase = &internal_erase;
        lfs_cfg.sync  = &internal_sync;
    }

    lfs_cfg.context     = (void *) this;
    lfs_cfg.read_size   = sector_sz;
    lfs_cfg.prog_size   = sector_sz;
    lfs_cfg.block_size  = sector_sz;
    lfs_cfg.block_count = block_cnt;
    lfs_cfg.lookahead   = cfg.lookahead;

    int err = lfs_mount(&lfs, &lfs_cfg);
    if (err < 0)
    {
        lfs_unmount(&lfs);
        if (!cfg.auto_format)
        {
            return ESP_FAIL;
        }

        lfs = {};
        err = lfs_format(&lfs, &lfs_cfg);
        if (err < 0)
        {
            lfs_unmount(&lfs);
            return ESP_FAIL;
        }

        lfs = {};
        err = lfs_mount(&lfs, &lfs_cfg);
        if (err < 0)
        {
            lfs_unmount(&lfs);
            return ESP_FAIL;
        }
    }
    mounted = true;

    fds = new vfs_fd_t[cfg.open_files];
    if (fds == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < cfg.open_files; i++)
    {
        fds[i].file = NULL;
        fds[i].name = NULL;
    }

    esp_vfs_t vfs = {};

    vfs.flags = ESP_VFS_FLAG_CONTEXT_PTR;
    vfs.write_p = &write_p;
    vfs.lseek_p = &lseek_p;
    vfs.read_p = &read_p;
    vfs.open_p = &open_p;
    vfs.close_p = &close_p;
    vfs.fstat_p = &fstat_p;
    vfs.stat_p = &stat_p;
    vfs.unlink_p = &unlink_p;
    vfs.rename_p = &rename_p;
    vfs.opendir_p = &opendir_p;
    vfs.readdir_p = &readdir_p;
    vfs.readdir_r_p = &readdir_r_p;
    vfs.telldir_p = &telldir_p;
    vfs.seekdir_p = &seekdir_p;
    vfs.closedir_p = &closedir_p;
    vfs.mkdir_p = &mkdir_p;
    vfs.rmdir_p = &rmdir_p;
    vfs.fsync_p = &fsync_p;

    esp_err_t esperr = esp_vfs_register(cfg.base_path, &vfs, this);
    if (esperr != ESP_OK)
    {
        return err;
    }

    registered = true;

    return ESP_OK;
}

void LittleFlash::term()
{
    ESP_LOGD(TAG, "%s", __func__);

    if (registered)
    {
        for (int i = 0; i < cfg.open_files; i++)
        {
            if (fds[i].file)
            {
                close_p(this, i);
                fds[i].file = NULL;
            }
        }

        esp_vfs_unregister(cfg.base_path);
        registered = false;
    }

    if (fds)
    {
        delete [] fds;
        fds = NULL;
    }

    if (mounted)
    {
        lfs_unmount(&lfs);
        mounted = false;
    }

    _lock_close(&lock);
}

// ============================================================================
// ESP32 VFS implementation
// ============================================================================

typedef struct
{
    DIR dir;                // must be first...ESP32 VFS expects it...
    struct dirent dirent;
    lfs_dir_t lfs_dir;
    long off;
} vfs_lfs_dir_t;

int LittleFlash::map_lfs_error(int err)
{
    if (err == LFS_ERR_OK)
    {
        return 0;
    }

    switch (err)
    {
        case LFS_ERR_IO:
            errno = EIO;
        break;
        case LFS_ERR_CORRUPT:
            errno = EIO;
        break;
        case LFS_ERR_NOENT:
            errno = ENOENT;
        break;
        case LFS_ERR_EXIST:
            errno = EEXIST;
        break;
        case LFS_ERR_NOTDIR:
            errno = ENOTDIR;
        break;
        case LFS_ERR_ISDIR:
            errno = EISDIR;
        break;
        case LFS_ERR_NOTEMPTY:
            errno = ENOTEMPTY;
        break;
        case LFS_ERR_INVAL:
            errno = EINVAL;
        break;
        case LFS_ERR_NOSPC:
            errno = ENOSPC;
        break;
        case LFS_ERR_NOMEM:
            errno = ENOMEM;
        break;
        default:
            errno = EINVAL;
        break;
    }

    return -1;
}

int LittleFlash::get_free_fd()
{
    for (int i = 0; i < cfg.open_files; i++)
    {
        if (fds[i].file == NULL)
        {
            return i;
        }
    }

    return -1;
}

ssize_t LittleFlash::write_p(void *ctx, int fd, const void *data, size_t size)
{
    LittleFlash *that = (LittleFlash *) ctx;

    _lock_acquire(&that->lock);

    if (that->fds[fd].file == NULL)
    {
        _lock_release(&that->lock);
        errno = EBADF;
        return -1;
    }

    lfs_ssize_t written = lfs_file_write(&that->lfs, that->fds[fd].file, data, size);

    _lock_release(&that->lock);

    if (written < 0)
    {
        return map_lfs_error(written);
    }

    return written;
}

off_t LittleFlash::lseek_p(void *ctx, int fd, off_t size, int mode)
{
    LittleFlash *that = (LittleFlash *) ctx;

    int lfs_mode = 0;
    if (mode == SEEK_SET)
    {
        lfs_mode = LFS_SEEK_SET;
    }
    else if (mode == SEEK_CUR)
    {
        lfs_mode = LFS_SEEK_CUR;
    }
    else if (mode == SEEK_END)
    {
        lfs_mode = LFS_SEEK_END;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }

    _lock_acquire(&that->lock);

    if (that->fds[fd].file == NULL)
    {
        _lock_release(&that->lock);
        errno = EBADF;
        return -1;
    }

    lfs_soff_t pos = lfs_file_seek(&that->lfs, that->fds[fd].file, size, lfs_mode);

    if (pos >= 0)
    {
        pos = lfs_file_tell(&that->lfs, that->fds[fd].file);
    }

    _lock_release(&that->lock);

    if (pos < 0)
    {
        return map_lfs_error(pos);
    }

    return pos;
}

ssize_t LittleFlash::read_p(void *ctx, int fd, void *dst, size_t size)
{
    LittleFlash *that = (LittleFlash *) ctx;

    _lock_acquire(&that->lock);

    if (that->fds[fd].file == NULL)
    {
        _lock_release(&that->lock);
        errno = EBADF;
        return -1;
    }

    lfs_ssize_t read = lfs_file_read(&that->lfs, that->fds[fd].file, dst, size);

    _lock_release(&that->lock);

    if (read < 0)
    {
        return map_lfs_error(read);
    }

    return read;
}

int LittleFlash::open_p(void *ctx, const char *path, int flags, int mode)
{
    LittleFlash *that = (LittleFlash *) ctx;

    int lfs_flags = 0;
    if ((flags & O_ACCMODE) == O_RDONLY)
    {
        lfs_flags = LFS_O_RDONLY;
    }
    else if ((flags & O_ACCMODE) == O_WRONLY)
    {
        lfs_flags = LFS_O_WRONLY;
    }
    else if ((flags & O_ACCMODE) == O_RDWR)
    {
        lfs_flags = LFS_O_RDWR;
    }

    if (flags & O_CREAT)
    {
        lfs_flags |= LFS_O_CREAT;
    }

    if (flags & O_EXCL)
    {
        lfs_flags |= LFS_O_EXCL;
    }

    if (flags & O_TRUNC)
    {
        lfs_flags |= LFS_O_TRUNC;
    }
    
    if (flags & O_APPEND)
    {
        lfs_flags |= LFS_O_APPEND;
    }

    lfs_file *file = (lfs_file *) malloc(sizeof(lfs_file));
    if (file == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    char *name = strdup(path);
    if (name == NULL)
    {
        free(file);
        errno = ENOMEM;
        return -1;
    }

    _lock_acquire(&that->lock);

    int fd = that->get_free_fd();
    if (fd == -1)
    {
        _lock_release(&that->lock);
        free(name);
        free(file);
        errno = ENFILE;
        return -1;
    }

    int err = lfs_file_open(&that->lfs, file, path, lfs_flags);
    if (err < 0)
    {
        _lock_release(&that->lock);
        free(name);
        free(file);
        return map_lfs_error(err);
    }

    that->fds[fd].file = file;
    that->fds[fd].name = name;

    _lock_release(&that->lock);

    return fd;
}

int LittleFlash::close_p(void *ctx, int fd)
{
    LittleFlash *that = (LittleFlash *) ctx;

    _lock_acquire(&that->lock);

    if (that->fds[fd].file == NULL)
    {
        _lock_release(&that->lock);
        errno = EBADF;
        return -1;
    }

    int err = lfs_file_close(&that->lfs, that->fds[fd].file);

    free(that->fds[fd].name);
    free(that->fds[fd].file);
    that->fds[fd] = {};

    _lock_release(&that->lock);

    return map_lfs_error(err);
}

int LittleFlash::fstat_p(void *ctx, int fd, struct stat *st)
{
    LittleFlash *that = (LittleFlash *) ctx;

    _lock_acquire(&that->lock);

    if (that->fds[fd].file == NULL)
    {
        _lock_release(&that->lock);
        errno = EBADF;
        return -1;
    }

    lfs_info lfs_info;
    int err = lfs_stat(&that->lfs, that->fds[fd].name, &lfs_info);

    _lock_release(&that->lock);

    if (err < 0)
    {
        return map_lfs_error(err);
    }

    *st = {};
    st->st_size = lfs_info.size;
    if (lfs_info.type == LFS_TYPE_DIR)
    {
        st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
    }
    else
    {
        st->st_mode = S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO;
    }

    return 0;
}

int LittleFlash::stat_p(void *ctx, const char *path, struct stat *st)
{
    LittleFlash *that = (LittleFlash *) ctx;

    _lock_acquire(&that->lock);

    struct lfs_info lfs_info;
    int err = lfs_stat(&that->lfs, path, &lfs_info);

    _lock_release(&that->lock);

    if (err < 0)
    {
        return map_lfs_error(err);
    }

    *st = {};
    st->st_size = lfs_info.size;
    if (lfs_info.type == LFS_TYPE_DIR)
    {
        st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
    }
    else
    {
        st->st_mode = S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO;
    }

    return 0;
}

int LittleFlash::unlink_p(void *ctx, const char *path)
{
    LittleFlash *that = (LittleFlash *) ctx;

    _lock_acquire(&that->lock);

    int err = lfs_remove(&that->lfs, path);

    _lock_release(&that->lock);

    return map_lfs_error(err);
}

int LittleFlash::rename_p(void *ctx, const char *src, const char *dst)
{
    LittleFlash *that = (LittleFlash *) ctx;

    _lock_acquire(&that->lock);

    int err = lfs_rename(&that->lfs, src, dst);

    _lock_release(&that->lock);

    return map_lfs_error(err);
}

DIR *LittleFlash::opendir_p(void *ctx, const char *name)
{
    LittleFlash *that = (LittleFlash *) ctx;

    vfs_lfs_dir_t *vfs_dir = (vfs_lfs_dir_t *) malloc(sizeof(vfs_lfs_dir_t));
    if (vfs_dir == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    *vfs_dir = {};

    _lock_acquire(&that->lock);

    int err = lfs_dir_open(&that->lfs, &vfs_dir->lfs_dir, name);

    _lock_release(&that->lock);

    if (err != LFS_ERR_OK)
    {
        free(vfs_dir);
        vfs_dir = NULL;
        map_lfs_error(err);
    }

    return (DIR *) vfs_dir;
}

struct dirent *LittleFlash::readdir_p(void *ctx, DIR *pdir)
{
    vfs_lfs_dir_t *vfs_dir = (vfs_lfs_dir_t *) pdir;
    if (vfs_dir == NULL)
    {
        errno = EBADF;
        return NULL;
    }

    struct dirent *out_dirent = NULL;

    int err = readdir_r_p(ctx, pdir, &vfs_dir->dirent, &out_dirent);
    if (err != 0)
    {
        errno = err;
    }

    return out_dirent;
}

int LittleFlash::readdir_r_p(void *ctx, DIR *pdir, struct dirent *entry, struct dirent **out_dirent)
{
    LittleFlash *that = (LittleFlash *) ctx;

    vfs_lfs_dir_t *vfs_dir = (vfs_lfs_dir_t *) pdir;
    if (vfs_dir == NULL)
    {
        errno = EBADF;
        return errno;
    }

    _lock_acquire(&that->lock);

    struct lfs_info lfs_info;
    int err = lfs_dir_read(&that->lfs, &vfs_dir->lfs_dir, &lfs_info);

    _lock_release(&that->lock);

    if (err == 0)
    {
        *out_dirent = NULL;
        return 0;
    }

    if (err < 0)
    {
        map_lfs_error(err);
        return errno;
    }

    entry->d_ino = 0;
    if (lfs_info.type == LFS_TYPE_REG)
    {
        entry->d_type = DT_REG;
    }
    else if (lfs_info.type == LFS_TYPE_DIR)
    {
        entry->d_type = DT_DIR;
    }
    else
    {
        entry->d_type = DT_UNKNOWN;
    }
    size_t len = strlcpy(entry->d_name, lfs_info.name, sizeof(entry->d_name));

    // This "shouldn't" happen, but the LFS name length can be customized and may
    // be longer than what's provided in "struct dirent"
    if (len >= sizeof(entry->d_name))
    {
        errno = ENAMETOOLONG;
        return errno;
    }

    vfs_dir->off++;

    *out_dirent = entry;

    return 0;
}

long LittleFlash::telldir_p(void *ctx, DIR *pdir)
{
    vfs_lfs_dir_t *vfs_dir = (vfs_lfs_dir_t *) pdir;
    if (vfs_dir == NULL)
    {
        errno = EBADF;
        return errno;
    }

    return vfs_dir->off;
}

void LittleFlash::seekdir_p(void *ctx, DIR *pdir, long offset)
{
    LittleFlash *that = (LittleFlash *) ctx;

    vfs_lfs_dir_t *vfs_dir = (vfs_lfs_dir_t *) pdir;
    if (vfs_dir == NULL)
    {
        errno = EBADF;
        return;
    }

    _lock_acquire(&that->lock);

    // ESP32 VFS expects simple 0 to n counted directory offsets but lfs
    // doesn't so we need to "translate"...
    int err = lfs_dir_rewind(&that->lfs, &vfs_dir->lfs_dir);
    if (err >= 0)
    {
        for (vfs_dir->off = 0; vfs_dir->off < offset; ++vfs_dir->off)
        {
            struct lfs_info lfs_info;
            err = lfs_dir_read(&that->lfs, &vfs_dir->lfs_dir, &lfs_info);
            if (err < 0)
            {
                break;
            }
        }
    }

    _lock_release(&that->lock);

    if (err < 0)
    {
        map_lfs_error(err);
        return;
    }

    return;
}

int LittleFlash::closedir_p(void *ctx, DIR *pdir)
{
    LittleFlash *that = (LittleFlash *) ctx;

    vfs_lfs_dir_t *vfs_dir = (vfs_lfs_dir_t *) pdir;
    if (vfs_dir == NULL)
    {
        errno = EBADF;
        return -1;
    }

    _lock_acquire(&that->lock);

    int err = lfs_dir_close(&that->lfs, &vfs_dir->lfs_dir);

    _lock_release(&that->lock);

    free(vfs_dir);

    return map_lfs_error(err);
}

int LittleFlash::mkdir_p(void *ctx, const char *name, mode_t mode)
{
    LittleFlash *that = (LittleFlash *) ctx;

    _lock_acquire(&that->lock);

    int err = lfs_mkdir(&that->lfs, name);

    _lock_release(&that->lock);

    return map_lfs_error(err);
}

int LittleFlash::rmdir_p(void *ctx, const char *name)
{
    LittleFlash *that = (LittleFlash *) ctx;

    _lock_acquire(&that->lock);

    int err = lfs_remove(&that->lfs, name);

    _lock_release(&that->lock);

    return map_lfs_error(err);
}

int LittleFlash::fsync_p(void *ctx, int fd)
{
    LittleFlash *that = (LittleFlash *) ctx;

    _lock_acquire(&that->lock);

    if (that->fds[fd].file == NULL)
    {
        _lock_release(&that->lock);
        errno = EBADF;
        return -1;
    }

    int err = lfs_file_sync(&that->lfs, that->fds[fd].file);

    _lock_release(&that->lock);

    return map_lfs_error(err);
}

// ============================================================================
// LFS disk interface for external flash
// ============================================================================

int LittleFlash::external_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    ESP_LOGD(TAG, "%s - block=0x%08x off=0x%08x size=%d", __func__, block, off, size);

    LittleFlash *that = (LittleFlash *) c->context;

    esp_err_t err = that->cfg.flash->read((block * that->sector_sz) + off, buffer, size);

    return err == ESP_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

int LittleFlash::external_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    ESP_LOGD(TAG, "%s - block=0x%08x off=0x%08x size=%d", __func__, block, off, size);

    LittleFlash *that = (LittleFlash *) c->context;

    esp_err_t err = that->cfg.flash->write((block * that->sector_sz) + off, buffer, size);

    return err == ESP_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

int LittleFlash::external_erase(const struct lfs_config *c, lfs_block_t block)
{
    ESP_LOGD(TAG, "%s - block=0x%08x", __func__, block);

    LittleFlash *that = (LittleFlash *) c->context;

    esp_err_t err = that->cfg.flash->erase_sector(block);

    return err == ESP_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

int LittleFlash::external_sync(const struct lfs_config *c)
{
    ESP_LOGD(TAG, "%s - c=%p", __func__, c);

    return LFS_ERR_OK;
}

// ============================================================================
// LFS disk interface for internal flash
// ============================================================================

int LittleFlash::internal_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    ESP_LOGD(TAG, "%s - block=0x%08x off=0x%08x size=%d", __func__, block, off, size);

    LittleFlash *that = (LittleFlash *) c->context;

    esp_err_t err = esp_partition_read(that->part, (block * that->sector_sz) + off, buffer, size);

    return err == ESP_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

int LittleFlash::internal_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    ESP_LOGD(TAG, "%s - block=0x%08x off=0x%08x size=%d", __func__, block, off, size);

    LittleFlash *that = (LittleFlash *) c->context;

    esp_err_t err = esp_partition_write(that->part, (block * that->sector_sz) + off, buffer, size);

    return err == ESP_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

int LittleFlash::internal_erase(const struct lfs_config *c, lfs_block_t block)
{
    ESP_LOGD(TAG, "%s - block=0x%08x", __func__, block);

    LittleFlash *that = (LittleFlash *) c->context;

    esp_err_t err = esp_partition_erase_range(that->part, block * that->sector_sz, that->sector_sz);

    return err == ESP_OK ? LFS_ERR_OK : LFS_ERR_IO;
}

int LittleFlash::internal_sync(const struct lfs_config *c)
{
    ESP_LOGD(TAG, "%s", __func__);

    return LFS_ERR_OK;
}

