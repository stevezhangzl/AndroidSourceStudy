//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/base/core/java/android/os/Binder.java?hl=zh-cn





public class Binder implements IBinder{

    public Binder(){
        init(); //这里会调用native中的binder_init 生成JavaBBinder
    }

    public final boolean transact(int code, Parcel data, Parcel reply,int flags) throws RemoteException {
        if (false) Log.v("Binder", "Transact: " + code + " to " + this);

        if (data != null) {
            data.setDataPosition(0);
        }
        boolean r = onTransact(code, data, reply, flags);
        if (reply != null) {
            reply.setDataPosition(0);
        }
        return r;
    }


    public IInterface queryLocalInterface(String descriptor) {
      if (mDescriptor.equals(descriptor)) {
          return mOwner;
      }
      return null;
    }



    private native final void init();
    private native final void destroy();


    /* mObject is used by native code, do not remove or rename */
    private long mObject;
    private IInterface mOwner;
    private String mDescriptor;
}


final class BinderProxy implements IBinder{
  
   public boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
      Binder.checkParcel(this, code, data, "Unreasonably large binder buffer");
      if (Binder.isTracingEnabled()) { Binder.getTransactionTracker().addTrace(); }
      //调用JNI  实际上调用的是c层的transact
      return transactNative(code, data, reply, flags);
  } 

  public IInterface queryLocalInterface(String descriptor) {
      return null;
  }
  
  //调用的是android_util_Binder.cpp中的 android_os_BinderProxy_transact 函数
  public native boolean transactNative(int code, Parcel data, Parcel reply,int flags) throws RemoteException;
}