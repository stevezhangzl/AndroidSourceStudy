

#include "IBinder.h"

class IPCThreadState;

class ProcessState{

  public:
    //保证同一个进程中只有一个ProcessState实例存在，而且只有在ProcessState对象创建时才打开Binder设备以及做内存映射
    static ProcessState* self();

    IBinder& getContextObject(const IBinder& caller);
    IBinder& getStrongProxyForHandle(int handle);

  private:
    friend class IPCThreadState;
    ProcessState();
    ~ProcessState();


    //全局列表来记录所有与Binder对象相关的信息，每个表项是一个handle_entry 
    struct handle_entry {
        IBinder* binder;  //实际上就是一个BpBinder
        //RefBase::weakref_type* refs;
    };


    int mDriverFD;
    void* mVMStart;



};