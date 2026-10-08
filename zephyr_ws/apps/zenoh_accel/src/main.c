/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zenoh-pico.h>

#include "accel.h"
#include "network.h"

/* Zenohの設定としてホストPCのIPアドレスとポートを指定する */
#define ROUTER_ENDPOINT "tcp/192.168.11.100:7447"
/* Zenoh通信のKey名を指定する */
#define ACCEL_KEY "cqif/device01/accel"

#define ACCEL_PERIOD_MS 100

LOG_MODULE_REGISTER(zenoh_accel, LOG_LEVEL_INF);

int main(void)
{
	int ret = accel_init();

	if (ret < 0)
	{
		LOG_ERR("Sensor initialization failed: %d", ret);
		return ret;
	}
	ret = network_connect();
	if (ret < 0)
	{
		LOG_ERR("Wi-Fi connection failed: %d", ret);
		return ret;
	}

	/* clientモードでPC上のルータへ接続する */
	z_owned_config_t config;

	ret = z_config_default(&config);
	if (ret < 0)
	{
		LOG_ERR("Config creation failed: %d", ret);
		return ret;
	}
	ret = zp_config_insert(z_loan_mut(config), Z_CONFIG_MODE_KEY, "client");
	if (ret == 0)
	{
		ret = zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, ROUTER_ENDPOINT);
	}
	if (ret < 0)
	{
		LOG_ERR("Config setup failed: %d", ret);
		z_drop(z_move(config));
		return ret;
	}

	/* Zenohのセッションを開く */
	z_owned_session_t session;
	ret = z_open(&session, z_move(config), NULL);
	if (ret < 0)
	{
		LOG_ERR("Session open failed: %d", ret);
		return ret;
	}
	LOG_INF("Session opened: %s", ROUTER_ENDPOINT);

	/* 加速度データを出版するキーを宣言する */
	z_view_keyexpr_t key;
	z_owned_publisher_t publisher;
	ret = z_view_keyexpr_from_str(&key, ACCEL_KEY);
	if (ret < 0)
	{
		goto close_session;
	}
	ret = z_declare_publisher(z_loan(session), &publisher, z_loan(key), NULL);
	if (ret < 0)
	{
		goto close_session;
	}
	LOG_INF("Publishing: %s", ACCEL_KEY);

	while (true)
	{
		char json[ACCEL_JSON_SIZE];

		/* 加速度データをJSON形式で取得する */
		ret = accel_read_json(json, sizeof(json));
		if (ret < 0)
		{
			break;
		}

		/* 1サンプルを1つのペイロードにコピーする */
		z_owned_bytes_t payload;
		ret = z_bytes_copy_from_str(&payload, json);
		if (ret < 0)
		{
			break;
		}

		/* ペイロードの所有権を渡して出版する */
		ret = z_publisher_put(z_loan(publisher), z_move(payload), NULL);
		if (ret < 0)
		{
			break;
		}
		k_msleep(ACCEL_PERIOD_MS);
	}
	z_drop(z_move(publisher));

close_session:
	LOG_ERR("Publication stopped: %d", ret);
	z_drop(z_move(session));
	return ret;
}
