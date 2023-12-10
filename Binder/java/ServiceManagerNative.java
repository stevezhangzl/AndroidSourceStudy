//frameworks/base/core/java/android/os/ServiceManagerNative.java
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/base/core/java/android/os/ServiceManagerNative.java?hl=zh-cn

public abstract class ServiceManagerNative extends Binder implements IServiceManager{


  /**
   * 把一个binder对象转化成IServiceManager 并在必要时创建ServiceManagerProxy
   */
  static public IServiceManager asInterface(IBinder obj){
    if(obj == null){
      return null;
    }
    IServiceManager in = (IServiceManager)obj.queryLocalInterface(descriptor);
    if(in != null){
      return in;
    }

    //这个obj 也就是IBinder也就是mRemote
    return new ServiceManagerProxy(obj);
  }
}


//Binder Client 获取的就是ServiceManagerProxy对象去跨进程访问Binder Server 实际上借助的就是
//IBinder 这个IBinder在java层就是BinderProxy通过BinderInternal.getContextObject实现的
class ServiceManagerProxy implements IServiceManager{

  public ServiceManagerProxy(IBinder remote){
    mRemote = remote;
  }

  public IBinder getService(String name) throws RemoteException{
    //1.准备数据-通过Parcel打包数据
    Parcel data = Parcel.obtain();
    Parcel reply = Parcel.obtain();
    data.writeInterfaceToken(IServiceManager.descriptor);
    data.writeString(name);
    //利用IBinder对象执行命令
    //2.利用IBinder的transact将请求发送出去，而不用理会Binder驱动的open mmap一大堆具体的Binder协议中的命令，
    //所以这个IBinder一定会在内部使用ProcessState和IPCThreadState来与Binder驱动进行通信。
    mRemote.transact(GET_SERVICE_TRANSACTION,data,reply,0);
    //transact后，我们就可以直接获取到结果了。如果大家用过socket，就知道这是一种阻塞式的函数调用。因为涉及进程
    //间的通信，结果并不是马上就能获取到，所以Binder驱动一定要先将调用者线程挂起，直到有了结果才能把它唤醒。
    //这样做的好处就是调用者可以像进程内函数调用一样去编写程序，而不必考虑很多IPC的细节。
    IBinder binder = reply.readStringBinder(); //这个也是JNI 调的native层的Parcel.cpp中的readStrongBinder()
    reply.recycle();
    data.recycle();
    return binder;
  }


  public void addService(String name, IBinder service, boolean allowIsolated)throws RemoteException {
    Parcel data = Parcel.obtain();
    Parcel reply = Parcel.obtain();
    data.writeInterfaceToken(IServiceManager.descriptor);
    data.writeString(name);
    data.writeStrongBinder(service); //写入一个StrongBinder 这里调用的是anroid_os_Parcel.cpp中的 writeStrongBinder
    data.writeInt(allowIsolated ? 1 : 0);
    mRemote.transact(ADD_SERVICE_TRANSACTION, data, reply, 0);
    reply.recycle();
    data.recycle();
  }


  //IBinder是餐馆的电话号码，先记下来，等需要的时候通过这个号码获取餐馆提供的服务。比如getService()这个接口
  private IBinder mRemote;

}

