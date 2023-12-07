//这里的sp 是StrongPointer的意思不是SmartPointer

template <typename T>
class sp{
  public:
    inline sp():m_ptr(0){}


    sp(T* other);//常用构造函数
    ~sp(); //析构函数
    sp& operator=(T* other);//重载运算符=

    inline T& operator*() const{ return *m_ptr; } //重载运算符*
    inline T* operator->() const{ reutrn m_ptr; } //重载运算符->
    inline T* get() const



  private:
    template<typename Y> friend class sp;
    template<typename Y> friend class wp;
    void set_pointer(T* ptr);
    T* m_ptr;

};


template<typename T>
sp<T>& sp<T>::operator=(T* other){
  if(other) other->incStrong(this); //增加引用计数  引用的对象本身增加引用计数
  if(m_ptr) m_ptr->decStrong(this);  //m_ptr不为空，说明之前已经引用了别的对象，先减少引用计数后
  m_ptr = other; //再引用到新对象
  return *this;
}

template<typename T>
sp<T>::sp(T* other):m_ptr(other){ //sp不一定是等号，也可以是构造函数  分配给一个对象
  if(other) other->incStrong(this); //因为是构造函数，不用担心m_ptr之前已经赋过值 因为这个是第一次调用
}


template<typename T>
sp<T>::~sp(){
  if(m_ptr)m_ptr->decStrong(this);
}

//强指针和SmartPointer的实现基本相同




struct CDad
{
  CChild *myChild;  //引用了CChild的，CChild引用计数不为零 
};



struct CChild
{
  CDad *myChild; //引用了CDad的，CDad引用计数不为零  好像死锁了
};

//解决上面的问题，CDad强指针引用CChild  CChild用弱指针引用CDad 规定当强引用计数为0时，不论弱引用是否为0都可以delete自己



//！！！！弱指针必须先升级为强指针，才能访问它所指赂的目标对象，弱指针主要使命就是解决循环引用的问题。















