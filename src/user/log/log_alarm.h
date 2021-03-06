#ifndef LOGALARM_H
#define LOGALARM_H

#include "log_running.h"

/*!
 * \brief Класс архива аварийных ситуаций
 */
class LogAlarm : public LogRunning
{
public:
  LogAlarm();
  ~LogAlarm();

  void init();

  /*!
   * \brief Запуск формирования и записи архива
   */
  void start(uint32_t eventId);
  void task();

private:
  /*!
   * \brief Метод добавления записи в архив
   */
  void add();

};

#endif // LOGALARM_H
