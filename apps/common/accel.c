/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include "accel.h"

/* AtomS3のボード定義にある別名を使う */
static const struct device *const accel_dev = DEVICE_DT_GET(DT_ALIAS(accel0));
static uint32_t sequence;

/* センサの初期化関数 */
int accel_init(void)
{
	return device_is_ready(accel_dev) ? 0 : -ENODEV;
}

/* センサからの加速度取得関数 */
int accel_read(struct accel_sample *sample)
{
	struct sensor_value axes[3];
	int ret;

	if (sample == NULL)
	{
		return -EINVAL;
	}

	/* センサからの最新のサンプルを取得する */
	ret = sensor_sample_fetch(accel_dev);
	if (ret < 0)
	{
		return ret;
	}
	/* センサからの加速度データを取得する */
	ret = sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, axes);
	if (ret < 0)
	{
		return ret;
	}

	/* 取得成功時にはサンプルの連番を進める */
	sample->seq = sequence++;
	sample->uptime_ms = k_uptime_get();
	for (size_t i = 0; i < 3; i++)
	{
		sample->micro_ms2[i] = (int64_t)axes[i].val1 * 1000000 + axes[i].val2;
	}
	return 0;
}

/* センサから取得した加速度データをJSON形式の文字列に変換する関数 */
int accel_read_json(char *buf, size_t size)
{
	struct accel_sample sample;
	char axis[3][32];
	int ret;

	if (buf == NULL || size == 0)
	{
		return -EINVAL;
	}

	/* 取得に失敗した場合は前回の文字列を誤って送らないようにする */
	buf[0] = '\0';
	ret = accel_read(&sample);
	if (ret < 0)
	{
		return ret;
	}

	for (size_t i = 0; i < 3; i++)
	{
		int64_t value = sample.micro_ms2[i];
		/* INT64_MINもオーバーフローさせずに絶対値を求める */
		uint64_t magnitude = value < 0 ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;

		ret = snprintf(axis[i], sizeof(axis[i]), "%s%" PRIu64 ".%06" PRIu64,
									 value < 0 ? "-" : "", magnitude / 1000000, magnitude % 1000000);
		if (ret < 0 || (size_t)ret >= sizeof(axis[i]))
		{
			buf[0] = '\0';
			return -ENOSPC;
		}
	}

	/* 単位は[m/s^2] 文字列ではなくJSONの数値として出力する */
	ret = snprintf(buf, size,
								 "{\"seq\":%" PRIu32 ",\"uptime_ms\":%" PRId64
								 ",\"ax\":%s,\"ay\":%s,\"az\":%s}",
								 sample.seq, sample.uptime_ms, axis[0], axis[1], axis[2]);
	if (ret < 0 || (size_t)ret >= size)
	{
		buf[0] = '\0';
		return -ENOSPC;
	}
	return ret;
}
