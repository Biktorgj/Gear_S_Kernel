/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "ssp.h"

/*************************************************************************/
/* SSP Kernel -> HAL input evnet function                                */
/*************************************************************************/
void convert_acc_data(s16 *iValue)
{
	if (*iValue > MAX_ACCEL_2G)
		*iValue = ((MAX_ACCEL_4G - *iValue)) * (-1);
}

void report_acc_data(struct ssp_data *data, struct sensor_value *accdata)
{
	data->buf[ACCELEROMETER_SENSOR].x = accdata->x;
	data->buf[ACCELEROMETER_SENSOR].y = accdata->y;
	data->buf[ACCELEROMETER_SENSOR].z = accdata->z;

	input_report_rel(data->acc_input_dev, REL_X,
		data->buf[ACCELEROMETER_SENSOR].x);
	input_report_rel(data->acc_input_dev, REL_Y,
		data->buf[ACCELEROMETER_SENSOR].y);
	input_report_rel(data->acc_input_dev, REL_Z,
		data->buf[ACCELEROMETER_SENSOR].z);
	input_sync(data->acc_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, accel=[%d %d %d]\n", __func__,
		accdata->x, accdata->y, accdata->z);
#endif
}

void report_gyro_data(struct ssp_data *data, struct sensor_value *gyrodata)
{
	long lTemp[3] = {0,};
	data->buf[GYROSCOPE_SENSOR].x = gyrodata->x;
	data->buf[GYROSCOPE_SENSOR].y = gyrodata->y;
	data->buf[GYROSCOPE_SENSOR].z = gyrodata->z;

#if 1
	lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x;
	lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y;
	lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z;
#else
	if (data->uGyroDps == GYROSCOPE_DPS500) {
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z;
	} else if (data->uGyroDps == GYROSCOPE_DPS250)	{
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x >> 1;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y >> 1;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z >> 1;
	} else if (data->uGyroDps == GYROSCOPE_DPS2000)	{
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x << 2;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y << 2;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z << 2;
	} else {
		lTemp[0] = (long)data->buf[GYROSCOPE_SENSOR].x;
		lTemp[1] = (long)data->buf[GYROSCOPE_SENSOR].y;
		lTemp[2] = (long)data->buf[GYROSCOPE_SENSOR].z;
	}
#endif

	input_report_rel(data->gyro_input_dev, REL_RX, lTemp[0]);
	input_report_rel(data->gyro_input_dev, REL_RY, lTemp[1]);
	input_report_rel(data->gyro_input_dev, REL_RZ, lTemp[2]);
	input_sync(data->gyro_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, gyro=[%d %d %d]\n", __func__,
		gyrodata->x, gyrodata->y, gyrodata->z);
#endif
}


void report_geomagnetic_raw_data(struct ssp_data *data,
	struct sensor_value *magrawdata)
{
	data->buf[GEOMAGNETIC_RAW].x = magrawdata->x;
	data->buf[GEOMAGNETIC_RAW].y = magrawdata->y;
	data->buf[GEOMAGNETIC_RAW].z = magrawdata->z;
#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, mag raw=[%d %d %d]\n", __func__,
		magrawdata->x, magrawdata->y, magrawdata->z);
#endif
}

void report_mag_data(struct ssp_data *data, struct sensor_value *magdata)
{
#ifdef SAVE_MAG_LOG
	s16 arrTemp[9] = {0, };

	arrTemp[0] = magdata->log_data[0];	// ST1 Reg
	arrTemp[1] = (short)((magdata->log_data[3] << 8) +\
				magdata->log_data[2]);
	arrTemp[2] = (short)((magdata->log_data[5] << 8) +\
				magdata->log_data[4]);
	arrTemp[3] = (short)((magdata->log_data[7] << 8) +\
				magdata->log_data[6]);
	arrTemp[4] = magdata->log_data[1];	// ST2 Reg
	arrTemp[5] = (short)((magdata->log_data[9] << 8) +\
				magdata->log_data[8]);
	arrTemp[6] = (short)((magdata->log_data[11] << 8) +\
				magdata->log_data[10]);
	arrTemp[7] = (short)((magdata->log_data[13] << 8) +\
				magdata->log_data[12]);

	/* We report data & register to HAL only when ST1 register sets to 1 */
	if (arrTemp[0] == 1) {
		input_report_rel(data->mag_input_dev, REL_X, arrTemp[0] + 1);
		input_report_rel(data->mag_input_dev, REL_Y, arrTemp[1]);
		input_report_rel(data->mag_input_dev, REL_Z, arrTemp[2]);
		input_report_rel(data->mag_input_dev, REL_RX, arrTemp[3]);
		input_report_rel(data->mag_input_dev, REL_RY, arrTemp[4] + 1);
		input_report_rel(data->mag_input_dev, REL_RZ, arrTemp[5]);
		input_report_rel(data->mag_input_dev, REL_HWHEEL, arrTemp[6]);
		input_report_rel(data->mag_input_dev, REL_DIAL, arrTemp[7]);
		mdelay(5);
		input_sync(data->mag_input_dev);
	} else {
		pr_info("[SSP] %s, not initialised, val = %d", __func__, arrTemp[0]);
	}
#else
	data->buf[GEOMAGNETIC_SENSOR].cal_x = magdata->cal_x;
	data->buf[GEOMAGNETIC_SENSOR].cal_y = magdata->cal_y;
	data->buf[GEOMAGNETIC_SENSOR].cal_z = magdata->cal_z;
	data->buf[GEOMAGNETIC_SENSOR].accuracy = magdata->accuracy;

	input_report_rel(data->mag_input_dev, REL_RX,
		data->buf[GEOMAGNETIC_SENSOR].cal_x);
	input_report_rel(data->mag_input_dev, REL_RY,
		data->buf[GEOMAGNETIC_SENSOR].cal_y);
	input_report_rel(data->mag_input_dev, REL_RZ,
		data->buf[GEOMAGNETIC_SENSOR].cal_z);
	input_report_rel(data->mag_input_dev, REL_HWHEEL,
		data->buf[GEOMAGNETIC_SENSOR].accuracy + 1);
	input_sync(data->mag_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, mag=[%d %d %d], accuray=%d]\n", __func__,
		magdata->cal_x, magdata->cal_y, magdata->cal_z,
		magdata->accuracy);
#endif
#endif
}

void report_mag_uncaldata(struct ssp_data *data, struct sensor_value *magdata)
{
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_x = magdata->uncal_x;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_y = magdata->uncal_y;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_z = magdata->uncal_z;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_x= magdata->offset_x;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_y= magdata->offset_y;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_z= magdata->offset_z;

	input_report_rel(data->uncal_mag_input_dev, REL_RX,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_x);
	input_report_rel(data->uncal_mag_input_dev, REL_RY,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_y);
	input_report_rel(data->uncal_mag_input_dev, REL_RZ,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_z);
	input_report_rel(data->uncal_mag_input_dev, REL_HWHEEL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_x);
	input_report_rel(data->uncal_mag_input_dev, REL_DIAL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_y);
	input_report_rel(data->uncal_mag_input_dev, REL_WHEEL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_z);
	input_sync(data->uncal_mag_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, uncal mag=[%d %d %d], offset=[%d %d %d]\n",
		__func__, magdata->uncal_x, magdata->uncal_y,
		magdata->uncal_z, magdata->offset_x, magdata->offset_y,
		magdata->offset_z);
#endif
}

void report_uncalib_gyro_data(struct ssp_data *data, struct sensor_value *gyrodata)
{
	data->buf[GYRO_UNCALIB_SENSOR].uncal_x = gyrodata->uncal_x;
	data->buf[GYRO_UNCALIB_SENSOR].uncal_y = gyrodata->uncal_y;
	data->buf[GYRO_UNCALIB_SENSOR].uncal_z = gyrodata->uncal_z;
	data->buf[GYRO_UNCALIB_SENSOR].offset_x = gyrodata->offset_x;
	data->buf[GYRO_UNCALIB_SENSOR].offset_y = gyrodata->offset_y;
	data->buf[GYRO_UNCALIB_SENSOR].offset_z = gyrodata->offset_z;

	input_report_rel(data->uncalib_gyro_input_dev, REL_RX,
		data->buf[GYRO_UNCALIB_SENSOR].uncal_x);
	input_report_rel(data->uncalib_gyro_input_dev, REL_RY,
		data->buf[GYRO_UNCALIB_SENSOR].uncal_y);
	input_report_rel(data->uncalib_gyro_input_dev, REL_RZ,
		data->buf[GYRO_UNCALIB_SENSOR].uncal_z);
	input_report_rel(data->uncalib_gyro_input_dev, REL_HWHEEL,
		data->buf[GYRO_UNCALIB_SENSOR].offset_x);
	input_report_rel(data->uncalib_gyro_input_dev, REL_DIAL,
		data->buf[GYRO_UNCALIB_SENSOR].offset_y);
	input_report_rel(data->uncalib_gyro_input_dev, REL_WHEEL,
		data->buf[GYRO_UNCALIB_SENSOR].offset_z);
	input_sync(data->uncalib_gyro_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, uncal gyro=[%d %d %d], offset=[%d %d %d]\n",
		__func__, gyrodata->uncal_x, gyrodata->uncal_y,
		gyrodata->uncal_z, gyrodata->offset_x, gyrodata->offset_y,
		gyrodata->offset_z);
#endif
}

#ifdef CONFIG_SENSORS_SSP_LPS25H
void report_pressure_data(struct ssp_data *data, struct sensor_value *predata)
{
	int temp[3] = {0, };
	data->buf[PRESSURE_SENSOR].pressure[0] =
		predata->pressure[0] - data->iPressureCal;
	data->buf[PRESSURE_SENSOR].pressure[1] = predata->pressure[1];

	temp[0] = data->buf[PRESSURE_SENSOR].pressure[0];
	temp[1] = data->buf[PRESSURE_SENSOR].pressure[1];
	temp[2] = data->sealevelpressure;

	/* pressure */
	input_report_rel(data->pressure_input_dev, REL_HWHEEL, temp[0]);
	/* sealevel */
	input_report_rel(data->pressure_input_dev, REL_DIAL, temp[2]);
	/* temperature */
	input_report_rel(data->pressure_input_dev, REL_WHEEL, temp[1]);
	input_sync(data->pressure_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, pressure=%d, temp=%d, sealevel=%d\n", __func__,
		temp[0], temp[1], temp[2]);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_AL3320
void report_light_data(struct ssp_data *data, struct sensor_value *lightdata)
{
	u16 raw_lux = ((lightdata->raw_high << 8) | lightdata->raw_low);

	data->buf[LIGHT_SENSOR].raw_high = lightdata->raw_high;
	data->buf[LIGHT_SENSOR].raw_low = lightdata->raw_low;

	data->lux = light_get_lux(raw_lux);
	input_report_rel(data->light_input_dev, REL_RX, data->lux + 1);
	input_sync(data->light_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, lux=%d\n", __func__, data->lux);
#endif
}
#endif

void report_rot_data(struct ssp_data *data, struct sensor_value *rotdata)
{
	int rot_buf[5];
	data->buf[ROTATION_VECTOR].quat_a = rotdata->quat_a;
	data->buf[ROTATION_VECTOR].quat_b = rotdata->quat_b;
	data->buf[ROTATION_VECTOR].quat_c = rotdata->quat_c;
	data->buf[ROTATION_VECTOR].quat_d = rotdata->quat_d;
	data->buf[ROTATION_VECTOR].acc_rot = rotdata->acc_rot;

	rot_buf[0] = rotdata->quat_a;
	rot_buf[1] = rotdata->quat_b;
	rot_buf[2] = rotdata->quat_c;
	rot_buf[3] = rotdata->quat_d;
	rot_buf[4] = rotdata->acc_rot;

	input_report_rel(data->rot_input_dev, REL_X, rot_buf[0]);
	input_report_rel(data->rot_input_dev, REL_Y, rot_buf[1]);
	input_report_rel(data->rot_input_dev, REL_Z, rot_buf[2]);
	input_report_rel(data->rot_input_dev, REL_RX, rot_buf[3]);
	input_report_rel(data->rot_input_dev, REL_RY, rot_buf[4] + 1);
	input_sync(data->rot_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, quat=[%d %d %d %d], acc_rot=%d\n", __func__,
		rotdata->quat_a, rotdata->quat_b, rotdata->quat_c,
		rotdata->quat_d, rotdata->acc_rot);
#endif
}

#ifdef SSP_SUPPORT_GAME_RV
void report_game_rot_data(struct ssp_data *data, struct sensor_value *grotdata)
{
	int rot_buf[5];
	data->buf[GAME_ROTATION_VECTOR].quat_a = grotdata->quat_a;
	data->buf[GAME_ROTATION_VECTOR].quat_b = grotdata->quat_b;
	data->buf[GAME_ROTATION_VECTOR].quat_c = grotdata->quat_c;
	data->buf[GAME_ROTATION_VECTOR].quat_d = grotdata->quat_d;
	data->buf[GAME_ROTATION_VECTOR].acc_rot = grotdata->acc_rot;

	rot_buf[0] = grotdata->quat_a;
	rot_buf[1] = grotdata->quat_b;
	rot_buf[2] = grotdata->quat_c;
	rot_buf[3] = grotdata->quat_d;
	rot_buf[4] = grotdata->acc_rot;

	input_report_rel(data->game_rot_input_dev, REL_X, rot_buf[0]);
	input_report_rel(data->game_rot_input_dev, REL_Y, rot_buf[1]);
	input_report_rel(data->game_rot_input_dev, REL_Z, rot_buf[2]);
	input_report_rel(data->game_rot_input_dev, REL_RX, rot_buf[3]);
	input_report_rel(data->game_rot_input_dev, REL_RY, rot_buf[4] + 1);
	input_sync(data->game_rot_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, quat=[%d %d %d %d], acc_rot=%d\n", __func__,
		grotdata->quat_a, grotdata->quat_b, grotdata->quat_c,
		grotdata->quat_d, grotdata->acc_rot);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_ADPD142
void report_hrm_raw_data(struct ssp_data *data, struct sensor_value *hrmdata)
{
	data->buf[BIO_HRM_RAW].ch_a_sum = hrmdata->ch_a_sum;
	data->buf[BIO_HRM_RAW].ch_a_x1 = hrmdata->ch_a_x1;
	data->buf[BIO_HRM_RAW].ch_a_x2 = hrmdata->ch_a_x2;
	data->buf[BIO_HRM_RAW].ch_a_y1 = hrmdata->ch_a_y1;
	data->buf[BIO_HRM_RAW].ch_a_y2 = hrmdata->ch_a_y2;
	data->buf[BIO_HRM_RAW].ch_b_sum = hrmdata->ch_b_sum;
	data->buf[BIO_HRM_RAW].ch_b_x1 = hrmdata->ch_b_x1;
	data->buf[BIO_HRM_RAW].ch_b_x2 = hrmdata->ch_b_x2;
	data->buf[BIO_HRM_RAW].ch_b_y1 = hrmdata->ch_b_y1;
	data->buf[BIO_HRM_RAW].ch_b_y2 = hrmdata->ch_b_y2;

	input_report_rel(data->hrm_raw_input_dev, REL_X,
		data->buf[BIO_HRM_RAW].ch_a_sum + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_Y,
		data->buf[BIO_HRM_RAW].ch_a_x1 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_Z,
		data->buf[BIO_HRM_RAW].ch_a_x2 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_RX,
		data->buf[BIO_HRM_RAW].ch_a_y1 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_RY,
		data->buf[BIO_HRM_RAW].ch_a_y2 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_RZ,
		data->buf[BIO_HRM_RAW].ch_b_sum + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_HWHEEL,
		data->buf[BIO_HRM_RAW].ch_b_x1 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_DIAL,
		data->buf[BIO_HRM_RAW].ch_b_x2 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_WHEEL,
		data->buf[BIO_HRM_RAW].ch_b_y1 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_MISC,
		data->buf[BIO_HRM_RAW].ch_b_y2 + 1);
	input_sync(data->hrm_raw_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, [%d %d %d %d %d] [%d %d %d %d %d]\n", __func__,
		hrmdata->ch_a_sum, hrmdata->ch_a_x1,
		hrmdata->ch_a_x2, hrmdata->ch_a_y1,
		hrmdata->ch_a_y2, hrmdata->ch_b_sum,
		hrmdata->ch_b_x1, hrmdata->ch_b_x2,
		hrmdata->ch_b_y1, hrmdata->ch_b_y2);
#endif
}

void report_hrm_raw_fac_data(struct ssp_data *data, struct sensor_value *hrmdata)
{
	data->buf[BIO_HRM_RAW_FAC].frequency = hrmdata->frequency;
	data->buf[BIO_HRM_RAW_FAC].green_dc_level =
		hrmdata->green_dc_level;
	data->buf[BIO_HRM_RAW_FAC].green_high_dc_level =
		hrmdata->green_high_dc_level;
	data->buf[BIO_HRM_RAW_FAC].green_mid_dc_level =
		hrmdata->green_mid_dc_level;
	data->buf[BIO_HRM_RAW_FAC].red_dc_levet = hrmdata->red_dc_levet;
	data->buf[BIO_HRM_RAW_FAC].ir_dc_level = hrmdata->ir_dc_level;
	data->buf[BIO_HRM_RAW_FAC].noise_level = hrmdata->noise_level;
	data->buf[BIO_HRM_RAW_FAC].oscRegValue = hrmdata->oscRegValue;
	memcpy(data->buf[BIO_HRM_RAW_FAC].adc_offset,
		hrmdata->adc_offset, sizeof(hrmdata->adc_offset));

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, [%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		__func__,
		hrmdata->frequency,
		hrmdata->green_dc_level,
		hrmdata->green_high_dc_level,
		hrmdata->green_mid_dc_level,
		hrmdata->red_dc_levet,
		hrmdata->ir_dc_level,
		hrmdata->noise_level,
		hrmdata->adc_offset[0],
		hrmdata->adc_offset[1],
		hrmdata->adc_offset[2],
		hrmdata->adc_offset[3],
		hrmdata->adc_offset[4],
		hrmdata->adc_offset[5],
		hrmdata->adc_offset[6],
		hrmdata->adc_offset[7]);
#endif
}

void report_hrm_lib_data(struct ssp_data *data, struct sensor_value *hrmdata)
{
	data->buf[BIO_HRM_LIB].hr = hrmdata->hr;
	data->buf[BIO_HRM_LIB].rri = hrmdata->rri;
	data->buf[BIO_HRM_LIB].snr = hrmdata->snr;

	input_report_rel(data->hrm_lib_input_dev, REL_X, data->buf[BIO_HRM_LIB].hr + 1);
	input_report_rel(data->hrm_lib_input_dev, REL_Y, data->buf[BIO_HRM_LIB].rri + 1);
	input_report_rel(data->hrm_lib_input_dev, REL_Z, data->buf[BIO_HRM_LIB].snr + 1);
	input_sync(data->hrm_lib_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, hr=%d rri=%d snr=%d\n", __func__,
		hrmdata->hr, hrmdata->rri, hrmdata->snr);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_UVIS25
void report_uv_data(struct ssp_data *data, struct sensor_value *uvdata)
{
	data->buf[UV_SENSOR].uv_raw = uvdata->uv_raw;

	input_report_rel(data->uv_input_dev, REL_MISC,
		data->buf[UV_SENSOR].uv_raw + 1);
	input_sync(data->uv_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, uv raw=%d\n", __func__, uvdata->uv_raw);
#endif
}
#endif

int initialize_event_symlink(struct ssp_data *data)
{
	int iRet = 0;

	iRet = sensors_create_symlink(data->acc_input_dev);
	if (iRet < 0)
		goto iRet_acc_sysfs_create_link;

	iRet = sensors_create_symlink(data->gyro_input_dev);
	if (iRet < 0)
		goto iRet_gyro_sysfs_create_link;

	iRet = sensors_create_symlink(data->mag_input_dev);
	if (iRet < 0)
		goto iRet_mag_sysfs_create_link;

	iRet = sensors_create_symlink(data->uncal_mag_input_dev);
	if (iRet < 0)
		goto iRet_uncal_mag_sysfs_create_link;

	iRet = sensors_create_symlink(data->uncalib_gyro_input_dev);
	if (iRet < 0)
		goto iRet_uncal_gyro_sysfs_create_link;

#ifdef CONFIG_SENSORS_SSP_LPS25H
	iRet = sensors_create_symlink(data->pressure_input_dev);
	if (iRet < 0)
		goto iRet_pressure_sysfs_create_link;
#endif

#ifdef CONFIG_SENSORS_SSP_AL3320
		iRet = sensors_create_symlink(data->light_input_dev);
		if (iRet < 0)
			goto iRet_light_sysfs_create_link;
#endif

	iRet = sensors_create_symlink(data->rot_input_dev);
	if (iRet < 0)
		goto iRet_rot_sysfs_create_link;
#ifdef SSP_SUPPORT_GAME_RV
	iRet = sensors_create_symlink(data->game_rot_input_dev);
	if (iRet < 0)
		goto iRet_game_rot_sysfs_create_link;
#endif
#ifdef CONFIG_SENSORS_SSP_ADPD142
	iRet = sensors_create_symlink(data->hrm_raw_input_dev);
	if (iRet < 0)
		goto iRet_hrm_raw_sysfs_create_link;

	iRet = sensors_create_symlink(data->hrm_lib_input_dev);
	if (iRet < 0)
		goto iRet_hrm_lib_sysfs_create_link;
#endif

#ifdef CONFIG_SENSORS_SSP_UVIS25
	iRet = sensors_create_symlink(data->uv_input_dev);
	if (iRet < 0)
		goto iRet_uv_sysfs_create_link;
#endif

	return SUCCESS;

#ifdef CONFIG_SENSORS_SSP_UVIS25
iRet_uv_sysfs_create_link:
#endif
#ifdef CONFIG_SENSORS_SSP_ADPD142
	sensors_remove_symlink(data->hrm_lib_input_dev);
iRet_hrm_lib_sysfs_create_link:
	sensors_remove_symlink(data->hrm_raw_input_dev);
iRet_hrm_raw_sysfs_create_link:
#endif
#ifdef SSP_SUPPORT_GAME_RV
	sensors_remove_symlink(data->game_rot_input_dev);
iRet_game_rot_sysfs_create_link:
#endif
	sensors_remove_symlink(data->rot_input_dev);
iRet_rot_sysfs_create_link:
#ifdef CONFIG_SENSORS_SSP_AL3320
	sensors_remove_symlink(data->light_input_dev);
iRet_light_sysfs_create_link:
#endif
#ifdef CONFIG_SENSORS_SSP_LPS25H
	sensors_remove_symlink(data->pressure_input_dev);
iRet_pressure_sysfs_create_link:
#endif
	sensors_remove_symlink(data->uncalib_gyro_input_dev);
iRet_uncal_gyro_sysfs_create_link:
	sensors_remove_symlink(data->uncal_mag_input_dev);
iRet_uncal_mag_sysfs_create_link:
	sensors_remove_symlink(data->mag_input_dev);
iRet_mag_sysfs_create_link:
	sensors_remove_symlink(data->gyro_input_dev);
iRet_gyro_sysfs_create_link:
	sensors_remove_symlink(data->acc_input_dev);
iRet_acc_sysfs_create_link:
	pr_err("[SSP]: %s - could not create event symlink\n", __func__);
	return FAIL;
}

void remove_event_symlink(struct ssp_data *data)
{
	sensors_remove_symlink(data->acc_input_dev);
	sensors_remove_symlink(data->gyro_input_dev);
	sensors_remove_symlink(data->mag_input_dev);
	sensors_remove_symlink(data->uncal_mag_input_dev);
	sensors_remove_symlink(data->uncalib_gyro_input_dev);
#ifdef CONFIG_SENSORS_SSP_LPS25H
	sensors_remove_symlink(data->pressure_input_dev);
#endif
#ifdef CONFIG_SENSORS_SSP_AL3320
	sensors_remove_symlink(data->light_input_dev);
#endif
	sensors_remove_symlink(data->rot_input_dev);
#ifdef SSP_SUPPORT_GAME_RV
	sensors_remove_symlink(data->game_rot_input_dev);
#endif
#ifdef CONFIG_SENSORS_SSP_ADPD142
	sensors_remove_symlink(data->hrm_raw_input_dev);
	sensors_remove_symlink(data->hrm_lib_input_dev);
#endif
#ifdef CONFIG_SENSORS_SSP_UVIS25
	sensors_remove_symlink(data->uv_input_dev);
#endif
}

int initialize_input_dev(struct ssp_data *data)
{
	int iRet = 0;

	data->acc_input_dev = input_allocate_device();
	if (data->acc_input_dev == NULL)
		goto err_initialize_acc_input_dev;

	data->acc_input_dev->name = "accelerometer_sensor";
	input_set_capability(data->acc_input_dev, EV_REL, REL_X);
	input_set_capability(data->acc_input_dev, EV_REL, REL_Y);
	input_set_capability(data->acc_input_dev, EV_REL, REL_Z);

	iRet = input_register_device(data->acc_input_dev);
	if (iRet < 0) {
		input_free_device(data->acc_input_dev);
		goto err_initialize_acc_input_dev;
	}
	input_set_drvdata(data->acc_input_dev, data);

	data->gyro_input_dev = input_allocate_device();
	if (data->gyro_input_dev == NULL)
		goto err_initialize_gyro_input_dev;

	data->gyro_input_dev->name = "gyro_sensor";
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RX);
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RY);
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RZ);

	iRet = input_register_device(data->gyro_input_dev);
	if (iRet < 0) {
		input_free_device(data->gyro_input_dev);
		goto err_initialize_gyro_input_dev;
	}
	input_set_drvdata(data->gyro_input_dev, data);

	data->mag_input_dev = input_allocate_device();
	if (data->mag_input_dev == NULL)
		goto err_initialize_mag_input_dev;

	data->mag_input_dev->name = "geomagnetic_sensor";
#ifdef SAVE_MAG_LOG
	input_set_capability(data->mag_input_dev, EV_REL, REL_X);
	input_set_capability(data->mag_input_dev, EV_REL, REL_Y);
	input_set_capability(data->mag_input_dev, EV_REL, REL_Z);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RX);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RY);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->mag_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->mag_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->mag_input_dev, EV_REL, REL_WHEEL);
#else
	input_set_capability(data->mag_input_dev, EV_REL, REL_RX);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RY);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->mag_input_dev, EV_REL, REL_HWHEEL);
#endif

	iRet = input_register_device(data->mag_input_dev);
	if (iRet < 0) {
		input_free_device(data->mag_input_dev);
		goto err_initialize_mag_input_dev;
	}
	input_set_drvdata(data->mag_input_dev, data);


	data->uncal_mag_input_dev = input_allocate_device();
	if (data->uncal_mag_input_dev == NULL)
		goto err_initialize_uncal_mag_input_dev;

	data->uncal_mag_input_dev->name = "uncal_geomagnetic_sensor";
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RX);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RY);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_WHEEL);

	iRet = input_register_device(data->uncal_mag_input_dev);
	if (iRet < 0) {
		input_free_device(data->uncal_mag_input_dev);
		goto err_initialize_uncal_mag_input_dev;
	}
	input_set_drvdata(data->uncal_mag_input_dev, data);

	data->uncalib_gyro_input_dev = input_allocate_device();
	if (data->uncalib_gyro_input_dev == NULL)
		goto err_initialize_uncal_gyro_input_dev;

	data->uncalib_gyro_input_dev->name = "uncal_gyro_sensor";
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_RX);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_RY);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_WHEEL);
	iRet = input_register_device(data->uncalib_gyro_input_dev);
	if (iRet < 0) {
		input_free_device(data->uncalib_gyro_input_dev);
		goto err_initialize_uncal_gyro_input_dev;
	}
	input_set_drvdata(data->uncalib_gyro_input_dev, data);

#ifdef CONFIG_SENSORS_SSP_LPS25H
	data->pressure_input_dev = input_allocate_device();
	if (data->pressure_input_dev == NULL)
		goto err_initialize_pressure_input_dev;

	data->pressure_input_dev->name = "pressure_sensor";
	input_set_capability(data->pressure_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->pressure_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->pressure_input_dev, EV_REL, REL_WHEEL);

	iRet = input_register_device(data->pressure_input_dev);
	if (iRet < 0) {
		input_free_device(data->pressure_input_dev);
		goto err_initialize_pressure_input_dev;
	}
	input_set_drvdata(data->pressure_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_AL3320
	data->light_input_dev = input_allocate_device();
	if (data->light_input_dev == NULL)
		goto err_initialize_light_input_dev;

	data->light_input_dev->name = "light_sensor";
	input_set_capability(data->light_input_dev, EV_REL, REL_RX);

	iRet = input_register_device(data->light_input_dev);
	if (iRet < 0) {
		input_free_device(data->light_input_dev);
		goto err_initialize_light_input_dev;
	}
	input_set_drvdata(data->light_input_dev, data);
#endif

	data->rot_input_dev = input_allocate_device();
	if (data->rot_input_dev == NULL)
		goto err_initialize_rot_input_dev;

	data->rot_input_dev->name = "rot_sensor";
	input_set_capability(data->rot_input_dev, EV_REL, REL_X);
	input_set_capability(data->rot_input_dev, EV_REL, REL_Y);
	input_set_capability(data->rot_input_dev, EV_REL, REL_Z);
	input_set_capability(data->rot_input_dev, EV_REL, REL_RX);
	input_set_capability(data->rot_input_dev, EV_REL, REL_RY);

	iRet = input_register_device(data->rot_input_dev);
	if (iRet < 0) {
		input_free_device(data->rot_input_dev);
		goto err_initialize_rot_input_dev;
	}
	input_set_drvdata(data->rot_input_dev, data);

#ifdef SSP_SUPPORT_GAME_RV
	data->game_rot_input_dev = input_allocate_device();
	if (data->game_rot_input_dev == NULL)
		goto err_initialize_game_rot_input_dev;

	data->game_rot_input_dev->name = "game_rot_sensor";
	input_set_capability(data->game_rot_input_dev, EV_REL, REL_X);
	input_set_capability(data->game_rot_input_dev, EV_REL, REL_Y);
	input_set_capability(data->game_rot_input_dev, EV_REL, REL_Z);
	input_set_capability(data->game_rot_input_dev, EV_REL, REL_RX);
	input_set_capability(data->game_rot_input_dev, EV_REL, REL_RY);

	iRet = input_register_device(data->game_rot_input_dev);
	if (iRet < 0) {
		input_free_device(data->game_rot_input_dev);
		goto err_initialize_game_rot_input_dev;
	}
	input_set_drvdata(data->game_rot_input_dev, data);
#endif
	data->key_input_dev = input_allocate_device();
	if (data->key_input_dev == NULL)
		goto err_initialize_key_input_dev;

	data->key_input_dev->name = "LPM_MOTION";
	input_set_capability(data->key_input_dev, EV_KEY, KEY_HOMEPAGE);

	iRet = input_register_device(data->key_input_dev);
	if (iRet < 0) {
		input_free_device(data->key_input_dev);
		goto err_initialize_key_input_dev;
	}
	input_set_drvdata(data->key_input_dev, data);

#ifdef CONFIG_SENSORS_SSP_ADPD142
	data->hrm_raw_input_dev = input_allocate_device();
	if (data->hrm_raw_input_dev == NULL)
		goto err_initialize_hrm_raw_input_dev;

	data->hrm_raw_input_dev->name = "hrm_raw_sensor";
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_Z);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_RX);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_RY);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_WHEEL);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_MISC);

	iRet = input_register_device(data->hrm_raw_input_dev);
	if (iRet < 0) {
		input_free_device(data->hrm_raw_input_dev);
		goto err_initialize_hrm_raw_input_dev;
	}
	input_set_drvdata(data->hrm_raw_input_dev, data);

	data->hrm_lib_input_dev = input_allocate_device();
	if (data->hrm_lib_input_dev == NULL)
		goto err_initialize_hrm_lib_input_dev;

	data->hrm_lib_input_dev->name = "hrm_lib_sensor";
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_Z);

	iRet = input_register_device(data->hrm_lib_input_dev);
	if (iRet < 0) {
		input_free_device(data->hrm_lib_input_dev);
		goto err_initialize_hrm_lib_input_dev;
	}
	input_set_drvdata(data->hrm_lib_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_UVIS25
	data->uv_input_dev = input_allocate_device();
	if (data->uv_input_dev == NULL)
		goto err_initialize_uv_input_dev;

	data->uv_input_dev->name = "uv_sensor";
	input_set_capability(data->uv_input_dev, EV_REL, REL_MISC);

	iRet = input_register_device(data->uv_input_dev);
	if (iRet < 0) {
		input_free_device(data->uv_input_dev);
		goto err_initialize_uv_input_dev;
	}
	input_set_drvdata(data->uv_input_dev, data);
#endif

	return SUCCESS;

#ifdef CONFIG_SENSORS_SSP_UVIS25
err_initialize_uv_input_dev:
	pr_err("[SSP]: %s - could not allocate uv input device\n", __func__);
#endif
#ifdef CONFIG_SENSORS_SSP_ADPD142
	input_unregister_device(data->hrm_lib_input_dev);
err_initialize_hrm_lib_input_dev:
	pr_err("[SSP]: %s - could not allocate hrm lib input device\n", __func__);
	input_unregister_device(data->hrm_raw_input_dev);
err_initialize_hrm_raw_input_dev:
	pr_err("[SSP]: %s - could not allocate hrm raw input device\n", __func__);
	input_unregister_device(data->key_input_dev);
#endif
err_initialize_key_input_dev:
	pr_err("[SSP]: %s - could not allocate key input device\n", __func__);
#ifdef SSP_SUPPORT_GAME_RV
	input_unregister_device(data->game_rot_input_dev);
err_initialize_game_rot_input_dev:
	pr_err("[SSP]: %s - could not allocate game_rot input device\n", __func__);
#endif
	input_unregister_device(data->rot_input_dev);
err_initialize_rot_input_dev:
	pr_err("[SSP]: %s - could not allocate rot input device\n", __func__);
#ifdef CONFIG_SENSORS_SSP_AL3320
	input_unregister_device(data->light_input_dev);
err_initialize_light_input_dev:
	pr_err("[SSP]: %s - could not allocate light input device\n", __func__);
#endif
#ifdef CONFIG_SENSORS_SSP_LPS25H
	input_unregister_device(data->pressure_input_dev);
err_initialize_pressure_input_dev:
	pr_err("[SSP]: %s - could not allocate pressure input device\n",
		__func__);
#endif
	input_unregister_device(data->uncalib_gyro_input_dev);
err_initialize_uncal_gyro_input_dev:
	pr_err("[SSP]: %s - could not allocate uncal gyro input device\n", __func__);
	input_unregister_device(data->uncal_mag_input_dev);
err_initialize_uncal_mag_input_dev:
	pr_err("[SSP]: %s - could not allocate uncal mag input device\n", __func__);
	input_unregister_device(data->mag_input_dev);
err_initialize_mag_input_dev:
	pr_err("[SSP]: %s - could not allocate mag input device\n", __func__);
	input_unregister_device(data->gyro_input_dev);
err_initialize_gyro_input_dev:
	pr_err("[SSP]: %s - could not allocate gyro input device\n", __func__);
	input_unregister_device(data->acc_input_dev);
err_initialize_acc_input_dev:
	pr_err("[SSP]: %s - could not allocate acc input device\n", __func__);
	return ERROR;
}

void remove_input_dev(struct ssp_data *data)
{
	input_unregister_device(data->acc_input_dev);
	input_unregister_device(data->gyro_input_dev);
	input_unregister_device(data->mag_input_dev);
	input_unregister_device(data->uncal_mag_input_dev);
	input_unregister_device(data->uncalib_gyro_input_dev);
#ifdef CONFIG_SENSORS_SSP_LPS25H
	input_unregister_device(data->pressure_input_dev);
#endif
#ifdef CONFIG_SENSORS_SSP_AL3320
	input_unregister_device(data->light_input_dev);
#endif
#ifdef SSP_SUPPORT_GAME_RV
	input_unregister_device(data->game_rot_input_dev);
#endif
	input_unregister_device(data->rot_input_dev);
	input_unregister_device(data->key_input_dev);
#ifdef CONFIG_SENSORS_SSP_ADPD142
	input_unregister_device(data->hrm_raw_input_dev);
	input_unregister_device(data->hrm_lib_input_dev);
#endif
#ifdef CONFIG_SENSORS_SSP_UVIS25
	input_unregister_device(data->uv_input_dev);
#endif
}
