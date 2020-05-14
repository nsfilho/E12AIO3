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
#include <errno.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <mbedtls/base64.h>
#include "config.h"
#include "wifi.h"
#include "httpd.h"

#ifdef CONFIG_COMPONENT_HTTPD
static const char *TAG = "httpd.cpp";
static httpd_handle_t g_server = NULL;

static char **g_userctx = NULL;
static uint8_t g_userctx_len = 0;

void e12aio_httpd_init_task(void *arg)
{
    e12aio_config_wait_load(TAG);
    e12aio_httpd_start();
    vTaskDelete(NULL);
}
#endif

void e12aio_httpd_init()
{
#ifdef CONFIG_COMPONENT_HTTPD
    xTaskCreate(e12aio_httpd_init_task, "http_init", 2048, NULL, 5, NULL);
#endif
}

#ifdef CONFIG_COMPONENT_HTTPD

bool e12aio_httpd_auth_ok(httpd_req_t *req)
{
    size_t l_hdr_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (l_hdr_len == 0)
        return false;

    // TODO: validate authentication
    char l_hdr_buff[l_hdr_len + 1];
    if (httpd_req_get_hdr_value_str(req, "Authorization", l_hdr_buff, l_hdr_len + 1) == ESP_OK)
    {
        char *l_partType = strtok(l_hdr_buff, " ");
        char *l_partAuth64 = strtok(NULL, " ");
        char l_decoded[50];
        size_t l_decodedSize;
        ESP_LOGD(TAG, "Auth Type: %s - [%s]", l_partType, l_partAuth64);
        if (mbedtls_base64_decode((unsigned char *)&l_decoded, 50, &l_decodedSize, (unsigned char *)l_partAuth64, strlen(l_partAuth64)) == 0)
        {
            l_decoded[l_decodedSize] = 0;
            char *l_auth_username = strtok(l_decoded, ":");
            char *l_auth_password = strtok(NULL, ":");
            ESP_LOGD(TAG, "Auth Decoded - User: [%s] - Password: [%s]", l_auth_username, l_auth_password);
            const char *l_config_username = e12aio_config_get()->httpd.username;
            const char *l_config_password = e12aio_config_get()->httpd.password;
            ESP_LOGD(TAG, "Auth Config - User: [%s] - Password: [%s]", l_config_username, l_config_password);
            if (strncasecmp(l_auth_username, l_config_username, strlen(l_config_username)) == 0 &&
                strncasecmp(l_auth_password, l_config_password, strlen(l_config_password)) == 0)
                return true;
        }
    }
    return false;
}

esp_err_t e12aio_httpd_spiffs_handler(httpd_req_t *req)
{
    // Check Authentication first
    if (!e12aio_httpd_auth_ok(req))
    {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Login Form\"");
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // Process request
    size_t readBytes = 0;
    char l_filename[E12AIO_MAX_FILENAME];
    char l_buffer[CONFIG_JSON_BUFFER_SIZE];
    snprintf(l_filename, E12AIO_MAX_FILENAME, "/spiffs/%s", (const char *)req->user_ctx);
    ESP_LOGI(TAG, "Serving page file: %s", l_filename);
    FILE *l_fp = fopen(l_filename, "r");
    if (l_fp == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s for reading", l_filename);
        return ESP_ERR_NOT_FOUND;
    }
    do
    {
        readBytes = fread(&l_buffer, 1, CONFIG_JSON_BUFFER_SIZE, l_fp);
        httpd_resp_send_chunk(req, l_buffer, readBytes);
    } while (readBytes == CONFIG_JSON_BUFFER_SIZE);
    fclose(l_fp);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

char *e12aio_httpd_userctx_add(char *value)
{
    g_userctx_len++;
    g_userctx = realloc(g_userctx, sizeof(char *) * g_userctx_len);
    g_userctx[g_userctx_len - 1] = strdup(value);
    return g_userctx[g_userctx_len - 1];
}

void e12aio_httpd_userctx_clear()
{
    for (; g_userctx_len != 0; g_userctx_len--)
    {
        free(g_userctx[g_userctx_len]);
    }
    free(g_userctx);
    g_userctx = NULL;
}

void e12aio_httpd_start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&g_server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");

        // Open spiffs directory
        DIR *dir = opendir("/spiffs");
        if (dir == NULL)
        {
            ESP_LOGE(TAG, "Fail to open /spiffs directory.");
            e12aio_httpd_stop();
            return;
        }

        // List spiffs files and register as handler
        struct dirent *de;
        while ((de = readdir(dir)) != NULL)
        {
            char l_uri[E12AIO_MAX_FILENAME];
            snprintf(l_uri, E12AIO_MAX_FILENAME, "/%s", de->d_name);
            ESP_LOGD(TAG, "SPIFF File: %s", de->d_name);
            httpd_uri_t l_uri_config = {
                .uri = l_uri,
                .method = HTTP_GET,
                .handler = e12aio_httpd_spiffs_handler,
                .user_ctx = e12aio_httpd_userctx_add(de->d_name),
            };
            httpd_register_uri_handler(g_server, &l_uri_config);
        }
        closedir(dir);

        // Register base (/) handler
        httpd_uri_t l_uri_config = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = e12aio_httpd_spiffs_handler,
            .user_ctx = e12aio_httpd_userctx_add("index.html"),
        };
        httpd_register_uri_handler(g_server, &l_uri_config);
    }
    else
    {
        ESP_LOGI(TAG, "Error starting server!");
    }
}

void e12aio_httpd_stop()
{
    if (g_server != NULL)
    {
        e12aio_httpd_userctx_clear();
        httpd_stop(g_server);
    }
}

#endif