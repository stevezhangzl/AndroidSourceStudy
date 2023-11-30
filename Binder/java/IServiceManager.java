//frameworks/base/core/java/android/os/IServiceManager.java

public interface IServiceManager{
  public IBinder getService(String name) throws RemoteException;
  public IBinder checkService() throws RemoteException;
  public void addService(String name,IBinder service,boolean allowIsolated) throws RemoteException;
  public String[] listServices() throws RemoteException;



  static final String descriptor = "android.os.IServiceManager";


  //这里FIRST_CALL_TRANSACTION是1  和service_manager.c中的 SVC_MGR_GET_SERVICE=1 对上了
  int GET_SERVICE_TRANSACTION = IBinder.FIRST_CALL_TRANSACTION;
  int CHECK_SERVICE_TRANSACTION = IBinder.FIRST_CALL_TRANSACTION + 1;
  int ADD_SERVICE_TRANSACTION = IBinder.FIRST_CALL_TRANSACTION + 2;
  int LIST_SERVICES_TRANSACTION = IBinder.FIRST_CALL_TRANSACTION + 3;
  int CHECK_SERVICES_TRANSACTION = IBinder.FIRST_CALL_TRANSACTION + 4;
  int SET_PERMISSION_CONTROLLER_TRANACTION = IBinder.FIRST_CALL_TRANSACTION + 5;
}