import argparse

from utils.cluster_manager import ClusterManager, cluster_ips
from utils.settings import FORGE_HOME, RSYNC_SOURCE_HOST


def main():
    parser = argparse.ArgumentParser(
        description="Push the FORGE project from RSYNC_SOURCE_HOST to all other nodes."
    )
    parser.parse_args()

    cluster_manager = ClusterManager(cluster_ips, FORGE_HOME)
    print(f"Pushing project from {RSYNC_SOURCE_HOST}:{FORGE_HOME} to all other nodes")
    cluster_manager.rsync_project()
    print("Done")


if __name__ == "__main__":
    main()
