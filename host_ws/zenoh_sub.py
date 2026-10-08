#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Zenohで加速度データを購読し，3軸の値を表示する．"""

import argparse
import json
import math
import sys

import zenoh

DEFAULT_CONNECT = "tcp/127.0.0.1:7447"
DEFAULT_KEY = "cqif/device01/accel"


def make_config(endpoint):
    # 指定したルータへclientとして接続する
    config = zenoh.Config()
    config.insert_json5("mode", '"client"')
    config.insert_json5("connect/endpoints", json.dumps([endpoint]))
    config.insert_json5("scouting/multicast/enabled", "false")
    return config


def decode_sample(payload):
    # 1つのペイロードを1サンプルのJSONとして解釈する
    sample = json.loads(payload.to_bytes())
    if not isinstance(sample, dict):
        raise ValueError("JSONオブジェクトが必要です")
    for field in ("seq", "uptime_ms"):
        if type(sample.get(field)) is not int or sample[field] < 0:
            raise ValueError(f"{field}が不正です")
    for field in ("ax", "ay", "az"):
        value = sample.get(field)
        if type(value) not in (int, float) or not math.isfinite(value):
            raise ValueError(f"{field}が不正です")
    return sample


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--connect", default=DEFAULT_CONNECT, help="ルータの接続先")
    parser.add_argument("--key", default=DEFAULT_KEY, help="購読するキー式")
    args = parser.parse_args()
    try:
        with (
            zenoh.open(make_config(args.connect)) as session,
            session.declare_subscriber(args.key) as subscriber,
        ):
            print(f"購読中: {args.key}  終了: Ctrl+C", flush=True)
            for received in subscriber:
                sample = decode_sample(received.payload)
                print(
                    f"seq={sample['seq']:6d}  t={sample['uptime_ms'] / 1000:8.3f} s  "
                    f"ax={sample['ax']:8.3f}  ay={sample['ay']:8.3f}  "
                    f"az={sample['az']:8.3f} m/s^2",
                    flush=True,
                )
    except KeyboardInterrupt:
        return 0
    except (zenoh.ZError, OSError, ValueError) as error:
        print(f"購読エラー: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
