cmake_minimum_required(VERSION 3.24)
project(AndroidSourceStudy)

set(CMAKE_CXX_STANDARD 17)

include_directories(Binder)
include_directories(Binder/servicemanager)
include_directories(mutex)
include_directories(ref)

add_executable(AndroidSourceStudy
        Binder/driver/binder.c
        Binder/servicemanager/binder.c
        Binder/servicemanager/binder.h
        Binder/android_util_Binder.cpp
        Binder/Binder.h
        Binder/BpBinder.cpp
        Binder/BpBinder.h
        Binder/IBinder.h
        Binder/IPCThreadState.cpp
        Binder/IPCThreadState.h
        Binder/Parcel.cpp
        Binder/Parcel.h
        Binder/ProcessState.cpp
        Binder/ProcessState.h
        Binder/service_manager.c
        mutex/Barrier.h
        mutex/Condition.h
        mutex/Mutex.h
        mutex/ReaderWriterMutex.h
        mutex/Timers.h
        ref/RefBase.cpp
        ref/RefBase.h
        ref/SmartPointer.h
        ref/StrongPointer.h)
