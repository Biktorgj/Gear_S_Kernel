#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/sec_class.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/slab.h>

#if defined CONFIG_MACH_B3
#include "b3_motor_name.h"
#endif
typedef struct mot_data{
	char* motor_name;
	char* motor_type;
}motor_info_data;

static int motor_info_uevent(struct device *, struct kobj_uevent_env *);
static motor_info_data *mot_data;
static struct class *sec_motor_info;
static struct device *motor_info;

static ssize_t show_motor_name(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	char *mot_name;
#if defined CONFIG_MACH_B3
	mot_name = get_motor_name();
#else
	mot_name = "NULL";
#endif

	return sprintf(buf, "%s\n", mot_name);
}

static DEVICE_ATTR(motor_name, 0664, show_motor_name, NULL);

static struct device_attribute *motor_class_attr[] ={
	&dev_attr_motor_name,
	NULL,
};

static int motor_info_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	int ret = 0;
	ret = add_uevent_var(env, "MOTOR_NAME=%s", mot_data->motor_name);
	if(ret){
		pr_info("tspdrv : add_uevent error\n");
	}
	ret = add_uevent_var(env, "MOTOR_TYPE=%s", mot_data->motor_type);
	if(ret){
		pr_info("tspdrv : add_uevent error\n");
	}

	return ret;

}

static void save_motor_data(void)
{
	mot_data->motor_name = kzalloc(20, GFP_KERNEL);
	mot_data->motor_type = kzalloc(20, GFP_KERNEL);

	mot_data->motor_name = get_motor_name();
	mot_data->motor_type = get_motor_type();
}

static int create_motor_class(void)
{
	int ret, i;
	mot_data = kzalloc(sizeof(motor_info_data), GFP_KERNEL);
	sec_motor_info = class_create(THIS_MODULE, "vibrator");
	save_motor_data();
	if (IS_ERR(sec_motor_info)) {
		pr_info("tspdrv : Failed to create class(sec_motor_info class)!!\n");
		return -1;
	}


	motor_info = device_create(sec_motor_info, NULL, 0, NULL, "motor_info");
	if(IS_ERR(motor_info)){
		ret = PTR_ERR(motor_info);
		pr_info("tspdrv : Failed to create device(motor_info)\n");
		return ret;
	}
	sec_motor_info->dev_uevent = motor_info_uevent;
	for(i=0; i<ARRAY_SIZE(motor_class_attr); i++){
		if(motor_class_attr[i] == NULL)
			break;
		ret = device_create_file(motor_info, *motor_class_attr);
		if(ret<0){
			printk("%s : create file error\n", __func__);
			return ret;
		}
	}
	return 0;
}

static int __init motor_driver_init(void)
{
	return create_motor_class();
}

static void __exit motor_driver_exit(void)
{
	kfree(mot_data->motor_name);
	kfree(mot_data->motor_type);
	kfree(mot_data);
	device_unregister(motor_info);
}

module_init(motor_driver_init);
module_exit(motor_driver_exit);

MODULE_DESCRIPTION("motor name class driver");
MODULE_LICENSE("GPL");
