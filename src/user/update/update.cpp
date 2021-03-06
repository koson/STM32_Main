/**
  ******************************************************************************
  * @file    src/command.c
  * @author  MCD Application Team
  * @author  Shilyaev E.V.
  * @version V2.0.0
  * @date    11.09.2014
  * @brief   This file provides all the IAP command functions.
  ******************************************************************************
  */

#include "update.h"
#include "usb_host.h"
#include "ff.h"
#include "user_main.h"

// Bytes
#define BUFFER_SIZE 512*8
// Таймаут обновление мастера - 30 сек
#define UPDATE_MASTER_TIMEOUT 6000

#if USE_EXT_MEM
static volatile uint8_t buffer[BUFFER_SIZE] __attribute__((section(".extmem")));
#else
static volatile uint8_t buffer[BUFFER_SIZE];
#endif

static UPDATE_HEADER updateHeader;
static UPDATE_HEADER updateHeaderOld;

static void getFile(char *fileName)
{
  FRESULT result;
  DIR dir;
  FILINFO fileInfo;
  char *fn;
#if _USE_LFN
  static char lfn[_MAX_LFN + 1];
  fileInfo.lfname = lfn;
  fileInfo.lfsize = sizeof(lfn);
#endif

  result = f_opendir(&dir, "/");
  if (result == FR_OK) {
    while (1) {
      result = f_readdir(&dir, &fileInfo);
      if (result != FR_OK || fileInfo.fname[0] == 0)
        break;
#if _USE_LFN
      fn = *fileInfo.lfname ? fileInfo.lfname : fileInfo.fname;
#else
      fn = fileInfo.fname;
#endif
      if (strstr(fn, SW_FILE_NAME)) {
        if (strstr(fn, ".bin")) {
          strcpy(fileName, fn); // файл прошивки найден
          return;
        }
      }
    }
    logDebug.add(WarningMsg, "update.getFile()  Firmware file not found");
    ksu.setError(NotFoundFwUpdateErr);
  }
  else {
    logDebug.add(WarningMsg, "update.getFile() Failed to open directory");
    ksu.setError(OpenDirUsbErr);
  }
}

static bool saveSwInFlashExt(char *fileName)
{
  UINT readSize = 0x00;
  uint8_t readflag = 1;
  FIL file;
  IMAGE_FILE_HEADER imageHeader;
  bool isSaveSw = false;
  uint16_t calcCrc = 0xFFFF;
  uint16_t calcCrcRx = 0xFFFF;
  uint32_t startAddress;
  uint32_t lastAddress;
  int size = 0;
  uint32_t allSize = 0;

  if (updateHeader.numRegion == 0) {
    updateHeader.numRegion = 1;
    startAddress = AddrUpdateFile2;
  }
  else {
    updateHeader.numRegion = 0;
    startAddress = AddrUpdateFile1;
  }
  lastAddress = startAddress;

  // Открытие файла прошивки и сохранение на внешнию flash
  if (f_open(&file, fileName, FA_READ) == FR_OK) {
    if (file.fsize <= (AddrUpdateFile2 - AddrUpdateFile1)) {
      f_read(&file, &imageHeader, sizeof(imageHeader), &readSize);

      if ((readSize == sizeof(imageHeader)) &&
          (imageHeader.size == file.fsize) &&
          (imageHeader.codeProduction == CODE_PRODUCTION) &&
          (imageHeader.codeEquip == CODE_EQUIP) &&
          (imageHeader.subCodeEquip == SUBCODE_EQUIP)) {
        parameters.set(CCS_PROGRESS_VALUE, 0);
        parameters.set(CCS_PROGRESS_MAX, 0);
        parameters.set(CCS_PROGRESS_MAX, (float)(imageHeader.size + imageHeader.swGuiSize)/1024);

        calcCrc = crc16_ibm((uint8_t*)&imageHeader, readSize);
        flashExtWriteEx(FlashSpi1, lastAddress, (uint8_t*)&imageHeader, readSize);
        flashExtRead(FlashSpi1, lastAddress, (uint8_t*)buffer, readSize);
        calcCrcRx = crc16_ibm((uint8_t*)buffer, readSize, calcCrcRx);
        lastAddress += readSize;
        allSize += readSize;

        int count = 0;
        while (readflag && usbIsReady()) {
          size = file.fsize - allSize - BUFFER_SIZE;
          if ((size > 0) && (size < 6)) {
            size = BUFFER_SIZE-6;
          } else if (size < 0) {
            size = file.fsize - allSize;
            readflag = 0;
          } else {
            size = BUFFER_SIZE;
          }
          f_read(&file, (uint8_t*)buffer, size, &readSize);

          if (readSize < 6) {
            logDebug.add(WarningMsg, "update.saveSwInFlashExt() Error reading firmware file");
            break;
          }

          if (readflag)
            calcCrc = crc16_ibm((uint8_t*)buffer, readSize, calcCrc);
          else
            calcCrc = crc16_ibm((uint8_t*)buffer, readSize-6, calcCrc);
          if (flashExtWriteEx(FlashSpi1, lastAddress, (uint8_t*)buffer, readSize))
            printf("Error: file %s on line %d\r\n", __FILE__, __LINE__);

          if (flashExtRead(FlashSpi1, lastAddress, (uint8_t*)buffer, readSize))
            printf("Error: file %s on line %d\r\n", __FILE__, __LINE__);
          if (readflag)
            calcCrcRx = crc16_ibm((uint8_t*)buffer, readSize, calcCrcRx);
          else
            calcCrcRx = crc16_ibm((uint8_t*)buffer, readSize-6, calcCrcRx);

          if (calcCrc != calcCrcRx)
            break;

          lastAddress += readSize;
          allSize += readSize;

          if (++count > 5) {
            count = 0;
            parameters.set(CCS_PROGRESS_VALUE, (float)(lastAddress - startAddress)/1024);
          }

          if (allSize >= file.fsize)
            break;
        }

        uint16_t crc = (buffer[readSize - 1 - 4] << 8) + buffer[readSize - 2 - 4];
        uint32_t finish = (buffer[readSize - 1] << 24) + (buffer[readSize - 2] << 16) +
            (buffer[readSize - 3] << 8) + (buffer[readSize - 4]);
        if ((calcCrc == crc) && (calcCrcRx == crc) && (finish == 0xFFFFFFFF)) {
          isSaveSw = true;
        }
        else {
          logDebug.add(WarningMsg, "update.saveSwInFlashExt() CRC error in firmware file (crc = %x; calcCrc = %x; calcCrcRx = %x; finish = %x)", crc, calcCrc, calcCrcRx, finish);
          ksu.setError(CrcFwUpdateErr);
        }
      }
      else {
        logDebug.add(WarningMsg, "update.saveSwInFlashExt() Error in anything of firmware file (readSize = %d; size = %d; Production = %d; Equip = %d; %d)",
                     readSize, imageHeader.size, imageHeader.codeProduction,
                     imageHeader.codeEquip, imageHeader.subCodeEquip);
        ksu.setError(HeaderFwUpdateErr);
      }
    } else {
      logDebug.add(WarningMsg, "update.saveSwInFlashExt() Error size of firmware file (file.fsize = %d)", file.fsize);
      ksu.setError(HeaderFwUpdateErr);
    }
    f_close(&file);
  }
  else {
    logDebug.add(WarningMsg, "update.saveSwInFlashExt() Error opening firmware file");
    ksu.setError(OpenFileUsbErr);
  }

  return isSaveSw;
}

bool updateFromUsb()
{
  static char fileName[_MAX_LFN];
  fileName[0] = 0;

  if (!usbIsReady()) {
    logDebug.add(WarningMsg, "update.updateFromUsb() Not connected USB drive");
    ksu.setError(NoConnectionUsbErr);
    return false;
  }

  getFile(fileName);
  if (!strlen(fileName)) {
    return false;
  }

  flashExtRead(FlashSpi1, AddrUpdateHeader, (uint8_t*)&updateHeader, sizeof(UPDATE_HEADER));
  memcpy((uint8_t*)&updateHeaderOld, (uint8_t*)&updateHeader, sizeof(UPDATE_HEADER));

  if (saveSwInFlashExt(fileName)) {
    flashExtWriteEx(FlashSpi1, AddrUpdateHeader, (uint8_t*)&updateHeader, sizeof(UPDATE_HEADER));

    parameters.set(CCS_CMD_UPDATE_SW_MASTER, 0.0);
    parameters.set(CCS_CMD_UPDATE_SW_MASTER, 1.0);
    int timeOut = UPDATE_MASTER_TIMEOUT;
    float oldValue = 0;
    while (1) {
      osDelay(5);
      if (parameters.get(CCS_CMD_UPDATE_SW_MASTER) == 0)
        break;
      if (parameters.get(CCS_CMD_UPDATE_SW_MASTER) == 2) {
        parameters.set(CCS_CMD_UPDATE_SW_MASTER, 0.0);
        flashExtWriteEx(FlashSpi1, AddrUpdateHeader, (uint8_t*)&updateHeaderOld, sizeof(UPDATE_HEADER));
        ksu.setError(SaveFwMasterUpdateErr);
        return false;
      }
      if (--timeOut <= 0) {
        parameters.set(CCS_CMD_UPDATE_SW_MASTER, 0.0);
        flashExtWriteEx(FlashSpi1, AddrUpdateHeader, (uint8_t*)&updateHeaderOld, sizeof(UPDATE_HEADER));
        ksu.setError(TimeoutMasterUpdateErr);
        return false;
      }
      if (parameters.get(CCS_PROGRESS_VALUE) != oldValue)
        timeOut = UPDATE_MASTER_TIMEOUT;
      oldValue = parameters.get(CCS_PROGRESS_VALUE);
    }

    updateHeader.flag = 0x5A;
    updateHeader.type = TypeNewUpdate;
    flashExtWriteEx(FlashSpi1, AddrUpdateHeader, (uint8_t*)&updateHeader, sizeof(UPDATE_HEADER));
    return true;
  }

  return false;
}

StatusType readSwFromFlashExt(uint32_t address, uint8_t *data, uint32_t size)
{
  StatusType status = flashExtRead(FlashSpi1, address, data, size);
  return status;
}

StatusType writeSwInFlashExt(uint32_t address, uint8_t *data, uint32_t size)
{
  StatusType status = flashExtWriteEx(FlashSpi1, address, data, size);
  return status;
}
