#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#define DEV_NAME	"BH1750FVI"

#define PowerDown	0x00
#define PowerOn		0x01
#define Reset		0x07
#define ContinuHigh 0x10
#define ContinuLow	0x13
#define OneTimeHigh	0x20
#define OneTimeLow	0x23

MODULE_LICENSE("Dual BSD/GPL");

int i2c_major = 0 ;

/* 定义设备结构体 */
typedef struct bh1750fvi_i2c {
     struct i2c_client client;
     struct cdev       cdev;
     struct class      *my_class;
	 dev_t             devno;
}bh1750fvi_i2c;

/* 使用设备结构体定义一个变量 */
bh1750fvi_i2c *i2c;

int lsbh1750fvi_i2c_open(struct inode * inode, struct file *filp)
{
    printk("<0> bh1750fvi open\n");
    return 0;
}

int lsbh1750fvi_i2c_release(struct inode * inode, struct file *filp)
{
    printk("<0> bh1750fvi close\n");
    return 0;
}

/* 读取设备数据,涉及到struct i2c_msg消息结构体的实现和使用 */
ssize_t lsbh1750fvi_i2c_read(struct file *filp, char __user *buf, size_t count, loff_t *loff)
{
	int ret = 0;
	char txbuf[1]	= {0};
    char rxbuf[2]	= {0};    
	/**************************************************************************************************/	
    /*   i2c设备通讯是通过系统定义的消息结构体实现的,这样可以简化驱动人员的工作,实现驱动的良好移植性. */
	/*   struct i2c_msg {                                                                             */
    /*         __u16 addr;           从机地址,从struct i2c_client中获取                               */
    /*         __u16 flags;          标志位,使用下面的宏,其中写数据可以直接传递0进去                  */
    /*   #define I2C_M_TEN           0x0010  代表i2c地址为10位地址数据                                */
    /*   #define I2C_M_RD            0x0001  读数据,从从机到主机                                      */
    /*   #define I2C_M_NOSTART       0x4000  if I2C_FUNC_PROTOCOL_MANGLING                            */
    /*   #define I2C_M_REV_DIR_ADDR  0x2000  if I2C_FUNC_PROTOCOL_MANGLING                            */
    /*   #define I2C_M_IGNORE_NAK    0x1000  if I2C_FUNC_PROTOCOL_MANGLING                            */
    /*   #define I2C_M_NO_RD_ACK     0x0800  if I2C_FUNC_PROTOCOL_MANGLING                            */ 
	/*   #define I2C_M_RECV_LEN      0x0400  首先受到的字节表示长度                                   */
    /*        __u16 len;             消息长度                                                         */
    /*        __u8 *buf;             指向消息数据的指针                                               */
    /*   };                                                                                           */
	/**************************************************************************************************/
	struct i2c_msg msg[2] = {
        {  i2c->client.addr, 0, 1, txbuf },
        {  i2c->client.addr, I2C_M_RD, 2,rxbuf },
    };

    if (sizeof(rxbuf) != count)
        count = sizeof(rxbuf);
	
	//设置上电开启
	ret = i2c_smbus_write_byte(&i2c->client,PowerOn);
	if (ret<0)
	{
		printk("PowerOn error!\n");
		return -EFAULT;
	}
	
	//设置高分辨率
	ret = i2c_smbus_write_byte(&i2c->client,ContinuHigh);
	if(ret<0)
	{
		printk("ContinuHigh Error!\n");
		return -EFAULT;
	}
		
	mdelay(124);
	
	//读取寄存器的数据
	ret = i2c_transfer(i2c->client.adapter,msg,ARRAY_SIZE(msg));
	if(ret<0)
	{
		printk("failure:i2c_transfer\n");
		return -EFAULT;
	}
	
	if(copy_to_user(buf,rxbuf,count))
		return -EFAULT;

	return count;
}

/* 方法绑定结构体 */
struct file_operations i2c_fops = {
    .owner   = THIS_MODULE,
    .open    = lsbh1750fvi_i2c_open,
    .release = lsbh1750fvi_i2c_release,
    .read    = lsbh1750fvi_i2c_read, 
};

/* 匹配函数,设备成功配对并取得设备信息struct i2c_client结构体后执行的函数 */
int lsbh1750fvi_i2c_probe(struct i2c_client *client, const struct i2c_device_id *i2c_id)
{
    int ret = 0;
	
	/********************************************************************************/	
    /* 申请空间                                                                     */
    /*   使用函数void *kmalloc(size_t size, gfp_t flags)                            */
    /*      size  代表申请的内存大小                                                */
    /*      flags 表示该函数的操作类型,使用系统提供的宏实现,用到的主要有下面这些:   */
	/*            GFP_ATOMIC 能申请到内存则申请,不能申请到内存则立即返回错误码      */
    /*            GFP_KERNEL 能申请则申请,不能申请则睡眠,直到申请到为止             */
	/********************************************************************************/
	i2c =(bh1750fvi_i2c*) kmalloc(sizeof(*i2c),GFP_KERNEL);
    
	//动态获取设备号
	ret = alloc_chrdev_region(&i2c->devno,0,1,DEV_NAME); 

    if (ret < 0) 
	{
        printk("<0> failure:register_chrdev_region\n");
        return ret;
    }
    else
    {
      	printk("<0> register bh1750 succeed!\n");
    }

    i2c_major = MKDEV(i2c->devno,0);
	
	//为设备创建一个class，这个class就是设备类
    i2c->my_class = class_create(THIS_MODULE,DEV_NAME);

	//为每个设备创建对应的设备，创建一个设备就会区创建一个设备文件
    device_create(i2c->my_class,NULL,i2c->devno,NULL,DEV_NAME);
    
    /* 设备初始化,将cdev和file_operations绑定,将设备信息和实现方法进行绑定 */
    cdev_init(&i2c->cdev, &i2c_fops);
    i2c->cdev.owner = THIS_MODULE;
    
	/* 添加设备 */
    ret = cdev_add(&(i2c->cdev), i2c->devno,1);
    if (ret < 0) 
	{
        ret = -EFAULT;
        printk("<0> failure:cdev_add\n");
        goto err1;
    }

    /* 导出client,获取系统提供的具体的i2c的设备信息,这样就实现了设备和驱动的关联*/
    i2c->client = *client;
		
    return 0;

    err1:
        kfree(i2c);
        unregister_chrdev_region(i2c->devno, 1);

    return ret;
}

/* remove处理函数 */
int lsbh1750fvi_i2c_remove(struct i2c_client *client)
{
    /* 释放需要释放的数据 */
	printk("<0> bh1750fvi remove\n");
    //int ret = 0;

    cdev_del(&i2c->cdev);
    kfree(i2c);
    unregister_chrdev_region(MKDEV(i2c_major,0),1);

    return 0;// ret;
}

static struct i2c_board_info __initdata gpio_i2c_board_info[]={ 
	{I2C_BOARD_INFO("lsBH1750FVI",0x23),},
};

/**********************************************************************************************/
/* 设备信息struct i2c_device_id结构体,保存i2c设备的设备信息,由于可能                          */
/* 存在多个i2c设备所以将它定义为一个结构体数组,方便添加新增设备                               */
/* 	   struct i2c_device_id {                                                                 */
/*    	    char name[I2C_NAME_SIZE];    设备的名字,这是一个宏定义2.6.35中默认是20个字符      */
/*    	    ulong driver_data;           设备的地址。                                         */
/*     }                                                                                      */
/**********************************************************************************************/
struct i2c_device_id i2c_id[] = {
    {  "lsBH1750FVI", 0x23  },
};

/**********************************************************************************************/ 
/* 构建i2c子系统的设备驱动结构体i2c_driver                                                    */
/* .driver.name 表示驱动程序的名字,这个名字可以由自己任意取                                   */
/* .id_table 是一个struct i2c_device_id的结构体,保存着i2c的设备信息                           */
/*   	struct i2c_device_id {                                                                */
/*      		char name[I2C_NAME_SIZE];    设备的名字,这是一个宏定义2.6.35中默认是20个字符  */
/*      		ulong driver_data;           设备的私有数据,可以自己随便填写                  */
/*		}                                                                                     */ 
/* .probe 表示匹配成功后要执行的函数                                                          */
/* .remove 表示移除设备的时候要执行的函数                                                     */ 
/**********************************************************************************************/ 
struct i2c_driver i2c_driver = {
    .driver.name = "BH1750FVI",
    .id_table    = i2c_id,
    .probe       = lsbh1750fvi_i2c_probe,
    .remove      = lsbh1750fvi_i2c_remove,
};

/* 加载函数 */
static int __init lsbh1750fvi_i2c_init(void)
{
	int ret;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	
	//指定i2c的总线号，这里跟mach-mini2440板级文件里注册的i2c-gpio的驱动相关联
	adapter = i2c_get_adapter(5);
	client = i2c_new_device(adapter,gpio_i2c_board_info);	

	/******************************************************************************/
    /* 注册i2c的设备驱动                                                          */
    /* 使用函数int i2c_add_driver(struct i2c_driver *i2c_driver)                  */
    /* 成功返回0,失败返回错误码                                                   */
	/* 注册成功则调用结构里的probe函数                                            */
	/******************************************************************************/
	ret  = i2c_add_driver(&i2c_driver); 
	
	if(ret==0)
	{
	  printk("<0> bh1750fvi init succeed!\n");
	  return ret;
	}
	else
	{
	  printk("<0> bh1750fvi init failed\n");
	  return -ENOMEM;
	}
}

/* 卸载函数 */
static void __exit lsbh1750fvi_i2c_cleanup(void)
{
    /* 删除i2c的设备驱动
     * 使用函数void i2c_del_driver(struct i2c_driver *i2c_driver)  */
    i2c_del_driver(&i2c_driver);
}

module_init(lsbh1750fvi_i2c_init);
module_exit(lsbh1750fvi_i2c_cleanup);
