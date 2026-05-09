# The controller runs on the controller node and collects results from clients and servers.

import time
import memcache
import argparse
import json
import numpy as np
from itertools import product

from scripts.utils.settings import workload_list


class DMCMemcachedController:
    def __init__(self, memcached_ip, memcached_port,
                 num_servers, num_clients):
        self.memcached_ip = memcached_ip
        self.memcached_port = memcached_port
        self.num_servers = num_servers
        self.num_clients = num_clients
        self.num_sync = 0
        self.num_result = 0
        self.mc = memcache.Client(
            ['{}:{}'.format(memcached_ip, memcached_port)], debug=False)
        assert (self.mc != None)
        self.mc.flush_all()

    def wait_msg(self, msg):
        val = self.mc.get(msg)
        while val == None:
            val = self.mc.get(msg)

    def sync_ready_clients(self, client_num = 0):
        if client_num == 0:
            client_num = self.num_clients
        for i in range(client_num):
            cid = i + 1
            key = "client-{}-ready-{}".format(cid, self.num_sync)   # num_sync is the epoch number of sync
            val = self.mc.get(key)
            while val == None:  # Keep trying to get the ready signal until sucess
                val = self.mc.get(key)
            print("Client {} ready: {}".format(cid, val))
        print("All client ready!")
        self.mc.set("all-client-ready-{}".format(self.num_sync), 1) # All clients ready. Set the signal to 1
        self.num_sync += 1  # What is the usage of num_sync?

# The following two functions are the same, except that the first one can gather results from any number of clients
    def gather_client_results(self, client_num = 0) -> dict:
        if client_num == 0:
            client_num = self.num_clients
        client_result_dict = {}
        for i in range(client_num):
            cid = i + 1  # Why + self.num_servers? To reserve id for the server?
            key = "client-{}-result-{}".format(cid, self.num_result)
            val = self.mc.get(key)
            while val == None:
                val = self.mc.get(key)  # Keep trying to get the result until success
            # print(json.loads(val))
            client_result_dict[cid] = json.loads(str(val.decode('UTF-8')))
        self.num_result += 1
        return client_result_dict

    def clear_all_contents(self):
        self.mc.flush_all()

    def stop_server(self):
        self.mc.set("server-stop", 1)

    def get_server_stats(self):
        key = "server-result"
        val = self.mc.get(key)
        while val == None:
            val = self.mc.get(key)
        return json.loads(str(val.decode('UTF-8')))

def control_bench(controller: DMCMemcachedController):
    # Synchronize clients for warmup and start
    controller.sync_ready_clients()  # sync warmup
    controller.sync_ready_clients()  # sync start
    
    # Collect results from all clients
    res_dict = controller.gather_client_results()
    
    # Initialize aggregation variables
    agg_ops = np.zeros(len(res_dict[1]['n_ops_cont']))
    agg_miss = np.zeros(len(res_dict[1]['n_miss_cont']))
    get_ops = 0
    set_ops = 0
    merged_map = {}
    
    # Aggregate data from all clients
    for i in range(1, controller.num_clients + 1):
        # Aggregate operation counts
        agg_ops += np.array(res_dict[i]['n_ops_cont'])
        agg_miss += np.array(res_dict[i]['n_miss_cont'])
        get_ops += res_dict[i]['n_get']
        set_ops += res_dict[i]['n_set']
        
        # Merge latency maps
        for ent in res_dict[i]['lat_map']:
            if ent[0] not in merged_map:
                merged_map[ent[0]] = 0
            merged_map[ent[0]] += ent[1]
    
    # Process latency data
    key_list = sorted(merged_map.keys())
    lat_list = []
    for k in key_list:
        lat_list += [k] * merged_map[k]
    
    # Calculate throughput and hit rates
    sft_ops = np.array([0] + list(agg_ops)[:-1])
    sft_miss = np.array([0] + list(agg_miss)[:-1])
    
    tpt = (agg_ops - sft_ops) / 0.5
    hr_fine = 1 - ((agg_miss - sft_miss) / (agg_ops - sft_ops))
    hr_coarse = 1 - (agg_miss / agg_ops)
    
    # Prepare result dictionaries
    combined_dict = {
        'get_ops': get_ops,
        'set_ops': set_ops,
        'tpt': list(tpt),
        'hr_overall': 1 - (agg_miss[-1] / agg_ops[-1]),
        'tpt_overall': agg_ops[-1] / 20,
        'hr_coarse': list(hr_coarse),
        'hr_fine': list(hr_fine),
        'p50': lat_list[int(len(lat_list) * 0.5)],
        'p90': lat_list[int(len(lat_list) * 0.9)],
        'p99': lat_list[int(len(lat_list) * 0.99)],
        'p999': lat_list[int(len(lat_list) * 0.999)],
    }
    
    # Output summary results as JSON
    json_res = {
        'tpt': combined_dict['tpt_overall'],
        'hr': combined_dict['hr_overall'],
        'p50': combined_dict['p50'],
        'p99': combined_dict['p99'],
    }
    
    if "n_exe_read" in res_dict[1]:
        # Initialize counters
        actions = ['exe', 'hotness', 'evict', 'gc']
        verbs = ['read', 'write', 'cas', 'faa']
        num_total = 0
        
        for action, verb in product(actions, verbs):
            json_res[f"n_{action}_{verb}"] = 0
            for i in range(1, controller.num_clients + 1):
                json_res[f"n_{action}_{verb}"] += res_dict[i][f"n_{action}_{verb}"]
                num_total += res_dict[i][f"n_{action}_{verb}"]
        
        # Normalize all counts by total
        for action, verb in product(actions, verbs):
            json_res[f"n_{action}_{verb}"] /= num_total
    
    print(json.dumps(json_res))
    return combined_dict
        
    
def stop_and_get_server_stats(controller: DMCMemcachedController): # What stats?
    controller.stop_server()
    res_dict = controller.get_server_stats()
    return res_dict


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='DMC Controller')
    parser.add_argument('workload', help='The workload name',
                        choices=workload_list)
    parser.add_argument('-s', '--num_servers',
                        type=int, default=1)
    parser.add_argument('-c', '--num_clients',
                        type=int, default=8)
    parser.add_argument('-m', '--memcached_ip',
                        type=str, default="127.0.0.1")
    parser.add_argument('-p', '--memcached_port',
                        type=int, default=11211)
    parser.add_argument('-o', '--out_fname',
                        type=str, default="{}".format(time.time()))
    parser.add_argument('-r', '--cache_ratio', type=str, default='0')

    args = parser.parse_args()

    controller = DMCMemcachedController(args.memcached_ip, args.memcached_port,
                                        args.num_servers, args.num_clients)

    res = control_bench(controller)

    # stop the server and get server results
    server_res = stop_and_get_server_stats(controller)
    res['server'] = server_res

    if (args.cache_ratio == '0'):
        with open("{}-{}-s{}-c{}.json".format(args.workload, args.out_fname, args.num_servers, args.num_clients), 'w') as f:
            json.dump(res, f)
    else:
        with open("{}-r{}-{}-s{}-c{}.json".format(args.workload, args.cache_ratio, args.out_fname, args.num_servers, args.num_clients), 'w') as f:
            json.dump(res, f)
    controller.clear_all_contents()
