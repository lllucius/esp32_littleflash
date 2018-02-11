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

#include <stdio.h>
#include <unistd.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "extflash.h"
#include "wb_w25q_dual.h"
#include "wb_w25q_dio.h"
#include "wb_w25q_quad.h"
#include "wb_w25q_qio.h"
#include "wb_w25q_qpi.h"

#include "littleflash.h"

extern "C"
{
#include "unity.h"
#include "test_lfs_common.h"
}

#define PIN_SPI_MOSI    GPIO_NUM_23     // PIN 5 - IO0 - DI
#define PIN_SPI_MISO    GPIO_NUM_19     // PIN 2 - IO1 - DO
#define PIN_SPI_WP      GPIO_NUM_22     // PIN 3 - IO2 - /WP
#define PIN_SPI_HD      GPIO_NUM_21     // PIN 7 - IO3 - /HOLD - /RESET
#define PIN_SPI_SCK     GPIO_NUM_18     // PIN 6 - CLK - CLK
#define PIN_SPI_SS      GPIO_NUM_5      // PIN 1 - /CS - /CS

#define MOUNT_POINT "/littleflash"
#define OPENFILES 4

// Set to the partition name if using internal flash,
// else comment to use externa flash
//#define CONFIG_LITTLEFS_PARTITION_LABEL  "littlefs"

static ExtFlash extflash;
static LittleFlash littleflash;

static void test_extflash_setup()
{
#if !defined(CONFIG_LITTLEFS_PARTITION_LABEL)
    ext_flash_config_t ext_cfg =
    {
        .vspi = true,
        .sck_io_num = PIN_SPI_SCK,
        .miso_io_num = PIN_SPI_MISO,
        .mosi_io_num = PIN_SPI_MOSI,
        .ss_io_num = PIN_SPI_SS,
        .hd_io_num = PIN_SPI_HD,
        .wp_io_num = PIN_SPI_WP,
        .speed_mhz = 40,
        .dma_channel = 1,
        .queue_size = 4,
        .max_dma_size = 0,
        .sector_size = 0,
        .capacity = 0,
    };

    TST(extflash.init(&ext_cfg) == ESP_OK, "ExtFlash initialization failed");
#endif
}

static void test_extflash_teardown()
{
#if !defined(CONFIG_LITTLEFS_PARTITION_LABEL)
    extflash.term();     
#endif
}

static void test_littleflash_setup(int openfiles)
{
    const little_flash_config_t little_cfg =
    {
#if !defined(CONFIG_LITTLEFS_PARTITION_LABEL)
        .flash = &extflash,
        .part_label = NULL,
#else
        .flash = NULL,
        .part_label = CONFIG_LITTLEFS_PARTITION_LABEL,
#endif
        .base_path = MOUNT_POINT,
        .open_files = openfiles,
        .auto_format = true,
        .lookahead = 32
    };

    TST(littleflash.init(&little_cfg) == ESP_OK, "LittleFlash initialization failed");
}

static void test_littleflash_teardown()
{
    littleflash.term();
}

static void test_format()
{
#if !defined(CONFIG_LITTLEFS_PARTITION_LABEL)
    test_extflash_setup();
    extflash.erase_sector(0);
    test_extflash_teardown();
#else
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                           ESP_PARTITION_SUBTYPE_ANY,
                                                           CONFIG_LITTLEFS_PARTITION_LABEL);
    TEST_ASSERT_NOT_NULL(part);
    TEST_ASSERT_EQUAL(esp_partition_erase_range(part, 0, SPI_FLASH_SEC_SIZE), ESP_OK);
#endif
}

static void test_setup(int openfiles)
{
    test_extflash_setup();
    test_littleflash_setup(openfiles);
}

static void test_teardown()
{
    test_littleflash_teardown();
    test_extflash_teardown();
}

//
// Tests shamelessly copied from "esp-idf/components/fatfs/test" and modified
//
TEST_CASE(can_format, "can format chip", "[fatfs][wear_levelling]")
{
    test_format();
    test_setup(OPENFILES);
    test_teardown();
}

TEST_CASE(can_create_write, "can create and write file", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_create_file_with_text(MOUNT_POINT "/hello.txt", lfs_test_hello_str);
    test_teardown();
}

TEST_CASE(can_read, "can read file", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_create_file_with_text(MOUNT_POINT "/hello.txt", lfs_test_hello_str);
    test_lfs_read_file(MOUNT_POINT "/hello.txt");
    test_teardown();
}

TEST_CASE(can_open_max, "can open maximum number of files", "[fatfs][wear_levelling]")
{
    int max_files = FOPEN_MAX - 3; /* account for stdin, stdout, stderr */
    test_setup(max_files);
    test_lfs_open_max_files(MOUNT_POINT "/f", max_files);
    test_teardown();
}

TEST_CASE(can_overwrite_append, "overwrite and append file", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_overwrite_append(MOUNT_POINT "/hello.txt");
    test_teardown();
}

TEST_CASE(can_lseek, "can lseek", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_lseek(MOUNT_POINT "/seek.txt");
    test_teardown();
}

TEST_CASE(can_stat, "stat returns correct values", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_stat(MOUNT_POINT "/stat.txt", MOUNT_POINT "");
    test_teardown();
}

TEST_CASE(can_unlink, "unlink removes a file", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_unlink(MOUNT_POINT "/unlink.txt");
    test_teardown();
}

TEST_CASE(can_rename, "rename moves a file", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_rename(MOUNT_POINT "/link");
    test_teardown();
}

TEST_CASE(can_create_remove, "can create and remove directories", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_mkdir_rmdir(MOUNT_POINT "/dir");
    test_teardown();
}

TEST_CASE(can_open_root, "can opendir root directory of FS", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_can_opendir(MOUNT_POINT "");
    test_teardown();
}

TEST_CASE(can_dir, "opendir, readdir, rewinddir, seekdir work as expected", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_opendir_readdir_rewinddir(MOUNT_POINT "/dir");
    test_teardown();
}

TEST_CASE(can_task, "multiple tasks can use same volume", "[fatfs][wear_levelling]")
{
    test_setup(OPENFILES);
    test_lfs_concurrent(MOUNT_POINT "/f");
    test_teardown();
}

TEST_CASE(can_read_write, "write/read speed test", "[fatfs][wear_levelling]")
{
    /* Erase partition before running the test to get consistent results */
    test_format();

    test_setup(OPENFILES);

    const size_t buf_size = 16 * 1024;
    uint32_t* buf = (uint32_t*) calloc(1, buf_size);
    for (size_t i = 0; i < buf_size / 4; ++i) {
        buf[i] = esp_random();
    }
    const size_t file_size = 256 * 1024;
    const char* file = MOUNT_POINT "/256k.bin";

    test_lfs_rw_speed(file, buf, 4 * 1024, file_size, true);
    test_lfs_rw_speed(file, buf, 8 * 1024, file_size, true);
    test_lfs_rw_speed(file, buf, 16 * 1024, file_size, true);

    test_lfs_rw_speed(file, buf, 4 * 1024, file_size, false);
    test_lfs_rw_speed(file, buf, 8 * 1024, file_size, false);
    test_lfs_rw_speed(file, buf, 16 * 1024, file_size, false);

    unlink(file);

    free(buf);
    test_teardown();
}

extern "C" void app_main(void *)
{
    can_format();
    can_create_write();
    can_read();
    can_open_max();
    can_overwrite_append();
    can_lseek();
    can_stat();
    can_unlink();
    can_rename();
    can_create_remove();
    can_open_root();
    can_dir();
    can_task();
    can_read_write();

    printf("All tests done...\n");

    vTaskDelay(portMAX_DELAY);
}
