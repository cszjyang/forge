import argparse
import os

from utils.cluster_manager import ClusterManager, cluster_ips
from utils.settings import FORGE_HOME


def main():
    parser = argparse.ArgumentParser(
        description="Push the local FORGE project to all configured cluster nodes."
    )
    parser.add_argument("--dry-run", action="store_true",
                        help="print rsync and configuration commands without running them")
    args = parser.parse_args()

    cluster_manager = ClusterManager(cluster_ips, FORGE_HOME)
    source_path = os.path.abspath(os.path.expanduser(FORGE_HOME))
    print(f"Pushing local project from {source_path} to configured cluster nodes")
    cluster_manager.rsync_project(dry_run=args.dry_run)
    print("Done")


if __name__ == "__main__":
    main()
