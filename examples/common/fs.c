#include "fs.h"

#include <string.h>

#if DEBUG
#include <io.h>
#include <stdlib.h>
#define PRINT_DBG(_s_) print(_s_);
#define PRINTV_DBG(_s_, _v_) { print(_s_); char b[32]; itoa(_v_, b, 10); print(b); print("\r\n"); }
#else
#define PRINT_DBG(_s_)
#define PRINTV_DBG(_s_, _v_)
#endif

#define FS_PARTITION_BLOCK_ADDR     (64*1024*1024 / SD_BLOCK_LEN)
#define FAT_NB_BLOCKS               ((sizeof(fs_fat_t) + SD_BLOCK_LEN - 1) / SD_BLOCK_LEN)

static bool sd_write(sd_context_t *sd_ctx, uint32_t first_block_addr, const uint8_t *buf, size_t nb_bytes)
{
    size_t remaining_bytes = nb_bytes;
    uint32_t block_addr = first_block_addr;
    uint8_t b[SD_BLOCK_LEN];
    while (remaining_bytes > 0) {
        PRINT_DBG(".");
        size_t s = remaining_bytes > SD_BLOCK_LEN ? SD_BLOCK_LEN : remaining_bytes;
        for (size_t i = 0; i < SD_BLOCK_LEN; ++i)
            b[i] = i < s ? buf[i] : 0xFF;
        if (!sd_write_single_block(sd_ctx, block_addr, b))
            return false;
        remaining_bytes -= s;
        block_addr++;
        buf += s;
    }
    return true;
}

static bool sd_read(sd_context_t *sd_ctx, uint32_t first_block_addr, uint8_t *buf, size_t nb_bytes)
{
    size_t remaining_bytes = nb_bytes;
    uint32_t block_addr = first_block_addr;
    uint8_t b[SD_BLOCK_LEN];
    while (remaining_bytes > 0) {
        PRINT_DBG(".");
        size_t s = remaining_bytes > SD_BLOCK_LEN ? SD_BLOCK_LEN : remaining_bytes;
        if (!sd_read_single_block(sd_ctx, block_addr, b))
            return false;
        for (size_t i = 0; i < s; ++i)
            buf[i] = b[i];
        remaining_bytes -= s;
        block_addr++;
        buf += s;
    }
    return true;
}

static bool read_fat(sd_context_t *sd_ctx, fs_fat_t *fat)
{
    if (!sd_read(sd_ctx, FS_PARTITION_BLOCK_ADDR, (uint8_t *)fat, sizeof(fs_fat_t)))
        return false;
    if (fat->magic[0] != 'F' || fat->magic[1] != 'S')
        return false;
    return true;
}

static bool write_fat(sd_context_t *sd_ctx, const fs_fat_t *fat)
{
    if (!sd_write(sd_ctx, FS_PARTITION_BLOCK_ADDR, (uint8_t *)fat, sizeof(fs_fat_t)))
        return false;
    return true;
}

static fs_file_info_t *find_file(fs_fat_t *fat, const char *filename)
{
    for (size_t i = 0; i < FS_MAX_NB_FILES; ++i) {
        if (strcmp(fat->file_infos[i].name, filename) == 0)
            return &fat->file_infos[i];
    }

    return NULL;
}

static uint16_t find_unused_block_table_index(fs_fat_t *fat, uint16_t start_block_table_index)
{
    uint16_t block_table_index;
    for (block_table_index = start_block_table_index; block_table_index < FS_MAX_NB_BLOCKS; ++block_table_index)
        if (!fat->blocks[block_table_index]) {
            // unused entry found in the table
            // make sure this index is not used in the file info entries
            bool is_available = true;
            for (size_t i = 0; i < FS_MAX_NB_FILES; ++i) {
                if (fat->file_infos[i].name[0] && fat->file_infos[i].first_block_table_index == block_table_index) {
                    // this index is used in the file info table
                    is_available = false;
                    break;
                }
            }
            if (is_available) {
                // unused index found
                break;
            }
        }

    if (block_table_index == FS_MAX_NB_BLOCKS) {
        // No free block available
        return 0;
    }
    return block_table_index;
}

bool fs_format(sd_context_t *sd_ctx)
{
    //
    // clear the FS partition
    //

    uint8_t buf[SD_BLOCK_LEN];
    memset(buf, 0xFF, sizeof(buf));

    uint32_t nb_partition_blocks = FAT_NB_BLOCKS + FS_MAX_NB_BLOCKS;
    
    for (uint32_t i = 0; i < nb_partition_blocks; ++i) {
        PRINT_DBG(".");
        if (!sd_write_single_block(sd_ctx, FS_PARTITION_BLOCK_ADDR + i, buf)) {
            PRINT_DBG("Unable to write single block\r\n");
            return false;
        }
    }

    //
    // write empty FAT
    //

    fs_fat_t fat;
    memset(&fat, 0, sizeof(fat));
    fat.magic[0] = 'F';
    fat.magic[1] = 'S';
    if (!write_fat(sd_ctx, &fat)) {
        PRINT_DBG("Unable to write FAT\r\n");
        return false;
    }
    
    return true;
}

bool fs_init(sd_context_t *sd_ctx, fs_context_t *fs_ctx)
{
    fs_ctx->sd_ctx = sd_ctx;
    if (!read_fat(sd_ctx, &fs_ctx->fat))
        return false;

    return true;
}

uint16_t fs_get_nb_files(fs_context_t *ctx)
{
    uint16_t nb_files = 0;
    for (size_t i = 0; i < FS_MAX_NB_FILES; ++i) {
        if (ctx->fat.file_infos[i].name[0])
            nb_files++;
    }
    return nb_files;
}

bool fs_get_file_info(fs_context_t *ctx, uint16_t file_index, fs_file_info_t *file_info)
{
    uint16_t current_file_index = 0;
    for (size_t i = 0; i < FS_MAX_NB_FILES; ++i) {
        if (ctx->fat.file_infos[i].name[0]) {
            if (current_file_index == file_index) {
                *file_info = ctx->fat.file_infos[i];
                return true;
            }
            current_file_index++;
        }
    }
    return false;
}

bool fs_delete(fs_context_t *ctx, const char *filename)
{
    fs_file_info_t *file_info = find_file(&ctx->fat, filename);
    if (!file_info) {
        PRINT_DBG("File not found\r\n");
        return false;
    }
    
    // erase block entries
    uint16_t block_table_index = file_info->first_block_table_index;
    while (block_table_index) {
        uint16_t next_block_table_index = ctx->fat.blocks[block_table_index];
        ctx->fat.blocks[block_table_index] = 0;
        block_table_index = next_block_table_index;
    }
    file_info->first_block_table_index = 0;

    // clear file info entry
    file_info->name[0] = '\0';

    // write FAT
    if (!write_fat(ctx->sd_ctx, &ctx->fat)) {
        // unable to write FAT
        return false;
    }

    return true;
}

bool fs_read(fs_context_t *ctx, const char *filename, uint8_t *buf, size_t nb_bytes)
{
    fs_file_info_t *file_info = find_file(&ctx->fat, filename);

    if (!file_info) {
        PRINT_DBG("File not found\r\n");
        return false;
    }

    uint32_t first_block_addr = FS_PARTITION_BLOCK_ADDR + FAT_NB_BLOCKS;
    
    size_t remaining_bytes = nb_bytes;
    uint8_t b[SD_BLOCK_LEN];
    uint16_t block_table_index = file_info->first_block_table_index;

    while (block_table_index && remaining_bytes > 0) {
        size_t s = remaining_bytes > SD_BLOCK_LEN ? SD_BLOCK_LEN : remaining_bytes;
        uint32_t block_addr = (uint32_t)block_table_index - 1;
        if (!sd_read_single_block(ctx->sd_ctx, first_block_addr + block_addr, b)) {
            PRINT_DBG("Unable to read block\r\n");
            return false;
        }
        for (size_t i = 0; i < s; ++i)
            buf[i] = b[i];
        remaining_bytes -= s;
        block_table_index = ctx->fat.blocks[block_table_index];
        buf += s;
    }

    if (remaining_bytes > 0) {
        PRINT_DBG("One or more blocks are missing in the FAT for the given number of bytes\r\n");
        return false;
    }

    return true;
}

bool fs_write(fs_context_t *ctx, const char *filename, const uint8_t *buf, size_t nb_bytes)
{
    fs_file_info_t *file_info = find_file(&ctx->fat, filename);

    if (!file_info) {
        // find empty file entry
        uint16_t file_index = 0;
        bool found = false;
        for (file_index = 0; file_index < FS_MAX_NB_FILES; ++file_index) {
            if (!ctx->fat.file_infos[file_index].name[0]) {
                found = true;
                break;
            }
        }
        file_info = &ctx->fat.file_infos[file_index];
    } else {
        PRINT_DBG("File already exists\r\n");
        return false;
    }

    // TODO: unsafe
    strcpy(file_info->name, filename);
    file_info->first_block_table_index = 0;
    file_info->size = 0;

    uint32_t first_block_addr = FS_PARTITION_BLOCK_ADDR + FAT_NB_BLOCKS;
    
    size_t remaining_bytes = nb_bytes;
    uint8_t b[SD_BLOCK_LEN];

    uint16_t last_block_table_index = 0;

    while (remaining_bytes > 0) {
        PRINT_DBG("Write block\r\n");

        size_t s = remaining_bytes > SD_BLOCK_LEN ? SD_BLOCK_LEN : remaining_bytes;

        // find first empty block table index
        uint16_t block_table_index = find_unused_block_table_index(&ctx->fat, last_block_table_index + 1);
        if (!block_table_index) {
            PRINT_DBG("No unused block found\r\n");
            return false;
        }

        // Set the last block table entry
        PRINTV_DBG("last_block_table_index: ", last_block_table_index);
        PRINTV_DBG("block_table_index: ", block_table_index);
        if (last_block_table_index) {
            ctx->fat.blocks[last_block_table_index] = block_table_index;
        } else {
            file_info->first_block_table_index = block_table_index;
        }

        last_block_table_index = block_table_index;

        for (size_t i = 0; i < SD_BLOCK_LEN; ++i)
            b[i] = i < s ? buf[i] : 0xFF;
       
        uint32_t block_addr = (uint32_t)block_table_index - 1;
        if (!sd_write_single_block(ctx->sd_ctx, first_block_addr + block_addr, b)) {
            PRINT_DBG("Unable to write block\r\n");
            return false;
        }

        remaining_bytes -= s;
        buf += s;
        file_info->size += s;
    }

    if (!write_fat(ctx->sd_ctx, &ctx->fat)) {
        PRINT_DBG("Unable to write FAT\r\n");
        return false;
    }

    return true;
}
