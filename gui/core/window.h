//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:system/core/include/system/window.h?hl=zh-cn


struct ANativeWindow
{
#ifdef __cplusplus
    ANativeWindow(): flags(0), minSwapInterval(0), maxSwapInterval(0), xdpi(0), ydpi(0)
    {
        common.magic = ANDROID_NATIVE_WINDOW_MAGIC;
        common.version = sizeof(ANativeWindow);
        memset(common.reserved, 0, sizeof(common.reserved));
    }

   
    void incStrong(const void* /*id*/) const {
        common.incRef(const_cast<android_native_base_t*>(&common));
    }
    void decStrong(const void* /*id*/) const {
        common.decRef(const_cast<android_native_base_t*>(&common));
    }
#endif

    struct android_native_base_t common;

    //与surface或updater有关的属性
    const uint32_t flags;

    //所支持的最小交换间隔时间
    const int   minSwapInterval;

    //所支持的最大交换间隔时间
    const int   maxSwapInterval;

    //水平方向的密度,以dpi为单位
    const float xdpi;
    //垂直方向的密度，以dpi为单位
    const float ydpi;

    //为OEM定制驱动所保留的空间
    intptr_t    oem[4];


    //设置交换间隔时间
    int     (*setSwapInterval)(struct ANativeWindow* window,int interval);

    int     (*dequeueBuffer_DEPRECATED)(struct ANativeWindow* window,struct ANativeWindowBuffer** buffer);

    
    int     (*lockBuffer_DEPRECATED)(struct ANativeWindow* window,struct ANativeWindowBuffer* buffer);



    int     (*queueBuffer_DEPRECATED)(struct ANativeWindow* window,struct ANativeWindowBuffer* buffer);

    //用于向本地窗口咨询相关信息
    int     (*query)(const struct ANativeWindow* window,int what, int* value);

    //用于执行本地窗口支持的各种操作
    int     (*perform)(struct ANativeWindow* window,int operation, ... );


    int     (*cancelBuffer_DEPRECATED)(struct ANativeWindow* window,struct ANativeWindowBuffer* buffer);

    //EGL通过这个接口来申请一个buffer,从前面我们所举的例子来说，两个本地窗口所提供的buffer分别来自  帧缓冲区   和内存空间，dequeue 出队列。这从侧面告诉我们，一个window所包含的的buffer很可能不止一份
    int     (*dequeueBuffer)(struct ANativeWindow* window,struct ANativeWindowBuffer** buffer, int* fenceFd);

    //当EGL对一块buffer渲染完成后，调用这个来unlock和post buffer
    int     (*queueBuffer)(struct ANativeWindow* window,struct ANativeWindowBuffer* buffer, int fenceFd);

    //取消一个已经dequed的buffer，但要特别注意同步的问题
    int     (*cancelBuffer)(struct ANativeWindow* window,struct ANativeWindowBuffer* buffer, int fenceFd);
};
