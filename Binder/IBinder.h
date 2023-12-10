//frameworks/native/include/binder/IBinder.h
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/native/include/binder/IBinder.h?hl=zh-cn



#include <string>
#include "ref/RefBase.h"


class BpBinder;
class IInterface;
class Parcel;





class IBinder : public virtual RefBase{
  public:
    enum{
      FIRST_CALL_TRANSACTION = 0x00000001,
    };
    virtual IInterface queryLocalInterface(const std::string& descriptor);
    virtual int transact(int code,const Parcel& data,Parcel* reply,int flags = 0) = 0;

  protected:
    virtual ~IBinder();
};