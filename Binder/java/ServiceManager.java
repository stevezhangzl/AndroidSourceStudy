//frameworks/base/core/java/android/os/ServiceManager.java
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/base/core/java/android/os/ServiceManager.java?hl=zh-cn


public final class ServiceManager{

  private static final String TAG = "ServiceManager";

  private static IServiceManager sServiceManager;
  private static HashMap<String, IBinder> sCache = new HashMap<String, IBinder>();

  private static IServiceManager getIServiceManager(){
    if(sServiceManager != null){
      sServiceManager = ServiceManagerNative.asInterface(BinderInternal.getContextObject());
      return sServiceManager;
    }
  }



  public static IBinder getService(String name){
    try{
      IBinder service = sCache.get(name);//查询缓存
      if(service != null){
        return service; //从缓存中找到结果，直接返回
      }else{
        return getIServiceManager().getService(name);//向SM发起查询
      }

    }catch(RemoteException e){

    }
    return null;
  }


  public static void addService(String name, IBinder service, boolean allowIsolated) {
    try {
        getIServiceManager().addService(name, service, allowIsolated);
    } catch (RemoteException e) {
        Log.e(TAG, "error in addService", e);
    }
  }


  public static void addService(String name, IBinder service) {
    try {
        getIServiceManager().addService(name, service, false);
    } catch (RemoteException e) {
        Log.e(TAG, "error in addService", e);
    }
  }

}