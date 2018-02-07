// Copyright 2015-2017 Espressif Systems (Shanghai) PTE LTD
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

// ============================================================================
// Copied from "/esp32/esp-idf/components/fatfs/test/test_fatfs_common.[ch]"
// ============================================================================

#pragma once

/**
 * @file test_lfs_common.h
 * @brief Common routines for FAT-on-SDMMC and FAT-on-WL tests
 */

#define HEAP_SIZE_CAPTURE(heap_size)  \
     heap_size = esp_get_free_heap_size();

#define HEAP_SIZE_CHECK(heap_size, tolerance) \
    do {\
        size_t final_heap_size = esp_get_free_heap_size(); \
        if (final_heap_size < heap_size - tolerance) { \
            printf("Initial heap size: %d, final: %d, diff=%d\n", heap_size, final_heap_size, heap_size - final_heap_size); \
        } \
    } while(0)


extern const char* lfs_test_hello_str;

void test_lfs_create_file_with_text(const char* name, const char* text);

void test_lfs_overwrite_append(const char* filename);

void test_lfs_read_file(const char* filename);

void test_lfs_open_max_files(const char* filename_prefix, size_t files_count);

void test_lfs_lseek(const char* filename);

void test_lfs_stat(const char* filename, const char* root_dir);

void test_lfs_unlink(const char* filename);

void test_lfs_rename(const char* filename_prefix);

void test_lfs_concurrent(const char* filename_prefix);

void test_lfs_mkdir_rmdir(const char* filename_prefix);

void test_lfs_can_opendir(const char* path);

void test_lfs_opendir_readdir_rewinddir(const char* dir_prefix);

void test_lfs_rw_speed(const char* filename, void* buf, size_t buf_size, size_t file_size, bool write);
