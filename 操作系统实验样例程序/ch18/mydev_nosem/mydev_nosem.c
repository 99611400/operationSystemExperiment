//-------------------------------------------------------------------
//	mydev_nosem.c: 本内核模块的精髓在于等待队列的使用 以及 环形缓冲区
//       $make
//	     $sudo  mknod /dev/mydev_nosem c 57 0 
//	     $sudo  chmod a+rw /dev/mydev_nosem
//       测试命令： 读信息： cat /dev//mydev_nosem
//       测试命令： 写信息例子： echo "this is test!!!!!" > /dev//mydev_nosem
//-------------------------------------------------------------------

#include <linux/module.h>	// for init_module() 
#include <linux/fs.h>		// for register_chrdev()
#include <linux/sched.h>	// for wait_event_interruptible() 
#include <asm/uaccess.h>	// for get_user(), put_user()

#define RINGSIZE 512

char modname[] = "mydev_nosem";	// keeps former pseudo-file name
int my_major = 57;		// and same device-file major ID
unsigned char ring[ RINGSIZE ];
volatile int head = 0, tail = 0;
wait_queue_head_t  wq_rd, wq_wr;

// here two semaphores are added
struct semaphore sem_wr, sem_rd;  

ssize_t my_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	// sleep if necessary until the ringbuffer has some data 
	if ( wait_event_interruptible( wq_rd, head != tail ) )
		return -ERESTARTSYS;

	// remove a byte of data from our ringbuffer
	if ( put_user( ring[ head ], buf ) ) return -EFAULT;
	head = ( 1 + head ) % RINGSIZE;

	// now awaken any sleeping writers
	wake_up_interruptible( &wq_wr );
	return	1;
}

ssize_t  my_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	// sleep if necessary until the ringbuffer has room for more data
	int	next = (1 + tail) % RINGSIZE;
	if ( wait_event_interruptible( wq_wr, next != head ) ) 
		return -ERESTARTSYS; 

	// insert a new byte of data into our ringbuffer
	if ( get_user( ring[ tail ], buf ) ) return -EFAULT; 
	tail = (1 + tail) % RINGSIZE;

	// now awaken any sleeping readers 
	wake_up_interruptible( &wq_rd );
	return	1;
}

static struct file_operations 
my_fops =	{
		owner:		THIS_MODULE,
		write:		my_write,
		read:		  my_read,
		};

int __init my_init( void )
{
	printk( "<1>\nInstalling \'%s\' module ", modname );
	printk( "(major=%d) \n", my_major );
	// initialize wait-queue structures
	init_waitqueue_head( &wq_rd );
	init_waitqueue_head( &wq_wr );
	
	// register this device-driver with the kernel
	return	register_chrdev( my_major, modname, &my_fops );
}

void __exit my_exit( void )
{
	printk( "<1>Removing \'%s\' module\n", modname );

	// unregister this device-driver 
	unregister_chrdev( my_major, modname );
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
