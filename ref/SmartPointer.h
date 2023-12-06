template <typename T>
class SmartPointer{
  private:
    T* m_ptr; //指向object对象


  public:
    inline SmartPointer():m_ptr(0){} //构造函数应将m_ptr置空
    //~wp();
    SmartPointer& operator = (T* other);//重载运算符
};



template<typename T>
SmartPointer<T>& SmartPointer<T>::operator=(T* other){
  if (other!=null){
    m_ptr = other; //指向内存对象
    other->incStrong();//主动增加计数值
  }

  return *this;
  
}

// template<typename T>
// wp<T>::~wp(){
  
// }



