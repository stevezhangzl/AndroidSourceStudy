#ifndef ANDROID_BARRIER_H
#define ANDROID_BARRIER_H


//system/core/libutils/include/utils/Mutex.h
//https://github.com/torvalds/linux/blob/v3.8/drivers/staging/android/lowmemorykiller.c
//#include <stdint.h>



#include <sys/types.h>
#include "Mutex.h"
#include "Condition.h"



namespace android{

/**
 * Barrier通常用于对某线程是否初始化完成进行判断，这种场景具有不可逆性
*/
class Barrier{

  public:
    inline Barrier():state(CLOSED){}
    inline ~Barrier(){}

    void open(){
      Mutex::Autolock _l(lock); //加解锁自动化操作 小技巧  Autolock
      state = OPENED;
      cv.broadcast();
    }


    void close(){
      Mutex::Autolock _l(lock);
      state = CLOSED;
    }

    void wait() const {
      Mutex::Autolock _l(lock);
      while(state == CLOSED){//这里必须是while  如果在close()末尾添加broadcast或者signal 这里是while就会再次检查状态，要不就是错误状态了因为state没有变成OPENED
        cv.wait(lock);
      }
    }



  private:
    enum{ OPENED,CLOSED };
    mutable Mutex lock;
    mutable Condition cv;
    volatile int state;
  };


};



#endif //ANDROID_BARRIER_H