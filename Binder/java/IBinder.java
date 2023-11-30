//frameworks/base/core/java/android/os/IBinder.java
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/base/core/java/android/os/IBinder.java?hl=zh-cn

public interface IBinder{
  //这个业务码就是1
  int FIRST_CALL_TRANSACTION = 0x00000001;



  public IIterface queryLocalInterface(String descriptor);

  public boolean transact(int code,Parcel reply,int flags) throws RemoteException;
}