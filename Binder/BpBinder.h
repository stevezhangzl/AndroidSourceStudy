//frameworks/native/include/binder/BpBinder.h
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/native/include/binder/BpBinder.h?hl=zh-cn


#include "IBinder.h"

class BpBinder : public IBinder{
  public:
    virtual int transact(int code,const Parcel& data,Parcel* reply,int flags = 0);


    BpBinder(int handle);


  

  protected:
    virtual ~BpBinder();


  private:
    const int mHandle;
};