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
  if(other) other->incStrong(this); //增加引用计数
  if(m_ptr) m_ptr->decStrong(this);
  m_ptr = other;
  return *this;
}

template<typename T>
sp<T>::sp(T* other):m_ptr(other){
  if(other) other->incStrong(this); //因为是构造函数，不用担心m_ptr之前已经赋过值
}


template<typename T>
sp<T>::~sp(){
  if(m_ptr)m_ptr->decStrong(this);
}




