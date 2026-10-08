#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Zenohで加速度データを購読し，CSVファイルに記録する．"""

import argparse
import csv
import sys
from datetime import datetime, timezone
from pathlib import Path

import zenoh
from zenoh_sub import DEFAULT_CONNECT, DEFAULT_KEY, decode_sample, make_config

DEFAULT_OUTPUT = Path("accel.csv")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--connect", default=DEFAULT_CONNECT, help="ルータの接続先")
    parser.add_argument("--key", default=DEFAULT_KEY, help="購読するキー式")
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help="保存先のCSVファイル（既定値: accel.csv）",
    )
    args = parser.parse_args()
    try:
        # 同名のファイルがあれば上書きし，新しく記録を始める
        with (
            args.output.open("w", encoding="utf-8", newline="") as stream,
            zenoh.open(make_config(args.connect)) as session,
            session.declare_subscriber(args.key) as subscriber,
        ):
            writer = csv.writer(stream)
            writer.writerow(["received_at_utc", "seq", "uptime_ms", "ax", "ay", "az"])
            stream.flush()
            print(f"記録中: {args.output}  終了: Ctrl+C", flush=True)
            for received in subscriber:
                # Pythonが購読キューから取り出した時点のPCの時刻を付ける
                received_at = datetime.now(timezone.utc).isoformat(
                    timespec="milliseconds"
                )
                sample = decode_sample(received.payload)
                writer.writerow(
                    [
                        received_at,
                        sample["seq"],
                        sample["uptime_ms"],
                        sample["ax"],
                        sample["ay"],
                        sample["az"],
                    ]
                )
                stream.flush()
    except KeyboardInterrupt:
        return 0
    except (zenoh.ZError, OSError, ValueError) as error:
        print(f"記録エラー: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
