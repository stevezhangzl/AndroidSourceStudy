//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/rs/cpp/util/RefBase.h;bpv=1?hl=zh-cn

#include "StrongPointer.h"


//和LightRefBase不同的是 RefBase不直接使用int 变量来保存引用计数值，而是采用了weakref_type类型的计数器

class RefBase{ //嵌套类的作用： 作为外部的类直接访问嵌套类，而外部的代码无法直接访问，外部的代码不必直接与weakref_type打交道，而是通过RefBase类提供的接口来处理，提交了类的封装性（直观类之间的关系）
  public:
    void incStrong(const void* id) const;
    void decStrong(const void* id) const;
    void onFirstRef();
    void onLastStrongRef(const void* id) const;

    class weakref_type{
      public:
        RefBase* refBase() const;
        void incWeak(const void* id);
        void decWeak(const void* id);

    };

    weakref_type* createWeak(const void* id) const;
    weakref_type* getWeakRefs() const;

    typedef RefBase basetype;

  protected:
    RefBase();
    virtual ~RefBase();

    enum{
      OBJECT_LIFETIME_STRONG = 0X0000,
      OBJECT_LIFETIME_WEAK = 0X0001,
      OBJECT_LIFETIME_MASK = 0X0001
    };

  private:
    friend class weakref_type;
    class weakref_impl;

    weakref_impl* const mRefs;
};




//相当于java中的object
template <class T>
class LightRefBase{
public:
  inline LightRefBase():mCount(0){}
  inline void incStrong() const{ //增加引用计数
    //android_atomic_inc(&mCount);
  }

  inline void decStrong() const{ //减少引用计数
    delete static_cast<const T*>(this); //删除内存对象
  }


protected:
  inline ~LightRefBase(){}


private:
  mutable volatile int32_t mCount; //引用计数值
};


template <typename T>
class wp{
  public:
    //typename告诉编译器，RefBase::weakref_type是一个类型，而不是一个成员变量或静态成员函数
    typedef typename RefBase::weakref_type weakref_type;

    inline wp():m_ptr(0){}

    wp(T *other); //构造函数

    ~wp();
    wp& operator = (T* other);//运算符重载

    sp<T> promote() const; //升级为强指针


    //与sp相比，wp 没有重载 -> * 等运算符

  private:

    template<typename Y> friend class sp;
    template<typename Y> friend class wp;
    T* m_ptr; //指向目标对象(继承自ReBase)
    weakref_type * m_refs; //与sp相比多了一个m_refs
};


template<typename T>
wp<T>::wp(T* other):m_ptr(other){
  if (other){
    m_refs = other->createWeak(this);
  }
  
}