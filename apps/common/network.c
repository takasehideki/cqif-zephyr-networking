/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/util.h>

#include "network.h"
#include "wifi_credentials.h"

LOG_MODULE_REGISTER(app_network, LOG_LEVEL_INF);

/* Wi-Fi APの接続先を読み込む */
static const uint8_t wifi_ssid[] = WIFI_SSID;
static const uint8_t wifi_password[] = WIFI_PASSWORD;

/* SSIDとパスワードの長さを確認する */
BUILD_ASSERT(sizeof(wifi_ssid) - 1 >= 1 && sizeof(wifi_ssid) - 1 <= 32);
BUILD_ASSERT(sizeof(wifi_password) - 1 >= 8 && sizeof(wifi_password) - 1 <= 63);

/* Wi-Fiに接続してIPv4アドレスの取得を待つ関数 */
int network_connect(void)
{
	struct net_if *iface = net_if_get_wifi_sta();
	static struct wifi_connect_req_params params = {
			.ssid = wifi_ssid,
			.ssid_length = sizeof(wifi_ssid) - 1,
			.psk = wifi_password,
			.psk_length = sizeof(wifi_password) - 1,
			.band = WIFI_FREQ_BAND_2_4_GHZ,
			.channel = WIFI_CHANNEL_ANY,
			.security = WIFI_SECURITY_TYPE_PSK,
			.mfp = WIFI_MFP_OPTIONAL,
			.timeout = 30,
	};
	int64_t deadline;
	int ret;

	/* Wi-Fiのネットワークインタフェースを確認する */
	if (iface == NULL)
	{
		return -ENODEV;
	}
	/* APへの接続を要求する */
	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
	if (ret < 0)
	{
		return ret;
	}
	LOG_INF("Wi-Fi connection requested");

	/* 接続要求の受理後，IPv4アドレスが取得できるまで最大30秒待つ */
	deadline = k_uptime_get() + 30000;
	while (k_uptime_get() < deadline)
	{
		if (net_if_is_up(iface))
		{
			const struct net_in_addr *addr;
			char address[NET_IPV4_ADDR_LEN];

			/* 取得したIPv4アドレスを文字列に変換して表示する */
			addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
			if (addr != NULL &&
					net_addr_ntop(NET_AF_INET, addr, address, sizeof(address)) != NULL)
			{
				LOG_INF("IPv4 ready: %s", address);
				return 0;
			}
		}
		/* 他のスレッドへ実行を譲ってから取得状態を再確認する */
		k_msleep(250);
	}
	return -ETIMEDOUT;
}
