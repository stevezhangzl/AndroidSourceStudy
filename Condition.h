#ifndef _LIBS_UTILS_CONDITION_H
#define _LIBS_UTILS_CONDITION_H


#include <pthread.h>
#include "Mutex.h"
#include "Timers.h"
#include <stdint.h>
#include <limits.h>
#include <time.h>

//system/core/libutils/include/utils/Condition.h
namespace android{

class Condition{
private:
  pthread_cond_t mCond;

public:
  enum{
    PRIVATE = 0,
    SHARED = 1
  };
  Condition();
  explicit Condition(int type);

  //在某个条件上等待
  int wait(Mutex& mutex);
  //也是在某个条件上等待，增加了超时退出功能
  int waitRelative(Mutex& mutex,nsecs_t reltime);

  //条件满足时通知相应等待者
  void signal();
  //条件满足时通知所有等待者
  void broadcast();

  ~Condition();
};

inline Condition::Condition():Condition(PRIVATE){

}

inline Condition::Condition(int type){
  pthread_condattr_t attr;
  pthread_condattr_init(&attr);
#if defined(__linux__)
  pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
#endif

  if (type == SHARED) {
      pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  }

  pthread_cond_init(&mCond, &attr);
  pthread_condattr_destroy(&attr);
}


inline int Condition::wait(Mutex& mutex) {
    return -pthread_cond_wait(&mCond, &mutex.mMutex);
}

inline int Condition::waitRelative(Mutex& mutex, nsecs_t reltime) {
  struct timespec ts;
#if defined(__linux__)
  clock_gettime(CLOCK_MONOTONIC,&ts);
#else  //__APPLE__
  // Apple doesn't support POSIX clocks.
  struct timeval t;
  gettimeofday(&t,nullptr);
  ts.tv_sec = t.tv_sec;
  ts.tv_nsec = t.tv_usec*1000;

#endif
  // On 32-bit devices, tv_sec is 32-bit, but `reltime` is 64-bit.
  int64_t reltime_sec = reltime/1000000000;
  ts.tv_nsec += static_cast<long>(reltime%1000000000);
  if (reltime_sec < INT64_MAX && ts.tv_nsec >= 1000000000) {
    ts.tv_nsec -= 1000000000;
    ++reltime_sec;
  }
  int64_t time_sec = ts.tv_sec;
  if (time_sec > INT64_MAX - reltime_sec) {
      time_sec = INT64_MAX;
  } else {
      time_sec += reltime_sec;
  }

  ts.tv_sec = (time_sec > LONG_MAX) ? LONG_MAX : static_cast<long>(time_sec);

  return -pthread_cond_timedwait(&mCond, &mutex.mMutex, &ts);

}

inline Condition::~Condition(){
  pthread_cond_destroy(&mCond);
}

inline void Condition::signal() {
    pthread_cond_signal(&mCond);
}
inline void Condition::broadcast() {
    pthread_cond_broadcast(&mCond);
}




}




#endif //_LIBS_UTILS_CONDITION_H