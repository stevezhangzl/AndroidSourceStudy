#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

//frameworks/native/cmds/servicemanager/binder.c
//https://cs.android.com/android/platform/superproject/+/android-7.0.0_r1:frameworks/native/cmds/servicemanager/binder.c?hl=zh-cn


struct binder_state *binder_open(size_t mapsize)
{
    //这个结构体记录了SM中的有关于Binder的所有信息，如fd、map的大小
    struct binder_state *bs;
    struct binder_version vers;

    bs = malloc(sizeof(*bs));
    if (!bs) {
        errno = ENOMEM;
        return NULL;
    }

    //打开Binder驱动节点
    bs->fd = open("/dev/binder", O_RDWR | O_CLOEXEC);
    if (bs->fd < 0) {
        fprintf(stderr,"binder: cannot open device (%s)\n",
                strerror(errno));
        goto fail_open;
    }

    //调用ioctl
    if ((ioctl(bs->fd, BINDER_VERSION, &vers) == -1) ||
        (vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION)) {
        fprintf(stderr,
                "binder: kernel driver version (%d) differs from user space version (%d)\n",
                vers.protocol_version, BINDER_CURRENT_PROTOCOL_VERSION);
        goto fail_open;
    }

    //128*1024  128K
    bs->mapsize = mapsize;
    //从文件的起始地址开始映射    映射到进程空间
    //mmap可以将某个设备或者文件映射到应用进程的内存空间中，这样直接访问内存就等于访问文件，不用通过read() write()
    //这样会快
    /**
     * addr:指出文件/设备应该被映射到进程空间的哪个起始地址。这个参数如果为空，则由内核驱动自动决定被映射的地址。
     * len：指出被映射到进程空间中内存块的大小。
     * prot:指定了被映射存的访问权限  PROT_READ 内存页是可读的
     * flags:指定了程序对内存块所做改变将造成的影响  MAP_PRIVATE 对内存块的修改是private的，即只在局部范围内有效。   影响是私有的不需要保存文件
     * fd:被映射到进程空间的文件描述符，通常是由open()返回的
     * offset:指定了从文件的哪一部分开始映射，一般设为0
     * 返回值：成功返回0  否则是错误码
     * 
    */
    bs->mapped = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, bs->fd, 0);
    if (bs->mapped == MAP_FAILED) {
        fprintf(stderr,"binder: cannot map device (%s)\n",
                strerror(errno));
        goto fail_map;
    }

    return bs;

fail_map:
    close(bs->fd);
fail_open:
    free(bs);
    return NULL;
}


static struct flat_binder_object *bio_alloc_obj(struct binder_io *bio)
{
    struct flat_binder_object *obj;

    obj = bio_alloc(bio, sizeof(*obj));  //确定obj的具体地址

    if (obj && bio->offs_avail) {
        bio->offs_avail--;  //个数减一
        *bio->offs++ = ((char*) obj) - ((char*) bio->data0); //原来offset区域用来记录每次data区域的写入大小的，这是“可变长度数据”的一种存储方法
        return obj;
    }

    bio->flags |= BIO_F_OVERFLOW;
    return NULL;
}

//初始化reply  binder_io是ServiceManager内部用于存储binder object的一种数据结构
void bio_init(struct binder_io *bio, void *data,
              size_t maxdata, size_t maxoffs)
{
    size_t n = maxoffs * sizeof(size_t); //最大偏移4个，共16字节

    if (n > maxdata) {
        bio->flags = BIO_F_OVERFLOW;
        bio->data_avail = 0;
        bio->offs_avail = 0;
        return;
    }

    bio->data = bio->data0 = (char *) data + n;
    bio->offs = bio->offs0 = data;
    bio->data_avail = maxdata - n;
    bio->offs_avail = maxoffs;
    bio->flags = 0;
}

uint32_t bio_get_ref(struct binder_io *bio)
{
    struct flat_binder_object *obj;

    obj = _bio_get_obj(bio);
    if (!obj)
        return 0;

    if (obj->type == BINDER_TYPE_HANDLE)
        return obj->handle;

    return 0;
}

//没有返回值，所以这个新创建的obj要与reply产生关系，看bio_alloc_obj 传入了reply
void bio_put_ref(struct binder_io *bio, uint32_t handle)
{
    //这里的关系是这样的 reply 也就是binder_io中有binder_object  obj的handle是SM查找到的Service指针（最终转为ibinder）
    struct flat_binder_object *obj;

    if (handle)
        obj = bio_alloc_obj(bio);  //handle之前查出的句柄
    else
        obj = bio_alloc(bio, sizeof(*obj));

    if (!obj)
        return;

    obj->flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
    obj->type = BINDER_TYPE_HANDLE;  //注意这个类型，后面还会用到
    obj->handle = handle;  //将handle保存到obj中
    obj->cookie = 0;
}



//servicemanager启动得很早，能保证它是系统中第一个向Binder驱动注册成管家的程序。
int binder_become_context_manager(struct binder_state *bs)
{
    //通过ioctl  执行 BINDER_SET_CONTEXT_MGR
    return ioctl(bs->fd, BINDER_SET_CONTEXT_MGR, 0);
}


//func 函数在SM中对应svcmgr_handler()
int binder_parse(struct binder_state *bs, struct binder_io *bio,
                 uintptr_t ptr, size_t size, binder_handler func)
{
    int r = 1;
    uintptr_t end = ptr + (uintptr_t) size;

    //如果为false说明Service Manager上一次从驱动层读取的消息都已处理完成
    while (ptr < end) {
        uint32_t cmd = *(uint32_t *) ptr; //从数据的头部取出cmd命令
        ptr += sizeof(uint32_t);
#if TRACE
        fprintf(stderr,"%s:\n", cmd_name(cmd));
#endif
        switch(cmd) {
        case BR_NOOP:
            break;
        case BR_TRANSACTION_COMPLETE:
            break;
        case BR_INCREFS:
        case BR_ACQUIRE:
        case BR_RELEASE:
        case BR_DECREFS:
#if TRACE
            fprintf(stderr,"  %p, %p\n", (void *)ptr, (void *)(ptr + sizeof(void *)));
#endif
            ptr += sizeof(struct binder_ptr_cookie);
            break;
        case BR_TRANSACTION: { //这个命令的处理，主要由func来完成  然后将结果返回Binder驱动
            struct binder_transaction_data *txn = (struct binder_transaction_data *) ptr;  //这个是Binder驱动中拷贝过来的ptr 转成binder_transaction_data
            if ((end - ptr) < sizeof(*txn)) {
                ALOGE("parse: txn too small!\n");
                return -1;
            }
            binder_dump_txn(txn); //dump追踪
            if (func) { //这个函数是svcmgr_handler()
                unsigned rdata[256/4];
                struct binder_io msg;
                //这里注意reply的数据格式
                struct binder_io reply; //这里是binder_io结构体 binder_io是SM内部用于存储binder object的结构体
                int res;

                //为reply初始化
                bio_init(&reply, rdata, sizeof(rdata), 4);
                bio_init_from_txn(&msg, txn);
                res = func(bs, txn, &msg, &reply); //具体处理消息 svcmgr_handler处理请求
                if (txn->flags & TF_ONE_WAY) {
                    binder_free_buffer(bs, txn->data.ptr.buffer);
                } else {
                    //将执行结果回复给底层Binder驱动，进而传递给 客户端 ，然后binder_parse会进入下一轮的while循环
                    //这里要进行数据组织 主要是binder_transaction_data的拼装
                    binder_send_reply(bs, &reply, txn->data.ptr.buffer, res); //回应消息处理结果，将reply发回给驱动
                }
            }
            ptr += sizeof(*txn); //处理完成，跳过这一段数据 指针向前运动
            break;
        }
        case BR_REPLY: { //SM对BR_REPLY并没有实质性的动作，因为SM是为别人服务的，并没有向其他Binder Server主动发起请求的情况
            struct binder_transaction_data *txn = (struct binder_transaction_data *) ptr;
            if ((end - ptr) < sizeof(*txn)) {
                ALOGE("parse: reply too small!\n");
                return -1;
            }
            binder_dump_txn(txn);//追踪调试
            if (bio) {
                bio_init_from_txn(bio, txn);
                bio = 0;
            } else {
                /* todo FREE BUFFER */
            }
            ptr += sizeof(*txn); //跳过命令所占空间
            r = 0;
            break;
        }
        case BR_DEAD_BINDER: {
            struct binder_death *death = (struct binder_death *)(uintptr_t) *(binder_uintptr_t *)ptr;
            ptr += sizeof(binder_uintptr_t);
            death->func(bs, death->ptr);
            break;
        }
        case BR_FAILED_REPLY:
            r = -1;
            break;
        case BR_DEAD_REPLY:
            r = -1;
            break;
        default:
            ALOGE("parse: OOPS %d\n", cmd);
            return -1;
        }
    }

    return r;
}


//SM开始等待客户端的请求  重点和难点
void binder_loop(struct binder_state *bs, binder_handler func)
{
    int res;
    struct binder_write_read bwr;  //这是执行BINDER_WRITE_READ命令所需的 数据格式
    uint32_t readbuf[32]; //一次读取容量

    //这里只是先初始化为0  下面会再赋值
    bwr.write_size = 0;
    bwr.write_consumed = 0;
    bwr.write_buffer = 0;

    //命令
    readbuf[0] = BC_ENTER_LOOPER;
    binder_write(bs, readbuf, sizeof(uint32_t));
    //从消息队列中读取消息，并处理  经典的事件驱动程序循环
    for (;;) {
        bwr.read_size = sizeof(readbuf);
        bwr.read_consumed = 0;
        bwr.read_buffer = (uintptr_t) readbuf; //读取的消息存储到readbuf

        //1.读取“消息”  这个是从Binder驱动那里获得的
        res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr); //向Binder驱动读写消息
        //这里也会执行Binder驱动的binder_thread_read


        //2.如果是 退出命令   马上结束循环
        if (res < 0) {
            ALOGE("binder_loop: ioctl failed (%s)\n", strerror(errno));
            break;
        }

        //3.如果消息为为空就继续循环，如果消息不为空，就进行处理 永远循环不会退出，要不就出现致命的错误了
        //3.处理这条消息，直到从驱动读取的消息都已处理完成，然后循环继续向Binder Driver发送 BINDER_WRITE_READ 以查询有没有新的消息
        res = binder_parse(bs, 0, (uintptr_t) readbuf, bwr.read_consumed, func); //解析 
        if (res == 0) {
            ALOGE("binder_loop: unexpected reply?!\n");
            break;
        }
        if (res < 0) {
            ALOGE("binder_loop: io error %d %s\n", res, strerror(errno));
            break;
        }
    }
}



int binder_write(struct binder_state *bs, void *data, size_t len)
{
    //数据填入binder_write_read
    struct binder_write_read bwr;
    int res;

    bwr.write_size = len;
    bwr.write_consumed = 0;
    bwr.write_buffer = (uintptr_t) data; //将data放入bwr
    bwr.read_size = 0; //不读取数据，只写入
    bwr.read_consumed = 0;
    bwr.read_buffer = 0;
    res = ioctl(bs->fd, BINDER_WRITE_READ, &bwr);  //与Binder驱动交互
    if (res < 0) {
        fprintf(stderr,"binder_write: ioctl failed (%s)\n",
                strerror(errno));
    }
    return res;
}


void binder_send_reply(struct binder_state *bs,
                       struct binder_io *reply,
                       binder_uintptr_t buffer_to_free,
                       int status)
{
    struct {
        uint32_t cmd_free;
        binder_uintptr_t buffer;
        uint32_t cmd_reply;
        struct binder_transaction_data txn; //对应Binder驱动中的binder_transaction_data数据格式
    } __attribute__((packed)) data;

    data.cmd_free = BC_FREE_BUFFER;
    data.buffer = buffer_to_free;
    data.cmd_reply = BC_REPLY;  //注意这里的命令类型
    data.txn.target.ptr = 0;
    data.txn.cookie = 0;
    data.txn.code = 0;
    if (status) {
        data.txn.flags = TF_STATUS_CODE;
        data.txn.data_size = sizeof(int);
        data.txn.offsets_size = 0;
        data.txn.data.ptr.buffer = (uintptr_t)&status;
        data.txn.data.ptr.offsets = 0;
    } else {  //没有error的情况
        data.txn.flags = 0;
        data.txn.data_size = reply->data - reply->data0;
        data.txn.offsets_size = ((char*) reply->offs) - ((char*) reply->offs0);
        data.txn.data.ptr.buffer = (uintptr_t)reply->data0;   //reply中数据区起始地址
        data.txn.data.ptr.offsets = (uintptr_t)reply->offs0;    //offset区起始地址
    }
    binder_write(bs, &data, sizeof(data));  //写入驱动
}






