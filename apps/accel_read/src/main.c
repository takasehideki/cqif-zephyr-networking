/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "accel.h"

#define ACCEL_PERIOD_MS 100

LOG_MODULE_REGISTER(accel_read, LOG_LEVEL_INF);
K_TIMER_DEFINE(sample_timer, NULL, NULL);

/* メイン関数 */
int main(void)
{
	/* 加速度センサの初期化 */
	int ret = accel_init();

	if (ret < 0)
	{
		LOG_ERR("MPU6886 is not ready: %d", ret);
		return ret;
	}
	k_timer_start(&sample_timer, K_NO_WAIT, K_MSEC(ACCEL_PERIOD_MS));
	while (true)
	{
		char json[ACCEL_JSON_SIZE];

		(void)k_timer_status_sync(&sample_timer);

		/* 加速度データをJSON形式で取得 */
		ret = accel_read_json(json, sizeof(json));
		if (ret < 0)
		{
			LOG_ERR("Read/encode failed: %d", ret);
			continue;
		}
		printk("%s\n", json);
	}
	return 0;
}
