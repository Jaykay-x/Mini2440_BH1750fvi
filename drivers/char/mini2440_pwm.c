#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <plat/regs-timer.h>
#include <mach/regs-irq.h>
#include <asm/mach/time.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include "memdev.h"

#define DEVICE_NAME     "pwm" //设备名

/***********************************/
/* 新版的ioctl的参数不再是这么玩里 */
/***********************************/
/*
#define PWM_IOCTL_SET_FREQ		1
#define PWM_IOCTL_STOP			0
*/
static struct semaphore lock;

/*******************************************/ 
/* freq:  pclk/50/16/65536 ~ pclk/50/16    */
/* if pclk = 50MHz, freq is 1Hz to 62500Hz */
/* human ear : 20Hz~ 20000Hz               */
/*******************************************/

//设置pwm的频率，配置各个寄存器
static void PWM_Set_Freq( unsigned long freq )
{
	unsigned long tcon;
	unsigned long tcnt;
	unsigned long tcfg1;
	unsigned long tcfg0;

	struct clk *clk_p;
	unsigned long pclk;

	//set GPB0 as tout0, pwm output 设置GPB0为tout0,pwm输出
	s3c2410_gpio_cfgpin(S3C2410_GPB(0), S3C2410_GPB0_TOUT0);

	tcon = __raw_readl(S3C2410_TCON);     //读取寄存器TCON  到 tcon
	tcfg1 = __raw_readl(S3C2410_TCFG1);   //读取寄存器TCFG1 到 tcfg1
	tcfg0 = __raw_readl(S3C2410_TCFG0);   //读取寄存器TCFG0 到 tcfg0

	//prescaler = 50
	tcfg0 &= ~S3C2410_TCFG_PRESCALER0_MASK;  //S3C2410_TCFG_PRESCALER0_MASK定时器0和1的预分频值的掩码,TCFG[0~8]
	tcfg0 |= (50 - 1);   //预分频为50

	//mux = 1/16
	tcfg1 &= ~S3C2410_TCFG1_MUX0_MASK;  //S3C2410_TCFG1_MUX0_MASK定时器0分割值的掩码:TCFG1[0~3]	
	tcfg1 |= S3C2410_TCFG1_MUX0_DIV16;  //定时器0进行16分割

	__raw_writel(tcfg1, S3C2410_TCFG1); //把tcfg1的值写到 分割寄存器   S3C2410_TCFG1 中
	__raw_writel(tcfg0, S3C2410_TCFG0); //把tcfg0的值写到 预分频寄存器 S3C2410_TCFG0 中

	clk_p = clk_get(NULL, "pclk"); //得到pclk
	pclk  = clk_get_rate(clk_p);
	tcnt  = (pclk/50/16)/freq;     //得到定时器的输入时钟,进而设置PWM的调制频率
	
	__raw_writel(tcnt, S3C2410_TCNTB(0));    //PWM脉宽调制的频率等于定时器的输入时钟
	__raw_writel(tcnt/2, S3C2410_TCMPB(0));  //占空比是50%
				
	tcon &= ~0x1f;
	tcon |= 0xb;		//disable deadzone, auto-reload, inv-off, update TCNTB0&TCMPB0, start timer 0
	__raw_writel(tcon, S3C2410_TCON);  //把tcon写到计数器控制寄存器S3C2410_TCON中 
	
	tcon &= ~2;			//clear manual update bit
	__raw_writel(tcon, S3C2410_TCON);
}

static void PWM_Stop(void)
{
	s3c2410_gpio_cfgpin(S3C2410_GPB(0), S3C2410_GPIO_OUTPUT);  //设置GPB0为输出
	s3c2410_gpio_setpin(S3C2410_GPB(0), 0);	//设置GPB0为低电平,使蜂鸣器停止
}

static int s3c24xx_pwm_open(struct inode *inode, struct file *file)
{
	if (!down_trylock(&lock)) //是否获得信号量，是down_trylock(&lock)=0,否则非0
		return 0;
	else
		return -EBUSY;  //返回错误信息:请求的资源不可用
}


static int s3c24xx_pwm_close(struct inode *inode, struct file *file)
{
	PWM_Stop();
	up(&lock);  //释放信号量lock
    return 0;
}

/***************************/ 
/* cmd是1,表示设置频率;    */
/* cmd是2,表示停止pwm;     */
/* cmd是3,表示设置PWM;     */
/***************************/
static int s3c24xx_pwm_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	
	//s3c2410_gpio_cfgpin(S3C2410_GPB(0), S3C2410_GPIO_OUTPUT);  //设置GPB0为输出
	//s3c2410_gpio_setpin(S3C2410_GPB(0), 0);	//设置GPB0为低电平,使蜂鸣器停止
	if(_IOC_TYPE(cmd)!=MEMDEV_IOC_MAGIC)
		return -EINVAL;

	if(_IOC_NR(cmd) > MEMDEV_IOC_MAXNR)
		return -EINVAL;

	switch (cmd) 
	{
	case PWM_IOCTL_SET_FREQ:  // if cmd = 1 即进入case PWM_IOCTL_SET_FREQ
		if (arg == 0)         // 如果设置的频率参数是0
			return -EINVAL;   // 返回错误信息,表示向参数传递了无效的参数
		PWM_Set_Freq(arg);    // 否则设置频率
		break;

	case PWM_IOCTL_STOP:      // if cmd = 2 即进入case PWM_IOCTL_STOP
		PWM_Stop();			  // 停止蜂鸣器
		break;

	case PWM_IOCTL_SET_DC:    // 设置占空比
		__raw_writel(arg,S3C2410_TCMPB(0));
		break;
	}

	return 0;
}

//初始化设备文件操作集的结构体
static struct file_operations dev_fops = {
    .owner   =   THIS_MODULE,
    .open    =   s3c24xx_pwm_open,
    .release =   s3c24xx_pwm_close, 
    .ioctl   =   s3c24xx_pwm_ioctl,
};

static struct miscdevice misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &dev_fops,
};

static int __init dev_init(void)
{
	int ret;

	init_MUTEX(&lock);  //初始化一个互斥锁
	ret = misc_register(&misc);  //注册一个misc设备

	printk (DEVICE_NAME"\tinitialized\n");
    	return ret;
}

static void __exit dev_exit(void)
{
	misc_deregister(&misc);  //注销混杂设备
}

module_init(dev_init);
module_exit(dev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("FriendlyARM Inc.");
MODULE_DESCRIPTION("S3C2410/S3C2440 PWM Driver");
