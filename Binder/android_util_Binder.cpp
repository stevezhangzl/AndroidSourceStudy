//frameworks/base/core/jni/android_util_Binder.cpp
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/base/core/jni/android_util_Binder.cpp?hl=zh-cn

#include <jni.h>
#include "ref/RefBase.h"
#include "Binder.h"
#include "Mutex.h"
#include "IPCThreadState.h"

static struct binderproxy_offsets_t
{
    // Class state.
    jclass mClass;
    jmethodID mConstructor;
    jmethodID mSendDeathNotice;

    // Object state.
    jfieldID mObject; //记录的是BinderProxy中的mObject的jfieldID
    jfieldID mSelf;
    jfieldID mOrgue;

} gBinderProxyOffsets;


static struct binderinternal_offsets_t{
    jclass mClass;
    jmethodID mExecTransact;

    jfieldID mObject;
} gBinderOffsets;


static struct error_offsets_t
{
    jclass mClass;
} gErrorOffsets;


static struct log_offsets_t
{
    // Class state.
    jclass mClass;
    jmethodID mLogE;
} gLogOffsets;


static JavaVM* jnienv_to_javavm(JNIEnv* env){
    JavaVM* vm;
    return env->GetJavaVM(&vm) >= 0 ? vm : NULL;
}


static JNIEnv* javavm_to_jnienv(JavaVM* vm){
    JNIEnv* env;
    return vm->GetEnv((void **)&env, JNI_VERSION_1_4) >= 0 ? env : NULL;
}


//c 往 java层抛异常
static void report_exception(JNIEnv* env, jthrowable excep, const char* msg){
    env->ExceptionClear();

    jstring tagstr = env->NewStringUTF(LOG_TAG);
    jstring msgstr = NULL;
    if (tagstr != NULL) {
        msgstr = env->NewStringUTF(msg);
    }

    if ((tagstr == NULL) || (msgstr == NULL)) {
        env->ExceptionClear();      /* assume exception (OOM?) was thrown */
        goto bail;
    }

    env->CallStaticIntMethod(gLogOffsets.mClass, gLogOffsets.mLogE, tagstr, msgstr, excep);
    if (env->ExceptionCheck()) {
        /* attempting to log the failure has failed */
        env->ExceptionClear();
    }

    if (env->IsInstanceOf(excep, gErrorOffsets.mClass)) {
        env->Throw(excep);
        env->ExceptionDescribe();
        ALOGE("Forcefully exiting");
        exit(1);
    }

bail:
    /* discard local refs created for us by VM */
    env->DeleteLocalRef(tagstr);
    env->DeleteLocalRef(msgstr);
}



static jobject android_os_BinderInternal_getContextObject(JNIEnv* env,jobject clazz){
  sp<IBinder> b = ProcessState::self()->getContextObject(NULL); //这里ProcessState返回的是BpBinder
  //将BpBinder 转成java层的 BinderProxy 一个是c层的IBinder的实现 一个是java层的IBinder的实现  
  return javaObjectForIBinder(env,b); //
}


static jboolean android_os_BinderProxy_transact(JNIEnv* env, jobject obj,jint code, jobject dataObj, jobject replyObj, jint flags) // throws RemoteException
{
    if (dataObj == NULL) {
        jniThrowNullPointerException(env, NULL);
        return JNI_FALSE;
    }

    //先把java的Parcel 转成native中的Parcel
    Parcel* data = parcelForJavaObject(env, dataObj);
    if (data == NULL) {
        return JNI_FALSE;
    }
    Parcel* reply = parcelForJavaObject(env, replyObj);
    if (reply == NULL && replyObj != NULL) {
        return JNI_FALSE;
    }

    //这里拿到提 BpBinder 对象的内存地址
    IBinder* target = (IBinder*)env->GetLongField(obj, gBinderProxyOffsets.mObject);
    if (target == NULL) {
        jniThrowException(env, "java/lang/IllegalStateException", "Binder has been finalized!");
        return JNI_FALSE;
    }

    ALOGV("Java code calling transact on %p in Java object %p with code %" PRId32 "\n",target, obj, code);


    bool time_binder_calls;
    int64_t start_millis;
    if (kEnableBinderSample) {
        // Only log the binder call duration for things on the Java-level main thread.
        // But if we don't
        time_binder_calls = should_time_binder_calls();

        if (time_binder_calls) {
            start_millis = uptimeMillis();
        }
    }

    //printf("Transact from Java code to %p sending: ", target); data->print();
    //这里调用的是BpBinder.cpp的trasact
    status_t err = target->transact(code, *data, reply, flags);
    //if (reply) printf("Transact from Java code to %p received: ", target); reply->print();

    if (kEnableBinderSample) {
        if (time_binder_calls) {
            conditionally_log_binder_call(start_millis, target, code);
        }
    }

    if (err == NO_ERROR) {
        return JNI_TRUE;
    } else if (err == UNKNOWN_TRANSACTION) {
        return JNI_FALSE;
    }

    signalExceptionForError(env, obj, err, true /*canThrowRemoteException*/, data->dataSize());
    return JNI_FALSE;
}

class JavaBBinderHolder;


static void android_os_Binder_init(JNIEnv* env, jobject obj){
    //这里new一个native层的JavaBBinderHolder对象  内部持有一个JavaBBinder的弱引用  提供一个get方法并转成强引用（通过promote）
    JavaBBinderHolder* jbh = new JavaBBinderHolder();
    if (jbh == NULL) {
        jniThrowException(env, "java/lang/OutOfMemoryError", NULL);
        return;
    }
    jbh->incStrong((void*)android_os_Binder_init); //增加强引用计数
    env->SetLongField(obj, gBinderOffsets.mObject, (jlong)jbh); //通过JNI把JavaBBinderHolder的指针值保存在Java Binder对象中
}



class JavaBBinder : public BBinder{
    public:
        JavaBBinder(JNIEnv* env,jobject object):mVM(jnienv_to_javavm(env)),mObject(env->NewGlobalRef(object)){
            android_atomic_inc(&gNumLocalRefs);
        }


    protected:
        virtual ~JavaBBinder(){
            JNIEnv* env = javavm_to_jnienv(mVM);
            env->DeleteGlobalRef(mObject);
        }

        virtual int onTransact(int code,const Parcel& data,Parcel* reply,int flags = 0){
            JNIEnv* env = javavm_to_jnienv(mVM);

            IPCThreadState* thread_state = IPCThreadState::self();

            jboolean res = env->CallBooleanMethod(mObject,gBinderOffsets.mExectTransact,code,reinterpret_cast<jlong>(&data),reinterpret_cast<jlong>(reply),flasg);
            
        }



    private:
        JavaVM* const mVM;
        jobject const mObject;

};

class JavaBBinderHolder : RefBase{
    public:
        sp<JavaBBinder> get(JNIEnv* env,jobject obj){
            android::AutoMutex _l(mLock);
            sp<JavaBBinder> b = mBinder.promote(); //把弱引用升级为强引用
            if (b == NULL){
                b = new JavaBBinder(env,obj);
                mBinder = b;
            }

            return b;
        }


    private:
        android::Mutex mLock;
        wp<JavaBBinder> mBinder;
}


//把java的IBinder 转成c的IBinder
sp<IBinder> ibinderForJavaObject(JNIEnv* env, jobject obj){
    if (obj == NULL) return NULL;

    //obj是一个Java对象  这里是IBinder   IsInstanceOf判断和java的 instanceof是一个意思，判断是否是class的实例
    if (env->IsInstanceOf(obj, gBinderOffsets.mClass)) { //这里Binder.mObject 保存的是JavaBBinderHolder的引用  取出来
        JavaBBinderHolder* jbh = (JavaBBinderHolder*)env->GetLongField(obj, gBinderOffsets.mObject);
        return jbh != NULL ? jbh->get(env, obj) : NULL;  //通过get拿到 JavaBBinder 这个就是c的BBinder 而且拿的是一个强指针
    }

    if (env->IsInstanceOf(obj, gBinderProxyOffsets.mClass)) {
        return (IBinder*)env->GetLongField(obj, gBinderProxyOffsets.mObject);
    }

    ALOGW("ibinderForJavaObject: %p is not a Binder object", obj);
    return NULL;
}