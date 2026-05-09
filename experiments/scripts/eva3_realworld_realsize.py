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

work_dir = f'{FORGE_HOME}/experiments/workload_throughput'
cluster_manager = ClusterManager(cluster_ips, work_dir)
cluster_manager.reset_cluster(dry_run=args.dry_run)
cluster_manager.rsync_project(dry_run=args.dry_run)
cluster_manager.set_exp_config(mn_cpu_num = 1, dry_run=args.dry_run)

method_list = ['forge']
workload_list = ['meta_kvcache_traces_4-100m', 'oG-twitter030-100m',]
cache_size_list = ['0.05', '0.1', '0.2']
num_c_list = [128]

running_option = {'eva_real_size': 'ON'}

timeout = 300

for wl, cache_size in product(workload_list, cache_size_list):
    # All methods in this experiment use the same compile options
    cluster_manager.set_exp_config(wl = wl, cache_size = cache_size, real_size = True, dry_run=args.dry_run)

    for num_c, method  in product(num_c_list, method_list):
        cluster_manager.set_run_params(method, wl, cache_size, 1, num_c, running_option, timeout=timeout, dry_run=args.dry_run)
        cluster_manager.compile_all()

        success, res = cluster_manager.run_all()
        if not success:
            cluster_manager.reset_cluster(dry_run=args.dry_run)
            continue
        print(f"{method}-{wl}-{cache_size}-{num_c} {res['tpt']} {res['hr']} {res['p50']} {res['p99']}")
