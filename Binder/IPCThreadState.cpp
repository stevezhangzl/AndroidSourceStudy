//frameworks/native/libs/binder/IPCThreadState.cpp
#include "IPCThreadState.h"
#include <pthread.h>

static bool gHaveTLS = false;
static bool gShutdown = false;
static pthread_key_t gTLS = 0;
//声明了全局的互斥锁  并使用 PTHREAD_MUTEX_INITIALIZER 进行初始化  这个在C/C++中常见的做法
static pthread_mutex_t gTLSMutex = PTHREAD_MUTEX_INITIALIZER;


//线程中的单实例 ProcessState是进程中的单实例  线程中的全局变量，在其他线程是无法访问到的，纯粹的全局变量无法满足  需要TLS
IPCThreadState* IPCThreadState::self(){

  if(gHaveTLS){ //这个变量的初始值为false
restart:  //当启用TLSrgk ,重新返回到这
    const pthread_key_t k = gTLS;
    //如果本线程已经创建过IPCThreadState 那么pthread_getspecific就不为空 否则就返回一个新那建的IPCThreadState
    IPCThreadState* st = (IPCThreadState*)pthread_getspecific(k);
    if(st) return st;
      return new IPCThreadState; //没有就new
  }

  if(gShutdown) return NULL;

  pthread_mutex_lock(&gTLSMutex);
  if(!gHaveTLS){
    if(pthread_key_create(&gTLS,threadDestructor) != 0){
      pthread_mutex_unlock(&gTLSMutex);
      return NULL;
    }
    gHaveTLS = true; //以后都是TLS了
  }


  pthread_mutex_unlock(&gTLSMutex);
  goto restart; //返回restart接着执行
}





int IPCThreadState::waitForResponse(Parcel *reply, int *acquireResult){
    uint32_t cmd;
    int32_t err;

    while (1) {
        //循环等待结果   将已有的数据进行必要的包装，发送给Binder驱动
        if ((err = talkWithDriver()) < NO_ERROR) break; //处理与Binder间的交互命令
        err = mIn.errorCheck(); //走到这里 Binder Server已经执行完请求，并返回结果  比如getService
        if (err < NO_ERROR) break; //出错退出
        if (mIn.dataAvail() == 0) continue;  //mIn中没有数据，继续循环
        
        //读取回复数据
        cmd = (uint32_t)mIn.readInt32(); //读取cmd
        
        IF_LOG_COMMANDS() {
            alog << "Processing waitForResponse Command: "
                << getReturnString(cmd) << endl;
        }

        switch (cmd) {
        case BR_TRANSACTION_COMPLETE: //第二轮，进到这里  这里是同步的，所以会再次调用talkWithDriver与Binder驱动进行交互
            if (!reply && !acquireResult) goto finish;  //非同步  直接finish
            break;
        
        case BR_DEAD_REPLY:
            err = DEAD_OBJECT;
            goto finish;

        case BR_FAILED_REPLY:
            err = FAILED_TRANSACTION;
            goto finish;
        
        case BR_ACQUIRE_RESULT:
            {
                ALOG_ASSERT(acquireResult != NULL, "Unexpected brACQUIRE_RESULT");
                const int32_t result = mIn.readInt32();
                if (!acquireResult) continue;
                *acquireResult = result ? NO_ERROR : INVALID_OPERATION;
            }
            goto finish;
        
        case BR_REPLY:
            {
                binder_transaction_data tr;
                err = mIn.read(&tr, sizeof(tr));
                ALOG_ASSERT(err == NO_ERROR, "Not enough command data for brREPLY");
                if (err != NO_ERROR) goto finish;

                if (reply) {
                    if ((tr.flags & TF_STATUS_CODE) == 0) {
                        reply->ipcSetDataReference(
                            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                            tr.data_size,
                            reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                            tr.offsets_size/sizeof(binder_size_t),
                            freeBuffer, this);
                    } else {
                        err = *reinterpret_cast<const status_t*>(tr.data.ptr.buffer);
                        freeBuffer(NULL,
                            reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                            tr.data_size,
                            reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                            tr.offsets_size/sizeof(binder_size_t), this);
                    }
                } else {
                    freeBuffer(NULL,
                        reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                        tr.data_size,
                        reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                        tr.offsets_size/sizeof(binder_size_t), this);
                    continue;
                }
            }
            goto finish;

        default: //BR_NOOP
            err = executeCommand(cmd);
            if (err != NO_ERROR) goto finish;
            break;
        }
    }

finish:
    if (err != NO_ERROR) {
        if (acquireResult) *acquireResult = err;
        if (reply) reply->setError(err);
        mLastError = err;
    }
    
    return err;
}



int IPCThreadState::transact(int handle,int code,const Parcel& data,Parcel* reply,int flags){
  int err = data.errorCheck();
  //允许回复中包含文件描述符
  flags |= TF_ACCEPT_FDS;

  //整理数据，打包成Binder
  if (err == NO_ERROR) {
      LOG_ONEWAY(">>>> SEND from pid %d uid %d %s", getpid(), getuid(),
          (flags & TF_ONE_WAY) == 0 ? "READ REPLY" : "ONE WAY");
      //只是整理数据，这里并不发送  发给Binder驱动的是mOut
      err = writeTransactionData(BC_TRANSACTION, flags, handle, code, data, NULL); //只是整理数据，把结果存入mOut
  }



  if ((flags & TF_ONE_WAY) == 0) { //非异步，走这里
      #if 0
      if (code == 4) { // relayout
          ALOGI(">>>>>> CALLING transaction 4");
      } else {
          ALOGI(">>>>>> CALLING transaction %d", code);
      }
      #endif
      if (reply) {//reply不为null 
          err = waitForResponse(reply); //看起来是发送命令
      } else {
          Parcel fakeReply;
          err = waitForResponse(&fakeReply);
      }
      #if 0
      if (code == 4) { // relayout
          ALOGI("<<<<<< RETURNING transaction 4");
      } else {
          ALOGI("<<<<<< RETURNING transaction %d", code);
      }
      #endif
      
      IF_LOG_TRANSACTIONS() {
          TextOutput::Bundle _b(alog);
          alog << "BR_REPLY thr " << (void*)pthread_self() << " / hand "
              << handle << ": ";
          if (reply) alog << indent << *reply << dedent << endl;
          else alog << "(none requested)" << endl;
      }
  } else { //异步情况
      err = waitForResponse(NULL, NULL);
  }


}

void IPCThreadState::threadDestructor(void *st){
  IPCThreadState* const self = static_cast<IPCThreadState*>(st);
  if (self){
    #if defined(__ANDROID__)
      //if(self.)
    #endif
  }
  
}


IPCThreadState::IPCThreadState()
        :mProcess(ProcessState::self()),
         mMyThreadId(1000),
         mStrictModePolicy(0){

  pthread_setspecific(gTLS,this);
  //clearCaller();
  //mIn 是一个Parcel 从名称可以看出来，它是用于接收Binder发过来的数据，最大容量256
  mIn.setDataCapacity(256);
  //mOut也是一个Parcel 它是用于存储要发送给Binder的命令数据，最大容量为256
  mOut.setDataCapacity(256);

}

IPCThreadState::~IPCThreadState(){

}


/**
 * 真正调用ioctl 与Binder驱动发送命令的地方
*/
int IPCThreadState::talkWithDriver(bool doReceive){
    if (mProcess->mDriverFD <= 0) { //Binder驱动设备还没打开
        return -EBADF;
    }
    
    //读写都使用这个数据结构
    binder_write_read bwr;
    
    // Is the read buffer empty?
    //当前已经处理过的数据量 >= Parcel中已经有的数据量  对mIn来说 读取数据是处理
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();
    
    // We don't want to write anything if we are still reading
    // from data left in the input buffer and the caller
    // has requested to read the next data.
    //doRecevice 调用者希望读取
    //doReceive 传的是true 说明调用才希望读取 
    //但是这个逻辑outAvail = 0 的情况是，有一个为真就为真  都为假为假，也就是
    //1.用户希望读取数据  2.mIn需要去读取数据
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;
    //什么情况outAvail = mOut.dataSize() 1.用户不希望读取数据 或者 已经读取的数据量 >= Parcel中已经有的数据量 

    bwr.write_size = outAvail;
    //Parcel内部数据的内存起始地址
    bwr.write_buffer = (uintptr_t)mOut.data();

    // This is what we'll read.   
    if (doReceive && needRead) { //用户希望读取，同时mIn也需要去读取，这里设置read_size
        bwr.read_size = mIn.dataCapacity();  //进行填写需要read的数据请求
        bwr.read_buffer = (uintptr_t)mIn.data();
    } else {  //要不就
        bwr.read_size = 0;
        bwr.read_buffer = 0;
    }

    IF_LOG_COMMANDS() {
        TextOutput::Bundle _b(alog);
        if (outAvail != 0) {
            alog << "Sending commands to driver: " << indent;
            const void* cmds = (const void*)bwr.write_buffer;
            const void* end = ((const uint8_t*)cmds)+bwr.write_size;
            alog << HexDump(cmds, bwr.write_size) << endl;
            while (cmds < end) cmds = printCommand(alog, cmds);
            alog << dedent;
        }
        alog << "Size of receive buffer: " << bwr.read_size
            << ", needRead: " << needRead << ", doReceive: " << doReceive << endl;
    }
    
    // Return immediately if there is nothing to do.
    //如果请求中既没有要读的数据，也没有要写的数据，那就直接返回
    if ((bwr.write_size == 0) && (bwr.read_size == 0)) return NO_ERROR;

    bwr.write_consumed = 0;
    bwr.read_consumed = 0;
    int err;
    do {
        IF_LOG_COMMANDS() {
            alog << "About to read/write, write size = " << mOut.dataSize() << endl;
        }
#if defined(__ANDROID__)
        //与Binder驱动进行通信的代码  唯一个地方  与Binder通信
        if (ioctl(mProcess->mDriverFD, BINDER_WRITE_READ, &bwr) >= 0)
            err = NO_ERROR;
        else
            err = -errno;
#else
        err = INVALID_OPERATION;
#endif
        if (mProcess->mDriverFD <= 0) {
            err = -EBADF;
        }
        IF_LOG_COMMANDS() {
            alog << "Finished read/write, write size = " << mOut.dataSize() << endl;
        }
    } while (err == -EINTR);

    IF_LOG_COMMANDS() {
        alog << "Our err: " << (void*)(intptr_t)err << ", write consumed: "
            << bwr.write_consumed << " (of " << mOut.dataSize()
                        << "), read consumed: " << bwr.read_consumed << endl;
    }


    //确定对BINDER_WRITE_READ 命令的处理情况
    if (err >= NO_ERROR) {
        if (bwr.write_consumed > 0) { //这个表示Binder驱动消耗了了mOut中的数据，因而把这部分已经处理过的数据移除掉
            if (bwr.write_consumed < mOut.dataSize()) //消耗的数据量小于mOut的总数据量  就单独去掉这部分数据
                mOut.remove(0, bwr.write_consumed);
            else
                mOut.setDataSize(0); //否则就设置当前总数据量为0
        }
        if (bwr.read_consumed > 0) { //Binder驱动已经成功帮我们读取到了数据，并写入了mIn.data()所指向的内存地址
            mIn.setDataSize(bwr.read_consumed); //设置mDataSize和mDataPos
            mIn.setDataPosition(0);
        }
        IF_LOG_COMMANDS() {
            TextOutput::Bundle _b(alog);
            alog << "Remaining data size: " << mOut.dataSize() << endl;
            alog << "Received commands from driver: " << indent;
            const void* cmds = mIn.data();
            const void* end = mIn.data() + mIn.dataSize();
            alog << HexDump(cmds, mIn.dataSize()) << endl;
            while (cmds < end) cmds = printReturnCommand(alog, cmds);
            alog << dedent;
        }
        return NO_ERROR;
    }
    
    return err;
}