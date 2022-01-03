#ifndef MY_DATE_TIME_CLASS_H
#define MY_DATE_TIME_CLASS_H
#include <DateTime.h>


/**
 * @brief DateTime Library Main Class, include time get/set/format methods.
 *
 */
class DateTimeMSClass: public DateTimeClass {
 public:

    /**
   * @brief Get os timestamp, in milli seconds
   *
   * @return time_t timestamp, in milli seconds
   */
  inline uint64_t osTimeMS() const {
    auto t = time(nullptr);
    uint64_t milliSec = micros64() / 1000 % 1000;
    return (uint64_t)t * 1000 + milliSec;
  }
};

/**
 * @brief Global DateTimeClass object.
 *
 */
extern DateTimeMSClass DateTimeMS;  // define in cpp

#endif
