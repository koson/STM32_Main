#ifndef PROTECTIONIMBALANCECURRENTMOTOR_H
#define PROTECTIONIMBALANCECURRENTMOTOR_H

#include "protection.h"

/*!
 * \brief Класс автомата защиты "Дисбаланс токов ПЭД"
 */
class ProtectionImbalanceCurrentMotor : public Protection
{
public:
  ProtectionImbalanceCurrentMotor();
  ~ProtectionImbalanceCurrentMotor();

private:

  bool checkAlarm();
  float calcValue();

};

#endif // PROTECTIONIMBALANCECURRENTMOTOR_H
