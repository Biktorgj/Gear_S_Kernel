/* 
 * Copyright (c)2013 Maxim Integrated Products, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include "../ssp.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define VENDOR		"ADI"
#define CHIP_ID		"ADPD142"
#define CHIP_ID2		"AD45251"

#define OSC_REG_VAL_FILE_PATH "/csa/sensor/hrm_osc_reg_val"
#define EOL_DATA_FILE_PATH "/csa/sensor/hrm_eol_data"

static ssize_t hrm_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t hrm_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	if (data->ap_rev > 1)
		return sprintf(buf, "%s\n", CHIP_ID2);
	else
		return sprintf(buf, "%s\n", CHIP_ID);
}

static int hrm_open_oscregvalue(struct ssp_data *data)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(OSC_REG_VAL_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);
		data->hrmcal.osc_reg = 0;

		return iRet;
	}

	iRet = cal_filp->f_op->read(cal_filp, (char *)&data->hrmcal.osc_reg,
		sizeof(u32), &cal_filp->f_pos);
	if (iRet != sizeof(u32))
		iRet = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	pr_info("[SSP] %s: %d\n", __func__, data->hrmcal.osc_reg);

	if (data->hrmcal.osc_reg == 0)
		return ERROR;

	return iRet;
}

static int hrm_open_eol_data(struct ssp_data *data)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;
	u32 eol_data[15];

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(EOL_DATA_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);

		data->hrmcal.green_70ma = 0;
		data->hrmcal.green_250ma = 0;
		data->hrmcal.red_70ma = 0;
		data->hrmcal.ir_70ma = 0;

		return iRet;
	}

	iRet = cal_filp->f_op->read(cal_filp, (char *)&eol_data,
		15 * sizeof(u32), &cal_filp->f_pos);
	if (iRet != 15 * sizeof(u32))
		iRet = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	pr_info("[SSP] %s: [%d %d %d %d %d]\n", __func__,
		eol_data[0], eol_data[1], eol_data[2],
		eol_data[3], eol_data[4]);
	pr_info("[SSP] %s: [%d %d %d %d %d]\n", __func__,
		eol_data[5], eol_data[6], eol_data[7],
		eol_data[8], eol_data[9]);
	pr_info("[SSP] %s: [%d %d %d %d %d]\n", __func__,
		eol_data[10], eol_data[11], eol_data[12],
		eol_data[13], eol_data[14]);

	data->hrmcal.green_70ma = eol_data[1];
	data->hrmcal.green_250ma = eol_data[2];
	data->hrmcal.red_70ma = eol_data[4];
	data->hrmcal.ir_70ma = eol_data[5];

	return iRet;
}

static int save_hrm_oscregvalue(struct ssp_data *data)
{
	int iRet = 0;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	pr_info("[SSP] %s: %d\n", __func__, data->hrmcal.osc_reg);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(OSC_REG_VAL_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("[SSP]: %s - Can't open osc_reg_value file\n", __func__);
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);
		return -EIO;
	}

	iRet = cal_filp->f_op->write(cal_filp, (char *)&data->hrmcal.osc_reg,
		sizeof(u32), &cal_filp->f_pos);
	if (iRet != sizeof(u32)) {
		pr_err("[SSP]: %s - Can't write hrm osc_reg_value to file\n", __func__);
		iRet = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return iRet;
}

static int save_hrm_eol_data(struct ssp_data *data)
{
	int iRet = 0;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(EOL_DATA_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("[SSP]: %s - Can't open osc_reg_value file\n", __func__);
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);
		return -EIO;
	}

	iRet = cal_filp->f_op->write(cal_filp,
		(char *)&data->buf[BIO_HRM_RAW_FAC],
		15 * sizeof(u32), &cal_filp->f_pos);
	if (iRet != 15 * sizeof(u32)) {
		pr_err("[SSP]: %s - Can't write hrm osc_reg_value to file\n",
			__func__);
		iRet = -EIO;
	}

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return iRet;
}


int set_hrm_calibration(struct ssp_data *data)
{
	int iRet = 0;
	struct ssp_msg *msg;

	if (!(data->uSensorState & (1 << BIO_HRM_RAW))) {
		pr_info("[SSP]: %s - Skip this function!!!"\
			", hrm sensor is not connected(0x%x)\n",
			__func__, data->uSensorState);
		return iRet;
	}

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n", __func__);
		return -ENOMEM;
	}
	msg->cmd = MSG2SSP_AP_MCU_SET_HRM_OSC_REG;
	msg->length = 20;
	msg->options = AP2HUB_WRITE;
	msg->buffer = (char *) kzalloc(20, GFP_KERNEL);

	msg->free_buffer = 1;
	memcpy(msg->buffer, &data->hrmcal, 20);

	iRet = ssp_spi_async(data, msg);

	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s - i2c fail %d\n", __func__, iRet);
		iRet = ERROR;
	}

	pr_info("[SSP] %s: [%d %d %d %d %d]\n", __func__,
		data->hrmcal.osc_reg, data->hrmcal.green_70ma,
		data->hrmcal.green_250ma, data->hrmcal.red_70ma,
		data->hrmcal.ir_70ma);

	return iRet;
}

int hrm_open_calibration(struct ssp_data *data)
{
	int iRet = 0;

	hrm_open_oscregvalue(data);
	hrm_open_eol_data(data);

	return iRet;
}

static ssize_t hrm_lib_version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 buffer  = 0;
	int iRet;
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg;

	msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	if (msg == NULL) {
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n", __func__);
		goto exit;
	}
	msg->cmd = HRM_LIB_VERSION_INFO;
	msg->length = HRM_LIB_VERSION_INFO_LENGTH;
	msg->options = AP2HUB_READ;
	msg->buffer = (char*)&buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 3000);
	if (iRet != SUCCESS) {
		pr_err("[SSP] %s - hrm lib version Timeout!!\n", __func__);
		goto exit;
	}

	pr_info("[SSP] %s - %x\n", __func__, buffer);

	return sprintf(buf, "%x\n", buffer);
exit:
	return sprintf(buf, "%d,%d,%d,%d\n", -5, 0, 0, 0);
}

static ssize_t hrm_eol_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	data->hrmcal.osc_reg = data->buf[BIO_HRM_RAW_FAC].oscRegValue;
	data->hrmcal.green_70ma = data->buf[BIO_HRM_RAW_FAC].green_dc_level;
	data->hrmcal.green_250ma
		= data->buf[BIO_HRM_RAW_FAC].green_high_dc_level;
	data->hrmcal.red_70ma = data->buf[BIO_HRM_RAW_FAC].red_dc_levet;
	data->hrmcal.ir_70ma = data->buf[BIO_HRM_RAW_FAC].ir_dc_level;

	save_hrm_oscregvalue(data);
	save_hrm_eol_data(data);
	set_hrm_calibration(data);

	return sprintf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			data->buf[BIO_HRM_RAW_FAC].frequency,
			data->buf[BIO_HRM_RAW_FAC].green_dc_level,
			data->buf[BIO_HRM_RAW_FAC].green_high_dc_level,
			data->buf[BIO_HRM_RAW_FAC].green_mid_dc_level,
			data->buf[BIO_HRM_RAW_FAC].red_dc_levet,
			data->buf[BIO_HRM_RAW_FAC].ir_dc_level,
			data->buf[BIO_HRM_RAW_FAC].noise_level,
			data->buf[BIO_HRM_RAW_FAC].adc_offset[0],
			data->buf[BIO_HRM_RAW_FAC].adc_offset[1],
			data->buf[BIO_HRM_RAW_FAC].adc_offset[2],
			data->buf[BIO_HRM_RAW_FAC].adc_offset[3],
			data->buf[BIO_HRM_RAW_FAC].adc_offset[4],
			data->buf[BIO_HRM_RAW_FAC].adc_offset[5],
			data->buf[BIO_HRM_RAW_FAC].adc_offset[6],
			data->buf[BIO_HRM_RAW_FAC].adc_offset[7]);
}

static ssize_t hrm_eol_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int iRet;
	int64_t dEnable;
	struct ssp_data *data = dev_get_drvdata(dev);

	iRet = kstrtoll(buf, 10, &dEnable);
	if (iRet < 0)
		return iRet;

	if (dEnable)
		atomic_set(&data->eol_enable, 1);
	else
		atomic_set(&data->eol_enable, 0);

	return size;
}

static ssize_t hrm_raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		data->buf[BIO_HRM_RAW].ch_a_sum,
		data->buf[BIO_HRM_RAW].ch_a_x1,
		data->buf[BIO_HRM_RAW].ch_a_x2,
		data->buf[BIO_HRM_RAW].ch_a_y1,
		data->buf[BIO_HRM_RAW].ch_a_y2,
		data->buf[BIO_HRM_RAW].ch_b_sum,
		data->buf[BIO_HRM_RAW].ch_b_x1,
		data->buf[BIO_HRM_RAW].ch_b_x2,
		data->buf[BIO_HRM_RAW].ch_b_y1,
		data->buf[BIO_HRM_RAW].ch_b_y2);
}

static ssize_t hrm_lib_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->buf[BIO_HRM_LIB].hr,
		data->buf[BIO_HRM_LIB].rri,
		data->buf[BIO_HRM_LIB].snr);
}

static ssize_t hrm_osc_reg_val_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	hrm_open_oscregvalue(data);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		data->hrmcal.osc_reg);
}

static ssize_t hrm_eol_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;
	u32 eol_data[15];

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(EOL_DATA_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		set_fs(old_fs);
		iRet = PTR_ERR(cal_filp);

		memset(eol_data, 0, sizeof(eol_data));
		goto exit;
	}

	iRet = cal_filp->f_op->read(cal_filp, (char *)&eol_data,
		15 * sizeof(u32), &cal_filp->f_pos);
	if (iRet != 15 * sizeof(u32))
		iRet = -EIO;

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

exit:
	return snprintf(buf, PAGE_SIZE, "%d %d %d %d "\
		"%d %d %d %d %d %d %d %d %d %d %d\n",
		eol_data[0], eol_data[1], eol_data[2], eol_data[3],
		eol_data[4], eol_data[5], eol_data[6], eol_data[7],
		eol_data[8], eol_data[9], eol_data[10], eol_data[11],
		eol_data[12], eol_data[13], eol_data[14]);
}

static DEVICE_ATTR(name, S_IRUGO, hrm_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, hrm_vendor_show, NULL);
static DEVICE_ATTR(hrm_lib_ver, S_IRUGO, hrm_lib_version_show, NULL);
static DEVICE_ATTR(hrm_eol, S_IRUGO | S_IWUSR | S_IWGRP, hrm_eol_show, hrm_eol_store);
static DEVICE_ATTR(hrm_raw, S_IRUGO, hrm_raw_data_read, NULL);
static DEVICE_ATTR(hrm_lib, S_IRUGO, hrm_lib_data_read, NULL);
static DEVICE_ATTR(hrm_oscregval, S_IRUGO, hrm_osc_reg_val_show, NULL);
static DEVICE_ATTR(hrm_eol_data, S_IRUGO, hrm_eol_data_show, NULL);

static struct device_attribute *hrm_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_hrm_lib_ver,
	&dev_attr_hrm_eol,
	&dev_attr_hrm_raw,
	&dev_attr_hrm_lib,
	&dev_attr_hrm_oscregval,
	&dev_attr_hrm_eol_data,
	NULL,
};

void initialize_hrm_factorytest(struct ssp_data *data)
{
	sensors_register(data->hrm_device, data, hrm_attrs,
		"hrm_sensor");
}

void remove_hrm_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->hrm_device, hrm_attrs);
}
