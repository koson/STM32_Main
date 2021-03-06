#include "protection.h"
#include "protection_main.h"

Protection::Protection()
  : resetRestartDelayFlag_(false)
  , alarm_(false)
  , attempt_(false)
  , delay_(false)
  , difStartFlag_(false)
{

}

Protection::~Protection()
{

}

void Protection::processing()
{ 
  getGeneralSetpointProt();                 // Получаем уставки защиты
  getOtherSetpointProt();
  getCurrentParamProt();                    // Получаем текущие параметры защиты
  if (!isModeOff() && isProtect()) {
    alarm_ = checkAlarm();                  // Определяем выполняется ли условие срабатывания защиты
    prevent_ = checkPrevent();              // Определяем есть ли запрещаюший параметр
  }
  else {
    alarm_ = false;
    prevent_ = false;
  }
  delay_ = alarm_ ? delay_ : false;
  checkRestartResetCount();                 // Проверяем и сбрасываем количество АПВ если нужно
  automatProtection();                      // Выполняем шаг автомата защиты
  setCurrentParamProt();                    // Сохраняем текущие параметры защиты
  setOtherParamProt();                      // Сохраняем текущие индивидуальные параметры защиты
}

void Protection::getGeneralSetpointProt()
{
  mode_           = ksu.getValue(idMode_);
  activDelay_     = ksu.getValue(idActivDelay_);
  tripDelay_      = ksu.getValue(idTripDelay_);
  restartDelay_   = ksu.getValue(idRestartDelay_);
  restartLimit_   = ksu.getValue(idRestartLimit_);
  restartResetTime_ = ksu.getValue(idRestartResetTime_);
  tripSetpoint_   = ksu.getValue(idTripSetpoint_);
  restartSetpoint_= ksu.getValue(idRestartSetpoint_);
}

void Protection::getCurrentParamProt()
{
  state_                = ksu.getValue(idState_);
  restart_              = ksu.getValue(idRestartFlag_);
  block_                = ksu.getValue(idBlockFlag_);
  restartCount_         = ksu.getValue(idRestartCount_);
  restartFirstTime_     = ksu.getValueUint32(idRestartFirstTime_);
}

void Protection::setCurrentParamProt()
{
  parameters.set(idState_, state_);
  parameters.set(idRestartFlag_, restart_);
  parameters.set(idBlockFlag_, block_);
  parameters.set(idDelayFlag_, delay_);
  parameters.set(idRestartCount_, restartCount_);
  parameters.set(idRestartFirstTime_, restartFirstTime_);
  parameters.set(idPrevent_, prevent_);
}

void Protection::setOtherParamProt()
{

}

void Protection::getOtherSetpointProt()
{

}

bool Protection::checkAlarm()
{
  return false;
}

bool Protection::isHigherLimit(float setpoint)
{
  valueParameter_ = calcValue();
  // Если текущий параметр больше уставки
  if (valueParameter_ > setpoint)
    return true;
  else
    return false;
}

bool Protection::isLowerLimit(float setpoint)
{
  valueParameter_ = calcValue();
  // Если текущий параметр меньше уставки
  if (valueParameter_ < setpoint)
    return true;
  else
    return false;
}

void Protection::checkRestartResetCount()
{
  // Если разница между текущим временем и временем первого АПВ больше уставки
  // "Время сброса АПВ" и счётчик АПВ не равен 0
  if ((ksu.getSecFromCurTime(restartFirstTime_) >= restartResetTime_) && restartCount_) {
    restartCount_ = 0;                      // Сбрасываем количество АПВ
    restartFirstTime_ = ksu.getTime();          // Фиксируем время сброса
  }
}

bool Protection::checkPrevent()
{
  return false;
}

float Protection::calcValue()
{
  return valueParameter_;
}

void Protection::addEventReactionProt()
{
  logEvent.add(ProtectCode, AutoType, protReactEventId_, valueParameter_, tripSetpoint_,
               parameters.getPhysic(idTripSetpoint_));
}

void Protection::saveValueParameter()
{
  parameters.set(CCS_PROT_VALUE_PARAMETER, valueParameter_);
}

bool Protection::isModeOff()
{
  if (parameters.get(idMode_) == ProtModeOff)
    return true;
  else
    return false;
}

bool Protection::isProtect()
{
  return true;
}

bool Protection::isModeBlock()
{
  if (parameters.get(idMode_) == ProtModeBlock)
    return true;
  else
    return false;
}

bool Protection::isModeRestart()
{
  if (parameters.get(idMode_) == ProtModeRestart)
    return true;
  else
    return false;
}

bool Protection::isModeOn()
{
  if (parameters.get(idMode_) == ProtModeOn)
    return true;
  else
    return false;
}

void Protection::setStateStop()
{
  timer_ = 0;
  state_ = StateStop;
}

void Protection::setStateRun()
{
  timer_ = 0;
  state_ = StateRun;
}

void Protection::processingStateRunning()
{  
  if (ksu.isRunOrWorkMotor()) {
    if (ksu.isAutoMode() || ksu.isManualMode()) {
      if (isModeOff()) {
        setStateStop();
      }
      else if (ksu.getValue(CCS_RUN_TIME) >= activDelay_) {
#if (USE_LOG_DEBUG == 1)
        logDebug.add(DebugMsg, "Protection::processingStateRunning() Activation (idMode = %d)",
                     idMode_);
#endif
        setStateRun();
      }
    }
    else {
      setStateStop();
    }
  }
  else {
    setStateStop();
  }
}

void Protection::processingStateRun()       // Состояние работа
{
  if (ksu.isRunOrWorkMotor()) {                  // Двигатель - работа;
    if (ksu.isAutoMode()) {                 // Двигатель - работа; Режим - авто;
      if (isModeOff()) {                    // Двигатель - работа; Режим - авто; Защита - выкл;
        setStateStop();
      }
      else if (isModeBlock()) {             // Двигатель - работа; Режим - авто; Защита - блок;
        if (alarm_ ) {                      // Двигатель - работа; Режим - авто; Защита - блок; Параметр - не в норме
          if ((timer_ == 0) && tripDelay_) {// Двигатель - работа; Режим - авто; Защита - блок; Параметр - не в норме; Срабатывание - начало;
            timer_ = ksu.getTime();         // Зафиксировали время начала задержки срабатывания
#if (USE_LOG_DEBUG == 1)
            logDebug.add(DebugMsg, "Protection::processingStateRun() Start tripping (idMode = %d, alarm = %d, tripDelay = %d, isModeBlock = %d)",
                         idMode_, alarm_, tripDelay_, isModeBlock());
#endif
            delay_ = true;
          }
          else {
            if (ksu.getSecFromCurTime(timer_) >= tripDelay_) {   // Двигатель - работа; Режим - авто; Защита - блок; Параметр - не в норме; Срабатывание - конец;
#if (USE_LOG_DEBUG == 1)
              logDebug.add(DebugMsg, "Protection::processingStateRun() Trigger -> Block (idMode = %d, alarm = %d, tripDelay = %d, isModeBlock = %d)",
                           idMode_, alarm_, tripDelay_, isModeBlock());
#endif
              addEventReactionProt();
              saveValueParameter();
              logEvent.add(ProtectCode, AutoType, protBlockedEventId_);
              ksu.setBlock();
              ksu.stop(lastReasonStop_);
              block_ = true;
              setStateStop();
            }
          }
        }
        else {                              // Двигатель - работа; Режим - авто; Защита - блок; Параметр - в норме
          setStateRun();
        }
      }
      else if (isModeRestart()) {           // Двигатель - работа; Режим - авто; Защита - АПВ;
        if (alarm_) {                       // Двигатель - работа; Режим - авто; Защита - АПВ; Параметр - не в норме
          if ((timer_ == 0) && tripDelay_) {// Двигатель - работа; Режим - авто; Защита - АПВ; Параметр - не в норме; Срабатывание - начало;
            timer_ = ksu.getTime();         // Зафиксировали время начала задержки срабатывания
#if (USE_LOG_DEBUG == 1)
            logDebug.add(DebugMsg, "Protection::processingStateRun() Start tripping (idMode = %d, alarm = %d, tripDelay = %d, isModeRestart = %d)",
                         idMode_, alarm_, tripDelay_, isModeRestart());
#endif
            delay_ = true;
          }
          else if (ksu.getSecFromCurTime(timer_) >= tripDelay_) { // Двигатель - работа; Режим - авто; Защита - АПВ; Параметр - не в норме; Срабатывание - конец;
            if (restartCount_ >= restartLimit_) {
#if (USE_LOG_DEBUG == 1)
              logDebug.add(DebugMsg, "Protection::processingStateRun() Lock in the number of AR (idMode = %d, tripDelay = %d, restartCount = %d, restartLimit = %d)",
                           idMode_, tripDelay_, restartCount_, restartLimit_);
#endif
              addEventReactionProt();
              saveValueParameter();
              logEvent.add(ProtectCode, AutoType, protBlockedEventId_);
              ksu.setBlock();
              ksu.stop(lastReasonStop_);
              block_ = true;
              setStateStop();
            }
            else {
#if (USE_LOG_DEBUG == 1)
              logDebug.add(DebugMsg, "Protection::processingStateRun() Trigger -> AR (idMode = %d, alarm = %d, tripDelay = %d, isModeRestart = %d)",
                           idMode_, alarm_, tripDelay_, isModeRestart());
#endif
              addEventReactionProt();
              saveValueParameter();
              parameters.set(CCS_RESTART_COUNT, restartCount_);
              ksu.setRestart();
              ksu.stop(lastReasonStop_);
              restart_ = true;
              state_ = StateStopping;
            }
          }
        }
        else {                              // Двигатель - работа; Режим - авто; Защита - АПВ; Параметр - в норме
          setStateRun();
        }
      }
      else if (isModeOn()) {                 // Двигатель - работа; Режим - авто; Защита - Вкл;
        if (alarm_ ) {                       // Двигатель - работа; Режим - авто; Защита - Вкл; Параметр - не в норме
          if ((timer_ == 0) && tripDelay_) { // Двигатель - работа; Режим - авто; Защита - Вкл; Параметр - не в норме; Срабатывание - начало;
            timer_ = ksu.getTime();          // Зафиксировали время начала задержки срабатывания
#if (USE_LOG_DEBUG == 1)
            logDebug.add(DebugMsg, "Protection::processingStateRun() Start tripping (idMode = %d, alarm = %d, tripDelay = %d, isModeOn = %d)",
                         idMode_, alarm_, tripDelay_, isModeOn());
#endif
            delay_ = true;
          }
          else if (timer_ >= tripDelay_) {  // Двигатель - работа; Режим - авто; Защита - Вкл; Параметр - не в норме; Срабатывание - конец;
#if (USE_LOG_DEBUG == 1)
            logDebug.add(DebugMsg, "Protection::processingStateRun() Trigger --> Stop (idMode = %d, alarm = %d, tripDelay = %d)",
                         idMode_, alarm_, tripDelay_);
#endif
            addEventReactionProt();
            saveValueParameter();
            ksu.resetDelay();
            ksu.stop(lastReasonStop_);
            setStateStop();
          }
        }
        else {                              // Двигатель - работа; Режим - авто; Защита - Вкл; Параметр - в норме
          setStateRun();
        }
      }
    }
    else if (ksu.isManualMode()) {          // Двигатель - работа; Режим - ручной;
      if (isModeOff()) {                    // Двигатель - работа; Режим - авто; Защита - выкл;
        setStateStop();
      }
      else {                                // Двигатель - работа; Режим - авто; Защита - вкл;
        if (alarm_) {                       // Двигатель - работа; Режим - авто; Защита - блок; Параметр - не в норме
          if ((timer_ == 0) && tripDelay_) {// Двигатель - работа; Режим - авто; Защита - блок; Параметр - не в норме; Срабатывание - начало;
            timer_ = ksu.getTime();         // Зафиксировали время начала задержки срабатывания
#if (USE_LOG_DEBUG == 1)
            logDebug.add(DebugMsg, "Protection::processingStateRun() Start tripping (idMode = %d, alarm = %d, tripDelay = %d, isManualMode = %d)",
                         idMode_, alarm_, tripDelay_, ksu.isManualMode());
#endif
            delay_ = true;
          }
          else if (ksu.getSecFromCurTime(timer_) >= tripDelay_) {   // Двигатель - работа; Режим - авто; Защита - блок; Параметр - не в норме; Срабатывание - конец;
#if (USE_LOG_DEBUG == 1)
            logDebug.add(DebugMsg, "Protection::processingStateRun() Trigger --> Stop manual (idMode = %d, alarm = %d, tripDelay = %d)",
                         idMode_, alarm_, tripDelay_);
#endif
            addEventReactionProt();
            saveValueParameter();
            logEvent.add(ProtectCode, AutoType, protBlockedEventId_);
            ksu.setBlock();
            ksu.stop(lastReasonStop_);
            block_ = true;
            setStateStop();
          }
        }
        else {                              // Двигатель - работа; Режим - авто; Защита - блок; Параметр - в норме
          setStateRun();
        }
      }
    }
    else {
      setStateStop();
    }
  }
  else {
    setStateStop();
  }
}

void Protection::proccessingStateStopping()
{
  if (ksu.isRunOrWorkMotor()) {                  // Двигатель - работа;
    if (ksu.isAutoMode()) {                 // Двигатель - работа; Режим - авто;
      if (restart_) {                       // Двигатель - работа; Режим - авто; Защита - Апв
        incRestartCount();
        state_ = StateRunning;
      }
      state_ = StateRunning;                // TODO: А точно?
    }
    else if (ksu.isManualMode()) {          // Двигатель - работа; Режим - ручной;
      if (restart_) {                       // Двигатель - работа; Режим - авто; Защита - Апв
        incRestartCount();
        state_ = StateRunning;
      }
      state_ = StateRunning;                // TODO: А точно?
    }
  }
  else if (ksu.isBreakOrStopMotor()) {             // Двигатель - стоп;
    if (ksu.getValue(CCS_CONDITION) == CCS_CONDITION_STOP) {
      setStateStop();
    }
  }
  else {
    uint32_t state = parameters.get(CCS_CONDITION);
    if (state > CCS_CONDITION_WORK) {
      logDebug.add(CriticalMsg, "Protection::proccessingStateStopping(): unknown state motor %d, idMode = %d",
                   state, idMode_);
    }
  }
}

void Protection::proccessingStateStop()
{
  if (ksu.isRunOrWorkMotor()) {                  // Двигатель - работа;
    if (ksu.isAutoMode()) {                 // Двигатель - работа; Режим - авто;
      if (isModeOff()) {
        restart_ = false;
//        restartCount_ = 0;
//        progressiveRestartCount_ = 0;
      }
      else {
        if (restart_) {                     // Двигатель - работа; Режим - авто; Защита - Апв
          incRestartCount();
        }
        state_ = StateRunning;
      }
    }
    else if (ksu.isManualMode()) {          // Двигатель - работа; Режим - ручной;
      if (isModeOff()) {
        restart_ = false;
//       restartCount_  = 0;
//        progressiveRestartCount_ = 0;
      }
      else {
        if (restart_) {                       // Двигатель - работа; Режим - авто; Защита - Апв
          incRestartCount();
        }
        state_ = StateRunning;
      }
    }
    else {                                  // Двигатель - работа; Режим - неизвестный
      uint32_t controlMode = parameters.get(CCS_WORKING_MODE);
      if (controlMode > CCS_WORKING_MODE_PROGRAM) {
        logDebug.add(CriticalMsg, "Protection::proccessingStateStop() unknown control mode %d, idMode = %d, line = %d",
                     controlMode, idMode_, __LINE__);
      }
    }
  }
  else if (ksu.isBreakOrStopMotor()) {               // Двигатель - стоп;
    if (ksu.isAutoMode() && !ksu.isBlock()) { // Двигатель - стоп; Режим - авто; Нет блокировки;
      if (restart_) {                         // Двигатель - стоп; Режим - авто; Флаг - АПВ;
        float restartTimer = 0;
        if (difStartFlag_) {                  // Защита с отсчётом АПВ после нормализации параметра (ВРП)
          if (!prevent_) {                    // Параметр защиты в норме
            if (timer_ == 0) {                //
              timer_ = ksu.getTime();         // Зафиксировали время начала отсёта АПВ
              ksu.setRestart();
            }
            else {
              restartTimer = restartDelay_ - ksu.getSecFromCurTime(timer_);
              if (restartTimer <= 0) {
                if (ksu.isPrevent()) {
                  if (!attempt_) {                // Первая попытка запуска по АПВ
                    attempt_ = true;
                    ksu.start(lastReasonRun_);
                  }
                }
                else {
                  incRestartCount();
                  ksu.start(lastReasonRun_);
                  state_ = StateRunning;
                }
              }
            }
          }
          else {
            timer_ = 0;
          }
        }
        else {
          restartTimer = restartDelay_ - ksu.getValue(CCS_STOP_TIME);
          if (restartTimer <= 0) {
            if (ksu.isPrevent() || prevent_) { // Есть запрещающий параметр
              if (!attempt_ && !resetRestartDelayFlag_) {                // Первая попытка АПВ
                attempt_ = true;
                ksu.start(lastReasonRun_);
              }
            }
            else {
              incRestartCount();
              ksu.start(lastReasonRun_);
              state_ = StateRunning;
            }
          }
        }
        if (restartTimer < 0)
          restartTimer = 0;
        parameters.set(CCS_RESTART_TIMER, restartTimer);
      }
    }
    else {
      if (isModeOff()) {
//        restartCount_ = 0;
//        progressiveRestartCount_ = 0;
      }
      restart_ = false;
    }
  }
  else {
    uint32_t state = parameters.get(CCS_CONDITION);
    if (state > CCS_CONDITION_WORK) {
      logDebug.add(CriticalMsg, "Protection::proccessingStateStop() unknown state motor %d, idMode = %d, line = %d",
                   state, idMode_, __LINE__);
    }
  }

  if (block_ && !ksu.isBlock()) {
    block_ = false;
    restartCount_ = 0;
    progressiveRestartCount_ = 0;
  }

  delay_ = false;
}

void Protection::incRestartCount()
{
  restartCount_++;                    // Увеличиваем счётчик АПВ
  progressiveRestartCount_++;         // Увеличиваем счётчик АПВ для прогрессивной
  if (restartCount_ == 1) {           // Первое АПВ
    restartFirstTime_ = ksu.getTime();   // Запоминаем время первого АПВ по защите
  }
  attempt_ = false;
  restart_ = false;
}

void Protection::automatProtection()
{
  switch (state_) {
  case StateRunning:
    processingStateRunning();
    break;
  case StateRun:
    processingStateRun();
    break;
  case StateStopping:
    proccessingStateStopping();
    break;
  case StateStop:
    proccessingStateStop();
    break;
  default:
    break;
  }
}
