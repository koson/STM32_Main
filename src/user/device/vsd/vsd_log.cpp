#include "vsd_log.h"
#include "user_main.h"

#define MAX_COUNT 120

VsdLog::VsdLog(int numPort,
               long baudRate,
               int dataBits,
               int stopBits,
               int parity,
               int address)
  : numPort_(numPort)
  , devAdrs_(address)
  , prevConnect_(true)
{
  mms_ = new ModbusMasterSerial();
  // Открываем порт
  mms_->openProtocol(numPort_, baudRate, dataBits, stopBits, parity);

  semaphoreId_ = osSemaphoreCreate(NULL, 1);
}

VsdLog::~VsdLog()
{
  mms_->closeProtocol(numPort_);
  delete mms_;

  osSemaphoreDelete(semaphoreId_);
}

bool VsdLog::isConnect()
{
  bool curConnect = mms_->isConnect();

  if (prevConnect_ && !curConnect) {
#if (USE_LOG_WARNING == 1)
    logDebug.add(WarningMsg, "VsdLog::isConnect() No connection (numPort = %d, devAdrs = %d)", numPort_, devAdrs_);
#endif
  }

  if (!prevConnect_ && curConnect) {
#if (USE_LOG_WARNING == 1)
    logDebug.add(WarningMsg, "VsdLog::isConnect() Connection (numPort = %d, devAdrs = %d)", numPort_, devAdrs_);
#endif
  }

  prevConnect_ = curConnect;
  return curConnect;
}

bool VsdLog::checkAlarm()
{
  return false;
}

bool VsdLog::checkReady()
{
  return false;
}

void VsdLog::resetReady()
{

}

void VsdLog::setAlarm()
{

}

void VsdLog::resetAlarm()
{

}

void VsdLog::readAlarmLog(uint16_t */*ia*/, uint16_t */*ib*/, uint16_t */*ic*/,
                          uint16_t */*ud*/)
{

}

void VsdLog::readRunningLog(uint16_t */*ia*/, uint16_t */*ib*/, uint16_t */*ic*/,
                            uint16_t */*ud*/, uint16_t */*cos*/)
{

}

void VsdLog::writeReg(uint32_t addr, int16_t value)
{
  osSemaphoreWait(semaphoreId_, osWaitForever);

  uint16_t res = 1;
  res = mms_->writeSingleRegister(devAdrs_, addr, value);
  if (res != MODBUS_OK) {
    asm("nop");
  }

  osSemaphoreRelease(semaphoreId_);
}

uint8_t VsdLog::readLog(uint32_t addr, uint16_t *buf, uint32_t size)
{
  osSemaphoreWait(semaphoreId_, osWaitForever);

  uint16_t res = MODBUS_ERROR_TIMEOUT;
  uint16_t address = addr;
  uint32_t count = MAX_COUNT;
  uint32_t readAll = 0;
  while (1) {
    if ((readAll + MAX_COUNT) > size)
      count = size - readAll;
    res = mms_->readMultipleRegisters(devAdrs_, address, &buf[readAll], count);
    if (res != MODBUS_OK) {
      logDebug.add(CriticalMsg, "VsdLog::readLog() Error reading");
      break;
    }
    readAll += count;
    address += count;
    if (readAll >= size)
      break;
  }

  osSemaphoreRelease(semaphoreId_);

  return res;
}

void VsdLog::readNovometLog(uint16_t *ia, uint16_t *ib, uint16_t *ic,
                                 uint16_t *ud)
{
  osSemaphoreWait(semaphoreId_, osWaitForever);

  uint16_t buffer[210] = {0};                             // Буфер с запросом данных
  int16_t field = 1999;                                   // Количество не готовых записей
  uint16_t fieldShift = 25;                               // Смещение от конца архива
  uint16_t fieldCnt = 25;                                 // Количество читаемых записей
  int16_t i = 0;
  int16_t res = 0;
  int16_t val = 0;

  float difCoefCur = 0; //parameters.get(VSD_MAXVAL_CAN_INV_IA);     // Смещение нуля тока
  float propCoefCur = 2047.0; //parameters.get(VSD_MAX_CAN_INV_IA);       // Максимум тока
  if (propCoefCur == 0)
    propCoefCur = 1;
  propCoefCur = (/*parameters.get(VSD_SCALE_CAN_INV_IA)*/545.0 / propCoefCur) * (parameters.get(VSD_I_SCALE) / 100.0);

  float difCoefVolt = parameters.get(VSD_MAXVAL_CAN_INV_UD);     // Смещение нуля напряжения
  float propCoefVolt = parameters.get(VSD_MAX_CAN_INV_UD);
  if (propCoefVolt == 0)
    propCoefVolt = 1;
  propCoefVolt = (parameters.get(VSD_SCALE_CAN_INV_UD) / propCoefVolt) * (parameters.get(VSD_U_SCALE) / 100.0);

  while (field >= 0) {                            // Пока не все записи
    res = mms_->readLogNovomet(devAdrs_, fieldShift, buffer, fieldCnt);
    if (res == 0) {                                       // Нет ответа зануляем то что опрашивали
      logDebug.add(CriticalMsg, "VsdLog::readNovometLog() Error reading");
      while (field >= 0) {                    // Все последующие данные зануляем
        ic[field] = 0;
        ib[field] = 0;
        ia[field] = 0;
        ud[field] = 0;
        field--;
      }
    }
    else {
      if (res == 1) {                                     // Получили сообщение что больше нет данных
        while (field >= 0) {                    // Все последующие данные зануляем
          ic[field] = 0;
          ib[field] = 0;
          ia[field] = 0;
          ud[field] = 0;
          field--;
        }
      }
      else {
        while (i <= res - 1) {
          val = (int)buffer[i];
          ud[field] = (((float)val - difCoefVolt) * propCoefVolt);
          i++;
          val = (int)buffer[i];
          ia[field] = int16_t((val - difCoefCur) * propCoefCur);
          i++;
          val = (int)buffer[i];
          ib[field] = int16_t((val - difCoefCur) * propCoefCur);
          i++;
          val = (int)buffer[i];
          ic[field] = int16_t((val - difCoefCur) * propCoefCur);
          i++;
          field--;
        }
        if ((res / 4) == fieldCnt) {
          fieldCnt = 25;
          fieldShift = fieldShift + fieldCnt;
        }
        else {
          fieldShift = fieldShift + res / 4;
          fieldCnt = fieldCnt - res / 4;
        }
      }
    }
    i = 0;
  }

  osSemaphoreRelease(semaphoreId_);
}
