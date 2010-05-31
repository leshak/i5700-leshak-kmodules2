/****************************************************************************
**
** COPYRIGHT(C)	: Samsung Electronics Co.Ltd, 2006-2015 ALL RIGHTS RESERVED
**
*****************************************************************************/
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>


#include <asm/io.h>
#include <mach/hardware.h>
#include <plat/gpio-cfg.h>
#include <linux/delay.h>

#include <linux/timed_output.h>

//#include "vibetonz.h"

/*********** for debug **********************************************************/
#if 0 
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/

#define OFFSET_VIBRATOR_ON      (0x1 << 0)

#define VIBRATOR_ON_WEEK_PERIOD		(300) // vibrator week value
#define VIBRATOR_OFF_WEEK_PERIOD	(300)

#define VIBRATOR_ON_STRONG_PERIOD	(300) // vibrator strong value
#define VIBRATOR_OFF_STRONG_PERIOD	(3) 

#define VIBRATOR_STRENGTH (0x40000000) // vibrator Week & Strong flag value

static struct hrtimer timer;

static int max_timeout = 5000;
static int vibrator_value = 0;

spinlock_t vib_lock;

extern int s3c6410_timer_setup (int channel, int usec, unsigned long g_tcnt, unsigned long g_tcmp);

#if defined(CONFIG_MACH_SPICA) || defined(CONFIG_MACH_INSTINCTQ)
extern void s3c_bat_set_compensation_for_drv(int mode,int offset);
#endif

static int set_vibetonz(int timeout)
{	
	int val = 0;
	
	if(!timeout) {	
		/* 2009.09.13(sunday) drkim - adjust vibratonz strength */
        s3c6410_timer_setup(1,10,VIBRATOR_ON_STRONG_PERIOD, VIBRATOR_OFF_STRONG_PERIOD);
		
		gpio_set_value(GPIO_VIB_EN, GPIO_LEVEL_LOW);

#if defined(CONFIG_MACH_SPICA) || defined(CONFIG_MACH_INSTINCTQ)
		s3c_bat_set_compensation_for_drv(0,OFFSET_VIBRATOR_ON);
#endif
	}
    else {
        /* 2009.09.13(sunday) drkim - adjust vibratonz strength */
        if(timeout == -1) {
            //printk("[VIBETONZ] HI TEST MODE\n");            
            s3c6410_timer_setup(1,10,VIBRATOR_ON_STRONG_PERIOD, VIBRATOR_OFF_STRONG_PERIOD);
        }
        else {
            if (timeout >= VIBRATOR_STRENGTH) {
               s3c6410_timer_setup(1,10,VIBRATOR_ON_WEEK_PERIOD, VIBRATOR_OFF_WEEK_PERIOD);
                timeout -= 0x40000000;
				//printk("[VIBETONZ] LOW, timeout= %x\n", timeout);
            }
            else {
                s3c6410_timer_setup(1,10,VIBRATOR_ON_STRONG_PERIOD,VIBRATOR_OFF_STRONG_PERIOD);                
            }
        }
		gpio_set_value(GPIO_VIB_EN, GPIO_LEVEL_HIGH);
        
#if defined(CONFIG_MACH_SPICA) || defined(CONFIG_MACH_INSTINCTQ)
		s3c_bat_set_compensation_for_drv(1,OFFSET_VIBRATOR_ON);
#endif
	}

	vibrator_value = timeout;
//    printk("[VIBETONZ] END, timeout = %x\n", vibrator_value);    
	
	return timeout;
}

static enum hrtimer_restart vibetonz_timer_func(struct hrtimer *timer)
{
	unsigned long flags;

	//gprintk("[VIBETONZ] %s : \n",__func__);
	spin_lock_irqsave(&vib_lock, flags);	
	set_vibetonz(0);
	spin_unlock_irqrestore(&vib_lock, flags);
	
	return HRTIMER_NORESTART;
}

static int get_time_for_vibetonz(struct timed_output_dev *dev)
{
	int remaining;

	if (hrtimer_active(&timer)) {
		ktime_t r = hrtimer_get_remaining(&timer);
		remaining = r.tv.sec * 1000 + r.tv.nsec / 1000000;
	} else
		remaining = 0;

	if (vibrator_value ==-1)
		remaining = -1;

	return remaining;
}
static void enable_vibetonz_from_user(struct timed_output_dev *dev,int value)
{
	unsigned long flags;

	//printk("[VIBETONZ] %s : time = %d msec \n",__func__,value);
	hrtimer_cancel(&timer);

	spin_lock_irqsave(&vib_lock, flags);
	value = set_vibetonz(value); /* 2009.09.13(sunday) drkim - adjust vibratonz strength */	
	vibrator_value = value;
	spin_unlock_irqrestore(&vib_lock, flags);

	if (value > 0) 
	{
		if (value > max_timeout)
			value = max_timeout;

		hrtimer_start(&timer,
						ktime_set(value / 1000, (value % 1000) * 1000000),
						HRTIMER_MODE_REL);
		vibrator_value = 0;
	}
}


static struct timed_output_dev timed_output_vt = {
	.name     = "vibrator",
	.get_time = get_time_for_vibetonz,
	.enable   = enable_vibetonz_from_user,
};
static void vibetonz_start(void)
{
	int ret = 0;

	//printk("[VIBETONZ] %s : \n",__func__);
	spin_lock_init(&vib_lock);

	/* hrtimer settings */
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timer.function = vibetonz_timer_func;

	if (gpio_is_valid(GPIO_VIB_EN)) {
		if (gpio_request(GPIO_VIB_EN, S3C_GPIO_LAVEL(GPIO_VIB_EN))) 
			printk(KERN_ERR "Failed to request GPIO_VIB_EN!\n");
		gpio_direction_output(GPIO_VIB_EN,0);
		mdelay(10);
		gpio_set_value(GPIO_VIB_EN, GPIO_LEVEL_LOW);
	}
	s3c_gpio_setpull(GPIO_VIB_EN, S3C_GPIO_PULL_NONE);

	/* pwm timer settings */
	s3c6410_timer_setup(1,10,VIBRATOR_ON_STRONG_PERIOD, VIBRATOR_OFF_STRONG_PERIOD);
	
	/* timed_output_device settings */
	ret = timed_output_dev_register(&timed_output_vt);
	if(ret)
		printk(KERN_ERR "[VIBETONZ] timed_output_dev_register is fail \n");	
}


static void vibetonz_end(void)
{
	printk("[VIBETONZ] %s \n",__func__);
}

static void __init vibetonz_init(void)
{
	
	vibetonz_start();
}


static void __exit vibetonz_exit(void)
{
	vibetonz_end();
}

module_init(vibetonz_init);
module_exit(vibetonz_exit);

MODULE_AUTHOR("SAMSUNG");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("vibetonz control interface");
