#include "protection_out_of_sync_motor.h"

ProtectionOutOfSyncMotor::ProtectionOutOfSyncMotor()
{
  idMode_= CCS_PROT_MOTOR_OUT_OF_SYNC_MODE;
  idPrevent_= CCS_PROT_MOTOR_OUT_OF_SYNC_PREVENT;
  idActivDelay_ = CCS_PROT_MOTOR_OUT_OF_SYNC_ACTIV_DELAY;
  idTripDelay_ = CCS_PROT_MOTOR_OUT_OF_SYNC_TRIP_DELAY;
  idRestartDelay_ = CCS_PROT_MOTOR_OUT_OF_SYNC_RESTART_DELAY;
  idRestartLimit_ = CCS_PROT_MOTOR_OUT_OF_SYNC_RESTART_LIMIT;
  idRestartResetTime_ = CCS_PROT_MOTOR_OUT_OF_SYNC_RESTART_RESET;
  idTripSetpoint_ = CCS_PROT_MOTOR_OUT_OF_SYNC_TRIP_SETPOINT;
  idRestartSetpoint_ = CCS_PROT_MOTOR_OUT_OF_SYNC_RESTART_SETPOINT;
  idParam_ = CCS_PROT_MOTOR_OUT_OF_SYNC_PARAMETER;
  idRestartFlag_ = CCS_PROT_MOTOR_OUT_OF_SYNC_RESTART_FLAG;
  idBlockFlag_ = CCS_PROT_MOTOR_OUT_OF_SYNC_BLOCK_FLAG;
  idDelayFlag_ = CCS_PROT_MOTOR_OUT_OF_SYNC_DELAY_FLAG;
  idState_ = CCS_PROT_MOTOR_OUT_OF_SYNC_STATE;
  idTimer_ = CCS_PROT_MOTOR_OUT_OF_SYNC_TIME;
  idRestartCount_ = CCS_PROT_MOTOR_OUT_OF_SYNC_RESTART_COUNT;
  idRestartFirstTime_ = CCS_PROT_MOTOR_OUT_OF_SYNC_RESTART_FIRST_TIME;

  lastReasonRun_ = LastReasonRunApvOutOfSyncMotor;
  lastReasonStop_ = LastReasonStopOutOfSyncMotor;

  protReactEventId_ = OutOfSyncMotorProtReactId;
  apvEventId_ = OutOfSyncMotorApvId;
  apvDisabledEventId_ = OutOfSyncMotorApvDisabledId;
  protBlockedEventId_ = OutOfSyncMotorProtBlockedId;
}

ProtectionOutOfSyncMotor::~ProtectionOutOfSyncMotor()
{

}

bool ProtectionOutOfSyncMotor::checkAlarm()
{
  float nominal = parameters.get(VSD_MOTOR_CURRENT);
  valueParameter2_ = parameters.get(CCS_MOTOR_CURRENT_AVARAGE);
  valueParameter_ = parameters.get(CCS_MOTOR_COS_PHI_NOW);

  float maxCurrent = nominal * restartSetpoint_ / 100;
  if ((valueParameter2_ > maxCurrent) && (valueParameter_ < tripSetpoint_))
    return true;
  else
    return false;
}

bool ProtectionOutOfSyncMotor::isProtect()
{
  if (parameters.get(VSD_MOTOR_TYPE) == VSD_MOTOR_TYPE_VENT)
    return true;
  return false;
}

void ProtectionOutOfSyncMotor::addEventReactionProt()
{
  float nominal = parameters.get(VSD_MOTOR_CURRENT);
  logEvent.add(ProtectCode, AutoType, protReactEventId_, valueParameter2_, nominal,
               parameters.getPhysic(VSD_MOTOR_CURRENT));
  logEvent.add(ProtectCode, AutoType, protReactEventId_, valueParameter_, tripSetpoint_,
               parameters.getPhysic(idTripSetpoint_));
}
