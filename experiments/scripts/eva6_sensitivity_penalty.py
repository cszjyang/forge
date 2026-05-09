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
workload_list = [
                'cphy02-100m',

                'ibm010-10m',
               ]

cache_size_list = ['0.2']
# In the paper's evaluation, we use 5 CNs with 400 threads and 1 MN for the penalty sensitivity experiment
# Please manually update the setting according to your cluster before running.
num_c_list = [300, 400]
penalty_us_list = [100, 500]

timeout = 300

for wl, cache_size in product(workload_list, cache_size_list):
    cluster_manager.set_exp_config(wl = wl, cache_size = cache_size, dry_run=args.dry_run)

    for penalty_us, num_c, method in product(penalty_us_list, num_c_list, method_list):
        cluster_manager.set_run_params(method, wl, cache_size, 1, num_c, timeout=timeout, dry_run=args.dry_run,
                                       running_option={'penalty_us': penalty_us})
        cluster_manager.compile_all()

        success, res = cluster_manager.run_all()
        if not success:
            cluster_manager.reset_cluster(dry_run=args.dry_run)
            continue
        print(f"{method}-{wl}-{cache_size}-{num_c}-{penalty_us} {res['tpt']} {res['hr']} {res['p50']} {res['p99']}")
