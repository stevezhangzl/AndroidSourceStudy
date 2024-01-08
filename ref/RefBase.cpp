//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:system/core/libutils/RefBase.cpp?hl=zh-cn


#include "RefBase.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <typeinfo>
#include <unistd.h>



#define INITIAL_STRONG_VALUE (1<<28)

class RefBase::weakref_impl : public RefBase::weakref_type{
  public:
    volatile int32_t mStrong; //强引用计数值
    volatile int32_t mWeak; //弱引用计数值
    RefBase* const mBase;
    volatile int32_t mFlags;


    weakref_impl(RefBase* base):mStrong(INITIAL_STRONG_VALUE),mWeak(0),mBase(base),mFlags(0){}

    void addStrongRef(const void*){};
    void removeStrongRef(const void*){}
    void addWeakRef(const void*){}
};


void RefBase::weakref_type::incWeak(const void* id){
  weakref_impl* const impl = static_cast<weakref_impl *>(this);
  impl->addWeakRef(id); //用于调试目的 
  const int32_t c = android_atomic_inc(&impl->mWeak); //原子，android提供
}


RefBase::weakref_type* RefBase::createWeak(const void* id) const {
  mRefs->incWeak(id); //使用的是weakref_type的实现对象，去增加弱引用计数
  return mRefs; //直接返回weakref_type对象
}


//同时增加弱引用和强引用的计数值
void RefBase::incStrong(const void* id) const{
  weakref_impl* const refs = mRefs;
  refs->incWeak(id); //增加弱引用计数值

  refs->addStrongRef(id); //用于调试目的
  const int32_t c = anroid_atomic_inc(&refs->mStrong); //增加强引用计数值

  if (c!=INITIAL_STRONG_VALUE){ //判断是一浊第一次
    return;  //不是第一次直接返回
  }

  //第一次1<<28 +1 所以要减掉INITIAL_STRONG_VALUE 才能得到1
  android_atomic_add(-INITIAL_STRONG_VALUE,&refs->mStrong);
  refs->mBase->onFirstRef();
}


void RefBase::decStrong(const void* id) const {
  weakref_impl* const refs = mRefs;
  const int32_t c = android_atomic_dec(&refs->mStrong); //减少强引用计数

  if (c == 1){
    refs->mBase->onLastStrongRef(id); //通知事件
    delete this;
  }

  refs->decWeak(id); //减少弱引用计数
  
}

void RefBase::weakref_type::decWeak(const void* id){
  
}

void RefBase::onFirstRef()
{
}

