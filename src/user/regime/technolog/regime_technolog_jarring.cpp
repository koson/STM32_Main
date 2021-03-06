#include "regime_technolog_jarring.h"

RegimeTechnologJarring::RegimeTechnologJarring()
{
  delay_ = 0;
}

RegimeTechnologJarring::~RegimeTechnologJarring()
{

}

void RegimeTechnologJarring::processing()
{
  // Получение уставок режима
  action_ = parameters.get(CCS_RGM_JARRING_MODE);
  firstFreq_ = parameters.get(CCS_RGM_JARRING_FREQ_1);
  secondFreq_ = parameters.get(CCS_RGM_JARRING_FREQ_2);
  period_ = parameters.get(CCS_RGM_JARRING_PERIOD);
  countJar_ = parameters.get(CCS_RGM_JARRING_COUNT);
  upTemp_ = parameters.get(CCS_RGM_JARRING_UPTEMP);
  downTemp_ = parameters.get(CCS_RGM_JARRING_DOWNTEMP);

  // Получение текущих параметров режима
  state_ = parameters.get(CCS_RGM_JARRING_STATE);
  beginPeriod_ = parameters.getU32(CCS_RGM_JARRING_TIMER);
//  count_ = parameters.get(CCS_RGM_JARRING_CURRENT_COUNT);



  // Режим выключен
  if ((action_ == OffAction) || ksu.isBreakOrStopMotor()) {              // Выключен режим
    if ((state_ != IdleState) && (state_ != RunningState) && (state_ != PauseState)) {             // Режим выключили во время работы
      #if (USE_LOG_DEBUG == 1)
      logDebug.add(DebugMsg, "Jarring::processing() shutdown in operation (state_=%d, action_=%d)",
                   (int)state_, (int)action_);
      #endif
      state_ = WorkState + 6;               // Возвращаем настройки работы установки
    }
  }

  switch (state_) {
  case IdleState:                                     // Режим не активен
    if ((action_ != OffAction) && ksu.isRunOrWorkMotor() ) {
      #if (USE_LOG_DEBUG == 1)
      logDebug.add(DebugMsg, "Jarring::processing() idle -> running (state_=%d, action_=%d)",
                   (int)state_, (int)action_);
      #endif
      state_ = RunningState;
    }
    break;

  case RunningState:                                  // Активация таймера
    beginPeriod_ = ksu.getTime();                     // Запоминаем время когда начался отсчёт периода встряхивания
    #if (USE_LOG_DEBUG == 1)
    logDebug.add(DebugMsg, "Jarring::processing() running -> pause (state_=%5.0f, beginPeriod_=%d)",
                 (int)state_, (int)beginPeriod_);
    #endif
    state_ = PauseState;
    break;

  case PauseState:
    if (ksu.getSecFromCurTime(beginPeriod_) > period_) {
      state_ = WorkState;                             // Переход в состояние задания частоты 1
#if (USE_LOG_DEBUG == 1)
      logDebug.add(DebugMsg, "Jarring::processing() Pause -> Work (state_=%5.0f, period_=%d)", state_, period_);
#endif
    }
    break;
  case WorkState:                                     // Задание настроек режима встряхивания
    saveBeforeJarring();                              // Сохраняем настройки перед включением режима
    configJarring();                                  // Задаём настройки режима
    state_ = WorkState + 1;
#if (USE_LOG_DEBUG == 1)
    logDebug.add(DebugMsg, "Jarring::processing() Work -> Work + 1 (state_=%5.0f)", state_);
#endif
    break;
  case WorkState + 1:                                 // Задание частоты 1
    ksu.setFreq(firstFreq_, NoneType, false);
    state_ = WorkState + 2;
#if (USE_LOG_DEBUG == 1)
    logDebug.add(DebugMsg, "Jarring::processing() Work + 1 -> Work + 2 (firstFreq_ = %5.0f)", firstFreq_);
#endif
    break;
  case WorkState + 2:                                 // Ожидание перехода на частоту
    if ((vsd->isSetPointFreq()) && (parameters.get(VSD_FREQUENCY_NOW) == firstFreq_)) {
      state_ = WorkState + 3;
      logEvent.add(OtherCode, AutoType, RgmJarringF1Id);
#if (USE_LOG_DEBUG == 1)
      logDebug.add(DebugMsg, "Jarring::processing() Work + 2 -> Work + 3");
#endif
    }
    else {
      delay_++;
      if (delay_ > 30) {
        delay_ = 0;
        state_ = WorkState + 1;
      }
    }
    break;
  case WorkState + 3:                                 // Выход на частоту 2
    ksu.setFreq(secondFreq_, NoneType, false);
    state_ = WorkState + 4;
#if (USE_LOG_DEBUG == 1)
    logDebug.add(DebugMsg, "Jarring::processing() Work + 3 -> Work + 4 (secondFreq_ = %5.0f)", secondFreq_);
#endif
    break;
  case WorkState + 4:                                 // Ожидание перехода на частоту 2
    if ((vsd->isSetPointFreq()) && (parameters.get(VSD_FREQUENCY_NOW) == secondFreq_)) {
      state_ = WorkState + 5;
      logEvent.add(OtherCode, AutoType, RgmJarringF2Id);
#if (USE_LOG_DEBUG == 1)
      logDebug.add(DebugMsg, "Jarring::processing() Work + 4 -> Work + 5");
#endif
    }
    else {
      delay_++;
      if (delay_ > 30) {
        delay_ = 0;
        state_ = WorkState + 3;
      }
    }
    break;
  case WorkState + 5:                                 // Проверка что выполнено нужное количество встряхиваний
    count_++;
    if (count_ >= countJar_) {
      state_ = WorkState + 6;                         // Выход из цикла встряхивания
#if (USE_LOG_DEBUG == 1)
      logDebug.add(DebugMsg, "Jarring::processing() Work + 5 -> Work + 6 (count_ = %5.0f, countJar_ = %5.0f)", count_, countJar_);
#endif
      count_ = 0;
    }
    else {
      state_ = WorkState + 1;
#if (USE_LOG_DEBUG == 1)
      logDebug.add(DebugMsg, "Jarring::processing() Work + 5 -> Work + 1 (count_ = %5.0f, countJar_ = %5.0f)",
                   count_, countJar_);
#endif
    }
    break;
  case WorkState + 6:                                 // Возвращение настроек
    loadAfterJarring();
    #if (USE_LOG_DEBUG == 1)
      logDebug.add(DebugMsg, "Jarring::processing() Work + 6 -> Idle");
    #endif
    state_ = IdleState;
    break;
  default:
    state_ = IdleState;
    break;
  }
  parameters.set(CCS_RGM_JARRING_STATE, state_);
  parameters.set(CCS_RGM_JARRING_TIMER, beginPeriod_);
}

void RegimeTechnologJarring::saveBeforeJarring()
{
  parameters.set(CCS_SAVE_VSD_LOW_LIM_SPEED_MOTOR, parameters.get(VSD_LOW_LIM_SPEED_MOTOR));
  parameters.set(CCS_SAVE_VSD_HIGH_LIM_SPEED_MOTOR, parameters.get(VSD_HIGH_LIM_SPEED_MOTOR));
  parameters.set(CCS_SAVE_VSD_TIMER_DISPERSAL, parameters.get(VSD_TIMER_DISPERSAL));
  parameters.set(CCS_SAVE_VSD_TIMER_DELAY, parameters.get(VSD_TIMER_DELAY));
  parameters.set(CCS_SETPOINT_FREQUENCY, parameters.get(VSD_FREQUENCY));
}

void RegimeTechnologJarring::loadAfterJarring()
{
  vsd->setMinFrequency(parameters.get(CCS_SAVE_VSD_LOW_LIM_SPEED_MOTOR));
  vsd->setMaxFrequency(parameters.get(CCS_SAVE_VSD_HIGH_LIM_SPEED_MOTOR));
  vsd->setTimeSpeedUp(parameters.get(CCS_SAVE_VSD_TIMER_DISPERSAL));
  vsd->setTimeSpeedDown(parameters.get(CCS_SAVE_VSD_TIMER_DELAY));
  ksu.setFreq(parameters.get(CCS_SETPOINT_FREQUENCY),NoneType, false);
}

void RegimeTechnologJarring::configJarring()
{
  vsd->setMinFrequency(min(parameters.get(VSD_HIGH_LIM_SPEED_MOTOR), min(firstFreq_, secondFreq_)));
  // Задание максимума частоты
  vsd->setMaxFrequency(max(parameters.get(VSD_LOW_LIM_SPEED_MOTOR), max(firstFreq_, secondFreq_)));
  // Задание темпа набора частоты
  if (upTemp_)
    vsd->setTimeSpeedUp(parameters.get(CCS_BASE_FREQUENCY) / upTemp_);
  // Задание темпа снижения частот
  if (downTemp_)
    vsd->setTimeSpeedDown(parameters.get(CCS_BASE_FREQUENCY) / downTemp_);
}

