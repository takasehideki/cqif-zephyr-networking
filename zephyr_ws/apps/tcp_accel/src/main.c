/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include "accel.h"
#include "network.h"

#define SERVER_PORT 4242
#define ACCEL_PERIOD_MS 100

LOG_MODULE_REGISTER(tcp_accel, LOG_LEVEL_INF);

/* 指定したバイト数のデータをすべて送信する関数 */
static int send_all(int sock, const char *buf, size_t len)
{
	size_t sent = 0;

	/* 途中まで送信できた場合は残りのデータを続けて送る */
	while (sent < len)
	{
		ssize_t ret = zsock_send(sock, buf + sent, len - sent, 0);

		if (ret < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return -errno;
		}
		if (ret == 0)
		{
			return -EIO;
		}
		sent += ret;
	}
	return 0;
}

/* 接続したPCへ加速度データを継続して送信する関数 */
static int stream_accel(int client)
{
	struct timeval timeout = {.tv_sec = 2};

	/* 送信の待ち時間を2秒に設定する */
	if (zsock_setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
	{
		return -errno;
	}
	while (true)
	{
		char json[ACCEL_JSON_SIZE];

		/* 加速度データをJSON形式で取得 */
		int ret = accel_read_json(json, sizeof(json));

		if (ret < 0)
		{
			LOG_ERR("Read/encode failed: %d", ret);
			return ret;
		}
		/* 終端NULを改行に置き換えて1サンプルを1行にする */
		json[ret] = '\n';
		ret = send_all(client, json, ret + 1);
		if (ret < 0)
		{
			return ret;
		}
		/* 取得と送信が終わってから100ms待機する */
		k_msleep(ACCEL_PERIOD_MS);
	}
}

/* メイン関数 */
int main(void)
{
	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT),
		.sin_addr.s_addr = htonl(INADDR_ANY),
	};
	int listener;
	int enable = 1;

	/* 加速度センサの初期化 */
	int ret = accel_init();

	if (ret < 0)
	{
		LOG_ERR("MPU6886 is not ready: %d", ret);
		return ret;
	}
	/* Wi-Fiに接続してIPv4アドレスを取得する */
	ret = network_connect();
	if (ret < 0)
	{
		LOG_ERR("Network connection failed: %d", ret);
		return ret;
	}
	/* TCP通信用のソケットを作成する */
	listener = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener < 0)
	{
		LOG_ERR("Socket creation failed: %d", errno);
		return -errno;
	}
	/* アドレスの再利用を許可してポート4242で接続を待ち受ける */
	(void)zsock_setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
	if (zsock_bind(listener, (struct sockaddr *)&address, sizeof(address)) < 0 ||
	    zsock_listen(listener, 1) < 0)
	{
		ret = -errno;
		LOG_ERR("Bind/listen failed: %d", ret);
		zsock_close(listener);
		return ret;
	}
	LOG_INF("TCP acceleration server listening on %d", SERVER_PORT);
	while (true)
	{
		/* PCからの接続を受け付ける */
		int client = zsock_accept(listener, NULL, NULL);

		if (client < 0)
		{
			if (errno != EINTR)
			{
				LOG_WRN("Accept failed: %d", errno);
				k_msleep(100);
			}
			continue;
		}
		/* 接続したPCへ加速度データを送信する */
		LOG_INF("Client connected");
		ret = stream_accel(client);
		LOG_INF("Stream ended: %d", ret);

		/* 接続を閉じて次のPC接続を待つ */
		zsock_close(client);
	}
	return 0;
}
