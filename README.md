# LittleFS filesystem for the ESP32
LittleFlash is a port of [LittleFS](https://github.com/geky/littlefs) to the ESP32,
providing support for usage on internal and external flash.  External support
utilizes my [ExtFlash](https://github.com/lllucius/esp32_extflash) component.

To clone this repo, use the following to automatically include the extflash
submodule:
```
git clone --recurse-submodules https://github.com/lllucius/esp32_littleflash
```

To use, just add the "extflash" and "littleflash" components to your
components directory and initialize it with something like:

```
#include "littleflash.h"

void app_main()
{
    esp_err_t err;

#if !defined(CONFIG_LITTLEFS_PARTITION_LABEL)
    ExtFlash flash;

    ext_flash_config_t cfg =
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
        .queue_size = 2,
        .sector_size = 0,
        .capacity = 0
    };

    err = flash.init(&cfg);
    if (err != ESP_OK)
    {
        ...
    }
#endif

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

    err = littleflash.init(&little_cfg);
    if (err != ESP_OK)
    {
        ...
    }
    ...
}
```

The configuration options for LittleFlash are:

```
typedef struct
{
    ExtFlash *flash;            // initialized ExtFlash, or NULL for internal flash
    const char *part_label;     // partition label if using internal flash
    const char *base_path;      // mount point
    int open_files;             // number of open files to support
    bool auto_format;           // true=format if not valid
    lfs_size_t lookahead;       // number of LFS lookahead blocks
} little_flash_config_t;
```

More documentation to follow.

