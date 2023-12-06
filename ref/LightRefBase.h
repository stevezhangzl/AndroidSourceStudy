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

    typedef typename 



  private:
    template<typename Y> friend class sp;
    template<typename Y> friend class wp;

    T*  m_ptr;
    //weakref_type* m_refs;
};



