#include "protection_pressure_intake.h"

ProtectionPressureIntake::ProtectionPressureIntake()
{
  idMode_= CCS_PROT_DHS_PRESSURE_INTAKE_MODE;
  idPrevent_= CCS_PROT_DHS_PRESSURE_INTAKE_PREVENT;
  idActivDelay_ = CCS_PROT_DHS_PRESSURE_INTAKE_ACTIV_DELAY;
  idTripDelay_ = CCS_PROT_DHS_PRESSURE_INTAKE_TRIP_DELAY;
  idRestartDelay_ = CCS_PROT_DHS_PRESSURE_INTAKE_RESTART_DELAY;
  idRestartLimit_ = CCS_PROT_DHS_PRESSURE_INTAKE_RESTART_LIMIT;
  idRestartResetTime_ = CCS_PROT_DHS_PRESSURE_INTAKE_RESTART_RESET;
  idTripSetpoint_ = CCS_PROT_DHS_PRESSURE_INTAKE_TRIP_SETPOINT;
  idRestartSetpoint_ = CCS_PROT_DHS_PRESSURE_INTAKE_RESTART_SETPOINT;
  idParam_ = CCS_PROT_DHS_PRESSURE_INTAKE_PARAMETER;
  idRestartFlag_ = CCS_PROT_DHS_PRESSURE_INTAKE_RESTART_FLAG;
  idBlockFlag_ = CCS_PROT_DHS_PRESSURE_INTAKE_BLOCK_FLAG;
  idDelayFlag_ = CCS_PROT_DHS_PRESSURE_INTAKE_DELAY_FLAG;
  idState_ = CCS_PROT_DHS_PRESSURE_INTAKE_STATE;
  idTimer_ = CCS_PROT_DHS_PRESSURE_INTAKE_TIME;
  idRestartCount_ = CCS_PROT_DHS_PRESSURE_INTAKE_RESTART_COUNT;
  idRestartFirstTime_ = CCS_PROT_DHS_PRESSURE_INTAKE_RESTART_FIRST_TIME;

  lastReasonRun_ = LastReasonRunApvPressureIntake;
  lastReasonStop_ = LastReasonStopPressureIntake;

  protReactEventId_ = PressureIntakeProtReactId;
  apvEventId_ = PressureIntakeApvId;
  apvDisabledEventId_ = PressureIntakeApvDisabledId;
  protBlockedEventId_ = PressureIntakeProtBlockedId;
}

ProtectionPressureIntake::~ProtectionPressureIntake()
{

}

void ProtectionPressureIntake::getOtherSetpointProt()
{
  resetRestartDelayFlag_ = ksu.getValue(idParam_);
  if (resetRestartDelayFlag_)
    restartDelay_ = 0;
}

bool ProtectionPressureIntake::checkAlarm()
{
  return Protection::isLowerLimit(tripSetpoint_);
}

bool ProtectionPressureIntake::checkPrevent()
{  
  if (restart_)
    return Protection::isLowerLimit(restartSetpoint_);
  else
    return Protection::isLowerLimit(tripSetpoint_);
}

float ProtectionPressureIntake::calcValue()
{
  return parameters.get(TMS_PRESSURE_INTAKE);
}

bool ProtectionPressureIntake::isProtect()
{
  if (parameters.get(CCS_DHS_TYPE) != TYPE_DHS_NONE)
    if (tms->isConnect())
      if (parameters.isValidity(TMS_PRESSURE_INTAKE))
        return true;
  return false;
}
