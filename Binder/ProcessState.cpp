//frameworks/native/libs/binder/ProcessState.cpp
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/native/libs/binder/ProcessState.cpp?hl=zh-cn

#include "ProcessState.h"
#include "../Mutex.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace android;





static int open_driver()
{
    int fd = open("/dev/binder", O_RDWR | O_CLOEXEC);
    if (fd >= 0) {
        int vers = 0;
        int result = ioctl(fd, BINDER_VERSION, &vers);
        if (result == -1) {
            ALOGE("Binder ioctl to obtain version failed: %s", strerror(errno));
            close(fd);
            fd = -1;
        }
        if (result != 0 || vers != BINDER_CURRENT_PROTOCOL_VERSION) {
            ALOGE("Binder driver protocol does not match user space protocol!");
            close(fd);
            fd = -1;
        }
        size_t maxThreads = DEFAULT_MAX_BINDER_THREADS;
        result = ioctl(fd, BINDER_SET_MAX_THREADS, &maxThreads);
        if (result == -1) {
            ALOGE("Binder ioctl to set max threads failed: %s", strerror(errno));
        }
    } else {
        ALOGW("Opening '/dev/binder' failed: %s\n", strerror(errno));
    }
    return fd;
}


IBinder& ProcessState::getContextObject(const IBinder&){
  return getStrongProxyForHandle(0);
}

IBinder& ProcessState::getStrongProxyForHandle(int handle){
  IBinder result; //需要返回的IBinder

  AutoMutex _l(mLock);

  //查找一个Vector表mHandleToObject，这里保存了这个进程已经建立的Binder相关信息
  handle_entry* e = lookupHandleLocked(handle);

  //如果上面的表中没有找到相应的节点，会自动添加一个，所以这里的变量e正常情况下都不会为空
  if (e != NULL) {
      IBinder* b = e->binder;
      //在列表中没找到对应的BpBinder 或 没办法顺利对这个BpBinder增加weak reference
      if (b == NULL || !e->refs->attemptIncWeak(this)) {
          if (handle == 0) {

              Parcel data;
              int status = IPCThreadState::self()->transact(
                      0, IBinder::PING_TRANSACTION, data, NULL, 0);
              if (status == DEAD_OBJECT)
                  return NULL;
          }

          
          b = new BpBinder(handle);  //BpBinder出现了
          e->binder = b;
          if (b) e->refs = b->getWeakRefs();
          result = b;
      } else {
          result.force_set(b);
          e->refs->decWeak(this);
      }
  }

  //把IBinder返回上层
  return result;  
}


/**
 * 访问ProcessState需要使用ProcessState::Self() 如果当前已经有实例(gProcess不为空)存在
 * ，就直接返回这个对象，否则，新建一个ProcessState
 * 
 * 
*/
ProcessState* ProcessState::self(){
  static ProcessState *gProcess;
  static Mutex gProcessMutex;


  Mutex::Autolock _l(gProcessMutex);
  if(gProcess!=NULL){
    return gProcess;
  }

  gProcess = new ProcessState;//创建对象
  return gProcess;
}


/**
 * Binder驱动使用的  首先就是要打开etc/binder 节点  然后执行mmap  映射的内存块大小为BINDER_VM_SIZE
*/
ProcessState::ProcessState():mDriverFD(open_driver()){
  if (mDriverFD >= 0) { //成功打开/dev/binder
    // mmap the binder, providing a chunk of virtual address space to receive transactions.
    mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
    if (mVMStart == MAP_FAILED) {
        // *sigh*
        ALOGE("Using /dev/binder failed: unable to mmap transaction memory.\n");
        close(mDriverFD);
        mDriverFD = -1;
    }
  }

  LOG_ALWAYS_FATAL_IF(mDriverFD < 0, "Binder driver could not be opened.  Terminating.");
}

