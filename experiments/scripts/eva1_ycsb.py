from itertools import product
import time
import argparse

from utils.cluster_manager import ClusterManager, cluster_ips
from utils.settings import FORGE_HOME





parser = argparse.ArgumentParser()
parser.add_argument("--dry-run", action="store_true",
                    help="dry run, only print commands")
args = parser.parse_args()

st = time.time()

# work_dir = f'{FORGE_HOME}/experiments/ycsb_test'
work_dir = f'{FORGE_HOME}/experiments/workload_throughput'
cluster_manager = ClusterManager(cluster_ips, work_dir)
cluster_manager.reset_cluster(dry_run=args.dry_run)
cluster_manager.rsync_project(dry_run=args.dry_run)
cluster_manager.set_exp_config(mn_cpu_num = 1, dry_run=args.dry_run)


# start experiment
method_list = ['forge']
client_num_list = list(range(16, 257, 16))
workload_list = [ 'ycsba', 'ycsbb', 'ycsbc', 'ycsbd', 'ycsbe']
cache_size_list = ['0.2']

timeout = 300

all_res = {}

for wl, cache_size in product(workload_list, cache_size_list):
    cluster_manager.set_exp_config(wl = wl, cache_size = cache_size, dry_run=args.dry_run)

    for method, num_c in product(method_list, client_num_list):
        cluster_manager.set_run_params(method, wl, cache_size, 1, num_c, timeout=timeout, dry_run=args.dry_run,
                                       running_option={'num_thread_per_smm': 128})
        cluster_manager.compile_all()

        success, res = cluster_manager.run_all()
        if not success:
            cluster_manager.reset_cluster(dry_run=args.dry_run)
            continue
        print(f"{method}-{wl}-{cache_size}-{num_c} {res['tpt']} {res['p50']} {res['p99']} {res['hr']}")
