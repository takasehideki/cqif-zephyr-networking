#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""AtomS3の改行区切りJSONを受信し，3軸加速度を単位付きで表示する．"""

import argparse
import json
import math
import socket
import sys


def receive_samples(stream):
    while True:
        # 改行までバッファリングする
        line = stream.readline(513)
        if not line:
            return
        if len(line) > 512 or not line.endswith(b"\n"):
            raise ValueError("JSONが長すぎるか，1行を受信する途中で切断されました")
        sample = json.loads(line)
        if not isinstance(sample, dict):
            raise ValueError("JSONオブジェクトが必要です")
        for field in ("seq", "uptime_ms"):
            if type(sample.get(field)) is not int or sample[field] < 0:
                raise ValueError(f"{field}が不正です")
        for field in ("ax", "ay", "az"):
            value = sample.get(field)
            if type(value) not in (int, float) or not math.isfinite(value):
                raise ValueError(f"{field}が不正です")
        yield sample


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="AtomS3のIPv4アドレス")
    args = parser.parse_args()
    try:
        # 分割・結合されたTCPデータを行単位に整える
        with (
            socket.create_connection((args.host, 4242), timeout=10) as sock,
            sock.makefile("rb") as stream,
        ):
            for sample in receive_samples(stream):
                # JSONから取り出した数値を単位と桁数をそろえて表示する
                print(
                    f"seq={sample['seq']:6d}  t={sample['uptime_ms'] / 1000:8.3f} s  "
                    f"ax={sample['ax']:8.3f}  ay={sample['ay']:8.3f}  "
                    f"az={sample['az']:8.3f} m/s^2",
                    flush=True,
                )
        print("接続が終了しました", file=sys.stderr)
    except KeyboardInterrupt:
        return 0
    except (OSError, ValueError) as error:
        print(f"受信エラー: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
