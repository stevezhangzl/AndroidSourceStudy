//frameworks/native/libs/binder/BpBinder.cpp
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/native/libs/binder/BpBinder.cpp?hl=zh-cn


#include "BpBinder.h"
#include "IPCThreadState.h"

BpBinder::BpBinder(int handle):mHandle(handle){ //如果是SM，handle为0
  extendObjectLifetime(OBJECT_LIFETIME_WEAK);
  //把handle对象 转为弱引用
  IPCThreadState::self()->incWeakHandle(handle);
}



int BpBinder::transact(int code,const Parcel& data,Parcel* reply,int flags){
  if(mAlive){
    int status = IPCThreadState::self()->transact(mHandle,code,data,reply,flags);
    if(status == DEAD_OBJECT) mAlive = 0;
  }

  return DEAD_OBJECT;
}




