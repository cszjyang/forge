from fabric import Connection
import json
import os
import shlex
import subprocess
import time

from utils.settings import (
    FORGE_HOME,
    build_dir,
    cn_list,
    config_dir,
    controller,
    env_cmd,
    get_cache_config_cmd,
    get_make_cmd,
    get_mn_cpu_cmd,
    mean_kv_size_map,
    mn_list,
    running_default_opt,
    server_list,
    user,
)


def _connection_name(host, user):
    if "@" in host or not user:
        return host
    return f"{user}@{host}"


mn_ids = [server_list.index(s) for s in mn_list]
cn_ids = [server_list.index(s) for s in cn_list]
controller_id = server_list.index(controller)
cluster_ips = [_connection_name(s, user) for s in server_list]

NUM_CN = len(cn_ids)
NUM_MN = len(mn_ids)

SSH_CONNECT_TIMEOUT = 10
SSH_AUTH_TIMEOUT = 15
SSH_STRICT_HOST_KEY_CHECKING = "accept-new"
RSYNC_EXCLUDES = [
    ".git/",
    "build/",
    ".venv/",
    ".env",
    ".cache/",
    "*.log",
    "experiments/workloads.tar.gz",
    "experiments/more_workloads.tar.gz",
]
RSYNC_SSH_EXTRA_OPTIONS = [
    "-o",
    "ServerAliveInterval=15",
    "-o",
    "ServerAliveCountMax=2",
]
MEMORY_CONFIG_CMD = (
    f"cd {config_dir} && python modify_config.py "
    f"memory_num={NUM_MN} memory_ip_list={shlex.quote(json.dumps(mn_list))}"
)

if user:
    RESET_CMD = f"pkill -u {user} -f '[c]ontroller.py' || true; pkill -u {user} -x init || true"
else:
    RESET_CMD = "pkill -f '[c]ontroller.py' || true; pkill -x init || true"


class ClusterCommandError(RuntimeError):
    pass


def _short_error(err):
    timeout = getattr(err, "timeout", None)
    if timeout:
        return f"{type(err).__name__}: timeout after {timeout}s"

    text = _error_text(err)
    if not text:
        text = type(err).__name__
    return f"{type(err).__name__}: {text.splitlines()[0]}"


def _error_text(err):
    result = getattr(err, "result", None)
    stderr = getattr(result, "stderr", "") if result is not None else getattr(err, "stderr", "")
    stdout = getattr(result, "stdout", "") if result is not None else getattr(err, "stdout", "")
    if isinstance(stderr, bytes):
        stderr = stderr.decode(errors="replace")
    if isinstance(stdout, bytes):
        stdout = stdout.decode(errors="replace")
    return (stderr or stdout or str(err)).strip()


def _ssh_options(extra=()):
    opts = [
        "-o",
        "BatchMode=yes",
        "-o",
        f"ConnectTimeout={SSH_CONNECT_TIMEOUT}",
        "-o",
        f"StrictHostKeyChecking={SSH_STRICT_HOST_KEY_CHECKING}",
    ]
    opts.extend(extra)
    return opts


def _remote_cmd(cmd):
    return f"{env_cmd} && {cmd}"


def _format_command_error(node, cmd, err):
    return "\n".join([
        f"[cluster] command failed on {node}",
        f"  command: {cmd}",
        f"  error: {_short_error(err)}",
    ])


def _format_local_command_error(cmd_args, err):
    return "\n".join([
        "[cluster] local command failed",
        f"  command: {shlex.join(cmd_args)}",
        f"  error: {_short_error(err)}",
    ])


class ClusterManager(object):
    def __init__(self, cluster_ips: list, work_dir: str, verbose: bool = False):
        self._cluster_ips = cluster_ips
        self._conn_list = [
            Connection(
                ip,
                connect_timeout=SSH_CONNECT_TIMEOUT,
                connect_kwargs={
                    "auth_timeout": SSH_AUTH_TIMEOUT,
                    "banner_timeout": SSH_CONNECT_TIMEOUT,
                },
            )
            for ip in self._cluster_ips
        ]
        self._uniq_cluster_ips = list(dict.fromkeys(cluster_ips))
        self._uniq_conn_indexes = [cluster_ips.index(ip) for ip in self._uniq_cluster_ips]
        self.work_dir = work_dir
        self.all_res = {}
        self.verbose = verbose
        self._promise_meta = {}

    def _run_remote(self, node_id, cmd, timeout=600, asynchronous=True):
        node = self._cluster_ips[node_id]
        remote_cmd = _remote_cmd(cmd)
        if self.verbose:
            print(f"[cluster] {node}: {cmd}")
        try:
            result = self._conn_list[node_id].run(
                remote_cmd,
                asynchronous=asynchronous,
                timeout=timeout,
                hide=not self.verbose,
            )
        except Exception as err:
            raise ClusterCommandError(_format_command_error(node, remote_cmd, err)) from None
        if asynchronous:
            self._promise_meta[id(result)] = (node_id, remote_cmd)
        return result

    def _run_async(self, node_id, cmd, timeout=600):
        return self._run_remote(node_id, cmd, timeout=timeout, asynchronous=True)

    def _join_promise(self, promise, node_id=None, cmd=None):
        if node_id is None or cmd is None:
            node_id, cmd = self._promise_meta.get(id(promise), (node_id, cmd))
        node = self._cluster_ips[node_id] if node_id is not None else "<unknown>"
        try:
            return promise.join()
        except Exception as err:
            raise ClusterCommandError(_format_command_error(node, cmd or "<unknown>", err)) from None

    def execute_all(self, cmd, timeout=600, dry_run=False):
        if dry_run:
            print(f'ALL NODES: {cmd}')
            return
        self.execute_on_nodes(self._uniq_conn_indexes, cmd, timeout=timeout)

    def execute_on_nodes(self, node_ids, cmd, timeout=600, dry_run=False):
        if dry_run:
            print(f"NODES {[self._cluster_ips[i] for i in node_ids]}: {cmd}")
            return
        promises = []
        errors = []
        for node_id in node_ids:
            try:
                promises.append((node_id, self._run_async(node_id, cmd, timeout=timeout)))
            except ClusterCommandError as err:
                errors.append(str(err))
        for node_id, promise in promises:
            try:
                self._join_promise(promise, node_id)
            except ClusterCommandError as err:
                errors.append(str(err))
        if errors:
            raise ClusterCommandError("\n".join(errors))

    def execute_on_node(self, node_id, cmd, asynchronous=True, timeout=600, dry_run=False):
        if dry_run:
            print(f'NODE {self._cluster_ips[node_id]}: {cmd}')
            return
        if asynchronous:
            return self._run_async(node_id, cmd, timeout=timeout)
        return self._run_remote(node_id, cmd, timeout=timeout, asynchronous=False)

    def get_file(self, node_id, remote, local):
        node_sftp = self._conn_list[node_id].sftp()
        node_sftp.get(remote, local)
        
        
    def reset_cluster(self, dry_run=False):
        self.execute_all(RESET_CMD, dry_run=dry_run)
        
    def rsync_project(self, dry_run=False):
        ssh_opts = shlex.join(["ssh", *_ssh_options(RSYNC_SSH_EXTRA_OPTIONS)])
        source_path = os.path.join(os.path.abspath(os.path.expanduser(FORGE_HOME)), "")
        if not dry_run and not os.path.isdir(source_path):
            raise ClusterCommandError(f"local FORGE_HOME source path does not exist: {source_path}")
        errors = []
        for node_id in self._uniq_conn_indexes:
            target = self._cluster_ips[node_id]
            target_path = f"{target}:{FORGE_HOME.rstrip('/')}/"
            rsync_cmd = [
                "rsync",
                "-az",
                "--delete",
                "-e",
                ssh_opts,
                *(f"--exclude={pattern}" for pattern in RSYNC_EXCLUDES),
                source_path,
                target_path,
            ]
            if dry_run:
                print(f"LOCAL: {shlex.join(rsync_cmd)}")
                continue
            if self.verbose:
                print(f"[cluster] local: {shlex.join(rsync_cmd)}")
            try:
                subprocess.run(
                    rsync_cmd,
                    capture_output=not self.verbose,
                    text=True,
                    timeout=600,
                    check=True,
                )
            except (subprocess.CalledProcessError, subprocess.TimeoutExpired, OSError) as err:
                errors.append(_format_local_command_error(rsync_cmd, err))
        if errors:
            raise ClusterCommandError("\n".join(errors))
        self.execute_all(MEMORY_CONFIG_CMD, dry_run=dry_run)
        
    def set_exp_config(self, dry_run=False, **kwargs):
        # Switch according to the kwargs
        for k, v in kwargs.items():
            if k == 'mn_cpu_num':
                MN_CPU_CMD = get_mn_cpu_cmd(config_dir, v)
                self.execute_all(MN_CPU_CMD, dry_run=dry_run)
            elif k == 'cache_size':
                if 'wl' not in kwargs:
                    raise ValueError("wl is not set")
                wl = kwargs['wl']
                if 'real_size' in kwargs and kwargs['real_size'] == True:
                    wl += '-real_size'
                CACHE_CONFIG_CMD = get_cache_config_cmd(config_dir, wl, v)
                self.execute_all(CACHE_CONFIG_CMD, dry_run=dry_run)
            elif k == 'wl':
                pass
            elif k == 'real_size':
                pass
            else:
                raise ValueError(f"Invalid keyword argument: {k}")
        
    def set_run_params(self, method, wl, cache_size, num_m, num_c, running_option=None, timeout=None, dry_run=False):
        if method != 'forge':
            raise ValueError(f"Unsupported method {method}; this artifact builds FORGE only")

        self.method = method
        self.wl = wl
        self.cache_size = cache_size
        self.num_m = num_m
        self.num_c = num_c
        self.running_option = {**running_default_opt, **(running_option or {})}
        self.running_option['num_cn'] = NUM_CN if num_c > NUM_CN else num_c
        self.running_option['num_mn'] = NUM_MN
        self.timeout = timeout
        self.dry_run = dry_run

        mean_kv_size = mean_kv_size_map[wl] if self.running_option.get('eva_real_size') == 'ON' else 256
        self.running_option['group_bytes'] = self.running_option['max_group_size'] * mean_kv_size
            
        # clear prom
        self.controller_prom = None
        self.mn_prom_list = None
        self.c_prom_list = None
        
    def compile_all(self):
        MAKE_CMD = get_make_cmd(build_dir, self.method, self.wl, self.cache_size, self.running_option)
        self.execute_all(MAKE_CMD, dry_run=self.dry_run)
        
    def run_controller(self):
        cmd = (
            f"cd {self.work_dir} && ./run_controller.sh "
            f"{self.method} {self.num_m} {self.num_c} {self.wl} {self.cache_size}"
        )
        self.controller_prom = self.execute_on_node(controller_id, cmd, timeout=self.timeout, dry_run=self.dry_run)
        return self.controller_prom
    
    def run_mns(self):
        cmd = f"cd {self.work_dir} && ./run_server.sh {self.method}"
        self.mn_prom_list = []
        for i in range(NUM_MN):
            mn_prom = self.execute_on_node(mn_ids[i], cmd, timeout=self.timeout, dry_run=self.dry_run)
            self.mn_prom_list.append(mn_prom)
        return self.mn_prom_list
        
        
    def run_clients(self):
        client_num = list()
        for i in range(NUM_CN - 1):
            node_num_c = int((self.num_c - sum(client_num)) / (NUM_CN - i))
            client_num.append(node_num_c)
        client_num.append(self.num_c - sum(client_num))
        
        self.c_prom_list = []
        for i in range(NUM_CN):
            if client_num[i] == 0:
                continue
            st_cid = sum(client_num[:i]) + 1
            cmd = (
                f"cd {self.work_dir} && ./run_client_master.sh "
                f"{self.method} {st_cid} {self.wl} {client_num[i]} {self.num_c}"
            )
            c_prom = self.execute_on_node(cn_ids[i], cmd, timeout=self.timeout, dry_run=self.dry_run)
            self.c_prom_list.append(c_prom)
        return self.c_prom_list
    
    def wait_result(self):
        if self.dry_run:
            return False, None
        if self.c_prom_list is None:
            raise ValueError("c_prom_list is not set")
        
        enc_err = False
        # wait for Clients and MN
        for c_prom in self.c_prom_list:
            try:
                self._join_promise(c_prom)
            except ClusterCommandError as e:
                print(e)
                print(f"{self.method}-{self.wl}-{self.cache_size}-{self.num_c} ERROR!")
                enc_err = True
                break
        if enc_err:
            # print("Resetting cluster...", file=sys.stderr)
            self.reset_cluster()
            return False, None
        
        for mn_prom in self.mn_prom_list:
            self._join_promise(mn_prom)

        raw_res = self._join_promise(self.controller_prom)
        line = raw_res.tail("stdout", 1).strip()
        res = json.loads(line)
        
        if self.wl not in self.all_res:
            self.all_res[self.wl] = {}
        if self.method not in self.all_res[self.wl]:
            self.all_res[self.wl][self.method] = {}
        if self.cache_size not in self.all_res[self.wl][self.method]:
            self.all_res[self.wl][self.method][self.cache_size] = {}
        self.all_res[self.wl][self.method][self.cache_size] = res
        
        # if "n_exe_read" in res:
        #     actions = ['exe', 'hotness', 'evict', 'gc']
        #     verbs = ['read', 'write', 'cas', 'faa']
        #     with open("breakdown.txt", "a") as f:
        #         f.write(f"{self.method}-{self.wl}-{self.cache_size}-{self.num_c}\n")
        #         for action in actions:
        #             for verb in verbs:
        #                 f.write(f"{res[f'n_{action}_{verb}']} ")
        #             f.write("\n")
        #         f.write("\n")
        
        return True, res
    
    def run_all(self):
        self.run_controller()
        self.run_mns()
        time.sleep(1)   # wait for controller and mn to start
        self.run_clients()
        return self.wait_result()
