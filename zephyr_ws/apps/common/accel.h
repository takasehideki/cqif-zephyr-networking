/* SPDX-License-Identifier: Apache-2.0 */
#ifndef CQIF_ACCEL_H_
#define CQIF_ACCEL_H_

#include <stddef.h>
#include <stdint.h>

#define ACCEL_JSON_SIZE 192

/* 加速度[m/s^2]は100万倍した整数で保持して浮動小数点のprintfを不要にする */
struct accel_sample
{
	uint32_t seq;
	int64_t uptime_ms;
	int64_t micro_ms2[3];
};

int accel_init(void);
int accel_read(struct accel_sample *sample);
int accel_read_json(char *buf, size_t size);

#endif /* CQIF_ACCEL_H_ */
