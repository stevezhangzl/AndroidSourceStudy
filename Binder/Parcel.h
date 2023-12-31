

class flat_binder_object;

class Parcel{
  public:
    Parcel();
    ~Parcel();



    void ipcSetDataReference(const uint8_t* data, size_t dataSize,const binder_size_t* objects, size_t objectsCount, release_func relFunc, void* relCookie);


    int readStrongBinder(IBinder* val) const;

    int writeStrongBinder(const sp<IBinder>& val)

    int writeObject(const flat_binder_object& val, bool nullMetaData);
};