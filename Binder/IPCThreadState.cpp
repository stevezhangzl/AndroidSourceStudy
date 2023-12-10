//frameworks/native/libs/binder/IPCThreadState.cpp
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/native/libs/binder/IPCThreadState.cpp?hl=zh-cn

#include "IPCThreadState.h"
#include <pthread.h>

static bool gHaveTLS = false;
static bool gShutdown = false;
static pthread_key_t gTLS = 0;
//声明了全局的互斥锁  并使用 PTHREAD_MUTEX_INITIALIZER 进行初始化  这个在C/C++中常见的做法
static pthread_mutex_t gTLSMutex = PTHREAD_MUTEX_INITIALIZER;


//线程中的单实例 ProcessState是进程中的单实例  线程中的全局变量，在其他线程是无法访问到的，纯粹的全局变量无法满足  需要TLS
IPCThreadState* IPCThreadState::self(){

  //通过gHaveTLS变量和判断和 goto 保证了第一次会gTLS 赋值 又不会
  if(gHaveTLS){ //这个变量的初始值为false  刚开始的时候不走这里
restart:  //当启用TLSrgk ,重新返回到这
    const pthread_key_t k = gTLS;
    //如果本线程已经创建过IPCThreadState 那么pthread_getspecific就不为空 否则就返回一个新那建的IPCThreadState
    IPCThreadState* st = (IPCThreadState*)pthread_getspecific(k);
    if(st) return st;
      return new IPCThreadState; //没有就new
  }

  if(gShutdown) return NULL;


  //第一次一定会走这里
  pthread_mutex_lock(&gTLSMutex);
  if(!gHaveTLS){ //如果不存在TLS 就创建  好像只是为了给gTLS这个key赋值
    if(pthread_key_create(&gTLS,threadDestructor) != 0){ //启动一个线程，但好像没干什么
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
        cmd = (uint32_t)mIn.readInt32(); //读取cmd  所以dataPos 会右移变大 这里会影响talkWithDriver的读写申请
        
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
                            reply->ipcSetDataReference( //重要  对数据进行处理，它是Parcel.cpp的一个函数
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


int IPCThreadState::writeTransactionData(int cmd, int binderFlags,int handle, int code, const Parcel& data, int* statusBuffer)
{
    //binder__transaction_data 才是Binder驱动认识的数据结构   这里是把数据转成tr 然后放在mOut中
    binder_transaction_data tr;

    tr.target.ptr = 0; /* Don't pass uninitialized stack data to a remote process */
    tr.target.handle = handle;
    tr.code = code;
    tr.flags = binderFlags;
    tr.cookie = 0;
    tr.sender_pid = 0;
    tr.sender_euid = 0;
    
    const status_t err = data.errorCheck();
    if (err == NO_ERROR) {
        tr.data_size = data.ipcDataSize();
        tr.data.ptr.buffer = data.ipcData();
        tr.offsets_size = data.ipcObjectsCount()*sizeof(binder_size_t);
        tr.data.ptr.offsets = data.ipcObjects();
    } else if (statusBuffer) {
        tr.flags |= TF_STATUS_CODE;
        *statusBuffer = err;
        tr.data_size = sizeof(status_t);
        tr.data.ptr.buffer = reinterpret_cast<uintptr_t>(statusBuffer);
        tr.offsets_size = 0;
        tr.data.ptr.offsets = 0;
    } else {
        return (mLastError = err);
    }
    
    mOut.writeInt32(cmd);
    mOut.write(&tr, sizeof(tr));
    
    return NO_ERROR;
}



/**
 * 负责与Binder驱动进行 命令 交互
*/
int IPCThreadState::transact(int handle,int code,const Parcel& data,Parcel* reply,int flags){
  int err = data.errorCheck();
  //允许回复中包含文件描述符
  flags |= TF_ACCEPT_FDS;

  //整理数据，打包成Binder
  if (err == NO_ERROR) {
      LOG_ONEWAY(">>>> SEND from pid %d uid %d %s", getpid(), getuid(),
          (flags & TF_ONE_WAY) == 0 ? "READ REPLY" : "ONE WAY");
      //只是整理数据，这里并不发送  发给Binder驱动的是mOut 
      //这里是把parcel转成  transact data 因为 parcel Binder驱动不认识  其实就是把 flags handle 命令-BC_TRANSACTION 这些填充tr中
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
    //当前已经处理过的数据量（上层） >= Parcel中已经有的数据量  对mIn来说 读取数据是处理（上层 使用mIn.readXX）
    const bool needRead = mIn.dataPosition() >= mIn.dataSize();  //前提是mIn是需要Binder驱动去读取数据的
    //你可以这么理解，上层 mIn.readXXA 读取数据（处理），Parcel中的数据量 dataPos 就增加   如果dataPos < dataSize 说明有数据需要上层继续读取（处理，还有数据没处理完，所以Binder不需要再申请数据） 这里needRead就是false（Binder你不要继续去请求数据，先把数据处理完） 那就一定不会设置read_size 
    
    // We don't want to write anything if we are still reading
    // from data left in the input buffer and the caller
    // has requested to read the next data.
    //doRecevice 调用者希望读取
    //doReceive 传的是true 说明调用才希望读取 
    //但是这个逻辑outAvail = 0 的情况是，有一个为真就为真  都为假为假，也就是
    //

    //1.用户不想接收数据 或者 2 需要Binder去请求数据  mOut才有值，因为要Binder驱动向Binder Server写入请求
    //！！！这里都是用户想接收数据，所以第一个条件一定是false  是不是Binder驱动写入数据 就看第二个  也就是当前上层读取的数据是不是已经读取干净了，需要Binder驱动继续申请数据，需要就设置write_size 但也要看mOut上层给没给东西，没有指令就没有给Binder的命令
    const size_t outAvail = (!doReceive || needRead) ? mOut.dataSize() : 0;  //这句话理解起来就是，用户想要去接收数据doReceive 但是不需要Binder去请求数据（没有处理完所有的数据）write_size就是0 驱动不发生读取事件
    //什么情况outAvail = mOut.dataSize() 1.用户不希望读取数据 或者 已经读取的数据量 >= Parcel中已经有的数据量 

    bwr.write_size = outAvail;
    //Parcel内部数据的内存起始地址  这里把mOut赋值给bwr数据结构
    bwr.write_buffer = (uintptr_t)mOut.data();  

    // This is what we'll read.   
    if (doReceive && needRead) { //用户希望读取，同时mIn中也有数据需要去处理，这里设置read_size
        bwr.read_size = mIn.dataCapacity();  //进行填写需要read的数据请求
        bwr.read_buffer = (uintptr_t)mIn.data();
    } else {  //
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
                mOut.setDataSize(0); //否则就设置当前总数据量为0 意味着没有需要 写入的数据了
        }
        if (bwr.read_consumed > 0) { //Binder驱动已经成功帮我们读取到了数据，并写入了mIn.data()所指向的内存地址
            mIn.setDataSize(bwr.read_consumed); //设置mDataSize      
            mIn.setDataPosition(0);// mDataPos   
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



int IPCThreadState::executeCommand(int cmd)
{
    BBinder* obj;
    RefBase::weakref_type* refs;
    int result = NO_ERROR;
    
    switch ((uint32_t)cmd) {
    case BR_ERROR:
        result = mIn.readInt32();
        break;
        
    case BR_OK:
        break;
        
    case BR_ACQUIRE:
        refs = (RefBase::weakref_type*)mIn.readPointer();
        obj = (BBinder*)mIn.readPointer();
        ALOG_ASSERT(refs->refBase() == obj,"BR_ACQUIRE: object %p does not match cookie %p (expected %p)",refs, obj, refs->refBase());
        obj->incStrong(mProcess.get());
        IF_LOG_REMOTEREFS() {
            LOG_REMOTEREFS("BR_ACQUIRE from driver on %p", obj);
            obj->printRefs();
        }
        mOut.writeInt32(BC_ACQUIRE_DONE);
        mOut.writePointer((uintptr_t)refs);
        mOut.writePointer((uintptr_t)obj);
        break;
        
    case BR_RELEASE:
        refs = (RefBase::weakref_type*)mIn.readPointer();
        obj = (BBinder*)mIn.readPointer();
        ALOG_ASSERT(refs->refBase() == obj,
                   "BR_RELEASE: object %p does not match cookie %p (expected %p)",
                   refs, obj, refs->refBase());
        IF_LOG_REMOTEREFS() {
            LOG_REMOTEREFS("BR_RELEASE from driver on %p", obj);
            obj->printRefs();
        }
        mPendingStrongDerefs.push(obj);
        break;
        
    case BR_INCREFS:
        refs = (RefBase::weakref_type*)mIn.readPointer();
        obj = (BBinder*)mIn.readPointer();
        refs->incWeak(mProcess.get());
        mOut.writeInt32(BC_INCREFS_DONE);
        mOut.writePointer((uintptr_t)refs);
        mOut.writePointer((uintptr_t)obj);
        break;
        
    case BR_DECREFS:
        refs = (RefBase::weakref_type*)mIn.readPointer();
        obj = (BBinder*)mIn.readPointer();
        // NOTE: This assertion is not valid, because the object may no
        // longer exist (thus the (BBinder*)cast above resulting in a different
        // memory address).
        //ALOG_ASSERT(refs->refBase() == obj,
        //           "BR_DECREFS: object %p does not match cookie %p (expected %p)",
        //           refs, obj, refs->refBase());
        mPendingWeakDerefs.push(refs);
        break;
        
    case BR_ATTEMPT_ACQUIRE:
        refs = (RefBase::weakref_type*)mIn.readPointer();
        obj = (BBinder*)mIn.readPointer();
         
        {
            const bool success = refs->attemptIncStrong(mProcess.get());
            ALOG_ASSERT(success && refs->refBase() == obj,
                       "BR_ATTEMPT_ACQUIRE: object %p does not match cookie %p (expected %p)",
                       refs, obj, refs->refBase());
            
            mOut.writeInt32(BC_ACQUIRE_RESULT);
            mOut.writeInt32((int32_t)success);
        }
        break;
    
    case BR_TRANSACTION:
        {
            binder_transaction_data tr;
            result = mIn.read(&tr, sizeof(tr));
            ALOG_ASSERT(result == NO_ERROR,
                "Not enough command data for brTRANSACTION");
            if (result != NO_ERROR) break;
            
            Parcel buffer;
            buffer.ipcSetDataReference(
                reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer),
                tr.data_size,
                reinterpret_cast<const binder_size_t*>(tr.data.ptr.offsets),
                tr.offsets_size/sizeof(binder_size_t), freeBuffer, this);
            
            const pid_t origPid = mCallingPid;
            const uid_t origUid = mCallingUid;
            const int32_t origStrictModePolicy = mStrictModePolicy;
            const int32_t origTransactionBinderFlags = mLastTransactionBinderFlags;

            mCallingPid = tr.sender_pid;
            mCallingUid = tr.sender_euid;
            mLastTransactionBinderFlags = tr.flags;

            int curPrio = getpriority(PRIO_PROCESS, mMyThreadId);
            if (gDisableBackgroundScheduling) {
                if (curPrio > ANDROID_PRIORITY_NORMAL) {
                    // We have inherited a reduced priority from the caller, but do not
                    // want to run in that state in this process.  The driver set our
                    // priority already (though not our scheduling class), so bounce
                    // it back to the default before invoking the transaction.
                    setpriority(PRIO_PROCESS, mMyThreadId, ANDROID_PRIORITY_NORMAL);
                }
            } else {
                if (curPrio >= ANDROID_PRIORITY_BACKGROUND) {
                    // We want to use the inherited priority from the caller.
                    // Ensure this thread is in the background scheduling class,
                    // since the driver won't modify scheduling classes for us.
                    // The scheduling group is reset to default by the caller
                    // once this method returns after the transaction is complete.
                    set_sched_policy(mMyThreadId, SP_BACKGROUND);
                }
            }

            //ALOGI(">>>> TRANSACT from pid %d uid %d\n", mCallingPid, mCallingUid);

            Parcel reply;
            status_t error;
            IF_LOG_TRANSACTIONS() {
                TextOutput::Bundle _b(alog);
                alog << "BR_TRANSACTION thr " << (void*)pthread_self()
                    << " / obj " << tr.target.ptr << " / code "
                    << TypeCode(tr.code) << ": " << indent << buffer
                    << dedent << endl
                    << "Data addr = "
                    << reinterpret_cast<const uint8_t*>(tr.data.ptr.buffer)
                    << ", offsets addr="
                    << reinterpret_cast<const size_t*>(tr.data.ptr.offsets) << endl;
            }
            if (tr.target.ptr) {  //指定了目标对象
                // We only have a weak reference on the target object, so we must first try to
                // safely acquire a strong reference before doing anything else with it.
                if (reinterpret_cast<RefBase::weakref_type*>(tr.target.ptr)->attemptIncStrong(this)) {
                    error = reinterpret_cast<BBinder*>(tr.cookie)->transact(tr.code, buffer,&reply, tr.flags); //转成BBinder执行transact
                    reinterpret_cast<BBinder*>(tr.cookie)->decStrong(this);
                } else {
                    error = UNKNOWN_TRANSACTION;
                }

            } else {
                error = the_context_object->transact(tr.code, buffer, &reply, tr.flags);
            }

            //ALOGI("<<<< TRANSACT from pid %d restore pid %d uid %d\n",
            //     mCallingPid, origPid, origUid);
            
            if ((tr.flags & TF_ONE_WAY) == 0) {
                LOG_ONEWAY("Sending reply to %d!", mCallingPid);
                if (error < NO_ERROR) reply.setError(error);
                sendReply(reply, 0);
            } else {
                LOG_ONEWAY("NOT sending reply to %d!", mCallingPid);
            }
            
            mCallingPid = origPid;
            mCallingUid = origUid;
            mStrictModePolicy = origStrictModePolicy;
            mLastTransactionBinderFlags = origTransactionBinderFlags;

            IF_LOG_TRANSACTIONS() {
                TextOutput::Bundle _b(alog);
                alog << "BC_REPLY thr " << (void*)pthread_self() << " / obj "
                    << tr.target.ptr << ": " << indent << reply << dedent << endl;
            }
            
        }
        break;
    
    case BR_DEAD_BINDER:
        {
            BpBinder *proxy = (BpBinder*)mIn.readPointer();
            proxy->sendObituary();
            mOut.writeInt32(BC_DEAD_BINDER_DONE);
            mOut.writePointer((uintptr_t)proxy);
        } break;
        
    case BR_CLEAR_DEATH_NOTIFICATION_DONE:
        {
            BpBinder *proxy = (BpBinder*)mIn.readPointer();
            proxy->getWeakRefs()->decWeak(proxy);
        } break;
        
    case BR_FINISHED:
        result = TIMED_OUT;
        break;
        
    case BR_NOOP: //啥也没做
        break;
        
    case BR_SPAWN_LOOPER:
        mProcess->spawnPooledThread(false);
        break;
        
    default:
        printf("*** BAD COMMAND %d received from Binder driver\n", cmd);
        result = UNKNOWN_ERROR;
        break;
    }

    if (result != NO_ERROR) {
        mLastError = result;
    }
    
    return result;
}



void IPCThreadState::joinThreadPool(bool isMain)
{
    LOG_THREADPOOL("**** THREAD %p (PID %d) IS JOINING THE THREAD POOL\n", (void*)pthread_self(), getpid());

    mOut.writeInt32(isMain ? BC_ENTER_LOOPER : BC_REGISTER_LOOPER);
    
    // This thread may have been spawned by a thread that was in the background
    // scheduling group, so first we will make sure it is in the foreground
    // one to avoid performing an initial transaction in the background.
    set_sched_policy(mMyThreadId, SP_FOREGROUND);
        
    status_t result;
    do {
        processPendingDerefs();
        // now get the next command to be processed, waiting if necessary
        result = getAndExecuteCommand();

        if (result < NO_ERROR && result != TIMED_OUT && result != -ECONNREFUSED && result != -EBADF) {
            ALOGE("getAndExecuteCommand(fd=%d) returned unexpected error %d, aborting",mProcess->mDriverFD, result);
            abort();
        }
        
        // Let this thread exit the thread pool if it is no longer
        // needed and it is not the main process thread.
        if(result == TIMED_OUT && !isMain) {
            break;
        }
    } while (result != -ECONNREFUSED && result != -EBADF);

    LOG_THREADPOOL("**** THREAD %p (PID %d) IS LEAVING THE THREAD POOL err=%p\n",(void*)pthread_self(), getpid(), (void*)result);
    
    mOut.writeInt32(BC_EXIT_LOOPER);
    talkWithDriver(false);
}




int IPCThreadState::getAndExecuteCommand()
{
    int result;
    int32_t cmd;

    result = talkWithDriver(); //核心
    if (result >= NO_ERROR) {
        size_t IN = mIn.dataAvail();
        if (IN < sizeof(int32_t)) return result;
        cmd = mIn.readInt32();
        IF_LOG_COMMANDS() {
            alog << "Processing top-level Command: "
                 << getReturnString(cmd) << endl;
        }

        pthread_mutex_lock(&mProcess->mThreadCountLock);
        mProcess->mExecutingThreadsCount++;
        if (mProcess->mExecutingThreadsCount >= mProcess->mMaxThreads &&
                mProcess->mStarvationStartTimeMs == 0) {
            mProcess->mStarvationStartTimeMs = uptimeMillis();
        }
        pthread_mutex_unlock(&mProcess->mThreadCountLock);

        result = executeCommand(cmd);

        pthread_mutex_lock(&mProcess->mThreadCountLock);
        mProcess->mExecutingThreadsCount--;
        if (mProcess->mExecutingThreadsCount < mProcess->mMaxThreads &&
                mProcess->mStarvationStartTimeMs != 0) {
            int64_t starvationTimeMs = uptimeMillis() - mProcess->mStarvationStartTimeMs;
            if (starvationTimeMs > 100) {
                ALOGE("binder thread pool (%zu threads) starved for %" PRId64 " ms",
                      mProcess->mMaxThreads, starvationTimeMs);
            }
            mProcess->mStarvationStartTimeMs = 0;
        }
        pthread_cond_broadcast(&mProcess->mThreadCountDecrement);
        pthread_mutex_unlock(&mProcess->mThreadCountLock);

        // After executing the command, ensure that the thread is returned to the
        // foreground cgroup before rejoining the pool.  The driver takes care of
        // restoring the priority, but doesn't do anything with cgroups so we
        // need to take care of that here in userspace.  Note that we do make
        // sure to go in the foreground after executing a transaction, but
        // there are other callbacks into user code that could have changed
        // our group so we want to make absolutely sure it is put back.
        set_sched_policy(mMyThreadId, SP_FOREGROUND);
    }

    return result;
}