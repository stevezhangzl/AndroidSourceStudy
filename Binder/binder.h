#include "IBinder.h"
#include <atomic>


class BBinder : public IBinder{
  public:
    BBinder();

    virtual int transact(int code,const Parcel& data,Parcel* reply,int flags = 0);

  protected:
    virtual ~BBinder();
    virtual int onTransact(int code,const Parcel& data,Parcel* reply,int flags = 0);


  private:
    BBinder(const BBinder& o); //声明为私有，不允许通过拷贝构造函数创建对象的完全相同副本
    BBinder& operator=(const BBinder& o);

    class Extras;



};