
#include "jni.h"
#include "Parcel.h"


static void android_os_Parcel_writeStrongBinder(JNIEnv* env, jclass clazz, jlong nativePtr, jobject object){
    Parcel* parcel = reinterpret_cast<Parcel*>(nativePtr);
    if (parcel != NULL) {
        //object是一个IBinder对象 需要通过  ibinderForJavaObject 转成c的IBinder
        const status_t err = parcel->writeStrongBinder(ibinderForJavaObject(env, object));
        if (err != NO_ERROR) {
            signalExceptionForError(env, clazz, err);
        }
    }
}



static jlong android_os_Parcel_create(JNIEnv* env, jclass clazz){
    Parcel* parcel = new Parcel(); //很简单就是New了一个对象
    return reinterpret_cast<jlong>(parcel);   //转成jlong 因为parcel是一个指针，是一个地址值，可以直接这么转换
}
