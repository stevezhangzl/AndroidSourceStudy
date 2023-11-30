

//https://github.com/torvalds/linux/blob/v3.8/drivers/staging/android/binder.c



#define MISC_DYNAMIC_MINOR	255

static struct miscdevice binder_miscdev = {
	.minor = MISC_DYNAMIC_MINOR, //动态分配次设备号
	.name = "binder", //驱动名称
	.fops = &binder_fops //Binder驱动支持的文件操作
};


/**
 * Android系统把Binder注册成misc device类型的驱动
 * Linux字符设备通常要经过alloc_chrdev_region() cdev_init()操作才能在内核中注册自己，而misc类型驱动
 * 相对简单，只需要调用 misc_register()就可以完成注册
*/
static int __init binder_init(void)
{
	int ret;

	binder_deferred_workqueue = create_singlethread_workqueue("binder");
	if (!binder_deferred_workqueue)
		return -ENOMEM;

	binder_debugfs_dir_entry_root = debugfs_create_dir("binder", NULL);
	if (binder_debugfs_dir_entry_root)
		binder_debugfs_dir_entry_proc = debugfs_create_dir("proc",
						 binder_debugfs_dir_entry_root);
	ret = misc_register(&binder_miscdev); //驱动注册
	if (binder_debugfs_dir_entry_root) {
		debugfs_create_file("state",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    NULL,
				    &binder_state_fops);
		debugfs_create_file("stats",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    NULL,
				    &binder_stats_fops);
		debugfs_create_file("transactions",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    NULL,
				    &binder_transactions_fops);
		debugfs_create_file("transaction_log",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    &binder_transaction_log,
				    &binder_transaction_log_fops);
		debugfs_create_file("failed_transaction_log",
				    S_IRUGO,
				    binder_debugfs_dir_entry_root,
				    &binder_transaction_log_failed,
				    &binder_transaction_log_fops);
	}
	return ret;
}



/**
 * proc:申请内存的进程所持有的binder_proc对象
 * allocate 是申请还是释放  1 申请 0 释放
 * start binder中的虚拟内存起点
 * end  binder中虚拟内存终点
 * vma 应用程序中虚拟内存段的描述
*/
static int binder_update_page_range(struct binder_proc *proc, int allocate,
				    void *start, void *end,
				    struct vm_area_struct *vma)
{
	void *page_addr;
	unsigned long user_page_addr;
	struct vm_struct tmp_area;
	struct page **page;
	struct mm_struct *mm;

	binder_debug(BINDER_DEBUG_BUFFER_ALLOC,
		     "%d: %s pages %p-%p\n", proc->pid,
		     allocate ? "allocate" : "free", start, end);

	if (end <= start)
		return 0;

	trace_binder_update_page_range(proc, allocate, start, end);

	if (vma)
		mm = NULL;
	else
		mm = get_task_mm(proc->tsk);

	if (mm) {
		down_write(&mm->mmap_sem);
		vma = proc->vma;
		if (vma && mm != proc->vma_vm_mm) {
			pr_err("%d: vma mm and task mm mismatch\n",
				proc->pid);
			vma = NULL;
		}
	}

	if (allocate == 0)
		goto free_range;

	if (vma == NULL) {
		pr_err("%d: binder_alloc_buf failed to map pages in userspace, no vma\n",
			proc->pid);
		goto err_no_vma;
	}

	for (page_addr = start; page_addr < end; page_addr += PAGE_SIZE) {
		int ret;
		struct page **page_array_ptr;
		page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];

		BUG_ON(*page);
		*page = alloc_page(GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO);
		if (*page == NULL) {
			pr_err("%d: binder_alloc_buf failed for page at %p\n",
				proc->pid, page_addr);
			goto err_alloc_page_failed;
		}
		tmp_area.addr = page_addr;
		tmp_area.size = PAGE_SIZE + PAGE_SIZE /* guard page? */;
		page_array_ptr = page;
		ret = map_vm_area(&tmp_area, PAGE_KERNEL, &page_array_ptr);
		if (ret) {
			pr_err("%d: binder_alloc_buf failed to map page at %p in kernel\n",
			       proc->pid, page_addr);
			goto err_map_kernel_failed;
		}
		user_page_addr =
			(uintptr_t)page_addr + proc->user_buffer_offset;
		ret = vm_insert_page(vma, user_page_addr, page[0]);
		if (ret) {
			pr_err("%d: binder_alloc_buf failed to map page at %lx in userspace\n",
			       proc->pid, user_page_addr);
			goto err_vm_insert_page_failed;
		}
		/* vm_insert_page does not seem to increment the refcount */
	}
	if (mm) {
		up_write(&mm->mmap_sem);
		mmput(mm);
	}
	return 0;

free_range:
	for (page_addr = end - PAGE_SIZE; page_addr >= start;
	     page_addr -= PAGE_SIZE) {
		page = &proc->pages[(page_addr - proc->buffer) / PAGE_SIZE];
		if (vma)
			zap_page_range(vma, (uintptr_t)page_addr +
				proc->user_buffer_offset, PAGE_SIZE, NULL);
err_vm_insert_page_failed:
		unmap_kernel_range((unsigned long)page_addr, PAGE_SIZE);
err_map_kernel_failed:
		__free_page(*page);
		*page = NULL;
err_alloc_page_failed:
		;
	}
err_no_vma:
	if (mm) {
		up_write(&mm->mmap_sem);
		mmput(mm);
	}
	return -ENOMEM;
}


/**
 * 上层进程在访问Binder驱动时，首先需要打开/dev/binder的节点 这个操作最终的实现是在binder_open()中
 * binder 创建自己的binder_proc实体  之后用户对Binder设备的操作将以这个对象为基础
 * 
 * /proc/binder/proc  /proc/binder/state  /proc/binder/stats
*/
static int binder_open(struct inode *nodp, struct file *filp)
{
	struct binder_proc *proc; //管理数据的记录体，每个进程都有独立记录

	binder_debug(BINDER_DEBUG_OPEN_CLOSE, "binder_open: %d:%d\n",
		     current->group_leader->pid, current->pid);

	proc = kzalloc(sizeof(*proc), GFP_KERNEL); //分配空间
	if (proc == NULL)
		return -ENOMEM;

  //对proc对象初始化
	get_task_struct(current);
	proc->tsk = current;
	INIT_LIST_HEAD(&proc->todo);//todo 链表
	init_waitqueue_head(&proc->wait); //wait链表
	proc->default_priority = task_nice(current);

	//完成对proc对象的初始化后，将它加入Binder的全局管理中。因为涉及到了资源互斥，所以需要保护机制
	binder_lock(__func__); //获取锁

  //binder_stats是binder中统计数据载体
	binder_stats_created(BINDER_STAT_PROC);
  //将proc加入到binder_procs队列头部
	hlist_add_head(&proc->proc_node, &binder_procs);
  //进程pid
	proc->pid = current->group_leader->pid;
	INIT_LIST_HEAD(&proc->delivered_death);
  //将proc和filp关联起来，这样下次通过filp就能找到这个proc
	filp->private_data = proc;

  //解除锁
	binder_unlock(__func__);

	if (binder_debugfs_dir_entry_proc) {
		char strbuf[11];
		snprintf(strbuf, sizeof(strbuf), "%u", proc->pid);
		proc->debugfs_entry = debugfs_create_file(strbuf, S_IRUGO,
			binder_debugfs_dir_entry_proc, proc, &binder_proc_fops);
	}

	return 0;
} //binder_open结束


/**
 * 承担Binder驱动的大部分业务，实现了应用进程与Binder驱动之间的命令交互，重中之重
 * 不提供read和write操作  因为binder_ioctl就可以完全替代它们
*/
static long binder_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;
	struct binder_proc *proc = filp->private_data;
	struct binder_thread *thread;
	unsigned int size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	/*pr_info("binder_ioctl: %d:%d %x %lx\n", proc->pid, current->pid, cmd, arg);*/

	trace_binder_ioctl(cmd, arg); //在保护区进行
	//让调用者进程暂时挂起，直到目标进程返回结果后，Binder再唤醒等待进程
	ret = wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret)
		goto err_unlocked;

	binder_lock(__func__);
	//首先向proc中的threads链表查询是否已经添加了当前线程的节点，如果没有则插入一个表示当前线程的新节点
	//按pid大小排序的，这样要吧加快查询速度
	thread = binder_get_thread(proc);
	if (thread == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	//处理具体命令
	switch (cmd) {
	case BINDER_WRITE_READ: { //读写操作，可以用此命令向Binder读取或写入数据
		struct binder_write_read bwr;
		if (size != sizeof(struct binder_write_read)) { //判断buffer大小是否符合要求，然后开始从用户空间复制数据
			ret = -EINVAL;
			goto err;
		}

		//通过copy_from_user  把Binder Server（比如ServiceManager）的应用进程copy到binder的proc->buff
		//经过copy_from_user后 一次复制，当前应用进程，可以操作Binder Server所在的内存区域
		if (copy_from_user(&bwr, ubuf, sizeof(bwr))) {
			ret = -EFAULT;
			goto err;
		}
		binder_debug(BINDER_DEBUG_READ_WRITE,
			     "%d:%d write %ld at %08lx, read %ld at %08lx\n",
			     proc->pid, thread->pid, bwr.write_size,
			     bwr.write_buffer, bwr.read_size, bwr.read_buffer);

		//我们在talkWithDriver中，根据实际情况分别填写了write和read请求
		//成功从用户空间取得数据后，接下来就开始执行write/read操作
		if (bwr.write_size > 0) {
			ret = binder_thread_write(proc, thread, (void __user *)bwr.write_buffer, bwr.write_size, &bwr.write_consumed);
			trace_binder_write_done(ret);
			if (ret < 0) {
				bwr.read_consumed = 0;
				if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
					ret = -EFAULT;
				goto err;
			}
		}
		if (bwr.read_size > 0) {
			ret = binder_thread_read(proc, thread, (void __user *)bwr.read_buffer, bwr.read_size, &bwr.read_consumed, filp->f_flags & O_NONBLOCK);
			trace_binder_read_done(ret);
			if (!list_empty(&proc->todo))
				wake_up_interruptible(&proc->wait);
			if (ret < 0) {
				if (copy_to_user(ubuf, &bwr, sizeof(bwr)))
					ret = -EFAULT;
				goto err;
			}
		}
		binder_debug(BINDER_DEBUG_READ_WRITE,
			     "%d:%d wrote %ld of %ld, read return %ld of %ld\n",
			     proc->pid, thread->pid, bwr.write_consumed, bwr.write_size,
			     bwr.read_consumed, bwr.read_size);
		if (copy_to_user(ubuf, &bwr, sizeof(bwr))) {
			ret = -EFAULT;
			goto err;
		}
		break;
	}
	case BINDER_SET_MAX_THREADS: //设置支持的最大线程数。因为客户端可以向服务器发送请求
		if (copy_from_user(&proc->max_threads, ubuf, sizeof(proc->max_threads))) {
			ret = -EINVAL;
			goto err;
		}
		break;
	case BINDER_SET_CONTEXT_MGR: //Service Manager专用，将自己设置成“Binder大管家” 系统中只能有一个SM存在
		if (binder_context_mgr_node != NULL) {
			pr_err("BINDER_SET_CONTEXT_MGR already set\n");
			ret = -EBUSY;
			goto err;
		}
		if (uid_valid(binder_context_mgr_uid)) {
			if (!uid_eq(binder_context_mgr_uid, current->cred->euid)) {
				pr_err("BINDER_SET_CONTEXT_MGR bad uid %d != %d\n",
				       from_kuid(&init_user_ns, current->cred->euid),
				       from_kuid(&init_user_ns, binder_context_mgr_uid));
				ret = -EPERM;
				goto err;
			}
		} else
			binder_context_mgr_uid = current->cred->euid;
		binder_context_mgr_node = binder_new_node(proc, NULL, NULL);
		if (binder_context_mgr_node == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		binder_context_mgr_node->local_weak_refs++;
		binder_context_mgr_node->local_strong_refs++;
		binder_context_mgr_node->has_strong_ref = 1;
		binder_context_mgr_node->has_weak_ref = 1;
		break;
	case BINDER_THREAD_EXIT: //通知Binder线程退出 每个线程在退出时都应该告知Binder驱动，这样才能释放相关资源
		binder_debug(BINDER_DEBUG_THREADS, "%d:%d exit\n",
			     proc->pid, thread->pid);
		binder_free_thread(proc, thread);
		thread = NULL;
		break;
	case BINDER_VERSION: //获取Binder版本号
		if (size != sizeof(struct binder_version)) {
			ret = -EINVAL;
			goto err;
		}
		if (put_user(BINDER_CURRENT_PROTOCOL_VERSION, &((struct binder_version *)ubuf)->protocol_version)) {
			ret = -EINVAL;
			goto err;
		}
		break;
	default:
		ret = -EINVAL;
		goto err;
	}
	ret = 0;
err:
	if (thread)
		thread->looper &= ~BINDER_LOOPER_STATE_NEED_RETURN;
	binder_unlock(__func__);
	wait_event_interruptible(binder_user_error_wait, binder_stop_on_user_error < 2);
	if (ret && ret != -ERESTARTSYS)
		pr_info("%d:%d ioctl %x %lx returned %d\n", proc->pid, current->pid, cmd, arg, ret);
err_unlocked:
	trace_binder_ioctl_done(ret);
	return ret;
}


/**
 * vm_area_struct *vma
 * 描述了一块供应用程序使用的虚拟内存，其中vma->vm_start和vma->vm_end分别是这块连续内存的起止点
*/
static int binder_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
  //vma是应用程序中对虚拟内存的描述，相应的area变量就是Binder驱动中对虚拟内存的描述。
	struct vm_struct *area;
  //取出这一进程对应的binder_proc对象，这个是Binder驱动为应用进程分配的一个数据结构，用于存储和该进程相关的所有信息
  //如内存分配、线程管理等。
	struct binder_proc *proc = filp->private_data;
	const char *failure_string;
	struct binder_buffer *buffer;

	if (proc->tsk != current)
		return -EINVAL;

  //判断应用程序申请的内存大小是否合理——它最多只支持4MB空间的mmap操作
	if ((vma->vm_end - vma->vm_start) > SZ_4M)
		vma->vm_end = vma->vm_start + SZ_4M; //当应用程序申请的内存大小超过4MB，并没有直接退出或报异常，而是只满足用户刚好4MB的请求

	binder_debug(BINDER_DEBUG_OPEN_CLOSE,
		     "binder_mmap: %d %lx-%lx (%ld K) vma %lx pagep %lx\n",
		     proc->pid, vma->vm_start, vma->vm_end,
		     (vma->vm_end - vma->vm_start) / SZ_1K, vma->vm_flags,
		     (unsigned long)pgprot_val(vma->vm_page_prot));

  //判断vma中的标志是否禁止了mmap,并加入新的标志
	if (vma->vm_flags & FORBIDDEN_MMAP_FLAGS) { //禁止MMAP标志
		ret = -EPERM;
		failure_string = "bad vm_flags";
		goto err_bad_arg;
	}
	vma->vm_flags = (vma->vm_flags | VM_DONTCOPY) & ~VM_MAYWRITE;

	mutex_lock(&binder_mmap_lock);
  //这里proc->buffer用于存储最终的mmap结果，属于Binder的那个虚拟内存，因而不为空，说明之前已经执行过mmap操作
  //这样就出错误了。
	if (proc->buffer) { //是否已经做过映射
		ret = -EBUSY;
		failure_string = "already mapped";
		goto err_already_mapped;
	}

  //用于为Binder驱动获取一段可用的虚拟内存空间。也就是说，这时候还没有分配实际的物理内存
	area = get_vm_area(vma->vm_end - vma->vm_start, VM_IOREMAP);
	if (area == NULL) {
		ret = -ENOMEM;
		failure_string = "get_vm_area";
		goto err_get_vm_area_failed;
	}

  //映射后的地址  将proc中的buffer指针指向这块虚拟内存起始地址
	//将proc中的buffer指针指向这块虚拟内存起始地址，并计算出它和应用程序中相关联的虚拟内存地址 vma->vm_start的偏移量
	proc->buffer = area->addr; 
	proc->user_buffer_offset = vma->vm_start - (uintptr_t)proc->buffer;
	mutex_unlock(&binder_mmap_lock);

#ifdef CONFIG_CPU_CACHE_VIPT
	if (cache_is_vipt_aliasing()) {
		while (CACHE_COLOUR((vma->vm_start ^ (uint32_t)proc->buffer))) {
			pr_info("binder_mmap: %d %lx-%lx maps %p bad alignment\n", proc->pid, vma->vm_start, vma->vm_end, proc->buffer);
			vma->vm_start += PAGE_SIZE;
		}
	}
#endif
	
	proc->pages = kzalloc(sizeof(proc->pages[0]) * ((vma->vm_end - vma->vm_start) / PAGE_SIZE), GFP_KERNEL);
	if (proc->pages == NULL) {
		ret = -ENOMEM;
		failure_string = "alloc page array";
		goto err_alloc_pages_failed;
	}
	proc->buffer_size = vma->vm_end - vma->vm_start;

	vma->vm_ops = &binder_vm_ops;
	vma->vm_private_data = proc;

  //计算虚拟块大小，开始真正的申请物理页面，需要将这一页内存与应用程序的虚拟内存联系起来，申请的时只申请一页的，而不是直接申请4M
	if (binder_update_page_range(proc, 1, proc->buffer, proc->buffer + PAGE_SIZE, vma)) {
		ret = -ENOMEM;
		failure_string = "alloc small buf";
		goto err_alloc_small_buf_failed;
	}
	buffer = proc->buffer;
	INIT_LIST_HEAD(&proc->buffers); //1.所有内存块都要在这里备案
	list_add(&buffer->entry, &proc->buffers);
	buffer->free = 1; //此内存可用
	binder_insert_free_buffer(proc, buffer); //2.所有可用的空闲内存
	proc->free_async_space = proc->buffer_size / 2;
	barrier();
	proc->files = get_files_struct(current);
	proc->vma = vma;
	proc->vma_vm_mm = vma->vm_mm;

	/*pr_info("binder_mmap: %d %lx-%lx maps %p\n",
		 proc->pid, vma->vm_start, vma->vm_end, proc->buffer);*/
	return 0;

err_alloc_small_buf_failed:
	kfree(proc->pages);
	proc->pages = NULL;
err_alloc_pages_failed:
	mutex_lock(&binder_mmap_lock);
	vfree(proc->buffer);
	proc->buffer = NULL;
err_get_vm_area_failed:
err_already_mapped:
	mutex_unlock(&binder_mmap_lock);
err_bad_arg:
	pr_err("binder_mmap: %d %lx-%lx %s failed %d\n",
	       proc->pid, vma->vm_start, vma->vm_end, failure_string, ret);
	return ret;
}





/**
 * Binder驱动总共为上层应用提供了6个接口，其中使用最多的就是binder_ioctl，binder_mmap和binder_open
*/
static const struct file_operations binder_fops = {
	.owner = THIS_MODULE,
	.poll = binder_poll,
	.unlocked_ioctl = binder_ioctl,
	.mmap = binder_mmap,
	.open = binder_open,
	.flush = binder_flush,
	.release = binder_release,
};


//Binder设备驱动入口函数   并不是module_init()  可能是由于Android系统不想支持动态编译的驱动模块
device_initcall(binder_init);