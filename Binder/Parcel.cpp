#include "Parcel.h"
#include "ProcessState.h"
#include "binder.h"


/**
 * 对数据进行处理，它把从Binder驱动中读取的SM 的回复  mIn 填入reply这个Parcel中，就是赋值操作
*/
void Parcel::ipcSetDataReference(const uint8_t* data, size_t dataSize,const binder_size_t* objects, size_t objectsCount, release_func relFunc, void* relCookie)
{
    binder_size_t minOffset = 0;
    freeDataNoInit();
    mError = NO_ERROR;
    mData = const_cast<uint8_t*>(data);
    mDataSize = mDataCapacity = dataSize;
    //ALOGI("setDataReference Setting data size of %p to %lu (pid=%d)", this, mDataSize, getpid());
    mDataPos = 0;
    ALOGV("setDataReference Setting data pos of %p to %zu", this, mDataPos);
    mObjects = const_cast<binder_size_t*>(objects);
    mObjectsSize = mObjectsCapacity = objectsCount;
    mNextObjectHint = 0;
    mOwner = relFunc;
    mOwnerCookie = relCookie;
    for (size_t i = 0; i < mObjectsSize; i++) {
        binder_size_t offset = mObjects[i];
        if (offset < minOffset) {
            ALOGE("%s: bad object offset %" PRIu64 " < %" PRIu64 "\n",
                  __func__, (uint64_t)offset, (uint64_t)minOffset);
            mObjectsSize = 0;
            break;
        }
        minOffset = offset + sizeof(flat_binder_object);
    }
    scanForFds();
}

/**
 * java层的实现，会调到这里
*/
int Parcel::readStrongBinder(IBinder* val) const
{
    return unflatten_binder(ProcessState::self(), *this, val);
}



int unflatten_binder(const ProcessState* proc,const Parcel& in, IBinder* out)
{
  //这个in里就是从SM返回的数据
    const flat_binder_object* flat = in.readObject(false);  //读取一个Binder Object

    if (flat) {
        switch (flat->type) {
            case BINDER_TYPE_BINDER: //同进程
                *out = reinterpret_cast<IBinder*>(flat->cookie);
                return finish_unflatten_binder(NULL, *flat, in);
            case BINDER_TYPE_HANDLE:  //不同进程
                *out = proc->self()->getStrongProxyForHandle(flat->handle);
                //*out = proc->getStrongProxyForHandle(flat->handle);  //取出handle
                return finish_unflatten_binder(static_cast<BpBinder*>(out->get()), *flat, in);
        }
    }
    return BAD_TYPE;
}


inline static  finish_flatten_binder(const sp<IBinder>& /*binder*/, const flat_binder_object& flat, Parcel* out)
{
    return out->writeObject(flat, false);
}


int flatten_binder(const sp<ProcessState>& /*proc*/,const sp<IBinder>& binder, Parcel* out)
{
    flat_binder_object obj;

    obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    if (binder != NULL) {
        IBinder *local = binder->localBinder(); //不为空
        if (!local) {
            BpBinder *proxy = binder->remoteBinder();
            if (proxy == NULL) {
                ALOGE("null proxy");
            }
            const int32_t handle = proxy ? proxy->handle() : 0;
            obj.type = BINDER_TYPE_HANDLE;
            obj.binder = 0; /* Don't pass uninitialized stack data to a remote process */
            obj.handle = handle;
            obj.cookie = 0;
        } else { //走这个分支
            obj.type = BINDER_TYPE_BINDER;
            obj.binder = reinterpret_cast<uintptr_t>(local->getWeakRefs());
            obj.cookie = reinterpret_cast<uintptr_t>(local);
        }
    } else {
        obj.type = BINDER_TYPE_BINDER;
        obj.binder = 0;
        obj.cookie = 0;
    }

    return finish_flatten_binder(binder, obj, out);
}


int Parcel::writeStrongBinder(const sp<IBinder>& val)
{
    return flatten_binder(ProcessState::self(), val, this);
}




int Parcel::writeObject(const flat_binder_object& val, bool nullMetaData){
    const bool enoughData = (mDataPos+sizeof(val)) <= mDataCapacity;
    const bool enoughObjects = mObjectsSize < mObjectsCapacity;
    if (enoughData && enoughObjects) {
restart_write:
        *reinterpret_cast<flat_binder_object*>(mData+mDataPos) = val;

        // remember if it's a file descriptor
        if (val.type == BINDER_TYPE_FD) {
            if (!mAllowFds) {
                // fail before modifying our object index
                return FDS_NOT_ALLOWED;
            }
            mHasFds = mFdsKnown = true;
        }

        // Need to write meta-data?
        if (nullMetaData || val.binder != 0) {
            mObjects[mObjectsSize] = mDataPos;
            acquire_object(ProcessState::self(), val, this, &mOpenAshmemSize);
            mObjectsSize++;
        }

        return finishWrite(sizeof(flat_binder_object));
    }

    if (!enoughData) {
        const int err = growData(sizeof(val));
        if (err != NO_ERROR) return err;
    }
    if (!enoughObjects) {
        size_t newSize = ((mObjectsSize+2)*3)/2;
        if (newSize < mObjectsSize) return NO_MEMORY;   // overflow
        binder_size_t* objects = (binder_size_t*)realloc(mObjects, newSize*sizeof(binder_size_t));
        if (objects == NULL) return NO_MEMORY;
        mObjects = objects;
        mObjectsCapacity = newSize;
    }

    goto restart_write;
}


