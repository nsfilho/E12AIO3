/**
 * E12AIO3 Firmware
 * Copyright (C) 2020 E01-AIO Automação Ltda.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author: Nelio Santos <nsfilho@icloud.com>
 * Repository: https://github.com/nsfilho/E12AIO3
 */
#include <stdio.h>
#include <string.h>
#include "spiffs.h"

static const char *TAG = "spiffs.c";
const char *g_basePath = "/v";

/**
 * Prepare spiffs structure do save and load data
 */
void e12aio_spiffs_init()
{
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t l_conf = {
        .base_path = g_basePath,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t l_ret = esp_vfs_spiffs_register(&l_conf);
    if (l_ret != ESP_OK)
    {
        if (l_ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (l_ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(l_ret));
        }
        return;
    }
}

char *e12aio_spiffs_get_basepath()
{
    return g_basePath;
}

bool e12aio_spiffs_has_basepath(char *filename)
{
    return (strlen(filename) > (strlen(g_basePath) + 1) &&
            strncmp(g_basePath, filename, strlen(g_basePath)) == 0 &&
            filename[strlen(g_basePath + 1)] == '/');
}

char *e12aio_spiffs_fullpath(char *filename)
{
    if (!e12aio_spiffs_has_basepath(filename))
        sprintf(filename, "%s/%s", g_basePath, filename);
    return filename;
}

/**
 * @brief This function allows you to write file of any size.
 * Important: Only one file per time (because share a static FILE*)
 * 
 * Sample:
 * 
 * e12aio_spiffs_write("/v/teste.txt")
 * 
 */
size_t e12aio_spiffs_write(char *filename, char *buffer, size_t sz)
{
    static FILE *s_fp = NULL;
    if (s_fp == NULL && filename != NULL)
    {
        // Open file to write
        s_fp = fopen(e12aio_spiffs_fullpath(filename), "w");
    }
    if (buffer == NULL)
    {
        // end write
        fclose(s_fp);
        s_fp = NULL;
    }
    else
    {
        // write to file
        fwrite(url, 1, sz, s_fp);
    }
}

/**
 * @brief Read a file in chunks
 */
size_t e12aio_spiffs_read(char *filename, char *buffer, size_t sz)
{
    static FILE *s_fp = NULL;
    if (s_fp == NULL && filename != NULL)
    {
        s_fp = fopen(e12aio_spiffs_fullpath(filename), "r");
        if (s_fp == NULL)
        {
            memset(buffer, 0, sz);
            ESP_LOGE(TAG, "Failed to open file: %s for reading", filename);
            return 0;
        }
    }
    if (fread(&buffer, 1, sz, s_fp) < sz)
        fclose(l_fp);
}
