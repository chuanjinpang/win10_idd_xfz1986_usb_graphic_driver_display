// Copyright 2024 Espressif Systems (Shanghai) CO., LTD.
// All rights reserved.

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "image_io.h"

#define MOUNT_POINT  "/sdcard"
#define PIN_NUM_MISO 38
#define PIN_NUM_MOSI 39
#define PIN_NUM_CLK  40
#define PIN_NUM_CS   41

static sdmmc_host_t  host = SDSPI_HOST_DEFAULT();
static sdmmc_card_t *card;
static const char    mount_point[] = MOUNT_POINT;

void mount_sd(void)
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    printf("Initializing SD card\n");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    printf("Using SPI peripheral\n");

    // sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    // host.slot = SPI3_HOST;
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        printf("Failed to initialize bus.\n");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    printf("Mounting filesystem\n");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount filesystem. "
                   "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.\n");
        } else {
            printf("Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.\n", esp_err_to_name(ret));
        }
        return;
    }
    printf("Filesystem mounted\n");

    // Card has been initialized, print its properties
    // sdmmc_card_print_info(stdout, card);

#if 0
    // First create a file.
    const char *file_hello = MOUNT_POINT"/hello.txt";

    printf("Opening file %s\n", file_hello);
    FILE *f = fopen(file_hello, "w");
    if (f == NULL) {
        printf("Failed to open file for writing\n");
        return;
    }
    fprintf(f, "Hello %s!\n", card->cid.name);
    fclose(f);
    printf("File written\n");

    const char *file_foo = MOUNT_POINT"/foo.txt";

    // Check if destination file exists before renaming
    struct stat st;
    if (stat(file_foo, &st) == 0) {
        // Delete it if it exists
        unlink(file_foo);
    }

    // Rename original file
    printf("Renaming file %s to %s\n", file_hello, file_foo);
    if (rename(file_hello, file_foo) != 0) {
        printf("Rename failed\n");
        return;
    }

    // Open renamed file for reading
    printf("Reading file %s\n", file_foo);
    f = fopen(file_foo, "r");
    if (f == NULL) {
        printf("Failed to open file for reading\n");
        return;
    }

    // Read a line from file
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);

    // Strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    printf("Read from file: '%s'\n", line);
#endif  /* 0 */
}

void unmount_sd(void)
{
    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    printf("Card unmounted\n");

    // deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);
}
