

//system/core/libutils/include/utils/Mutex.h
#include <pthread.h>
#include <sys/types.h>
#include <stdint.h>


namespace android{

class Mutex{

  public:
    enum{
      PRIVATE = 0,
      SHARED = 1
    };
    Mutex();
    explicit Mutex(const char *name);
    explicit Mutex(int type,const char *name = nullptr);
    ~Mutex();

    int lock();
    void unlock();

    int tryLock();


    class Autolock{
      public:
        inline explicit Autolock(Mutex& mutex):mLock(mutex){
          mLock.lock();
        }

        inline explicit Autolock(Mutex* mutex):mLock(*mutex){
          mLock.lock();
        }

        inline ~Autolock(){
          mLock.unlock();
        }

      private:
        Mutex& mLock;
        Autolock(const Autolock&);
        Autolock& operator=(const Autolock&);
    };


  private:
    friend class Condition;

    Mutex(const Mutex&);
    Mutex& operator=(const Mutex&);
    pthread_mutex_t mMutex;
  };


inline Mutex::Mutex(){
  pthread_mutex_init(&mMutex,nullptr);
}

inline Mutex::Mutex(int type,__attribute__((unused)) const char *name){
  if (type == SHARED){
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr,PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&mMutex, &attr);
    pthread_mutexattr_destroy(&attr);
  }else{
    pthread_mutex_init(&mMutex, nullptr);
  }
  
}

inline Mutex::~Mutex(){
  pthread_mutex_destroy(&mMutex);
}

inline int Mutex::lock(){
  return -pthread_mutex_lock(&mMutex);
}

inline void Mutex::unlock(){
  pthread_mutex_unlock(&mMutex);
}

inline int Mutex::tryLock() {
    return -pthread_mutex_trylock(&mMutex);
}



typedef Mutex::Autolock AutoMutex;

}



