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


workload_list = [
                'cphy02-100m',

                'ibm010-10m',
               ]

cache_size_list = ['0.2']
group_size_list = [16, 32, 64, 128, 256]
num_c = 128
running_option = {'max_group_size': 512, 'eva_real_size': 'OFF'}

timeout = 120

method = 'forge'
for wl, cache_size in product(workload_list, cache_size_list):
    # All methods in this experiment use the same compile options
    cluster_manager.set_exp_config(wl = wl, cache_size = cache_size, dry_run=args.dry_run)

    for group_size in group_size_list:
        running_option['max_group_size'] = group_size
        cluster_manager.set_run_params(method, wl, cache_size, 1, num_c, running_option, timeout=timeout, dry_run=args.dry_run)
        cluster_manager.compile_all()

        success, res = cluster_manager.run_all()
        if not success:
            print(res)
            cluster_manager.reset_cluster(dry_run=args.dry_run)
            continue
        print(f"{method}-{wl}-{cache_size}-{group_size} {res['tpt']} {res['hr']} {res['p50']} {res['p99']}")
