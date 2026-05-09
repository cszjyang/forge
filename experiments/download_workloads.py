#!/usr/bin/env python3

import argparse
import hashlib
import subprocess
from pathlib import Path

import gdown


WORKLOAD_ARCHIVES = {
    "eval": {
        "url": "https://drive.google.com/file/d/1IngJvgn_o2erYt4ky_LJhZprO52V93QY/view?usp=sharing",
        "archive_name": "workloads.tar.gz",
        "sha256": "d9d6537028b32ba37f8be755d339c6566ae4beb5f1a3910538fc8a6172c6fbba",
    },
    "more": {
        "url": "https://drive.google.com/file/d/1OpFVtUmbzAOi9tqMRWPzORvwxCR3AwpA/view?usp=sharing",
        "archive_name": "more_workloads.tar.gz",
        "sha256": "5c543f9adddb2e204b670abe1cfe414afd2b86ace0a2f62cb5268203950f905f",
    },
}


def sha256sum(path):
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(
        description="Download FORGE workloads and extract them under experiments/."
    )
    parser.add_argument(
        "--more",
        action="store_true",
        help="Download the larger workload archive collected for future research.",
    )
    args = parser.parse_args()

    experiments_dir = Path(__file__).resolve().parent
    archive = WORKLOAD_ARCHIVES["more" if args.more else "eval"]
    archive_path = experiments_dir / archive["archive_name"]

    print(f"Downloading {archive['archive_name']} from Google Drive")
    gdown.download(archive["url"], str(archive_path), fuzzy=True)

    print("Checking SHA256")
    actual_sha256 = sha256sum(archive_path)
    if actual_sha256 != archive["sha256"]:
        raise RuntimeError(
            f"SHA256 mismatch for {archive_path}\n"
            f"Expected: {archive['sha256']}\n"
            f"Actual:   {actual_sha256}"
        )

    print(f"Extracting {archive_path}")
    subprocess.run(
        ["tar", "-xzf", str(archive_path), "-C", str(experiments_dir)],
        check=True,
    )
    print(f"Done: {experiments_dir / 'workloads'}")


if __name__ == "__main__":
    main()
