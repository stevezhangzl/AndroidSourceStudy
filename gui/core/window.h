//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:system/core/include/system/window.h?hl=zh-cn




/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SYSTEM_CORE_INCLUDE_ANDROID_WINDOW_H
#define SYSTEM_CORE_INCLUDE_ANDROID_WINDOW_H

#include <cutils/native_handle.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/cdefs.h>
#include <system/graphics.h>
#include <unistd.h>

#ifndef __UNUSED
#define __UNUSED __attribute__((__unused__))
#endif
#ifndef __deprecated
#define __deprecated __attribute__((__deprecated__))
#endif

__BEGIN_DECLS

/*****************************************************************************/

#define ANDROID_NATIVE_MAKE_CONSTANT(a,b,c,d) \
    (((unsigned)(a)<<24)|((unsigned)(b)<<16)|((unsigned)(c)<<8)|(unsigned)(d))

#define ANDROID_NATIVE_WINDOW_MAGIC \
    ANDROID_NATIVE_MAKE_CONSTANT('_','w','n','d')

#define ANDROID_NATIVE_BUFFER_MAGIC \
    ANDROID_NATIVE_MAKE_CONSTANT('_','b','f','r')

// ---------------------------------------------------------------------------

// This #define may be used to conditionally compile device-specific code to
// support either the prior ANativeWindow interface, which did not pass libsync
// fences around, or the new interface that does.  This #define is only present
// when the ANativeWindow interface does include libsync support.
#define ANDROID_NATIVE_WINDOW_HAS_SYNC 1

// ---------------------------------------------------------------------------

typedef const native_handle_t* buffer_handle_t;

// ---------------------------------------------------------------------------

typedef struct android_native_rect_t
{
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} android_native_rect_t;

// ---------------------------------------------------------------------------

typedef struct android_native_base_t
{
    /* a magic value defined by the actual EGL native type */
    int magic;

    /* the sizeof() of the actual EGL native type */
    int version;

    void* reserved[4];

    /* reference-counting interface */
    void (*incRef)(struct android_native_base_t* base);
    void (*decRef)(struct android_native_base_t* base);
} android_native_base_t;

typedef struct ANativeWindowBuffer
{
#ifdef __cplusplus
    ANativeWindowBuffer() {
        common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
        common.version = sizeof(ANativeWindowBuffer);
        memset(common.reserved, 0, sizeof(common.reserved));
    }

    // Implement the methods that sp<ANativeWindowBuffer> expects so that it
    // can be used to automatically refcount ANativeWindowBuffer's.
    void incStrong(const void* /*id*/) const {
        common.incRef(const_cast<android_native_base_t*>(&common));
    }
    void decStrong(const void* /*id*/) const {
        common.decRef(const_cast<android_native_base_t*>(&common));
    }
#endif

    struct android_native_base_t common;

    int width; //宽
    int height; //高
    int stride;
    int format;
    int usage;

    void* reserved[2];

    buffer_handle_t handle; //代表内存块的句柄，比如ashmem机制。

    void* reserved_proc[8];
} ANativeWindowBuffer_t;

// Old typedef for backwards compatibility.
typedef ANativeWindowBuffer_t android_native_buffer_t;

// ---------------------------------------------------------------------------

/* attributes queriable with query() */
enum {
    NATIVE_WINDOW_WIDTH     = 0,
    NATIVE_WINDOW_HEIGHT    = 1,
    NATIVE_WINDOW_FORMAT    = 2,

    /* The minimum number of buffers that must remain un-dequeued after a buffer
     * has been queued.  This value applies only if set_buffer_count was used to
     * override the number of buffers and if a buffer has since been queued.
     * Users of the set_buffer_count ANativeWindow method should query this
     * value before calling set_buffer_count.  If it is necessary to have N
     * buffers simultaneously dequeued as part of the steady-state operation,
     * and this query returns M then N+M buffers should be requested via
     * native_window_set_buffer_count.
     *
     * Note that this value does NOT apply until a single buffer has been
     * queued.  In particular this means that it is possible to:
     *
     * 1. Query M = min undequeued buffers
     * 2. Set the buffer count to N + M
     * 3. Dequeue all N + M buffers
     * 4. Cancel M buffers
     * 5. Queue, dequeue, queue, dequeue, ad infinitum
     */
    NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS = 3,

    /* Check whether queueBuffer operations on the ANativeWindow send the buffer
     * to the window compositor.  The query sets the returned 'value' argument
     * to 1 if the ANativeWindow DOES send queued buffers directly to the window
     * compositor and 0 if the buffers do not go directly to the window
     * compositor.
     *
     * This can be used to determine whether protected buffer content should be
     * sent to the ANativeWindow.  Note, however, that a result of 1 does NOT
     * indicate that queued buffers will be protected from applications or users
     * capturing their contents.  If that behavior is desired then some other
     * mechanism (e.g. the GRALLOC_USAGE_PROTECTED flag) should be used in
     * conjunction with this query.
     */
    NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER = 4,

    /* Get the concrete type of a ANativeWindow.  See below for the list of
     * possible return values.
     *
     * This query should not be used outside the Android framework and will
     * likely be removed in the near future.
     */
    NATIVE_WINDOW_CONCRETE_TYPE = 5,


    /*
     * Default width and height of ANativeWindow buffers, these are the
     * dimensions of the window buffers irrespective of the
     * NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS call and match the native window
     * size unless overridden by NATIVE_WINDOW_SET_BUFFERS_USER_DIMENSIONS.
     */
    NATIVE_WINDOW_DEFAULT_WIDTH = 6,
    NATIVE_WINDOW_DEFAULT_HEIGHT = 7,

    /*
     * transformation that will most-likely be applied to buffers. This is only
     * a hint, the actual transformation applied might be different.
     *
     * INTENDED USE:
     *
     * The transform hint can be used by a producer, for instance the GLES
     * driver, to pre-rotate the rendering such that the final transformation
     * in the composer is identity. This can be very useful when used in
     * conjunction with the h/w composer HAL, in situations where it
     * cannot handle arbitrary rotations.
     *
     * 1. Before dequeuing a buffer, the GL driver (or any other ANW client)
     *    queries the ANW for NATIVE_WINDOW_TRANSFORM_HINT.
     *
     * 2. The GL driver overrides the width and height of the ANW to
     *    account for NATIVE_WINDOW_TRANSFORM_HINT. This is done by querying
     *    NATIVE_WINDOW_DEFAULT_{WIDTH | HEIGHT}, swapping the dimensions
     *    according to NATIVE_WINDOW_TRANSFORM_HINT and calling
     *    native_window_set_buffers_dimensions().
     *
     * 3. The GL driver dequeues a buffer of the new pre-rotated size.
     *
     * 4. The GL driver renders to the buffer such that the image is
     *    already transformed, that is applying NATIVE_WINDOW_TRANSFORM_HINT
     *    to the rendering.
     *
     * 5. The GL driver calls native_window_set_transform to apply
     *    inverse transformation to the buffer it just rendered.
     *    In order to do this, the GL driver needs
     *    to calculate the inverse of NATIVE_WINDOW_TRANSFORM_HINT, this is
     *    done easily:
     *
     *        int hintTransform, inverseTransform;
     *        query(..., NATIVE_WINDOW_TRANSFORM_HINT, &hintTransform);
     *        inverseTransform = hintTransform;
     *        if (hintTransform & HAL_TRANSFORM_ROT_90)
     *            inverseTransform ^= HAL_TRANSFORM_ROT_180;
     *
     *
     * 6. The GL driver queues the pre-transformed buffer.
     *
     * 7. The composer combines the buffer transform with the display
     *    transform.  If the buffer transform happens to cancel out the
     *    display transform then no rotation is needed.
     *
     */
    NATIVE_WINDOW_TRANSFORM_HINT = 8,

    /*
     * Boolean that indicates whether the consumer is running more than
     * one buffer behind the producer.
     */
    NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND = 9,

    /*
     * The consumer gralloc usage bits currently set by the consumer.
     * The values are defined in hardware/libhardware/include/gralloc.h.
     */
    NATIVE_WINDOW_CONSUMER_USAGE_BITS = 10,

    /**
     * Transformation that will by applied to buffers by the hwcomposer.
     * This must not be set or checked by producer endpoints, and will
     * disable the transform hint set in SurfaceFlinger (see
     * NATIVE_WINDOW_TRANSFORM_HINT).
     *
     * INTENDED USE:
     * Temporary - Please do not use this.  This is intended only to be used
     * by the camera's LEGACY mode.
     *
     * In situations where a SurfaceFlinger client wishes to set a transform
     * that is not visible to the producer, and will always be applied in the
     * hardware composer, the client can set this flag with
     * native_window_set_buffers_sticky_transform.  This can be used to rotate
     * and flip buffers consumed by hardware composer without actually changing
     * the aspect ratio of the buffers produced.
     */
    NATIVE_WINDOW_STICKY_TRANSFORM = 11,

    /**
     * The default data space for the buffers as set by the consumer.
     * The values are defined in graphics.h.
     */
    NATIVE_WINDOW_DEFAULT_DATASPACE = 12,

    /*
     * Returns the age of the contents of the most recently dequeued buffer as
     * the number of frames that have elapsed since it was last queued. For
     * example, if the window is double-buffered, the age of any given buffer in
     * steady state will be 2. If the dequeued buffer has never been queued, its
     * age will be 0.
     */
    NATIVE_WINDOW_BUFFER_AGE = 13,
};

/* Valid operations for the (*perform)() hook.
 *
 * Values marked as 'deprecated' are supported, but have been superceded by
 * other functionality.
 *
 * Values marked as 'private' should be considered private to the framework.
 * HAL implementation code with access to an ANativeWindow should not use these,
 * as it may not interact properly with the framework's use of the
 * ANativeWindow.
 */
enum {
    NATIVE_WINDOW_SET_USAGE                 =  0,
    NATIVE_WINDOW_CONNECT                   =  1,   /* deprecated */
    NATIVE_WINDOW_DISCONNECT                =  2,   /* deprecated */
    NATIVE_WINDOW_SET_CROP                  =  3,   /* private */
    NATIVE_WINDOW_SET_BUFFER_COUNT          =  4,
    NATIVE_WINDOW_SET_BUFFERS_GEOMETRY      =  5,   /* deprecated */
    NATIVE_WINDOW_SET_BUFFERS_TRANSFORM     =  6,
    NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP     =  7,
    NATIVE_WINDOW_SET_BUFFERS_DIMENSIONS    =  8,
    NATIVE_WINDOW_SET_BUFFERS_FORMAT        =  9,
    NATIVE_WINDOW_SET_SCALING_MODE          = 10,   /* private */
    NATIVE_WINDOW_LOCK                      = 11,   /* private */
    NATIVE_WINDOW_UNLOCK_AND_POST           = 12,   /* private */
    NATIVE_WINDOW_API_CONNECT               = 13,   /* private */
    NATIVE_WINDOW_API_DISCONNECT            = 14,   /* private */
    NATIVE_WINDOW_SET_BUFFERS_USER_DIMENSIONS = 15, /* private */
    NATIVE_WINDOW_SET_POST_TRANSFORM_CROP   = 16,   /* private */
    NATIVE_WINDOW_SET_BUFFERS_STICKY_TRANSFORM = 17,/* private */
    NATIVE_WINDOW_SET_SIDEBAND_STREAM       = 18,
    NATIVE_WINDOW_SET_BUFFERS_DATASPACE     = 19,
    NATIVE_WINDOW_SET_SURFACE_DAMAGE        = 20,   /* private */
};

/* parameter for NATIVE_WINDOW_[API_][DIS]CONNECT */
enum {
    /* Buffers will be queued by EGL via eglSwapBuffers after being filled using
     * OpenGL ES.
     */
    NATIVE_WINDOW_API_EGL = 1,

    /* Buffers will be queued after being filled using the CPU
     */
    NATIVE_WINDOW_API_CPU = 2,

    /* Buffers will be queued by Stagefright after being filled by a video
     * decoder.  The video decoder can either be a software or hardware decoder.
     */
    NATIVE_WINDOW_API_MEDIA = 3,

    /* Buffers will be queued by the the camera HAL.
     */
    NATIVE_WINDOW_API_CAMERA = 4,
};

/* parameter for NATIVE_WINDOW_SET_BUFFERS_TRANSFORM */
enum {
    /* flip source image horizontally */
    NATIVE_WINDOW_TRANSFORM_FLIP_H = HAL_TRANSFORM_FLIP_H ,
    /* flip source image vertically */
    NATIVE_WINDOW_TRANSFORM_FLIP_V = HAL_TRANSFORM_FLIP_V,
    /* rotate source image 90 degrees clock-wise, and is applied after TRANSFORM_FLIP_{H|V} */
    NATIVE_WINDOW_TRANSFORM_ROT_90 = HAL_TRANSFORM_ROT_90,
    /* rotate source image 180 degrees */
    NATIVE_WINDOW_TRANSFORM_ROT_180 = HAL_TRANSFORM_ROT_180,
    /* rotate source image 270 degrees clock-wise */
    NATIVE_WINDOW_TRANSFORM_ROT_270 = HAL_TRANSFORM_ROT_270,
    /* transforms source by the inverse transform of the screen it is displayed onto. This
     * transform is applied last */
    NATIVE_WINDOW_TRANSFORM_INVERSE_DISPLAY = 0x08
};

/* parameter for NATIVE_WINDOW_SET_SCALING_MODE */
enum {
    /* the window content is not updated (frozen) until a buffer of
     * the window size is received (enqueued)
     */
    NATIVE_WINDOW_SCALING_MODE_FREEZE           = 0,
    /* the buffer is scaled in both dimensions to match the window size */
    NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW  = 1,
    /* the buffer is scaled uniformly such that the smaller dimension
     * of the buffer matches the window size (cropping in the process)
     */
    NATIVE_WINDOW_SCALING_MODE_SCALE_CROP       = 2,
    /* the window is clipped to the size of the buffer's crop rectangle; pixels
     * outside the crop rectangle are treated as if they are completely
     * transparent.
     */
    NATIVE_WINDOW_SCALING_MODE_NO_SCALE_CROP    = 3,
};

/* values returned by the NATIVE_WINDOW_CONCRETE_TYPE query */
enum {
    NATIVE_WINDOW_FRAMEBUFFER               = 0, /* FramebufferNativeWindow */
    NATIVE_WINDOW_SURFACE                   = 1, /* Surface */
};

/* parameter for NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP
 *
 * Special timestamp value to indicate that timestamps should be auto-generated
 * by the native window when queueBuffer is called.  This is equal to INT64_MIN,
 * defined directly to avoid problems with C99/C++ inclusion of stdint.h.
 */
static const int64_t NATIVE_WINDOW_TIMESTAMP_AUTO = (-9223372036854775807LL-1);


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
