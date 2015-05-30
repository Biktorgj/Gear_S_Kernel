/*
** =============================================================================
**
** File: ponte_immvibespi.c
**
** Description:
**     Device-dependent functions called by Immersion TSP API
**     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
**
**
** Copyright (c) 2012-2013 Immersion Corporation. All Rights Reserved.
**
** This file contains Original Code and/or Modifications of Original Code
** as defined in and that are subject to the GNU Public License v2 -
** (the 'License'). You may not use this file except in compliance with the
** License. You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
** the License for the specific language governing rights and limitations
** under the License.
**
** =============================================================================
*/
//#warning ********* Compiling SPI for DRV2604 using LRA actuator ************

#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/slab.h>
#include <linux/types.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/device.h>

#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include <linux/gpio.h>
#include <mach/gpiomux.h>


#include <linux/workqueue.h>
#include "tspdrv.h"
#include "drv2604.h"

#ifdef CONFIG_USING_HW_I2C_DRV2604L
#include <linux/of_device.h>
#include <linux/of.h>
#endif

#if defined (CONFIG_DC_MOTOR)
#include <linux/dc_motor.h>
#endif
#define TSPDRV_BUF_SIZE PAGE_SIZE*16
#define DC_MOTOR_CHECK_NOT_YET -3
int is_dc=-3;
EXPORT_SYMBOL(is_dc);


/*
** Copy ImmVibeSPI.c, autocal.seq, and init.seq from
** actuator subdirectory into the same directory as tspdrv.c
*/

/*
** Enable workqueue to allow DRV2604 time to brake
*/
#define GUARANTEE_AUTOTUNE_BRAKE_TIME  1

/*
** Enable to use DRV2604 EN pin to enter standby mode
*/
#define USE_DRV2604_EN_PIN  1

/*
** gpio connected to DRV2604 EN pin
*/
#define GPIO_AMP_EN	35
#define GPIO_HAPTIC_I2C_SCL	7
#define GPIO_HAPTIC_I2C_SDA	6

/*
** Enable to use DRV2604 i2c command to enter standby mode
*/
#define USE_DRV2604_STANDBY 1

/*
** Address of our device
*/
#define DEVICE_ADDR 0x5A

/*
** i2c bus that it sits on
*/
#define DEVICE_BUS  3

/*
** This SPI supports only one actuator.
*/
#define NUM_ACTUATORS 1

/*
** Name of the DRV2604 board
*/
#define DRV2604_BOARD_NAME   "DRV2604L"
#define HAPTIC_I2C_ID				15
/*
** Go
*/
#define GO_REG 0x0C
#define GO     0x01
#define STOP   0x00

/*
** Status
*/
#define STATUS_REG          0x00
#define STATUS_DEFAULT      0x00

#define DIAG_RESULT_MASK    (1 << 3)
#define AUTO_CAL_PASSED     (0 << 3)
#define AUTO_CAL_FAILED     (1 << 3)
#define DIAG_GOOD           (0 << 3)
#define DIAG_BAD            (1 << 3)

#define DEV_ID_MASK (7 << 5)
#define DRV2605 (3 << 5) /* per DRV2604 datasheet */
#define DRV2604 (4 << 5)
#define DRV2605L (7<<5)
#define DRV2604L (6<<5)
/*
** Mode
*/
#define MODE_REG            0x01
#define MODE_STANDBY        0x40
#define MODE_DEVICE_READY   0x00

#define DRV2604_MODE_MASK           0x07
#define MODE_INTERNAL_TRIGGER       0
#define MODE_REAL_TIME_PLAYBACK     5
#define MODE_DIAGNOSTICS            6
#define AUTO_CALIBRATION            7

#define MODE_RESET                  0x80

/*
** Real Time Playback
*/
#define REAL_TIME_PLAYBACK_REG      0x02

/*
** Library Selection
*/
#define LIBRARY_SELECTION_REG       0x03
#define LIBRARY_SELECTION_DEFAULT   0x00

/*
** Waveform Sequencer
*/
#define WAVEFORM_SEQUENCER_REG      0x04
#define WAVEFORM_SEQUENCER_REG2     0x05
#define WAVEFORM_SEQUENCER_REG3     0x06
#define WAVEFORM_SEQUENCER_REG4     0x07
#define WAVEFORM_SEQUENCER_REG5     0x08
#define WAVEFORM_SEQUENCER_REG6     0x09
#define WAVEFORM_SEQUENCER_REG7     0x0A
#define WAVEFORM_SEQUENCER_REG8     0x0B
#define WAVEFORM_SEQUENCER_MAX      8
#define WAVEFORM_SEQUENCER_DEFAULT  0x00

/*
** OverDrive Time Offset
*/
#define OVERDRIVE_TIME_OFFSET_REG  0x0D

/*
** Sustain Time Offset, postive
*/
#define SUSTAIN_TIME_OFFSET_POS_REG 0x0E

/*
** Sustain Time Offset, negative
*/
#define SUSTAIN_TIME_OFFSET_NEG_REG 0x0F

/*
** Brake Time Offset
*/
#define BRAKE_TIME_OFFSET_REG       0x10

/*
** Rated Voltage
*/
#define RATED_VOLTAGE_REG           0x16

/*
** Overdrive Clamp Voltage
*/
#define OVERDRIVE_CLAMP_VOLTAGE_REG 0x17

/*
** Auto Calibrationi Compensation Result
*/
#define AUTO_CALI_RESULT_REG        0x18

/*
** Auto Calibration Back-EMF Result
*/
#define AUTO_CALI_BACK_EMF_RESULT_REG 0x19

/*
** Feedback Control
*/
#define FEEDBACK_CONTROL_REG        0x1A

#define FEEDBACK_CONTROL_BEMF_LRA_GAIN0 0 /* 5x */
#define FEEDBACK_CONTROL_BEMF_LRA_GAIN1 1 /* 10x */
#define FEEDBACK_CONTROL_BEMF_LRA_GAIN2 2 /* 20x */
#define FEEDBACK_CONTROL_BEMF_LRA_GAIN3 3 /* 30x */

#define LOOP_RESPONSE_SLOW      (0 << 2)
#define LOOP_RESPONSE_MEDIUM    (1 << 2) /* default */
#define LOOP_RESPONSE_FAST      (2 << 2)
#define LOOP_RESPONSE_VERY_FAST (3 << 2)

#define FB_BRAKE_FACTOR_1X   (0 << 4) /* 1x */
#define FB_BRAKE_FACTOR_2X   (1 << 4) /* 2x */
#define FB_BRAKE_FACTOR_3X   (2 << 4) /* 3x (default) */
#define FB_BRAKE_FACTOR_4X   (3 << 4) /* 4x */
#define FB_BRAKE_FACTOR_6X   (4 << 4) /* 6x */
#define FB_BRAKE_FACTOR_8X   (5 << 4) /* 8x */
#define FB_BRAKE_FACTOR_16X  (6 << 4) /* 16x */
#define FB_BRAKE_DISABLED    (7 << 4)

#define FEEDBACK_CONTROL_MODE_ERM 0 /* default */
#define FEEDBACK_CONTROL_MODE_LRA (1 << 7)

/*
** Control1
*/
#define Control1_REG            0x1B
#define STARTUP_BOOST_ENABLED   (1 << 7)
#define STARTUP_BOOST_DISABLED  (0 << 7) /* default */

/*
** Control2
*/
#define Control2_REG            0x1C

#define IDISS_TIME_MASK         0x03
#define IDISS_TIME_VERY_SHORT   0
#define IDISS_TIME_SHORT        1
#define IDISS_TIME_MEDIUM       2 /* default */
#define IDISS_TIME_LONG         3

#define BLANKING_TIME_MASK          0x0C
#define BLANKING_TIME_VERY_SHORT    (0 << 2)
#define BLANKING_TIME_SHORT         (1 << 2)
#define BLANKING_TIME_MEDIUM        (2 << 2) /* default */
#define BLANKING_TIME_VERY_LONG     (3 << 2)

#define AUTO_RES_GAIN_MASK         0x30
#define AUTO_RES_GAIN_VERY_LOW     (0 << 4)
#define AUTO_RES_GAIN_LOW          (1 << 4)
#define AUTO_RES_GAIN_MEDIUM       (2 << 4) /* default */
#define AUTO_RES_GAIN_HIGH         (3 << 4)

#define SOFT_BRAKE_MASK            0x40
#define SOFT_BRAKE                 (1 << 6)

#define BIDIR_INPUT_MASK           0x80
#define UNIDIRECT_INPUT            (0 << 7)
#define BIDIRECT_INPUT             (1 << 7) /* default */

/*
** Control3
*/
#define Control3_REG 0x1D

#define ERM_OpenLoop_Enabled (1 << 5)
#define NG_Thresh_1 (1 << 6)
#define NG_Thresh_2 (2 << 6)
#define NG_Thresh_3 (3 << 6)

/*
** Auto Calibration Memory Interface
*/
#define AUTOCAL_MEM_INTERFACE_REG   0x1E

#define AUTOCAL_TIME_150MS          (0 << 4)
#define AUTOCAL_TIME_250MS          (1 << 4)
#define AUTOCAL_TIME_500MS          (2 << 4)
#define AUTOCAL_TIME_1000MS         (3 << 4)

#define SILICON_REVISION_REG        0x3B
#define SILICON_REVISION_MASK       0x07

#define DEFAULT_DRIVE_TIME      0x17

#define MAX_AUTOCALIBRATION_ATTEMPT 10
#define SKIP_AUTOCAL        1
#define GO_BIT_POLL_INTERVAL    15

#define MAX_REVISION_STRING_SIZE 20

#ifndef GPIO_LEVEL_HIGH
#define GPIO_LEVEL_HIGH 1
#endif
#ifndef GPIO_LEVEL_LOW
#define GPIO_LEVEL_LOW 0
#endif

#ifdef CONFIG_REG_DRV2604L
struct dentry *drv_debugfs_dir;
#endif

static int g_nDeviceID = -1;
static struct i2c_client* g_pTheClient = NULL;
static bool g_bAmpEnabled = false;
static bool g_bStarted;
static void drv2604_set_en(bool);
static const unsigned char init_sequence[] = {
#include "init.seq"
};

#if SKIP_AUTOCAL == 0
static const unsigned char autocal_sequence[] = {
#include "init.seq"
};
#endif

#ifndef ACTUATOR_NAME
#define ACTUATOR_NAME "haptic_motor"
#endif

#if defined(CONFIG_DRV2604L)
#define RATE_VOLTAGE_VALUE init_sequence[45]
#endif
#define SET_BIT_NUM 127
extern int system_rev;

static void drv2604_write_reg_val(const unsigned char* data, unsigned int size)
{
    int i = 0;

    if (size % 2 != 0)
        return;

    while (i < size)
    {
        i2c_smbus_write_byte_data(g_pTheClient, data[i], data[i+1]);
        i+=2;
    }
}

static unsigned char drv2604_read_reg(unsigned char reg)
{
    return i2c_smbus_read_byte_data(g_pTheClient, reg);
}

#if SKIP_AUTOCAL == 0
static void drv2604_poll_go_bit(void)
{
    while (drv2604_read_reg(GO_REG) == GO)
      schedule_timeout_interruptible(msecs_to_jiffies(GO_BIT_POLL_INTERVAL));
}
#endif

static void drv2604_set_rtp_val(char value)
{
    char rtp_val[] =
    {
        REAL_TIME_PLAYBACK_REG, value
    };

    drv2604_write_reg_val(rtp_val, sizeof(rtp_val));

}

static void drv2604_change_mode(char mode)
{
    unsigned char tmp[] =
    {
        MODE_REG, mode
    };
    drv2604_write_reg_val(tmp, sizeof(tmp));
}

#if USE_DRV2604_EN_PIN
static void drv2604_set_en(bool enabled)
{
    gpio_direction_output(GPIO_AMP_EN, enabled ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}
#endif

#ifdef CONFIG_REG_DRV2604L

static int drv_debug_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t drv_debug_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char *buf;
	size_t len = 0;
	ssize_t ret;
	int i;

	if (*ppos != 0)
		return 0;

	if (count < sizeof(buf))
		return -ENOSPC;

	buf = kzalloc(TSPDRV_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	drv2604_set_en(true);
	usleep_range(1000, 1000);
	for(i = 0x00; i <= 0xFF; i += 0x01){
		if(( 0x00 <= i && i <= 0x10) || (0x16 <= i && i <= 0x22) || (0xFD <= i && i<=0xFF)){
			len += snprintf(buf + len, TSPDRV_BUF_SIZE - len,
			"drv2604 reg addr = %x , value =%x\n", i, drv2604_read_reg(i));
		}
	}
	ret = simple_read_from_buffer(buffer, len, ppos, buf, TSPDRV_BUF_SIZE);
	kfree(buf);

	return ret;
}


static ssize_t drv_debug_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *ppos)
{

	unsigned int addr=0;
	unsigned int value=0;
	char *start_ptr = NULL;

	start_ptr = strstr(buffer,"0x");
	sscanf(start_ptr+2, "%x", &addr);


	start_ptr = strstr(start_ptr+2,"0x");
	sscanf(start_ptr+2, "%x", &value);

	pr_info("drv2604L : addr = %x value = %x\n",addr, value);
	drv2604_set_en(true);
	usleep_range(1000, 1000);
	i2c_smbus_write_byte_data(g_pTheClient, addr, value);
	return count;
}

static const struct file_operations drv_debugfs_fops = {
	.owner = THIS_MODULE,
	.open =  drv_debug_open,
	.read = drv_debug_read,
	.write = drv_debug_write,
};
#endif

static int32_t ImmVibeSPI_ForceOut_AmpDisable(u_int8_t nActuatorIndex);
static void drv2604_shutdown(struct i2c_client* client);
static int drv2604_probe(struct i2c_client* client, const struct i2c_device_id* id);
static int drv2604_remove(struct i2c_client* client);

static const struct i2c_device_id drv2604_id[] =
{
    {DRV2604_BOARD_NAME,0},
    {}
};

#ifdef CONFIG_USING_HW_I2C_DRV2604L
static struct of_device_id tidrv2604Li2c_of_match[] = {
	{ .compatible = DRV2604_BOARD_NAME, },
	{ }
};
#endif

static int drv2604_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
    char status;
#if SKIP_AUTOCAL == 0
    int nCalibrationCount = 0;
    int values=0x29;
#endif
#ifdef CONFIG_USING_HW_I2C_DRV2604L
	struct device *dev;
	const struct of_device_id *match;
	dev = &client->dev;
#endif
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        DbgOut((DBL_ERROR, "drv2604L probe failed"));
        return -ENODEV;
    }
#ifdef CONFIG_USING_HW_I2C_DRV2604L
	match = of_match_device(of_match_ptr(tidrv2604Li2c_of_match), dev);
#endif

    g_pTheClient = client;

#if USE_DRV2604_EN_PIN
    gpio_request(GPIO_AMP_EN, "vibrator-en");
    drv2604_set_en(true);
    usleep_range(1000, 1000);
#endif
    status = drv2604_read_reg(MODE_REG);
    pr_info("drv2604L : mode register value = %x\n", status);

#if USE_DRV2604_STANDBY
    /* Wait 1000 us for chip power to stabilize */
    usleep_range(100000, 100000);
    drv2604_change_mode(MODE_DEVICE_READY);
#endif
    /* Wait 1000 us for chip power to stabilize */
    usleep_range(1000, 1000);

#if SKIP_AUTOCAL
    drv2604_write_reg_val(init_sequence, sizeof(init_sequence));
    status = drv2604_read_reg(STATUS_REG);
#else
    /* Run auto-calibration */
    do{
        drv2604_write_reg_val(autocal_sequence, sizeof(autocal_sequence));
	i2c_smbus_write_byte_data(g_pTheClient, 0x16, values);
	values-=0x2;
        /* Wait until the procedure is done */
        drv2604_poll_go_bit();

        /* Read status */
        status = drv2604_read_reg(STATUS_REG);
        nCalibrationCount++;

    } while (((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED) && (nCalibrationCount < MAX_AUTOCALIBRATION_ATTEMPT));

    /* Check result */
    if ((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED)
    {
      DbgOut((DBL_ERROR, "drv2604 auto-calibration failed after %d attempts.\n", nCalibrationCount));
    }
    else
    {
        /* Read calibration results */
        drv2604_read_reg(AUTO_CALI_RESULT_REG);
        drv2604_read_reg(AUTO_CALI_BACK_EMF_RESULT_REG);
        drv2604_read_reg(FEEDBACK_CONTROL_REG);
    }
#endif

    /* Read device ID */
    g_nDeviceID = (status & DEV_ID_MASK);
    i2c_smbus_write_byte_data(g_pTheClient, 0x16, RATE_VOLTAGE_VALUE);

    switch (g_nDeviceID)
    {
        case DRV2605:
            DbgOut((DBL_INFO, "drv2604 driver found: drv2605.\n"));
            break;
        case DRV2604:
            DbgOut((DBL_INFO, "drv2604 driver found: drv2604.\n"));
            break;
        case DRV2604L:
            DbgOut((DBL_INFO, "drv2604 driver found: drv2604L.\n"));
            break;
        case DRV2605L:
            DbgOut((DBL_INFO, "drv2604 driver found: drv2605L.\n"));
            break;
        default:
            DbgOut((DBL_INFO, "drv2604 driver found: unknown.\n"));
            break;
    }

#if USE_DRV2604_STANDBY
    /* Put hardware in standby */
    drv2604_change_mode(MODE_STANDBY);
#elif USE_DRV2604_EN_PIN
    /* enable RTP mode that will be toggled on/off with EN pin */
#endif

#if USE_DRV2604_EN_PIN
    /* turn off chip */
    drv2604_set_en(false);
#endif

    DbgOut((DBL_INFO, "drv2604 probe succeeded"));
#ifdef CONFIG_REG_DRV2604L
    {
	drv_debugfs_dir = kzalloc(sizeof(struct dentry), GFP_KERNEL);

	if (!drv_debugfs_dir)
		return -ENOMEM;

	drv_debugfs_dir =
		debugfs_create_dir("drv_debug", NULL);
	if (drv_debugfs_dir) {
		if (!debugfs_create_file("drv_history", 0666,
			drv_debugfs_dir,
			NULL, &drv_debugfs_fops))
			pr_err("%s : debugfs_create_file, error\n", __func__);

	} else
		pr_err("%s : debugfs_create_dir, error\n", __func__);
    }
#endif
  return 0;
}

struct i2c_driver drv2604_driver= {
    .probe = drv2604_probe,
    .remove = drv2604_remove,
    .id_table = drv2604_id,
    .shutdown = drv2604_shutdown,
    .driver =
    {
        .name = DRV2604_BOARD_NAME,
    },
};

static void drv2604_shutdown(struct i2c_client* client)
{

    ImmVibeSPI_ForceOut_AmpDisable(0);

    /* Remove TS5000 driver */
    i2c_del_driver(&drv2604_driver);

    /* Reverse i2c_new_device */
    i2c_unregister_device(g_pTheClient);

    DbgOut((DBL_ERROR, "drv2604_shutdown.\n"));
    return;
}


static int drv2604_remove(struct i2c_client* client)
{
    DbgOut((DBL_VERBOSE, "drv2604_remove.\n"));
    return 0;
}




#if GUARANTEE_AUTOTUNE_BRAKE_TIME

#define AUTOTUNE_BRAKE_TIME 25

static VibeInt8 g_lastForce = 0;
static bool g_brake = false;

static void autotune_brake_complete(struct work_struct *work)
{
    /* new nForce value came in before workqueue terminated */
    if (g_lastForce > 0)
        return;

    pr_info("drv2604L : %s [%d-%d], isAmp=%d\n",__func__, raw_smp_processor_id(),current->pid, g_bAmpEnabled);
#if USE_DRV2604_STANDBY
    /* Put hardware in standby */
    drv2604_change_mode(MODE_STANDBY);
#endif

#if USE_DRV2604_EN_PIN
    drv2604_set_en(false);
#endif
}

DECLARE_DELAYED_WORK(g_brake_complete, autotune_brake_complete);

static struct workqueue_struct *g_workqueue;

#endif

/*
** Called to disable amp (disable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
	if(DC_MOTOR_CHECK_NOT_YET == is_dc)
		is_dc  = is_dc_motor();
	if(!is_dc)
	{
	    if (g_bAmpEnabled)
	    {
	        DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpDisable.\n"));

	        /* Set the force to 0 */
	        drv2604_set_rtp_val(0);

#if GUARANTEE_AUTOTUNE_BRAKE_TIME
	        /* if a brake signal arrived from daemon, let the chip stay on
	         * extra time to allow it to brake */
	        if (g_brake && g_workqueue)
	        {
	            queue_delayed_work(g_workqueue,
	                               &g_brake_complete,
	                               msecs_to_jiffies(AUTOTUNE_BRAKE_TIME));
	        }
	        else /* disable immediately (smooth effect style) */
#endif
	        {
#if USE_DRV2604_STANDBY
	            /* Put hardware in standby via i2c */
	            drv2604_change_mode(MODE_STANDBY);
#endif

#if USE_DRV2604_EN_PIN
	            /* Disable hardware via pin */
	            drv2604_set_en(false);
#endif
	        }

	        g_bAmpEnabled = false;
	    }
	}else{
		if (g_bAmpEnabled)
		{
			DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpDisable.\n"));
			vibtonz_enable(0);
	        	g_bAmpEnabled = false;
		}
	}
	return VIBE_S_SUCCESS;
}

/*
** Called to enable amp (enable output force)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
	if(DC_MOTOR_CHECK_NOT_YET == is_dc)
		is_dc  = is_dc_motor();
	if(!is_dc)
	{
	    if (!g_bAmpEnabled)
	    {
	        DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpEnable.\n"));

#if GUARANTEE_AUTOTUNE_BRAKE_TIME
	        cancel_delayed_work_sync(&g_brake_complete);
#endif

#if USE_DRV2604_EN_PIN
		drv2604_set_en(true);
		usleep_range(1000, 1000);
#endif

#if USE_DRV2604_STANDBY
	        drv2604_change_mode(MODE_DEVICE_READY);
#endif
	        // Chip requires minimum 250us power-up delay before RTP value can be set
	        // If the chip is powered on <10ms after powering off, it needs 1000us
	        // for the internal LDO voltage to stabilize
	        usleep_range(1000, 1000);

	        /* Workaround for power issue in the DRV2604 */
	        /* Restore the register settings if they have reset to the defaults */
		/*
	        if(drv2604_read_reg(RATED_VOLTAGE_REG) != RATE_VOLTAGE_VALUE)
	        {
	            drv2604_write_reg_val(init_sequence, sizeof(init_sequence));
	        }
		*/
	        drv2604_change_mode(MODE_REAL_TIME_PLAYBACK);
	        g_bAmpEnabled = true;
	    }
	}else{
		if (!g_bAmpEnabled)
		{
			DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_AmpEnable.\n"));
			vibtonz_enable(1);
			g_bAmpEnabled = true;
		}
	}
	return VIBE_S_SUCCESS;
}

/*
** Called at initialization time.
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{
	DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_Initialize.\n"));
	if(DC_MOTOR_CHECK_NOT_YET == is_dc)
		is_dc  = is_dc_motor();
	if(false == is_dc)
	{
		int retVal;
		retVal= i2c_add_driver(&drv2604_driver);
		if (retVal) {
		    return VIBE_E_FAIL;
		}

#if GUARANTEE_AUTOTUNE_BRAKE_TIME
	    g_workqueue = create_workqueue("tspdrv_workqueue");
#endif
	}else{
		DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_Initialize.\n"));
		g_bAmpEnabled = true;   /* to force ImmVibeSPI_ForceOut_AmpDisable disabling the amp */

		    /*
		    ** Disable amp.
		    ** If multiple actuators are supported, please make sure to call
		    ** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
		    ** input argument).
		    */
		ImmVibeSPI_ForceOut_AmpDisable(0);
	}
    	return VIBE_S_SUCCESS;
}

/*
** Called at termination time to disable amp, etc...
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
	if(DC_MOTOR_CHECK_NOT_YET == is_dc)
		is_dc  = is_dc_motor();
	if(false == is_dc)
	{
	    DbgOut((DBL_VERBOSE, "ImmVibeSPI_ForceOut_Terminate.\n"));

#if GUARANTEE_AUTOTUNE_BRAKE_TIME
	    if (g_workqueue)
	    {
	        destroy_workqueue(g_workqueue);
	        g_workqueue = 0;
	    }
#endif

	    ImmVibeSPI_ForceOut_AmpDisable(0);

	    /* Remove TS5000 driver */
	    i2c_del_driver(&drv2604_driver);

	    /* Reverse i2c_new_device */
	    i2c_unregister_device(g_pTheClient);
	}else{

	    DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_Terminate.\n"));

	    /*
	    ** Disable amp.
	    ** If multiple actuators are supported, please make sure to call
	    ** ImmVibeSPI_ForceOut_AmpDisable for each actuator (provide the actuator index as
	    ** input argument).
	    */
	    ImmVibeSPI_ForceOut_AmpDisable(0);
	}
	return VIBE_S_SUCCESS;
}

/*
** Called by the real-time loop to set the force
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth,
														VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
	if(DC_MOTOR_CHECK_NOT_YET == is_dc)
		is_dc  = is_dc_motor();
	if(false == is_dc)
	{
#if GUARANTEE_AUTOTUNE_BRAKE_TIME
	    VibeInt8 force = pForceOutputBuffer[0];
	    if (force > 0 && g_lastForce <= 0)
	    {
	        g_brake = false;

	        ImmVibeSPI_ForceOut_AmpEnable(nActuatorIndex);
	    }
	    else if (force <= 0 && g_lastForce > 0)
	    {
	        g_brake = force < 0;

	        ImmVibeSPI_ForceOut_AmpDisable(nActuatorIndex);
	    }

	    if (g_lastForce != force)
	    {
	        /* AmpDisable sets force to zero, so need to here */
	        if (force > 0)
	            drv2604_set_rtp_val(pForceOutputBuffer[0]);

	        g_lastForce = force;
	    }
#else
	    drv2604_set_rtp_val(pForceOutputBuffer[0]);
#endif
	}else{
		VibeInt8 nForce;
	    unsigned int calc_value;

	    g_bStarted = true;

	    switch (nOutputSignalBitDepth)
	    {
	        case 8:
	            /* pForceOutputBuffer is expected to contain 1 byte */
	            if (nBufferSizeInBytes != 1) return VIBE_E_FAIL;

	            nForce = pForceOutputBuffer[0];
	            break;
	        case 16:
	            /* pForceOutputBuffer is expected to contain 2 byte */
	            if (nBufferSizeInBytes != 2) return VIBE_E_FAIL;

	            /* Map 16-bit value to 8-bit */
	            nForce = ((VibeInt16*)pForceOutputBuffer)[0] >> 8;
	            break;
	        default:
	            /* Unexpected bit depth */
	            return VIBE_E_FAIL;
	    }

	    if (nForce <= 0)
	    {
	        vibtonz_set_voltage(0);
	    }
	    else
	    {
		calc_value = (nForce * DC_MOTOR_VOL_MAX) / SET_BIT_NUM;
		if(calc_value < DC_MOTOR_VOL_MIN)
			calc_value = DC_MOTOR_VOL_MIN;

		if(calc_value > DC_MOTOR_VOL_MAX)
			calc_value = DC_MOTOR_VOL_MAX;
		/* 0 ~ 27 */
		vibtonz_set_voltage(calc_value);
	   }
	}

    return VIBE_S_SUCCESS;
}

/*
** Called to set force output frequency parameters
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
	if(DC_MOTOR_CHECK_NOT_YET == is_dc)
		is_dc  = is_dc_motor();
	if(false == is_dc)
	{
	    if (nActuatorIndex != 0) return VIBE_S_SUCCESS;

	    switch (nFrequencyParameterID)
	    {
	        case VIBE_KP_CFG_FREQUENCY_PARAM1:
	            /* Update frequency parameter 1 */
	            break;

	        case VIBE_KP_CFG_FREQUENCY_PARAM2:
	            /* Update frequency parameter 2 */
	            break;

	        case VIBE_KP_CFG_FREQUENCY_PARAM3:
	            /* Update frequency parameter 3 */
	            break;

	        case VIBE_KP_CFG_FREQUENCY_PARAM4:
	            /* Update frequency parameter 4 */
	            break;

	        case VIBE_KP_CFG_FREQUENCY_PARAM5:
	            /* Update frequency parameter 5 */
	            break;

	        case VIBE_KP_CFG_FREQUENCY_PARAM6:
	            /* Update frequency parameter 6 */
	            break;
	    }
	}
    return VIBE_S_SUCCESS;
}

/*
** Called to get the device name (device name must be returned as ANSI char)
*/
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
	if(DC_MOTOR_CHECK_NOT_YET == is_dc)
		is_dc  = is_dc_motor();
	if(false == is_dc)
	{
	    char szRevision[MAX_REVISION_STRING_SIZE];

	    if ((!szDevName) || (nSize < 1)) return VIBE_E_FAIL;

	    DbgOut((DBL_VERBOSE, "ImmVibeSPI_Device_GetName.\n"));

	    switch (g_nDeviceID)
	    {
	        case DRV2605:
	            strncpy(szDevName, "DRV2605", nSize-1);
	            break;
	        case DRV2604:
	            strncpy(szDevName, "DRV2604", nSize-1);
		 case DRV2604L:
	            strncpy(szDevName, "DRV2604L", nSize-1);
	            break;
		 case DRV2605L:
	            strncpy(szDevName, "DRV2605L", nSize-1);
	        default:
	            strncpy(szDevName, "Unknown", nSize-1);
	            break;
	    }

	    /* Append revision number to the device name */
	    sprintf(szRevision, "r%d %s", (drv2604_read_reg(SILICON_REVISION_REG) & SILICON_REVISION_MASK), ACTUATOR_NAME);
	    if ((strlen(szRevision) + strlen(szDevName)) < nSize-1)
	        strcat(szDevName, szRevision);

	    szDevName[nSize - 1] = '\0'; /* make sure the string is NULL terminated */
	}else{
		if ((!szDevName) || (nSize < 1)) return VIBE_E_FAIL;
	    	DbgOut((DBL_INFO, "ImmVibeSPI_Device_GetName.\n"));
	    	pr_info("tspdrv : %s  : name = %s\n", __func__, szDevName);
	    	strncpy(szDevName, "Project B", nSize-1);
	    	szDevName[nSize - 1] = '\0';    /* make sure the string is NULL terminated */
	}

    return VIBE_S_SUCCESS;
}
