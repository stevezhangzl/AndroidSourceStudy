public final class Parcel{

  
  private long mNativePtr;


  //java层的Parcel 创建 在native层也会创建一个Parcel对应的是通过nativeCreate()
  private Parcel(long nativePtr){
    init(nativePtr);
  }


  private void init(long nativePtr){
    if(nativePtr!=0){
      mNativePtr = nativePtr;
    }else{
      mNativePtr = nativeCreate();  
    }
  }



  private static native long nativeCreate();
}