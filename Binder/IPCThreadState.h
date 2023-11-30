#include "Parcel.h"
#include "ProcessState.h"


class IPCThreadState{
  public:
    static IPCThreadState* self();

    int    transact(int handle,int code,const Parcel& data,Parcel* reply,int flags);
    int waitForResponse(Parcel *reply, int *acquireResult);
    int talkWithDriver(bool doReceive=true);

  private:
    IPCThreadState();
    ~IPCThreadState();

    static void threadDestructor(void *st);


    const ProcessState* mProcess;
    const pid_t mMyThreadId;
    int mStrictModePolicy;
    Parcel mIn;
    Parcel mOut;

};