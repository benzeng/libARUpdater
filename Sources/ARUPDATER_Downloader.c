/*
    Copyright (C) 2014 Parrot SA

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Parrot nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
    OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/
/**
 * @file ARUPDATER_Downloader.c
 * @brief libARUpdater Downloader c file.
 * @date 23/05/2014
 * @author djavan.bertrand@parrot.com
 **/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <libARSAL/ARSAL_Print.h>
#include <libARSAL/ARSAL_Error.h>
#include <libARUtils/ARUTILS_Http.h>
#include "ARUPDATER_Manager.h"
#include "ARUPDATER_Downloader.h"
#include "ARUPDATER_Utils.h"

/* ***************************************
 *
 *             define :
 *
 *****************************************/
#define ARUPDATER_DOWNLOADER_TAG   "ARUPDATER_Downloader"

#define ARUPDATER_DOWNLOADER_SERVER_URL                    "download.parrot.com"
#define ARUPDATER_DOWNLOADER_BEGIN_URL                     "Drones/"
#define ARUPDATER_DOWNLOADER_PHP_URL                       "/update.php"
#define ARUPDATER_DOWNLOADER_PARAM_MAX_LENGTH              255
#define ARUPDATER_DOWNLOADER_VERSION_BUFFER_MAX_LENGHT     10
#define ARUPDATER_DOWNLOADER_PRODUCT_PARAM                 "?product="
#define ARUPDATER_DOWNLOADER_SERIAL_PARAM                  "&serialNo="
#define ARUPDATER_DOWNLOADER_VERSION_PARAM                 "&version="
#define ARUPDATER_DOWNLOADER_APP_PLATFORM_PARAM            "&platform="
#define ARUPDATER_DOWNLOADER_APP_VERSION_PARAM             "&appVersion="
#define ARUPDATER_DOWNLOADER_VERSION_SEPARATOR             "."
#define ARUPDATER_DOWNLOADER_DOWNLOADED_FILE_PREFIX        "tmp_"
#define ARUPDATER_DOWNLOADER_DOWNLOADED_FILE_SUFFIX        ".tmp"
#define ARUPDATER_DOWNLOADER_SERIAL_DEFAULT_VALUE          "0000"

#define ARUPDATER_DOWNLOADER_PHP_ERROR_OK                       "0"
#define ARUPDATER_DOWNLOADER_PHP_ERROR_UPDATE                   "5"
#define ARUPDATER_DOWNLOADER_PHP_ERROR_APP_VERSION_OUT_TO_DATE  "3"

#define ARUPDATER_DOWNLOADER_CHUNK_SIZE                    255
#define ARUPDATER_DOWNLOADER_MD5_TXT_SIZE                  32
#define ARUPDATER_DOWNLOADER_MD5_HEX_SIZE                  16

#define ARUPDATER_DOWNLOADER_HTTP_HEADER                   "http://"

#define ARUPDATER_DOWNLOADER_ANDROID_PLATFORM_NAME         "Android"
#define ARUPDATER_DOWNLOADER_IOS_PLATFORM_NAME             "iOS"

/* ***************************************
 *
 *             function implementation :
 *
 *****************************************/

eARUPDATER_ERROR ARUPDATER_Downloader_New(ARUPDATER_Manager_t* manager, const char *const rootFolder, ARSAL_MD5_Manager_t *md5Manager, eARUPDATER_Downloader_Platforms appPlatform, const char* const appVersion, ARUPDATER_Downloader_ShouldDownloadPlfCallback_t shouldDownloadCallback, void *downloadArg, ARUPDATER_Downloader_WillDownloadPlfCallback_t willDownloadPlfCallback, void *willDownloadPlfArg, ARUPDATER_Downloader_PlfDownloadProgressCallback_t progressCallback, void *progressArg, ARUPDATER_Downloader_PlfDownloadCompletionCallback_t completionCallback, void *completionArg)
{
    ARUPDATER_Downloader_t *downloader = NULL;
    eARUPDATER_ERROR err = ARUPDATER_OK;
    int i = 0;

    // Check parameters
    if ((manager == NULL) || (rootFolder == NULL) || (md5Manager == NULL) || (appVersion == NULL))
    {
        err = ARUPDATER_ERROR_BAD_PARAMETER;
    }

    if(err == ARUPDATER_OK)
    {
        /* Create the downloader */
        downloader = malloc (sizeof (ARUPDATER_Downloader_t));
        if (downloader == NULL)
        {
            err = ARUPDATER_ERROR_ALLOC;
        }
    }

    if (err == ARUPDATER_OK)
    {
        if (manager->downloader != NULL)
        {
            err = ARUPDATER_ERROR_MANAGER_ALREADY_INITIALIZED;
        }
        else
        {
            manager->downloader = downloader;
        }
    }

    /* Initialize to default values */
    if(err == ARUPDATER_OK)
    {
        int rootFolderLength = strlen(rootFolder) + 1;
        char *slash = strrchr(rootFolder, ARUPDATER_MANAGER_FOLDER_SEPARATOR[0]);
        if ((slash != NULL) && (strcmp(slash, ARUPDATER_MANAGER_FOLDER_SEPARATOR) != 0))
        {
            rootFolderLength += 1;
        }
        downloader->rootFolder = (char*) malloc(rootFolderLength);
        strcpy(downloader->rootFolder, rootFolder);

        if ((slash != NULL) && (strcmp(slash, ARUPDATER_MANAGER_FOLDER_SEPARATOR) != 0))
        {
            strcat(downloader->rootFolder, ARUPDATER_MANAGER_FOLDER_SEPARATOR);
        }

        downloader->appPlatform = appPlatform;
        downloader->appVersion = malloc(strlen(appVersion)+1);
        strcpy(downloader->appVersion, appVersion);

        downloader->md5Manager = md5Manager;

        downloader->downloadArg = downloadArg;
        downloader->willDownloadPlfArg = willDownloadPlfArg;
        downloader->progressArg = progressArg;
        downloader->completionArg = completionArg;
        downloader->productList = NULL;
        downloader->productCount = 0;

        downloader->shouldDownloadCallback = shouldDownloadCallback;
        downloader->willDownloadPlfCallback = willDownloadPlfCallback;
        downloader->plfDownloadProgressCallback = progressCallback;
        downloader->plfDownloadCompletionCallback = completionCallback;

        downloader->isRunning = 0;
        downloader->isCanceled = 0;
        downloader->updateHasBeenChecked = 0;

        downloader->requestConnection = NULL;
        downloader->downloadConnection = NULL;

        downloader->downloadInfos = malloc(sizeof(ARUPDATER_DownloadInformation_t*) * ARDISCOVERY_PRODUCT_MAX);
        if (downloader->downloadInfos == NULL)
        {
            err = ARUPDATER_ERROR_ALLOC;
        }
        else
        {
            for (i = 0; i < ARDISCOVERY_PRODUCT_MAX; i++)
            {
                downloader->downloadInfos[i] = NULL;
            }
        }
        manager->downloader->productList = malloc(sizeof(eARDISCOVERY_PRODUCT) * ARDISCOVERY_PRODUCT_MAX);
        if (manager->downloader->productList == NULL)
        {
            err = ARUPDATER_ERROR_ALLOC;
        }
        else
        {
            manager->downloader->productCount = ARDISCOVERY_PRODUCT_MAX;
            for (i=0; i<ARDISCOVERY_PRODUCT_MAX; i++)
            {
                manager->downloader->productList[i] = i;
            }
        }
    }

    if (err == ARUPDATER_OK)
    {
        int resultSys = ARSAL_Mutex_Init(&manager->downloader->requestLock);

        if (resultSys != 0)
        {
            err = ARUPDATER_ERROR_SYSTEM;
        }
    }

    if (err == ARUPDATER_OK)
    {
        int resultSys = ARSAL_Mutex_Init(&manager->downloader->downloadLock);

        if (resultSys != 0)
        {
            err = ARUPDATER_ERROR_SYSTEM;
        }
    }

    /* delete the downloader if an error occurred */
    if (err != ARUPDATER_OK)
    {
        ARSAL_PRINT (ARSAL_PRINT_ERROR, ARUPDATER_DOWNLOADER_TAG, "error: %s", ARUPDATER_Error_ToString (err));
        ARUPDATER_Downloader_Delete (manager);
    }

    return err;
}


eARUPDATER_ERROR ARUPDATER_Downloader_Delete(ARUPDATER_Manager_t *manager)
{
    eARUPDATER_ERROR error = ARUPDATER_OK;
    if (manager == NULL)
    {
        error = ARUPDATER_ERROR_BAD_PARAMETER;
    }
    else
    {
        if (manager->downloader == NULL)
        {
            error = ARUPDATER_ERROR_MANAGER_NOT_INITIALIZED;
        }
        else
        {
            if (manager->downloader->isRunning != 0)
            {
                error = ARUPDATER_ERROR_THREAD_PROCESSING;
            }
            else
            {
                ARSAL_Mutex_Destroy(&manager->downloader->requestLock);
                ARSAL_Mutex_Destroy(&manager->downloader->downloadLock);

                free(manager->downloader->rootFolder);

                free(manager->downloader->appVersion);

                int product = 0;
                for (product = 0; product < ARDISCOVERY_PRODUCT_MAX; product++)
                {
                    ARUPDATER_DownloadInformation_t *downloadInfo = manager->downloader->downloadInfos[product];
                    if (downloadInfo != NULL)
                    {
                        ARUPDATER_DownloadInformation_Delete(&downloadInfo);
                        manager->downloader->downloadInfos[product] = NULL;
                    }
                }
                free(manager->downloader->downloadInfos);

                if (manager->downloader->productList != NULL)
                {
                    free(manager->downloader->productList);
                    manager->downloader->productList = NULL;
                }

                free(manager->downloader);
                manager->downloader = NULL;
            }
        }
    }

    return error;
}

eARUPDATER_ERROR ARUPDATER_Downloader_SetUpdatesProductList(ARUPDATER_Manager_t *manager, eARDISCOVERY_PRODUCT *productList, int productCount)
{
    eARUPDATER_ERROR error = ARUPDATER_OK;
    int i;

    if (manager == NULL)
    {
        error = ARUPDATER_ERROR_BAD_PARAMETER;
    }

    if (manager->downloader == NULL)
    {
        error = ARUPDATER_ERROR_MANAGER_NOT_INITIALIZED;
    }

    if (ARUPDATER_OK == error)
    {
        if (manager->downloader->productList != NULL)
        {
            free(manager->downloader->productList);
            manager->downloader->productList= NULL;
            manager->downloader->productCount = 0;
        }

        if (productList == NULL)
        {
            manager->downloader->productList = malloc(sizeof(eARDISCOVERY_PRODUCT) * ARDISCOVERY_PRODUCT_MAX);
            if (manager->downloader->productList == NULL)
            {
                error = ARUPDATER_ERROR_ALLOC;
            }
            else
            {
                manager->downloader->productCount = ARDISCOVERY_PRODUCT_MAX;
                for (i=0; i<ARDISCOVERY_PRODUCT_MAX; i++)
                {
                    manager->downloader->productList[i] = i;
                }
            }
        }
        else
        {
            manager->downloader->productList = malloc(sizeof(eARDISCOVERY_PRODUCT) * productCount);
            if (manager->downloader->productList == NULL)
            {
                error = ARUPDATER_ERROR_ALLOC;
            }
            else
            {
                memcpy(manager->downloader->productList, productList, sizeof(eARDISCOVERY_PRODUCT) * productCount);
                manager->downloader->productCount = productCount;
            }
        }
    }

    return error;
}

int ARUPDATER_Downloader_CheckUpdatesSync(ARUPDATER_Manager_t *manager, eARUPDATER_ERROR *err)
{
    eARUPDATER_ERROR error = ARUPDATER_OK;
    int nbUpdatesToDownload = 0;
    if (manager == NULL)
    {
        error = ARUPDATER_ERROR_BAD_PARAMETER;
    }

    if (manager->downloader == NULL)
    {
        error = ARUPDATER_ERROR_MANAGER_NOT_INITIALIZED;
    }

    if (ARUPDATER_OK == error)
    {
        manager->downloader->updateHasBeenChecked = 1;
    }

    int version;
    int edit;
    int ext;
    eARUTILS_ERROR utilsError = ARUTILS_OK;
    char *device = NULL;
    char *deviceFolder = NULL;
    char *existingPlfFilePath = NULL;
    uint32_t dataSize;
    char *dataPtr = NULL;
    char *data;
    ARSAL_Sem_t requestSem;
    char *platform = NULL;

    char *plfFolder = malloc(strlen(manager->downloader->rootFolder) + strlen(ARUPDATER_MANAGER_PLF_FOLDER) + 1);
    strcpy(plfFolder, manager->downloader->rootFolder);
    strcat(plfFolder, ARUPDATER_MANAGER_PLF_FOLDER);

    if (error == ARUPDATER_OK)
    {
        platform = ARUPDATER_Downloader_GetPlatformName(manager->downloader->appPlatform);
        if (platform == NULL)
        {
            error = ARUPDATER_ERROR_DOWNLOADER_PLATFORM_ERROR;
        }
    }

    int productIndex = 0;
    while ((error == ARUPDATER_OK) && (productIndex < manager->downloader->productCount) && (manager->downloader->isCanceled == 0))
    {
        // for each product, check if update is needed
        eARDISCOVERY_PRODUCT product = manager->downloader->productList[productIndex];
        uint16_t productId = ARDISCOVERY_getProductID(product);

        device = malloc(ARUPDATER_MANAGER_DEVICE_STRING_MAX_SIZE);
        snprintf(device, ARUPDATER_MANAGER_DEVICE_STRING_MAX_SIZE, "%04x", productId);
        
        // read the header of the plf file
        deviceFolder = malloc(strlen(plfFolder) + strlen(device) + strlen(ARUPDATER_MANAGER_FOLDER_SEPARATOR) + 1);
        strcpy(deviceFolder, plfFolder);
        strcat(deviceFolder, device);
        strcat(deviceFolder, ARUPDATER_MANAGER_FOLDER_SEPARATOR);

        char *fileName = NULL;
        error = ARUPDATER_Utils_GetPlfInFolder(deviceFolder, &fileName);
        if (error == ARUPDATER_OK)
        {
            // file path = deviceFolder + plfFilename + \0
            existingPlfFilePath = malloc(strlen(deviceFolder) + strlen(fileName) + 1);
            strcpy(existingPlfFilePath, deviceFolder);
            strcat(existingPlfFilePath, fileName);

            error = ARUPDATER_Utils_GetPlfVersion(existingPlfFilePath, &version, &edit, &ext);
        }
        // else if the file does not exist, force to download
        else if (error == ARUPDATER_ERROR_PLF_FILE_NOT_FOUND)
        {
            version = 0;
            edit = 0;
            ext = 0;
            error = ARUPDATER_OK;

            // also check that the directory exists
            FILE *dir = fopen(plfFolder, "r");
            if (dir == NULL)
            {
                mkdir(plfFolder, S_IRWXU);
            }
            else
            {
                fclose(dir);
            }

            dir = fopen(deviceFolder, "r");
            if (dir == NULL)
            {
                mkdir(deviceFolder, S_IRWXU);
            }
            else
            {
                fclose(dir);
            }
        }

        free(fileName);

        // init the request semaphore
        ARSAL_Mutex_Lock(&manager->downloader->requestLock);
        if (error == ARUPDATER_OK)
        {
            int resultSys = ARSAL_Sem_Init(&requestSem, 0, 0);
            if (resultSys != 0)
            {
                error = ARUPDATER_ERROR_SYSTEM;
            }
        }

        // init the connection
        if (error == ARUPDATER_OK)
        {
            manager->downloader->requestConnection = ARUTILS_Http_Connection_New(&requestSem, ARUPDATER_DOWNLOADER_SERVER_URL, 80, HTTPS_PROTOCOL_FALSE, NULL, NULL, &utilsError);
            if (utilsError != ARUTILS_OK)
            {
                ARUTILS_Http_Connection_Delete(&manager->downloader->requestConnection);
                manager->downloader->requestConnection = NULL;
                ARSAL_Sem_Destroy(&requestSem);
                error = ARUPDATER_ERROR_DOWNLOADER_ARUTILS_ERROR;
            }
        }
        ARSAL_Mutex_Unlock(&manager->downloader->requestLock);

        // request the php
        if (error == ARUPDATER_OK)
        {
            char buffer[ARUPDATER_DOWNLOADER_VERSION_BUFFER_MAX_LENGHT];
            // create the url params
            char *params = malloc(ARUPDATER_DOWNLOADER_PARAM_MAX_LENGTH);
            strcpy(params, ARUPDATER_DOWNLOADER_PRODUCT_PARAM);
            strcat(params, device);

            strcat(params, ARUPDATER_DOWNLOADER_SERIAL_PARAM);
            strcat(params, ARUPDATER_DOWNLOADER_SERIAL_DEFAULT_VALUE);

            strcat(params, ARUPDATER_DOWNLOADER_VERSION_PARAM);
            sprintf(buffer,"%i",version);
            strncat(params, buffer, strlen(buffer));
            strcat(params, ARUPDATER_DOWNLOADER_VERSION_SEPARATOR);
            sprintf(buffer,"%i",edit);
            strncat(params, buffer, strlen(buffer));
            strcat(params, ARUPDATER_DOWNLOADER_VERSION_SEPARATOR);
            sprintf(buffer,"%i",ext);
            strncat(params, buffer, strlen(buffer));

            strcat(params, ARUPDATER_DOWNLOADER_APP_PLATFORM_PARAM);
            strcat(params, platform);

            strcat(params, ARUPDATER_DOWNLOADER_APP_VERSION_PARAM);
            strcat(params, manager->downloader->appVersion);

            char *endUrl = malloc(strlen(ARUPDATER_DOWNLOADER_BEGIN_URL) + strlen(device) + strlen(ARUPDATER_DOWNLOADER_PHP_URL) + strlen(params) + 1);
            strcpy(endUrl, ARUPDATER_DOWNLOADER_BEGIN_URL);
            strcat(endUrl, device);
            strcat(endUrl, ARUPDATER_DOWNLOADER_PHP_URL);
            strcat(endUrl, params);
            
            utilsError = ARUTILS_Http_Get_WithBuffer(manager->downloader->requestConnection, endUrl, (uint8_t**)&dataPtr, &dataSize, NULL, NULL);
            if (utilsError != ARUTILS_OK)
            {
                error = ARUPDATER_ERROR_DOWNLOADER_ARUTILS_ERROR;
            }

            ARSAL_Mutex_Lock(&manager->downloader->requestLock);
            if (manager->downloader->requestConnection != NULL)
            {
                ARUTILS_Http_Connection_Delete(&manager->downloader->requestConnection);
                manager->downloader->requestConnection = NULL;
                ARSAL_Sem_Destroy(&requestSem);
            }
            ARSAL_Mutex_Unlock(&manager->downloader->requestLock);

            free(endUrl);
            endUrl = NULL;
            free(params);
            params = NULL;
        }

        // check if data fetch from request is valid
        if (error == ARUPDATER_OK)
        {
            dataPtr = realloc(dataPtr, dataSize + 1);
            if (dataPtr != NULL)
            {
                (dataPtr)[dataSize] = '\0';
                if (strlen(dataPtr) != dataSize)
                {
                    error = ARUPDATER_ERROR_DOWNLOADER_DOWNLOAD;
                }
            }
        }

        // check if plf file need to be updated
        if (error == ARUPDATER_OK)
        {
            data = dataPtr;
            char *result;
            result = strtok(data, "|");

            // if this plf is not up to date
            if(strcmp(result, ARUPDATER_DOWNLOADER_PHP_ERROR_UPDATE) == 0)
            {
                nbUpdatesToDownload++;
                char *downloadUrl = strtok(NULL, "|");
                char *remoteMD5 = strtok(NULL, "|");
                char *remoteSizeStr = strtok(NULL, "|");
                int remoteSize = 0;
                if (remoteSizeStr != NULL)
                {
                    remoteSize = atoi(remoteSizeStr);
                }
                char *remoteVersion = strtok(NULL, "|");

                manager->downloader->downloadInfos[product] = ARUPDATER_DownloadInformation_New(downloadUrl, remoteMD5, remoteVersion, remoteSize, product, &error);
            }
            else if(strcmp(result, ARUPDATER_DOWNLOADER_PHP_ERROR_OK) == 0)
            {
                manager->downloader->downloadInfos[product] = NULL;
            }
            else if(strcmp(result, ARUPDATER_DOWNLOADER_PHP_ERROR_APP_VERSION_OUT_TO_DATE) == 0)
            {
                error = ARUPDATER_ERROR_DOWNLOADER_PHP_APP_OUT_TO_DATE_ERROR;
            }
            else
            {
                error = ARUPDATER_ERROR_DOWNLOADER_PHP_ERROR;
            }
        }

        if (deviceFolder != NULL)
        {
            free(deviceFolder);
            deviceFolder = NULL;
        }
        if (existingPlfFilePath != NULL)
        {
            free(existingPlfFilePath);
            existingPlfFilePath = NULL;
        }
        if (device != NULL)
        {
            free(device);
            device = NULL;
        }
        if (dataPtr != NULL)
        {
            free(dataPtr);
            dataPtr = NULL;
        }

        productIndex++;
    }

    free(plfFolder);
    plfFolder = NULL;

    if (err != NULL)
    {
        *err = error;
    }

    return nbUpdatesToDownload;
}

void* ARUPDATER_Downloader_CheckUpdatesAsync(void *managerArg)
{
    eARUPDATER_ERROR error = ARUPDATER_OK;
    int nbUpdatesToDownload = 0;

    ARUPDATER_Manager_t *manager = NULL;
    if (managerArg != NULL)
    {
        manager = (ARUPDATER_Manager_t*)managerArg;
    }
    else
    {
        error = ARUPDATER_ERROR_BAD_PARAMETER;
    }

    if ((manager == NULL) || (manager->downloader == NULL))
    {
        error = ARUPDATER_ERROR_MANAGER_NOT_INITIALIZED;
    }

    if (ARUPDATER_OK == error)
    {
        nbUpdatesToDownload = ARUPDATER_Downloader_CheckUpdatesSync(manager, &error);
    }

    if ((manager != NULL) && (manager->downloader != NULL) && (manager->downloader->shouldDownloadCallback != NULL))
    {
        manager->downloader->shouldDownloadCallback(manager->downloader->downloadArg, nbUpdatesToDownload, error);
    }

    return (void*)error;
}

void* ARUPDATER_Downloader_ThreadRun(void *managerArg)
{
    eARUPDATER_ERROR error = ARUPDATER_OK;

    ARUPDATER_Manager_t *manager = NULL;
    if (managerArg != NULL)
    {
        manager = (ARUPDATER_Manager_t*)managerArg;
    }
    else
    {
        error = ARUPDATER_ERROR_BAD_PARAMETER;
    }

    if ((manager != NULL) && (manager->downloader != NULL))
    {
        manager->downloader->isRunning = 1;
    }
    else
    {
        error = ARUPDATER_ERROR_MANAGER_NOT_INITIALIZED;
    }

    int shouldDownload = 0;

    if (error == ARUPDATER_OK)
    {
        // if the check has not already been done, do it
        if (manager->downloader->updateHasBeenChecked == 0)
        {
            int nbDownloadsToDo = ARUPDATER_Downloader_CheckUpdatesSync(manager, &error);
            if (nbDownloadsToDo > 0)
            {
                shouldDownload = 1;
            }
        }
        else
        {
            shouldDownload = 1;
        }

    }


    if ((ARUPDATER_OK == error) && shouldDownload != 0)
    {
        eARUTILS_ERROR utilsError = ARUTILS_OK;
        char *device = NULL;
        char *deviceFolder = NULL;
        char *existingPlfFilePath = NULL;
        ARSAL_Sem_t dlSem;

        char *plfFolder = malloc(strlen(manager->downloader->rootFolder) + strlen(ARUPDATER_MANAGER_PLF_FOLDER) + 1);
        strcpy(plfFolder, manager->downloader->rootFolder);
        strcat(plfFolder, ARUPDATER_MANAGER_PLF_FOLDER);

        int productIndex = 0;
        while ((error == ARUPDATER_OK) && (productIndex < manager->downloader->productCount) && (manager->downloader->isCanceled == 0))
        {
            // for each product, check if update is needed
            eARDISCOVERY_PRODUCT product = manager->downloader->productList[productIndex];
            uint16_t productId = ARDISCOVERY_getProductID(product);

            device = malloc(ARUPDATER_MANAGER_DEVICE_STRING_MAX_SIZE);
            snprintf(device, ARUPDATER_MANAGER_DEVICE_STRING_MAX_SIZE, "%04x", productId);

            deviceFolder = malloc(strlen(plfFolder) + strlen(device) + strlen(ARUPDATER_MANAGER_FOLDER_SEPARATOR) + 1);
            strcpy(deviceFolder, plfFolder);
            strcat(deviceFolder, device);
            strcat(deviceFolder, ARUPDATER_MANAGER_FOLDER_SEPARATOR);

            ARUPDATER_DownloadInformation_t *downloadInfo = manager->downloader->downloadInfos[product];
            if (downloadInfo != NULL)
            {
                const char *const downloadUrl = downloadInfo->downloadUrl;
                char *remoteMD5 = downloadInfo->md5Expected;
                char *remoteVersion = downloadInfo->plfVersion;

                if (manager->downloader->willDownloadPlfCallback != NULL)
                {
                    manager->downloader->willDownloadPlfCallback(manager->downloader->completionArg, product, remoteVersion);
                }

                char *downloadEndUrl;
                char *downloadServer;
                char *downloadedFileName = strrchr(downloadUrl, ARUPDATER_MANAGER_FOLDER_SEPARATOR[0]);
                if(downloadedFileName != NULL && strlen(downloadedFileName) > 0)
                {
                    downloadedFileName = &downloadedFileName[1];
                }

                char *downloadedFilePath = malloc(strlen(deviceFolder) + strlen(ARUPDATER_DOWNLOADER_DOWNLOADED_FILE_PREFIX) + strlen(downloadedFileName) + strlen(ARUPDATER_DOWNLOADER_DOWNLOADED_FILE_SUFFIX) + 1);
                strcpy(downloadedFilePath, deviceFolder);
                strcat(downloadedFilePath, ARUPDATER_DOWNLOADER_DOWNLOADED_FILE_PREFIX);
                strcat(downloadedFilePath, downloadedFileName);
                strcat(downloadedFilePath, ARUPDATER_DOWNLOADER_DOWNLOADED_FILE_SUFFIX);

                char *downloadedFinalFilePath = malloc(strlen(deviceFolder) + strlen(downloadedFileName) + 1);
                strcpy(downloadedFinalFilePath, deviceFolder);
                strcat(downloadedFinalFilePath, downloadedFileName);

                // explode the download url into server and endUrl
                if (strncmp(downloadUrl, ARUPDATER_DOWNLOADER_HTTP_HEADER, strlen(ARUPDATER_DOWNLOADER_HTTP_HEADER)) != 0)
                {
                    error = ARUPDATER_ERROR_DOWNLOADER_PHP_ERROR;
                }

                // construct the url
                if (error == ARUPDATER_OK)
                {
                    const char *const urlWithoutHttpHeader = downloadUrl + strlen(ARUPDATER_DOWNLOADER_HTTP_HEADER);
                    const char delimiter = '/';

                    downloadEndUrl = strchr(urlWithoutHttpHeader, delimiter);
                    int serverLength = strlen(urlWithoutHttpHeader) - strlen(downloadEndUrl);
                    downloadServer = malloc(serverLength + 1);
                    strncpy(downloadServer, urlWithoutHttpHeader, serverLength);
                    downloadServer[serverLength] = '\0';
                }

                ARSAL_Mutex_Lock(&manager->downloader->downloadLock);
                // init the request semaphore
                if (error == ARUPDATER_OK)
                {
                    int resultSys = ARSAL_Sem_Init(&dlSem, 0, 0);
                    if (resultSys != 0)
                    {
                        error = ARUPDATER_ERROR_SYSTEM;
                    }
                }

                if (error == ARUPDATER_OK)
                {
                    manager->downloader->downloadConnection = ARUTILS_Http_Connection_New(&dlSem, downloadServer, 80, HTTPS_PROTOCOL_FALSE, NULL, NULL, &utilsError);
                    if (utilsError != ARUTILS_OK)
                    {
                        ARUTILS_Http_Connection_Delete(&manager->downloader->downloadConnection);
                        manager->downloader->downloadConnection = NULL;
                        ARSAL_Sem_Destroy(&dlSem);
                        error = ARUPDATER_ERROR_DOWNLOADER_ARUTILS_ERROR;
                    }
                }
                ARSAL_Mutex_Unlock(&manager->downloader->downloadLock);

                // download the file
                if ((error == ARUPDATER_OK) && (manager->downloader->isCanceled == 0))
                {
                    utilsError = ARUTILS_Http_Get(manager->downloader->downloadConnection, downloadEndUrl, downloadedFilePath, manager->downloader->plfDownloadProgressCallback, manager->downloader->progressArg);
                    if (utilsError != ARUTILS_OK)
                    {
                        error = ARUPDATER_ERROR_DOWNLOADER_ARUTILS_ERROR;
                    }
                }

                ARSAL_Mutex_Lock(&manager->downloader->downloadLock);
                if (manager->downloader->downloadConnection != NULL)
                {
                    ARUTILS_Http_Connection_Delete(&manager->downloader->downloadConnection);
                    manager->downloader->downloadConnection = NULL;
                    ARSAL_Sem_Destroy(&dlSem);
                }
                ARSAL_Mutex_Unlock(&manager->downloader->downloadLock);

                // check md5 match
                if (error == ARUPDATER_OK)
                {
                    eARSAL_ERROR arsalError = ARSAL_MD5_Manager_Check(manager->downloader->md5Manager, downloadedFilePath, remoteMD5);
                    if(ARSAL_OK != arsalError)
                    {
                        // delete the downloaded file if md5 don't match
                        unlink(downloadedFilePath);
                        error = ARUPDATER_ERROR_DOWNLOADER_MD5_DONT_MATCH;
                    }
                }

                if (error == ARUPDATER_OK)
                {
                    // if the existingPlfFilePath was set, a plf was in the folder, so delete it before renaming the file
                    if (existingPlfFilePath != NULL)
                    {
                        unlink(existingPlfFilePath);
                    }
                    if (rename(downloadedFilePath, downloadedFinalFilePath) != 0)
                    {
                        error = ARUPDATER_ERROR_DOWNLOADER_RENAME_FILE;
                    }
                }

                if (downloadServer != NULL)
                {
                    free(downloadServer);
                    downloadServer = NULL;
                }
                if (downloadedFilePath != NULL)
                {
                    free(downloadedFilePath);
                    downloadedFilePath = NULL;
                }
                if (downloadedFinalFilePath != NULL)
                {
                    free(downloadedFinalFilePath);
                    downloadedFinalFilePath = NULL;
                }
            }

            if (deviceFolder != NULL)
            {
                free(deviceFolder);
                deviceFolder = NULL;
            }
            if (existingPlfFilePath != NULL)
            {
                free(existingPlfFilePath);
                existingPlfFilePath = NULL;
            }
            if (device != NULL)
            {
                free(device);
                device = NULL;
            }

            productIndex++;
        }

        if (plfFolder != NULL)
        {
            free(plfFolder);
            plfFolder = NULL;
        }
    }

    // delete the content of the downloadInfos
    if (ARUPDATER_OK == error)
    {
        manager->downloader->updateHasBeenChecked = 0;
        int product = 0;
        for (product = 0; product < ARDISCOVERY_PRODUCT_MAX; product++)
        {
            ARUPDATER_DownloadInformation_t *downloadInfo = manager->downloader->downloadInfos[product];
            if (downloadInfo != NULL)
            {
                ARUPDATER_DownloadInformation_Delete(&downloadInfo);
                manager->downloader->downloadInfos[product] = NULL;
            }
        }
    }


    if (error != ARUPDATER_OK)
    {
        ARSAL_PRINT (ARSAL_PRINT_ERROR, ARUPDATER_DOWNLOADER_TAG, "error: %s", ARUPDATER_Error_ToString (error));
    }

    if ((manager != NULL) && (manager->downloader != NULL))
    {
        manager->downloader->isRunning = 0;
    }

    if (manager->downloader->plfDownloadCompletionCallback != NULL)
    {
        manager->downloader->plfDownloadCompletionCallback(manager->downloader->completionArg, error);
    }

    return (void*)error;
}

eARUPDATER_ERROR ARUPDATER_Downloader_CancelThread(ARUPDATER_Manager_t *manager)
{
    eARUPDATER_ERROR error = ARUPDATER_OK;
    int resultSys = 0;

    if (manager == NULL)
    {
        error = ARUPDATER_ERROR_BAD_PARAMETER;
    }

    if ((error == ARUPDATER_OK) && (manager->downloader == NULL))
    {
        error = ARUPDATER_ERROR_MANAGER_NOT_INITIALIZED;
    }

    if (error == ARUPDATER_OK)
    {
        manager->downloader->isCanceled = 1;

        ARSAL_Mutex_Lock(&manager->downloader->requestLock);
        if (manager->downloader->requestConnection != NULL)
        {
            ARUTILS_Http_Connection_Cancel(manager->downloader->requestConnection);
        }
        ARSAL_Mutex_Unlock(&manager->downloader->requestLock);

        ARSAL_Mutex_Lock(&manager->downloader->downloadLock);
        if (manager->downloader->downloadConnection != NULL)
        {
            ARUTILS_Http_Connection_Cancel(manager->downloader->downloadConnection);
        }
        ARSAL_Mutex_Unlock(&manager->downloader->downloadLock);

        if (resultSys != 0)
        {
            error = ARUPDATER_ERROR_SYSTEM;
        }
    }

    return error;
}

int ARUPDATER_Downloader_ThreadIsRunning(ARUPDATER_Manager_t* manager, eARUPDATER_ERROR *error)
{
    eARUPDATER_ERROR err = ARUPDATER_OK;
    int isRunning = 0;

    if (manager == NULL)
    {
        err = ARUPDATER_ERROR_BAD_PARAMETER;
    }

    if ((err == ARUPDATER_OK) && (manager->downloader == NULL))
    {
        err = ARUPDATER_ERROR_MANAGER_NOT_INITIALIZED;
    }

    if (err == ARUPDATER_OK)
    {
        isRunning = manager->downloader->isRunning;
    }

    if (error != NULL)
    {
        *error = err;
    }

    return isRunning;
}

char *ARUPDATER_Downloader_GetPlatformName(eARUPDATER_Downloader_Platforms platform)
{
    char *toReturn = NULL;
    switch (platform)
    {
        case ARUPDATER_DOWNLOADER_ANDROID_PLATFORM :
            toReturn = ARUPDATER_DOWNLOADER_ANDROID_PLATFORM_NAME;
            break;
        case ARUPDATER_DOWNLOADER_IOS_PLATFORM :
            toReturn = ARUPDATER_DOWNLOADER_IOS_PLATFORM_NAME;
        default:
            break;
    }

    return toReturn;
}


int ARUPDATER_Downloader_GetUpdatesInfoSync(ARUPDATER_Manager_t *manager, eARUPDATER_ERROR *err, ARUPDATER_DownloadInformation_t*** informations)
{
    eARUPDATER_ERROR error = ARUPDATER_OK;
    int nbUpdatesToDownload = 0;
    if (manager == NULL)
    {
        error = ARUPDATER_ERROR_BAD_PARAMETER;
    }
    
    if (manager->downloader == NULL)
    {
        error = ARUPDATER_ERROR_MANAGER_NOT_INITIALIZED;
    }
    
    if (ARUPDATER_OK == error)
    {
        manager->downloader->updateHasBeenChecked = 1;
    }
    int version = 0;
    int edit = 0;
    int ext = 0;
    eARUTILS_ERROR utilsError = ARUTILS_OK;
    char *device = NULL;
    uint32_t dataSize;
    char *dataPtr = NULL;
    char *data;
    ARSAL_Sem_t requestSem;
    char *platform = NULL;
    
    if (error == ARUPDATER_OK)
    {
        platform = ARUPDATER_Downloader_GetPlatformName(manager->downloader->appPlatform);
        if (platform == NULL)
        {
            error = ARUPDATER_ERROR_DOWNLOADER_PLATFORM_ERROR;
        }
    }
    //int product = 0;
    int productIndex = 0;
    while ((error == ARUPDATER_OK) && (productIndex < manager->downloader->productCount))
    {
        // for each product, check if update is needed
        eARDISCOVERY_PRODUCT product = manager->downloader->productList[productIndex];
    //while ((error == ARUPDATER_OK) && (product < ARDISCOVERY_PRODUCT_MAX))
    //{
        // for each product, check if update is needed
        uint16_t productId = ARDISCOVERY_getProductID(product);
        
        device = malloc(ARUPDATER_MANAGER_DEVICE_STRING_MAX_SIZE);
        snprintf(device, ARUPDATER_MANAGER_DEVICE_STRING_MAX_SIZE, "%04x", productId);
        
        // init the request semaphore
        ARSAL_Mutex_Lock(&manager->downloader->requestLock);
        if (error == ARUPDATER_OK)
        {
            int resultSys = ARSAL_Sem_Init(&requestSem, 0, 0);
            if (resultSys != 0)
            {
                error = ARUPDATER_ERROR_SYSTEM;
            }
        }
        // init the connection
        if (error == ARUPDATER_OK)
        {
            manager->downloader->requestConnection = ARUTILS_Http_Connection_New(&requestSem, ARUPDATER_DOWNLOADER_SERVER_URL, 80, HTTPS_PROTOCOL_FALSE, NULL, NULL, &utilsError);
            if (utilsError != ARUTILS_OK)
            {
                ARUTILS_Http_Connection_Delete(&manager->downloader->requestConnection);
                manager->downloader->requestConnection = NULL;
                ARSAL_Sem_Destroy(&requestSem);
                error = ARUPDATER_ERROR_DOWNLOADER_ARUTILS_ERROR;
            }
        }
        ARSAL_Mutex_Unlock(&manager->downloader->requestLock);
        // request the php
        if (error == ARUPDATER_OK)
        {
            char buffer[ARUPDATER_DOWNLOADER_VERSION_BUFFER_MAX_LENGHT];
            // create the url params
            char *params = malloc(ARUPDATER_DOWNLOADER_PARAM_MAX_LENGTH);
            strcpy(params, ARUPDATER_DOWNLOADER_PRODUCT_PARAM);
            strcat(params, device);
            
            strcat(params, ARUPDATER_DOWNLOADER_SERIAL_PARAM);
            strcat(params, ARUPDATER_DOWNLOADER_SERIAL_DEFAULT_VALUE);
            
            strcat(params, ARUPDATER_DOWNLOADER_VERSION_PARAM);
            sprintf(buffer,"%i",version);
            strncat(params, buffer, strlen(buffer));
            strcat(params, ARUPDATER_DOWNLOADER_VERSION_SEPARATOR);
            sprintf(buffer,"%i",edit);
            strncat(params, buffer, strlen(buffer));
            strcat(params, ARUPDATER_DOWNLOADER_VERSION_SEPARATOR);
            sprintf(buffer,"%i",ext);
            strncat(params, buffer, strlen(buffer));
            
            strcat(params, ARUPDATER_DOWNLOADER_APP_PLATFORM_PARAM);
            strcat(params, platform);
            
            strcat(params, ARUPDATER_DOWNLOADER_APP_VERSION_PARAM);
            strcat(params, manager->downloader->appVersion);
            
            char *endUrl = malloc(strlen(ARUPDATER_DOWNLOADER_BEGIN_URL) + strlen(device) + strlen(ARUPDATER_DOWNLOADER_PHP_URL) + strlen(params) + 1);
            strcpy(endUrl, ARUPDATER_DOWNLOADER_BEGIN_URL);
            strcat(endUrl, device);
            strcat(endUrl, ARUPDATER_DOWNLOADER_PHP_URL);
            strcat(endUrl, params);
            ARSAL_PRINT (ARSAL_PRINT_DEBUG, ARUPDATER_DOWNLOADER_TAG, "%s", endUrl);
            utilsError = ARUTILS_Http_Get_WithBuffer(manager->downloader->requestConnection, endUrl, (uint8_t**)&dataPtr, &dataSize, NULL, NULL);
            if (utilsError != ARUTILS_OK)
            {
                ARSAL_PRINT (ARSAL_PRINT_DEBUG, ARUPDATER_DOWNLOADER_TAG, "%d", utilsError);
                error = ARUPDATER_ERROR_DOWNLOADER_ARUTILS_ERROR;
            }
            
            ARSAL_Mutex_Lock(&manager->downloader->requestLock);
            if (manager->downloader->requestConnection != NULL)
            {
                ARUTILS_Http_Connection_Delete(&manager->downloader->requestConnection);
                manager->downloader->requestConnection = NULL;
                ARSAL_Sem_Destroy(&requestSem);
            }
            ARSAL_Mutex_Unlock(&manager->downloader->requestLock);
            free(endUrl);
            endUrl = NULL;
            free(params);
            params = NULL;
        }
        
        // check if data fetch from request is valid
        if (error == ARUPDATER_OK)
        {
            if (dataPtr != NULL)
            {
                (dataPtr)[dataSize] = '\0';
                if (strlen(dataPtr) != dataSize)
                {
                    error = ARUPDATER_ERROR_DOWNLOADER_DOWNLOAD;
                }
            }
        }
        
        // check if plf file need to be updated
        if (error == ARUPDATER_OK)
        {
            data = dataPtr;
            char *result;
            result = strtok(data, "|");
            
            // if this plf is not up to date
            if(strcmp(result, ARUPDATER_DOWNLOADER_PHP_ERROR_UPDATE) == 0)
            {
                nbUpdatesToDownload++;
                char *downloadUrl = strtok(NULL, "|");
                char *remoteMD5 = strtok(NULL, "|");
                char *remoteSizeStr = strtok(NULL, "|");
                int remoteSize = 0;
                if (remoteSizeStr != NULL)
                {
                    remoteSize = atoi(remoteSizeStr);
                }
                char *remoteVersion = strtok(NULL, "|");
                manager->downloader->downloadInfos[productIndex] = ARUPDATER_DownloadInformation_New(downloadUrl, remoteMD5, remoteVersion, remoteSize, product, &error);
            }
            else if(strcmp(result, ARUPDATER_DOWNLOADER_PHP_ERROR_OK) == 0)
            {
                manager->downloader->downloadInfos[productIndex] = NULL;
            }
            else if(strcmp(result, ARUPDATER_DOWNLOADER_PHP_ERROR_APP_VERSION_OUT_TO_DATE) == 0)
            {
                error = ARUPDATER_ERROR_DOWNLOADER_PHP_APP_OUT_TO_DATE_ERROR;
            }
            else
            {
                error = ARUPDATER_ERROR_DOWNLOADER_PHP_ERROR;
            }
        }
        if (device != NULL)
        {
            free(device);
            device = NULL;
        }
        
        productIndex++;
    }
    
    if (err != NULL)
    {
        *err = error;
    }
    *informations = manager->downloader->downloadInfos;
    return  productIndex;
}
