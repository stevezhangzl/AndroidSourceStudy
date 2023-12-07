template <typename T>  //和template <class T>是等效的，愿意用哪个，看喜欢
class SmartPointer{
  private:
    T* m_ptr; //指向object对象


  public:
    inline SmartPointer():m_ptr(0){} //技巧 构造函数应将m_ptr置空
    //~wp();
    SmartPointer& operator = (T* other);//重载运算符  因为智能指针引用(=)对象object后，就要调用object的incStrong 增加引用计数

    ~SmartPointer();
};



template<typename T>
SmartPointer<T>& SmartPointer<T>::operator = (T* other){
  if (other!=null){
    m_ptr = other; //指向内存对象  指向这个object对象
    other->incStrong();//主动增加计数值
  }

  return *this;
  
}

template<typename T>
SmartPointer<T>::~SmartPointer(){
  if(m_ptr) m_ptr->decStrong();
}



