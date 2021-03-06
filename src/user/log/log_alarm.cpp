#include "log_alarm.h"
#include "user_main.h"
#include "adc_ext.h"

#include <string.h>

#if USE_EXT_MEM
static uint16_t uValue[ADC_CNANNELS_NUM*ADC_POINTS_NUM] __attribute__((section(".extmem")));
static uint16_t iaValue[ADC_POINTS_NUM] __attribute__((section(".extmem")));
static uint16_t ibValue[ADC_POINTS_NUM] __attribute__((section(".extmem")));
static uint16_t icValue[ADC_POINTS_NUM] __attribute__((section(".extmem")));
static uint16_t udValue[ADC_POINTS_NUM] __attribute__((section(".extmem")));
#else
static uint16_t uValue[ADC_CNANNELS_NUM*ADC_POINTS_NUM];
static uint16_t iaValue[ADC_POINTS_NUM];
static uint16_t ibValue[ADC_POINTS_NUM];
static uint16_t icValue[ADC_POINTS_NUM];
static uint16_t udValue[ADC_POINTS_NUM];
#endif

LogAlarm::LogAlarm()
  : LogRunning(AlarmTypeLog)
{

}

LogAlarm::~LogAlarm()
{

}

void LogAlarm::init()
{
  LogRunning::init("LogAlarm");
}

void LogAlarm::start(uint32_t eventId)
{
  eventId_ = eventId;
  LogRunning::start();
}

int timeTest;

void LogAlarm::task()
{
  osDelay(10);
  while (1) {
    osDelay(1);

    if (vsd->log()) {
      if (vsd->log()->checkAlarm()) {
        add();
      }
    }
  }
}

void LogAlarm::add()
{
  int time = HAL_GetTick();
  eventId_ = logEvent.add(AlarmCode, AutoType, WriteAlarmLogId);

  // Получение значений Ua, Ub, Uc
  copyAdcData(uValue);

  int timeout = HAL_GetTick();
  while (!vsd->log()->checkReady()) {
    osDelay(10);
    if ((HAL_GetTick() - timeout) > 1500) {
      logDebug.add(CriticalMsg, "LogAlarm::add() CheckReady - timeout");
      break;
    }
  }

  uint16_t typeVsd = parameters.get(CCS_TYPE_VSD);

  // Получение значений с ЧРП Ia, Ib, Ic, Ud
  vsd->log()->readAlarmLog(iaValue, ibValue, icValue, udValue);

  memset((uint8_t *)buffer, 0, sizeof(buffer));
  *(uint32_t*)(buffer) = eventId_;
  *(float*)(buffer+4) = parameters.get(CCS_RESISTANCE_ISOLATION);
  write((uint8_t *)buffer, SIZE_BUF_LOG, false);

  int idxU = 0;
  int idxI = 0;
  int idxUd = 0;
  int shiftUd = 0;
  int shiftIa = 0;
  float current = 0;
  float maxCurrent = parameters.get(CCS_SU_MAX_CURRENT);
  int pauseCount = 0;

  for (int i = 0; i < ADC_POINTS_NUM; ) {
    memset((uint8_t *)buffer, 0xFF, sizeof(buffer));
    for (int j = 0; j < 4; ++j) {
      switch (typeVsd) {
      case VSD_TYPE_ETALON:
        if (i >= ADC_POINTS_NUM/2) {
          *(float*)(buffer + j*64) = (int16_t)iaValue[idxI];
          *(float*)(buffer + 4 + j*64) = (int16_t)ibValue[idxI];
          *(float*)(buffer + 8 + j*64) = (int16_t)icValue[idxI];
          *(float*)(buffer + 12 + j*64) = udValue[idxUd];
          shiftUd++;
          if (shiftUd >= 10) {
            shiftUd = 0;
            idxUd++;
          }
          idxI++;
        }
        break;
      case VSD_TYPE_NOVOMET:
        *(float*)(buffer + j*64) = (int16_t)iaValue[idxI];
        *(float*)(buffer + 4 + j*64) = (int16_t)ibValue[idxI];
        *(float*)(buffer + 8 + j*64) = (int16_t)icValue[idxI];
        *(float*)(buffer + 12 + j*64) = udValue[idxI];
        idxI++;
        break;

      case VSD_TYPE_DANFOSS:
        if (shiftIa == 0) {                                           // Если новая точка тока
          current = (float)iaValue[idxI] * 10000.0 / 0xFFF / 493;              // Вычисляем милиА на входе
          if (current < 4.1) {                                       // Граница "разумности" параметра 4100 милиапмер
            current = 0;                                              // Ток ЧРП 0
          }
          else {
            current = (maxCurrent * (current - 4.0)) / 16.0;        // Вычисляем ток ЧРП
          }
        }

        *(float*)(buffer + j*64) = current;                           // Переписываем токи
        *(float*)(buffer + 4 + j*64) = current;
        *(float*)(buffer + 8 + j*64) = current;

        shiftIa++;
        if (shiftIa >= (ADC_POINTS_NUM / ADC_EXT_INPUTS_6_POINTS)) {
          shiftIa = 0;
          idxI++;
        }

        break;
      }

      *(float*)(buffer + 20 + j*64) = ((uValue[0 + idxU] - 2048) * 627.747 * 2.5) / 0xFFF;
      *(float*)(buffer + 24 + j*64) = ((uValue[1 + idxU] - 2048) * 627.747 * 2.5) / 0xFFF;
      *(float*)(buffer + 28 + j*64) = ((uValue[2 + idxU] - 2048) * 627.747 * 2.5) / 0xFFF;
      idxU += 3;
      i++;
    }
    if (i == ADC_POINTS_NUM)
      write((uint8_t *)buffer, SIZE_BUF_LOG, true, true);
    else
      write((uint8_t *)buffer, SIZE_BUF_LOG, false);

    if (pauseCount < 20) {
      pauseCount++;
    }
    else {
      pauseCount = 0;
      osDelay(1);
    }
  }

  time = HAL_GetTick() - time;
  logDebug.add(WarningMsg, "LogAlarm::add() Recording time %d", time);

  vsd->log()->resetReady();
  osDelay(500);
}
