#include <sys/ioctl.h>



struct binder_io
{
    char *data;            /* pointer to read/write from */            //数据区当前地址
    binder_size_t *offs;   /* array of offsets */                     //offset区当前地址
    size_t data_avail;     /* bytes available in data buffer */       //数据区剩余空间
    size_t offs_avail;     /* entries available in offsets array */   //offset区剩余地址

    char *data0;           /* start of data buffer */                 //数据区起始地址
    binder_size_t *offs0;  /* start of offsets buffer */              //offset区起始地址
    uint32_t flags;
    uint32_t unused;
};


enum {
	BINDER_TYPE_BINDER	= B_PACK_CHARS('s', 'b', '*', B_TYPE_LARGE),  //BinderClient和Binder Server在同一进程  内存地址
	BINDER_TYPE_WEAK_BINDER	= B_PACK_CHARS('w', 'b', '*', B_TYPE_LARGE), //BinderClient和Binder Server在同一进程
	BINDER_TYPE_HANDLE	= B_PACK_CHARS('s', 'h', '*', B_TYPE_LARGE),   //相当于电话号，可以在进程中传递
	BINDER_TYPE_WEAK_HANDLE	= B_PACK_CHARS('w', 'h', '*', B_TYPE_LARGE), //相当于电话号，可以在进程中传递
	BINDER_TYPE_FD		= B_PACK_CHARS('f', 'd', '*', B_TYPE_LARGE),
};