//frameworks/base/core/jni/android_util_Binder.cpp
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/base/core/jni/android_util_Binder.cpp?hl=zh-cn

#include <jni.h>

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