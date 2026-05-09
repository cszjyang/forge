#!/usr/bin/env python3
"""Generate workload-specific settings from downloaded workload traces.

The experiment launcher consumes the generated values through
experiments/scripts/utils/settings.py:

- mean_kv_size_map
- cache_config_cmd_map
- hash_bucket_num_setting

This script prints a JSON object with those three tables. Copy the relevant
entries into settings.py after checking the output. Missing workload files are
skipped with a warning so partially downloaded families can still be processed.
"""

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

import numpy as np
import pandas as pd

from scripts.utils.settings import (
    cphy_wl_list,
    ibm_wl_list,
    meta_wl_list,
    msr_wl_list,
    twitter_oG_wl_list,
    twitter_wl_list,
    wiki_wl_list,
    ycsb_wl_list,
)


OBJ_HEADER_SIZE = 48
UNI_BLOCK_SIZE = 256
MAX_GROUP_SIZE = 512
# 255 is reserved for the invalid/-1 block index encoding.
MAX_KV_LEN = 254
NUM_SEGMENT = 128
HASH_BUCKET_ASSOC = 8
HASH_LOAD_FACTOR = 0.25
CACHE_RATIOS = [0.01, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5]
WORKLOAD_HOME = Path(__file__).resolve().parent / "workloads"


@dataclass
class WorkloadSetting:
    cache_config: dict
    bucket_setting: dict
    mean_kv_size: Optional[int] = None
    real_size: bool = False


def roundup(x, align):
    return (x // align) * align + ((x % align) > 0) * align


def rounddown(x, align):
    return (x // align) * align


def ratio_key(ratio):
    return f"{ratio:g}"


def get_mean_kv_size(average_size):
    mean_kv_size = rounddown(average_size, UNI_BLOCK_SIZE)
    if float(average_size) / float(mean_kv_size) > 1.2:
        mean_kv_size = roundup(average_size, UNI_BLOCK_SIZE)
    return int(mean_kv_size)


def column_names(trace_type):
    if trace_type == "twitter":
        return ["timestamp", "key", "key_size", "val_size", "client_id", "operation", "ttl"]
    if trace_type in {"cphy", "msr", "wiki", "meta", "twitter-og"}:
        return ["timestamp", "key", "size", "next_access_time"]
    if trace_type == "ibm":
        return ["timestamp", "operation", "key", "size"]
    if trace_type == "ycsb":
        return ["operation", "key"]
    raise ValueError(f"unsupported trace type: {trace_type}")


def print_summary(workload, data, quiet):
    if quiet:
        return
    print(f"==== {workload} ====", file=sys.stderr)
    print(pd.DataFrame([data]).to_string(index=False), file=sys.stderr)


def warn(message):
    print(f"[generate_workload_settings] WARNING: {message}", file=sys.stderr)


def cache_config_for_fixed_block(num_unique_keys):
    config = {}
    for ratio in CACHE_RATIOS:
        server_data_len = UNI_BLOCK_SIZE * int(num_unique_keys) * ratio
        segment_size = 1048576
        server_data_len = roundup(int(server_data_len), segment_size)
        config[ratio_key(ratio)] = (
            "python modify_config.py "
            f"server_data_len={server_data_len} "
            f"segment_size={segment_size} "
            f"block_size={UNI_BLOCK_SIZE}"
        )
    return config


def cache_config_for_trace(stats, real_size=False):
    config = {}
    average_size = int(stats["ave_uni_key_size"]) if real_size else UNI_BLOCK_SIZE
    group_bytes = MAX_GROUP_SIZE * get_mean_kv_size(average_size)

    for ratio in CACHE_RATIOS:
        server_data_len = average_size * int(stats["num_uni_key"]) * ratio
        segment_size = roundup(int(server_data_len / NUM_SEGMENT), group_bytes)
        server_data_len = segment_size * NUM_SEGMENT
        config[ratio_key(ratio)] = (
            "python modify_config.py "
            f"server_data_len={server_data_len} "
            f"segment_size={segment_size} "
            f"block_size={UNI_BLOCK_SIZE}"
        )
    return config


def bucket_setting(stats):
    base_bucket_num = int(stats["num_uni_key"]) / HASH_LOAD_FACTOR / HASH_BUCKET_ASSOC
    return {
        ratio_key(ratio): int(base_bucket_num * ratio)
        for ratio in CACHE_RATIOS
    }


def size_stats(df):
    unique_key_sizes = dict(zip(df["key"], df["size"]))
    unique_sizes = list(unique_key_sizes.values())
    return {
        "num_key": len(df["key"]),
        "ave_size": float(np.mean(df["size"])),
        "min_size": int(np.min(df["size"])),
        "max_size": int(np.max(df["size"])),
        "num_uni_key": len(unique_key_sizes),
        "ave_uni_key_size": float(np.average(unique_sizes)),
    }


def analyze_ycsb(workload_home, workload, quiet):
    df_load = pd.read_csv(
        workload_home / "ycsb" / f"{workload}.load",
        sep=" ",
        names=column_names("ycsb"),
    )
    df_trans = pd.read_csv(
        workload_home / "ycsb" / f"{workload}.trans",
        sep=" ",
        names=column_names("ycsb"),
    )
    df = pd.concat([df_load, df_trans], ignore_index=True)
    df_write = df[(df["operation"] == "INSERT") | (df["operation"] == "UPDATE")]
    num_first_write = len(df_write["key"].unique())
    stats = {
        "num_key": len(df["key"]),
        "num_uni_key": len(df["key"].unique()),
        "write_ratio": len(df_write) / len(df["key"]),
        "update_ratio": (len(df_write) - num_first_write) / len(df["key"]),
    }
    print_summary(workload, stats, quiet)
    return WorkloadSetting(
        cache_config=cache_config_for_fixed_block(stats["num_uni_key"]),
        bucket_setting=bucket_setting(stats),
    )


def analyze_simple_trace(workload_home, family, workload, quiet):
    df = pd.read_csv(
        workload_home / family / workload,
        sep=" ",
        names=column_names(family),
    )
    stats = size_stats(df)
    print_summary(workload, stats, quiet)
    return WorkloadSetting(
        cache_config=cache_config_for_trace(stats),
        bucket_setting=bucket_setting(stats),
    )


def analyze_ibm(workload_home, workload, quiet):
    df = pd.read_csv(
        workload_home / "ibm" / workload,
        sep=" ",
        header=None,
        usecols=[0, 1, 2, 3],
        names=column_names("ibm"),
        on_bad_lines="skip",
    )
    stats = size_stats(df)
    df_write = df[df["operation"] == "REST.PUT.OBJECT"]
    num_first_write = len(df_write["key"].unique())
    stats["write_ratio"] = len(df_write) / len(df["key"])
    stats["update_ratio"] = (len(df_write) - num_first_write) / len(df["key"])
    print_summary(workload, stats, quiet)
    return WorkloadSetting(
        cache_config=cache_config_for_trace(stats),
        bucket_setting=bucket_setting(stats),
    )


def analyze_twitter(workload_home, workload, quiet, real_size=False):
    df = pd.read_csv(
        workload_home / "twitter" / workload,
        sep=" ",
        names=column_names("twitter"),
    )
    df["size"] = df["key_size"] + df["val_size"]
    return analyze_sized_dataframe(workload, df, quiet, real_size)


def analyze_twitter_og(workload_home, workload, quiet, real_size=False):
    df = pd.read_csv(
        workload_home / "twitter" / workload,
        sep=" ",
        names=column_names("twitter-og"),
    )
    return analyze_sized_dataframe(workload, df, quiet, real_size)


def analyze_sized_dataframe(workload, df, quiet, real_size):
    if real_size:
        df["size"] = roundup(df["size"] + OBJ_HEADER_SIZE, UNI_BLOCK_SIZE)
        df["size"] = df["size"].clip(upper=UNI_BLOCK_SIZE * MAX_KV_LEN)

    stats = size_stats(df)
    print_summary(workload, stats, quiet)
    return WorkloadSetting(
        cache_config=cache_config_for_trace(stats, real_size=real_size),
        bucket_setting=bucket_setting(stats),
        mean_kv_size=get_mean_kv_size(stats["ave_uni_key_size"]) if real_size else None,
        real_size=real_size,
    )


def analyze_meta_real_size(workload_home, workload, quiet):
    df = pd.read_csv(
        workload_home / "meta" / workload,
        sep=" ",
        names=column_names("meta"),
    )
    return analyze_sized_dataframe(workload, df, quiet, real_size=True)


FAMILIES = {
    "ycsb": (ycsb_wl_list, analyze_ycsb),
    "cphy": (cphy_wl_list, lambda home, wl, quiet: analyze_simple_trace(home, "cphy", wl, quiet)),
    "msr": (msr_wl_list, lambda home, wl, quiet: analyze_simple_trace(home, "msr", wl, quiet)),
    "wiki": (wiki_wl_list, lambda home, wl, quiet: analyze_simple_trace(home, "wiki", wl, quiet)),
    "meta": (meta_wl_list, lambda home, wl, quiet: analyze_simple_trace(home, "meta", wl, quiet)),
    "ibm": (ibm_wl_list, analyze_ibm),
    "twitter": (twitter_wl_list, lambda home, wl, quiet: analyze_twitter(home, wl, quiet)),
    "twitter-real-size": (twitter_wl_list, lambda home, wl, quiet: analyze_twitter(home, wl, quiet, real_size=True)),
    "twitter-og-real-size": (
        twitter_oG_wl_list,
        lambda home, wl, quiet: analyze_twitter_og(home, wl, quiet, real_size=True),
    ),
    "meta-real-size": (meta_wl_list, analyze_meta_real_size),
}


def selected_families(name):
    if name == "all":
        return [family for family in FAMILIES]
    return [name]


def main():
    parser = argparse.ArgumentParser(
        description="Generate workload-specific settings from workload traces."
    )
    parser.add_argument(
        "--family",
        choices=["all", *sorted(FAMILIES)],
        default="ycsb",
        help="workload family to analyze; default uses the complete YCSB family",
    )
    parser.add_argument(
        "--workload",
        action="append",
        help="analyze only this workload within the selected family; repeat as needed",
    )
    parser.add_argument(
        "--workload-home",
        type=Path,
        default=WORKLOAD_HOME,
        help="path to experiments/workloads",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="suppress per-workload summaries on stderr",
    )
    args = parser.parse_args()

    output = {
        "mean_kv_size_map": {},
        "cache_config_cmd_map": {},
        "hash_bucket_num_setting": {},
    }

    for family in selected_families(args.family):
        workloads, analyzer = FAMILIES[family]
        selected_workloads = args.workload or workloads
        for workload in selected_workloads:
            try:
                setting = analyzer(args.workload_home, workload, args.quiet)
            except FileNotFoundError as exc:
                warn(f"skip missing workload {workload}: {exc}")
                continue
            cache_key = f"{workload}-real_size" if setting.real_size else workload
            output["cache_config_cmd_map"][cache_key] = setting.cache_config
            output["hash_bucket_num_setting"][workload] = setting.bucket_setting
            if setting.mean_kv_size is not None:
                output["mean_kv_size_map"][workload] = setting.mean_kv_size

    print(json.dumps(output, indent=4, separators=(",", ": ")))


if __name__ == "__main__":
    main()
