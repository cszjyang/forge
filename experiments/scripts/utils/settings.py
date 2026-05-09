"""
Cluster and experiment settings.

Before running the experiment scripts, adjust the cluster settings below for your
machines and project path.
"""

FORGE_HOME = "~/forge"
user = ""
controller = "10.0.0.7"
server_list = [
    "10.0.0.1",
    # "10.0.0.3",
    "10.0.0.5",
    "10.0.0.7",
    "10.0.0.9",
    # "10.0.0.11",
]
mn_list = [
    "10.0.0.7",
    # "10.0.0.3",
    # "10.0.0.11",
]
cn_list = [s for s in server_list if s not in mn_list]

build_dir = f"{FORGE_HOME}/build"
config_dir = f"{FORGE_HOME}/experiments"
PYTHON_ENV = f"{FORGE_HOME}/.venv"
env_cmd = f"export PATH={PYTHON_ENV}/bin:$PATH"

# By default, the controller holds the authoritative project copy for rsync.
# Change this only if you keep the source tree on a different node.
RSYNC_SOURCE_HOST = controller


# Workload catalog shared by the controller and offline workload-setting tools.
ycsb_wl_list = ['ycsba', 'ycsbb', 'ycsbc', 'ycsbd', 'ycsbe']
cphy_wl_list = ['cphy01-50m', 'cphy02-50m', 'cphy03-50m', 'cphy04-50m', 'cphy05-50m',
                'cphy06-50m', 'cphy07-50m', 'cphy08-50m', 'cphy09-50m', 'cphy10-50m',
                'cphy02-100m']
wiki_wl_list = ['wiki_2016u-100m']
msr_wl_list = ['msr_hm_0', 'msr_prn_0', 'msr_prn_1', 'msr_proj_0', 'msr_proj_1',
               'msr_proj_2', 'msr_proj_4', 'msr_prxy_0', 'msr_prxy_1', 'msr_src1_0',
               'msr_src1_1', 'msr_usr_1', 'msr_usr_2', 'msr_web_2']
meta_wl_list = ['meta_kvcache_traces_1-100m', 'meta_kvcache_traces_2-100m',
                'meta_kvcache_traces_3-100m', 'meta_kvcache_traces_4-100m',
                'meta_kvcache_traces_5-100m']
twitter_10m_wl_list = ['twitter020-10m', 'twitter042-10m', 'twitter049-10m']
twitter_wl_list = ['twitter007-100m', 'twitter020-100m', 'twitter024-100m',
                   'twitter030-100m', 'twitter042-100m', 'twitter049-100m',
                   'twitter053-100m']
twitter_oG_wl_list = ['oG-twitter007-100m', 'oG-twitter020-100m',
                      'oG-twitter024-100m', 'oG-twitter030-100m',
                      'oG-twitter042-100m', 'oG-twitter049-100m',
                      'oG-twitter053-100m']
ibm_wl_list = ['ibm000-40m', 'ibm005-40m', 'ibm010-10m', 'ibm012-10m', 'ibm018-40m',
               'ibm024-10m', 'ibm026-10m', 'ibm029-10m', 'ibm031-10m', 'ibm034-10m',
               'ibm036-10m', 'ibm044-10m', 'ibm045-10m', 'ibm049-10m', 'ibm050-10m',
               'ibm058-40m', 'ibm061-10m', 'ibm063-40m', 'ibm075-40m', 'ibm083-40m',
               'ibm085-10m', 'ibm096-40m', 'ibm097-40m']

workload_list = (
    twitter_10m_wl_list + twitter_wl_list + twitter_oG_wl_list +
    ycsb_wl_list + ibm_wl_list + cphy_wl_list + msr_wl_list +
    meta_wl_list + wiki_wl_list
)

hash_bucket_assoc_setting = 8

hash_bucket_num_setting = {
    "ycsb": {
        "0.01": 171704,
        "0.05": 858521,
        "0.1": 1717043,
        "0.2": 3434086,
        "0.3": 5151129,
        "0.4": 6868172,
        "0.5": 8585216,
        "1": 17170432
    },
    "ycsba": {
        "0.01": 171704,
        "0.05": 858521,
        "0.1": 1717043,
        "0.2": 3434086,
        "0.3": 5151129,
        "0.4": 6868172,
        "0.5": 8585216,
        "1": 17170432
    },
    "ycsbb": {
        "0.01": 171704,
        "0.05": 858521,
        "0.1": 1717043,
        "0.2": 3434086,
        "0.3": 5151129,
        "0.4": 6868172,
        "0.5": 8585216,
        "1": 17170432
    },
    "ycsbc": {
        "0.01": 171704,
        "0.05": 858521,
        "0.1": 1717043,
        "0.2": 3434086,
        "0.3": 5151129,
        "0.4": 6868172,
        "0.5": 8585216,
        "1": 17170432
    },
    "ycsbd": {
        "0.01": 171704,
        "0.05": 858521,
        "0.1": 1717043,
        "0.2": 3434086,
        "0.3": 5151129,
        "0.4": 6868172,
        "0.5": 8585216,
        "1": 17170432
    },
    "ycsbe": {
        "0.01": 171704,
        "0.05": 858521,
        "0.1": 1717043,
        "0.2": 3434086,
        "0.3": 5151129,
        "0.4": 6868172,
        "0.5": 8585216,
        "1": 17170432
    },
   "twitter020-10m": {
        "0.01": 6481,
        "0.05": 32405,
        "0.1": 64810,
        "0.2": 129621,
        "0.3": 194431,
        "0.4": 259242,
        "0.5": 324052
    },
    "twitter042-10m": {
        "0.01": 2048,
        "0.05": 10242,
        "0.1": 20484,
        "0.2": 40969,
        "0.3": 61453,
        "0.4": 81938,
        "0.5": 102423
    },
    "twitter049-10m": {
        "0.01": 3076,
        "0.05": 15382,
        "0.1": 30765,
        "0.2": 61530,
        "0.3": 92296,
        "0.4": 123061,
        "0.5": 153826
    },
    # "ibm000-40m": {
    #     "0.01": 967,
    #     "0.05": 4835,
    #     "0.1": 9671,
    #     "0.2": 19343,
    #     "0.3": 29014,
    #     "0.4": 38686,
    #     "0.5": 48358
    # },
    # "ibm005-40m": {
    #     "0.01": 499,
    #     "0.05": 2495,
    #     "0.1": 4990,
    #     "0.2": 9980,
    #     "0.3": 14970,
    #     "0.4": 19960,
    #     "0.5": 24950
    # },
    "ibm010-10m": {
        "0.01": 25137,
        "0.05": 125688,
        "0.1": 251377,
        "0.2": 502754,
        "0.3": 754131,
        "0.4": 1005509,
        "0.5": 1256886
    },
    # "ibm012-10m": {
    #     "0.01": 252,
    #     "0.05": 1261,
    #     "0.1": 2523,
    #     "0.2": 5046,
    #     "0.3": 7569,
    #     "0.4": 10093,
    #     "0.5": 12616
    # },
    "ibm018-40m": {
        "0.01": 137891,
        "0.05": 689457,
        "0.1": 1378914,
        "0.2": 2757828,
        "0.3": 4136742,
        "0.4": 5515656,
        "0.5": 6894571
    },
    # "ibm024-10m": {
    #     "0.01": 24,
    #     "0.05": 121,
    #     "0.1": 243,
    #     "0.2": 487,
    #     "0.3": 731,
    #     "0.4": 975,
    #     "0.5": 1219
    # },
    # "ibm026-10m": {
    #     "0.01": 1567,
    #     "0.05": 7835,
    #     "0.1": 15670,
    #     "0.2": 31340,
    #     "0.3": 47010,
    #     "0.4": 62680,
    #     "0.5": 78350
    # },
    "ibm029-10m": {
        "0.01": 16871,
        "0.05": 84359,
        "0.1": 168718,
        "0.2": 337437,
        "0.3": 506155,
        "0.4": 674874,
        "0.5": 843592
    },
    # "ibm031-10m": {
    #     "0.01": 912,
    #     "0.05": 4562,
    #     "0.1": 9124,
    #     "0.2": 18248,
    #     "0.3": 27372,
    #     "0.4": 36496,
    #     "0.5": 45620
    # },
    "ibm034-10m": {
        "0.01": 5907,
        "0.05": 29535,
        "0.1": 59070,
        "0.2": 118141,
        "0.3": 177212,
        "0.4": 236283,
        "0.5": 295354
    },
    # "ibm036-10m": {
    #     "0.01": 1995,
    #     "0.05": 9976,
    #     "0.1": 19953,
    #     "0.2": 39907,
    #     "0.3": 59860,
    #     "0.4": 79814,
    #     "0.5": 99767
    # },
    "ibm044-10m": {
        "0.01": 5512,
        "0.05": 27564,
        "0.1": 55128,
        "0.2": 110256,
        "0.3": 165384,
        "0.4": 220512,
        "0.5": 275640
    },
    "ibm045-10m": {
        "0.01": 14120,
        "0.05": 70600,
        "0.1": 141200,
        "0.2": 282400,
        "0.3": 423601,
        "0.4": 564801,
        "0.5": 706002
    },
    "ibm049-10m": {
        "0.01": 23928,
        "0.05": 119642,
        "0.1": 239285,
        "0.2": 478570,
        "0.3": 717856,
        "0.4": 957141,
        "0.5": 1196427
    },
    "ibm050-10m": {
        "0.01": 24006,
        "0.05": 120030,
        "0.1": 240061,
        "0.2": 480123,
        "0.3": 720184,
        "0.4": 960246,
        "0.5": 1200307
    },
    "ibm058-40m": {
        "0.01": 6177,
        "0.05": 30889,
        "0.1": 61779,
        "0.2": 123559,
        "0.3": 185339,
        "0.4": 247118,
        "0.5": 308898
    },
    # "ibm061-10m": {
    #     "0.01": 15,
    #     "0.05": 75,
    #     "0.1": 151,
    #     "0.2": 302,
    #     "0.3": 453,
    #     "0.4": 605,
    #     "0.5": 756
    # },
    "ibm063-40m": {
        "0.01": 41020,
        "0.05": 205101,
        "0.1": 410203,
        "0.2": 820406,
        "0.3": 1230609,
        "0.4": 1640812,
        "0.5": 2051015
    },
    "ibm075-40m": {
        "0.01": 150478,
        "0.05": 752394,
        "0.1": 1504789,
        "0.2": 3009579,
        "0.3": 4514369,
        "0.4": 6019159,
        "0.5": 7523949
    },
    "ibm083-40m": {
        "0.01": 104825,
        "0.05": 524127,
        "0.1": 1048254,
        "0.2": 2096508,
        "0.3": 3144763,
        "0.4": 4193017,
        "0.5": 5241271
    },
    "ibm085-10m": {
        "0.01": 13913,
        "0.05": 69567,
        "0.1": 139134,
        "0.2": 278268,
        "0.3": 417402,
        "0.4": 556536,
        "0.5": 695671
    },
    "ibm096-40m": {
        "0.01": 144929,
        "0.05": 724648,
        "0.1": 1449296,
        "0.2": 2898592,
        "0.3": 4347888,
        "0.4": 5797184,
        "0.5": 7246480
    },
    # "ibm097-40m": {
    #     "0.01": 34,
    #     "0.05": 174,
    #     "0.1": 348,
    #     "0.2": 697,
    #     "0.3": 1046,
    #     "0.4": 1395,
    #     "0.5": 1744
    # },
    "cphy01-50m": {
        "0.01": 178359,
        "0.05": 891795,
        "0.1": 1783591,
        "0.2": 3567183,
        "0.3": 5350775,
        "0.4": 7134367,
        "0.5": 8917959
    },
    "cphy02-50m": {
        "0.01": 1914,
        "0.05": 19144,
        "0.1": 19144,
        "0.2": 38288,
        "0.3": 57432,
        "0.4": 76576,
        "0.5": 95720
    },
    "cphy02-100m": {
        "0.01": 23521,
        "0.05": 23521,
        "0.1": 23521,
        "0.2": 47043,
        "0.3": 70565,
        "0.4": 94086,
        "0.5": 117608
    },
    "cphy03-50m": {
        "0.01": 106193,
        "0.05": 530966,
        "0.1": 1061933,
        "0.2": 2123866,
        "0.3": 3185799,
        "0.4": 4247732,
        "0.5": 5309665
    },
    "cphy04-50m": {
        "0.01": 70799,
        "0.05": 353997,
        "0.1": 707995,
        "0.2": 1415990,
        "0.3": 2123985,
        "0.4": 2831980,
        "0.5": 3539975
    },
    "cphy05-50m": {
        "0.01": 179036,
        "0.05": 895182,
        "0.1": 1790364,
        "0.2": 3580728,
        "0.3": 5371093,
        "0.4": 7161457,
        "0.5": 8951822
    },
    "cphy06-50m": {
        "0.01": 8176,
        "0.05": 40880,
        "0.1": 81760,
        "0.2": 163520,
        "0.3": 245280,
        "0.4": 327040,
        "0.5": 408800
    },
    "cphy07-50m": {
        "0.01": 39204,
        "0.05": 196022,
        "0.1": 392044,
        "0.2": 784089,
        "0.3": 1176134,
        "0.4": 1568179,
        "0.5": 1960224
    },
    "cphy08-50m": {
        "0.01": 45200,
        "0.05": 226001,
        "0.1": 452002,
        "0.2": 904004,
        "0.3": 1356006,
        "0.4": 1808008,
        "0.5": 2260010
    },
    "cphy09-50m": {
        "0.01": 74337,
        "0.05": 371686,
        "0.1": 743373,
        "0.2": 1486747,
        "0.3": 2230120,
        "0.4": 2973494,
        "0.5": 3716867
    },
    "cphy10-50m": {
        "0.01": 62649,
        "0.05": 313245,
        "0.1": 626490,
        "0.2": 1252980,
        "0.3": 1879470,
        "0.4": 2505960,
        "0.5": 3132451
    },
    "msr_hm_0": {
        "0.01": 2195,
        "0.05": 10979,
        "0.1": 21959,
        "0.2": 43918,
        "0.3": 65878,
        "0.4": 87837,
        "0.5": 109796
    },
    "msr_prn_0": {
        "0.01": 3556,
        "0.05": 17784,
        "0.1": 35569,
        "0.2": 71138,
        "0.3": 106707,
        "0.4": 142277,
        "0.5": 177846
    },
    "msr_prn_1": {
        "0.01": 10867,
        "0.05": 54339,
        "0.1": 108678,
        "0.2": 217357,
        "0.3": 326036,
        "0.4": 434715,
        "0.5": 543393
    },
    "msr_proj_0": {
        "0.01": 1431,
        "0.05": 7155,
        "0.1": 14311,
        "0.2": 28622,
        "0.3": 42934,
        "0.4": 57245,
        "0.5": 71557
    },
    "msr_proj_1": {
        "0.01": 77260,
        "0.05": 386300,
        "0.1": 772600,
        "0.2": 1545200,
        "0.3": 2317800,
        "0.4": 3090400,
        "0.5": 3863000
    },
    "msr_proj_2": {
        "0.01": 80901,
        "0.05": 404506,
        "0.1": 809012,
        "0.2": 1618024,
        "0.3": 2427036,
        "0.4": 3236048,
        "0.5": 4045060
    },
    "msr_proj_4": {
        "0.01": 15012,
        "0.05": 75063,
        "0.1": 150126,
        "0.2": 300252,
        "0.3": 450378,
        "0.4": 600505,
        "0.5": 750631
    },
    "msr_prxy_0": {
        "0.01": 778,
        "0.05": 3892,
        "0.1": 7784,
        "0.2": 15568,
        "0.3": 23352,
        "0.4": 31136,
        "0.5": 38920
    },
    "msr_prxy_1": {
        "0.01": 1951,
        "0.05": 9755,
        "0.1": 19511,
        "0.2": 39022,
        "0.3": 58533,
        "0.4": 78045,
        "0.5": 97556
    },
    "msr_src1_0": {
        "0.01": 28296,
        "0.05": 141483,
        "0.1": 282967,
        "0.2": 565934,
        "0.3": 848901,
        "0.4": 1131868,
        "0.5": 1414835
    },
    "msr_src1_1": {
        "0.01": 30852,
        "0.05": 154264,
        "0.1": 308529,
        "0.2": 617059,
        "0.3": 925588,
        "0.4": 1234118,
        "0.5": 1542647
    },
    "msr_usr_1": {
        "0.01": 69830,
        "0.05": 349151,
        "0.1": 698302,
        "0.2": 1396605,
        "0.3": 2094908,
        "0.4": 2793211,
        "0.5": 3491514
    },
    "msr_usr_2": {
        "0.01": 36873,
        "0.05": 184368,
        "0.1": 368737,
        "0.2": 737475,
        "0.3": 1106213,
        "0.4": 1474951,
        "0.5": 1843689
    },
    "msr_web_2": {
        "0.01": 6606,
        "0.05": 33031,
        "0.1": 66063,
        "0.2": 132127,
        "0.3": 198190,
        "0.4": 264254,
        "0.5": 330317
    },
    "wiki_2016u-100m": {
        "0.01": 37115,
        "0.05": 185578,
        "0.1": 371156,
        "0.2": 742312,
        "0.3": 1113468,
        "0.4": 1484624,
        "0.5": 1855780
    },
    "meta_kvcache_traces_1-100m": {
        "0.01": 35443,
        "0.05": 177216,
        "0.1": 354433,
        "0.2": 708866,
        "0.3": 1063300,
        "0.4": 1417733,
        "0.5": 1772167
    },
    "meta_kvcache_traces_2-100m": {
        "0.01": 35519,
        "0.05": 177596,
        "0.1": 355193,
        "0.2": 710386,
        "0.3": 1065580,
        "0.4": 1420773,
        "0.5": 1775967
    },
    "meta_kvcache_traces_3-100m": {
        "0.01": 35167,
        "0.05": 175835,
        "0.1": 351670,
        "0.2": 703341,
        "0.3": 1055012,
        "0.4": 1406683,
        "0.5": 1758354
    },
    "meta_kvcache_traces_4-100m": {
        "0.01": 34545,
        "0.05": 172727,
        "0.1": 345454,
        "0.2": 690908,
        "0.3": 1036363,
        "0.4": 1381817,
        "0.5": 1727271
    },
    "meta_kvcache_traces_5-100m": {
        "0.01": 34551,
        "0.05": 172756,
        "0.1": 345512,
        "0.2": 691025,
        "0.3": 1036537,
        "0.4": 1382050,
        "0.5": 1727562
    },
    "twitter007-100m": {
        "0.01": 15650,
        "0.05": 78251,
        "0.1": 156503,
        "0.2": 313007,
        "0.3": 469511,
        "0.4": 626015,
        "0.5": 782519
    },
    "twitter020-100m": {
        "0.01": 42669,
        "0.05": 213346,
        "0.1": 426692,
        "0.2": 853385,
        "0.3": 1280077,
        "0.4": 1706770,
        "0.5": 2133463
    },
    "twitter024-100m": {
        "0.01": 8019,
        "0.05": 40096,
        "0.1": 80192,
        "0.2": 160385,
        "0.3": 240578,
        "0.4": 320771,
        "0.5": 400964
    },
    "twitter030-100m": {
        "0.01": 7103,
        "0.05": 35515,
        "0.1": 71031,
        "0.2": 142062,
        "0.3": 213093,
        "0.4": 284125,
        "0.5": 355156
    },
    "twitter042-100m": {
        "0.01": 6141,
        "0.05": 30709,
        "0.1": 61418,
        "0.2": 122836,
        "0.3": 184254,
        "0.4": 245673,
        "0.5": 307091
    },
    "twitter049-100m": {
        "0.01": 6529,
        "0.05": 32646,
        "0.1": 65292,
        "0.2": 130584,
        "0.3": 195876,
        "0.4": 261168,
        "0.5": 326460
    },
    "twitter053-100m": {
        "0.01": 18280,
        "0.05": 91402,
        "0.1": 182805,
        "0.2": 365610,
        "0.3": 548416,
        "0.4": 731221,
        "0.5": 914026
    },
    "oG-twitter007-100m": {
        "0.01": 15650,
        "0.05": 78251,
        "0.1": 156503,
        "0.2": 313007,
        "0.3": 469511,
        "0.4": 626015,
        "0.5": 782519
    },
    "oG-twitter020-100m": {
        "0.01": 42669,
        "0.05": 213346,
        "0.1": 426692,
        "0.2": 853385,
        "0.3": 1280077,
        "0.4": 1706770,
        "0.5": 2133463
    },
    "oG-twitter024-100m": {
        "0.01": 8019,
        "0.05": 40096,
        "0.1": 80192,
        "0.2": 160385,
        "0.3": 240578,
        "0.4": 320771,
        "0.5": 400964
    },
    "oG-twitter030-100m": {
        "0.01": 7103,
        "0.05": 35515,
        "0.1": 71031,
        "0.2": 142062,
        "0.3": 213093,
        "0.4": 284125,
        "0.5": 355156
    },
    "oG-twitter042-100m": {
        "0.01": 6141,
        "0.05": 30709,
        "0.1": 61418,
        "0.2": 122836,
        "0.3": 184254,
        "0.4": 245673,
        "0.5": 307091
    },
    "oG-twitter049-100m": {
        "0.01": 6529,
        "0.05": 32646,
        "0.1": 65292,
        "0.2": 130584,
        "0.3": 195876,
        "0.4": 261168,
        "0.5": 326460
    },
    "oG-twitter053-100m": {
        "0.01": 18280,
        "0.05": 91402,
        "0.1": 182805,
        "0.2": 365610,
        "0.3": 548416,
        "0.4": 731221,
        "0.5": 914026
    }
}

cache_config_cmd_map = {
    "ycsb": {
        "0.01": "python modify_config.py server_data_len=26214400 segment_size=1048576 block_size=256",
        "0.05": "python modify_config.py server_data_len=128974848 segment_size=1048576 block_size=256",
        "0.1": "python modify_config.py server_data_len=256901120 segment_size=1048576 block_size=256",
        "0.2": "python modify_config.py server_data_len=512753664 segment_size=1048576 block_size=256",
        "0.3": "python modify_config.py server_data_len=768606208 segment_size=1048576 block_size=256",
        "0.4": "python modify_config.py server_data_len=1024458752 segment_size=1048576 block_size=256",
        "0.5": "python modify_config.py server_data_len=1280311296 segment_size=1048576 block_size=256",
        "1": "python modify_config.py server_data_len=21474836480 segment_size=1048576 block_size=256"
    },
    "ycsba": {
        "0.01": "python modify_config.py server_data_len=26214400 segment_size=1048576 block_size=256",
        "0.05": "python modify_config.py server_data_len=128974848 segment_size=1048576 block_size=256",
        "0.1": "python modify_config.py server_data_len=256901120 segment_size=1048576 block_size=256",
        "0.2": "python modify_config.py server_data_len=512753664 segment_size=1048576 block_size=256",
        "0.3": "python modify_config.py server_data_len=768606208 segment_size=1048576 block_size=256",
        "0.4": "python modify_config.py server_data_len=1024458752 segment_size=1048576 block_size=256",
        "0.5": "python modify_config.py server_data_len=1280311296 segment_size=1048576 block_size=256",
        "1": "python modify_config.py server_data_len=21474836480 segment_size=1048576 block_size=256"
    },
    "ycsbb": {
        "0.01": "python modify_config.py server_data_len=26214400 segment_size=1048576 block_size=256",
        "0.05": "python modify_config.py server_data_len=128974848 segment_size=1048576 block_size=256",
        "0.1": "python modify_config.py server_data_len=256901120 segment_size=1048576 block_size=256",
        "0.2": "python modify_config.py server_data_len=512753664 segment_size=1048576 block_size=256",
        "0.3": "python modify_config.py server_data_len=768606208 segment_size=1048576 block_size=256",
        "0.4": "python modify_config.py server_data_len=1024458752 segment_size=1048576 block_size=256",
        "0.5": "python modify_config.py server_data_len=1280311296 segment_size=1048576 block_size=256",
        "1": "python modify_config.py server_data_len=21474836480 segment_size=1048576 block_size=256"
    },
    "ycsbc": {
        "0.01": "python modify_config.py server_data_len=26214400 segment_size=1048576 block_size=256",
        "0.05": "python modify_config.py server_data_len=128974848 segment_size=1048576 block_size=256",
        "0.1": "python modify_config.py server_data_len=256901120 segment_size=1048576 block_size=256",
        "0.2": "python modify_config.py server_data_len=512753664 segment_size=1048576 block_size=256",
        "0.3": "python modify_config.py server_data_len=768606208 segment_size=1048576 block_size=256",
        "0.4": "python modify_config.py server_data_len=1024458752 segment_size=1048576 block_size=256",
        "0.5": "python modify_config.py server_data_len=1280311296 segment_size=1048576 block_size=256",
        "1": "python modify_config.py server_data_len=21474836480 segment_size=1048576 block_size=256"
    },
    "ycsbd": {
        "0.01": "python modify_config.py server_data_len=26214400 segment_size=1048576 block_size=256",
        "0.05": "python modify_config.py server_data_len=128974848 segment_size=1048576 block_size=256",
        "0.1": "python modify_config.py server_data_len=256901120 segment_size=1048576 block_size=256",
        "0.2": "python modify_config.py server_data_len=512753664 segment_size=1048576 block_size=256",
        "0.3": "python modify_config.py server_data_len=768606208 segment_size=1048576 block_size=256",
        "0.4": "python modify_config.py server_data_len=1024458752 segment_size=1048576 block_size=256",
        "0.5": "python modify_config.py server_data_len=1280311296 segment_size=1048576 block_size=256",
        "1": "python modify_config.py server_data_len=21474836480 segment_size=1048576 block_size=256"
    },
    "ycsbe": {
        "0.01": "python modify_config.py server_data_len=26214400 segment_size=1048576 block_size=256",
        "0.05": "python modify_config.py server_data_len=128974848 segment_size=1048576 block_size=256",
        "0.1": "python modify_config.py server_data_len=256901120 segment_size=1048576 block_size=256",
        "0.2": "python modify_config.py server_data_len=512753664 segment_size=1048576 block_size=256",
        "0.3": "python modify_config.py server_data_len=768606208 segment_size=1048576 block_size=256",
        "0.4": "python modify_config.py server_data_len=1024458752 segment_size=1048576 block_size=256",
        "0.5": "python modify_config.py server_data_len=1280311296 segment_size=1048576 block_size=256",
        "1": "python modify_config.py server_data_len=21474836480 segment_size=1048576 block_size=256"
    },
    "twitter020-10m": {
        "0.3": "python modify_config.py server_data_len=100663296 segment_size=1572864 block_size=256",
        "0.2": "python modify_config.py server_data_len=67108864 segment_size=1048576 block_size=256",
        "0.1": "python modify_config.py server_data_len=33554432 segment_size=524288 block_size=256",
        "0.05": "python modify_config.py server_data_len=16777216 segment_size=262144 block_size=256",
    },
    "twitter042-10m": {
        "0.3": "python modify_config.py server_data_len=31457280 segment_size=491520 block_size=256",
        "0.2": "python modify_config.py server_data_len=20971520 segment_size=327680 block_size=256",
        "0.1": "python modify_config.py server_data_len=10485760 segment_size=163840 block_size=256",
        "0.05": "python modify_config.py server_data_len=5242880 segment_size=81920 block_size=256",
    },
    "twitter049-10m": {
        "0.3": "python modify_config.py server_data_len=50331648 segment_size=786432 block_size=256",
        "0.2": "python modify_config.py server_data_len=33554432 segment_size=524288 block_size=256",
        "0.1": "python modify_config.py server_data_len=16777216 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=8388608 segment_size=131072 block_size=256",
    },
    # "twitter020-10m": {
    #     "0.3": "python modify_config.py server_data_len=100663296 segment_size=1572864 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=67108864 segment_size=1048576 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=33554432 segment_size=524288 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=16777216 segment_size=262144 block_size=256",
    # },
    # "twitter042-10m": {
    #     "0.2": "python modify_config.py server_data_len=231505920 segment_size=3617280 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=115752960 segment_size=1808640 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=57884672 segment_size=904448 block_size=256",
    #     "0.01": "python modify_config.py server_data_len=11583488 segment_size=180992 block_size=256"
    # },
    # "twitter049-10m": {
    #     "0.2": "python modify_config.py server_data_len=2900688896 segment_size=45323264 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=1450344448 segment_size=22661632 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=725172224 segment_size=11330816 block_size=256",
    #     "0.01": "python modify_config.py server_data_len=145047552 segment_size=2266368 block_size=256"
    # },
    "twitter020-100m": {
        "0.3": "python modify_config.py server_data_len=1006632960 segment_size=1572864 block_size=256",
        "0.2": "python modify_config.py server_data_len=671088640 segment_size=1048576 block_size=256",
        "0.1": "python modify_config.py server_data_len=335544320 segment_size=524288 block_size=256",
        "0.05": "python modify_config.py server_data_len=167772160 segment_size=262144 block_size=256",
    },
    # "twitter020-10m-fs": {
    #     "0.3": "python modify_config.py server_data_len=100663296 segment_size=1572864 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=67108864 segment_size=1048576 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=33554432 segment_size=524288 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=16777216 segment_size=262144 block_size=256",
    # },
    # "twitter042-10m-fs": {
    #     "0.3": "python modify_config.py server_data_len=31457280 segment_size=491520 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=20971520 segment_size=327680 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=10485760 segment_size=163840 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=5242880 segment_size=81920 block_size=256",
    # },
    # "twitter049-10m-fs": {\
    #     "0.3": "python modify_config.py server_data_len=50331648 segment_size=786432 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=33554432 segment_size=524288 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=16777216 segment_size=262144 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=8388608 segment_size=131072 block_size=256",
    # },
    # "ibm000-40m": {
    #     "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.3": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.4": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
    #     "0.5": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256"
    # },
    # "ibm005-40m": {
    #     "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.3": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.4": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.5": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256"
    # },
    "ibm010-10m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.1": "python modify_config.py server_data_len=134217728 segment_size=1048576 block_size=256",
        "0.2": "python modify_config.py server_data_len=268435456 segment_size=2097152 block_size=256",
        "0.3": "python modify_config.py server_data_len=402653184 segment_size=3145728 block_size=256",
        "0.4": "python modify_config.py server_data_len=520093696 segment_size=4063232 block_size=256",
        "0.5": "python modify_config.py server_data_len=654311424 segment_size=5111808 block_size=256"
    },
    # "ibm012-10m": {
    #     "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.3": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.4": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.5": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256"
    # },
    "ibm018-40m": {
        "0.01": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.05": "python modify_config.py server_data_len=369098752 segment_size=2883584 block_size=256",
        "0.1": "python modify_config.py server_data_len=721420288 segment_size=5636096 block_size=256",
        "0.2": "python modify_config.py server_data_len=1426063360 segment_size=11141120 block_size=256",
        "0.3": "python modify_config.py server_data_len=2130706432 segment_size=16646144 block_size=256",
        "0.4": "python modify_config.py server_data_len=2835349504 segment_size=22151168 block_size=256",
        "0.5": "python modify_config.py server_data_len=3539992576 segment_size=27656192 block_size=256"
    },
    "ibm024-10m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.2": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.3": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.4": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.5": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256"
    },
    # "ibm026-10m": {
    #     "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.3": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
    #     "0.4": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
    #     "0.5": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256"
    # },
    "ibm029-10m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.1": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.2": "python modify_config.py server_data_len=184549376 segment_size=1441792 block_size=256",
        "0.3": "python modify_config.py server_data_len=268435456 segment_size=2097152 block_size=256",
        "0.4": "python modify_config.py server_data_len=352321536 segment_size=2752512 block_size=256",
        "0.5": "python modify_config.py server_data_len=436207616 segment_size=3407872 block_size=256"
    },
    # "ibm031-10m": {
    #     "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.3": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.4": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
    #     "0.5": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256"
    # },
    "ibm034-10m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.1": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.2": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.3": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.4": "python modify_config.py server_data_len=134217728 segment_size=1048576 block_size=256",
        "0.5": "python modify_config.py server_data_len=167772160 segment_size=1310720 block_size=256"
    },
    # "ibm036-10m": {
    #     "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
    #     "0.3": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
    #     "0.4": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
    #     "0.5": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256"
    # },
    "ibm044-10m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.1": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.2": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.3": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.4": "python modify_config.py server_data_len=117440512 segment_size=917504 block_size=256",
        "0.5": "python modify_config.py server_data_len=150994944 segment_size=1179648 block_size=256"
    },
    "ibm045-10m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.1": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.2": "python modify_config.py server_data_len=150994944 segment_size=1179648 block_size=256",
        "0.3": "python modify_config.py server_data_len=218103808 segment_size=1703936 block_size=256",
        "0.4": "python modify_config.py server_data_len=301989888 segment_size=2359296 block_size=256",
        "0.5": "python modify_config.py server_data_len=369098752 segment_size=2883584 block_size=256"
    },
    "ibm049-10m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.1": "python modify_config.py server_data_len=134217728 segment_size=1048576 block_size=256",
        "0.2": "python modify_config.py server_data_len=251658240 segment_size=1966080 block_size=256",
        "0.3": "python modify_config.py server_data_len=369098752 segment_size=2883584 block_size=256",
        "0.4": "python modify_config.py server_data_len=503316480 segment_size=3932160 block_size=256",
        "0.5": "python modify_config.py server_data_len=620756992 segment_size=4849664 block_size=256"
    },
    "ibm050-10m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.1": "python modify_config.py server_data_len=134217728 segment_size=1048576 block_size=256",
        "0.2": "python modify_config.py server_data_len=251658240 segment_size=1966080 block_size=256",
        "0.3": "python modify_config.py server_data_len=369098752 segment_size=2883584 block_size=256",
        "0.4": "python modify_config.py server_data_len=503316480 segment_size=3932160 block_size=256",
        "0.5": "python modify_config.py server_data_len=620756992 segment_size=4849664 block_size=256"
    },
    "ibm058-40m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.1": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.2": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.3": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.4": "python modify_config.py server_data_len=134217728 segment_size=1048576 block_size=256",
        "0.5": "python modify_config.py server_data_len=167772160 segment_size=1310720 block_size=256"
    },
    # "ibm061-10m": {
    #     "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.3": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.4": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.5": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256"
    # },
    "ibm063-40m": {
        "0.01": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=117440512 segment_size=917504 block_size=256",
        "0.1": "python modify_config.py server_data_len=218103808 segment_size=1703936 block_size=256",
        "0.2": "python modify_config.py server_data_len=436207616 segment_size=3407872 block_size=256",
        "0.3": "python modify_config.py server_data_len=637534208 segment_size=4980736 block_size=256",
        "0.4": "python modify_config.py server_data_len=855638016 segment_size=6684672 block_size=256",
        "0.5": "python modify_config.py server_data_len=1056964608 segment_size=8257536 block_size=256"
    },
    "ibm075-40m": {
        "0.01": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.05": "python modify_config.py server_data_len=385875968 segment_size=3014656 block_size=256",
        "0.1": "python modify_config.py server_data_len=771751936 segment_size=6029312 block_size=256",
        "0.2": "python modify_config.py server_data_len=1543503872 segment_size=12058624 block_size=256",
        "0.3": "python modify_config.py server_data_len=2315255808 segment_size=18087936 block_size=256",
        "0.4": "python modify_config.py server_data_len=3087007744 segment_size=24117248 block_size=256",
        "0.5": "python modify_config.py server_data_len=3858759680 segment_size=30146560 block_size=256"
    },
    "ibm083-40m": {
        "0.01": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.05": "python modify_config.py server_data_len=268435456 segment_size=2097152 block_size=256",
        "0.1": "python modify_config.py server_data_len=536870912 segment_size=4194304 block_size=256",
        "0.2": "python modify_config.py server_data_len=1073741824 segment_size=8388608 block_size=256",
        "0.3": "python modify_config.py server_data_len=1610612736 segment_size=12582912 block_size=256",
        "0.4": "python modify_config.py server_data_len=2147483648 segment_size=16777216 block_size=256",
        "0.5": "python modify_config.py server_data_len=2684354560 segment_size=20971520 block_size=256"
    },
    "ibm085-10m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.1": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.2": "python modify_config.py server_data_len=150994944 segment_size=1179648 block_size=256",
        "0.3": "python modify_config.py server_data_len=218103808 segment_size=1703936 block_size=256",
        "0.4": "python modify_config.py server_data_len=285212672 segment_size=2228224 block_size=256",
        "0.5": "python modify_config.py server_data_len=369098752 segment_size=2883584 block_size=256"
    },
    "ibm096-40m": {
        "0.01": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.05": "python modify_config.py server_data_len=385875968 segment_size=3014656 block_size=256",
        "0.1": "python modify_config.py server_data_len=754974720 segment_size=5898240 block_size=256",
        "0.2": "python modify_config.py server_data_len=1493172224 segment_size=11665408 block_size=256",
        "0.3": "python modify_config.py server_data_len=2231369728 segment_size=17432576 block_size=256",
        "0.4": "python modify_config.py server_data_len=2969567232 segment_size=23199744 block_size=256",
        "0.5": "python modify_config.py server_data_len=3724541952 segment_size=29097984 block_size=256"
    },
    # "ibm097-40m": {
    #     "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.2": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.3": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.4": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
    #     "0.5": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256"
    # },
    "cphy01-50m": {
        "0.01": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.05": "python modify_config.py server_data_len=469762048 segment_size=3670016 block_size=256",
        "0.1": "python modify_config.py server_data_len=922746880 segment_size=7208960 block_size=256",
        "0.2": "python modify_config.py server_data_len=1828716544 segment_size=14286848 block_size=256",
        "0.3": "python modify_config.py server_data_len=2751463424 segment_size=21495808 block_size=256",
        "0.4": "python modify_config.py server_data_len=3657433088 segment_size=28573696 block_size=256",
        "0.5": "python modify_config.py server_data_len=4580179968 segment_size=35782656 block_size=256"
    },
    "cphy02-50m": {
        "0.01": "python modify_config.py server_data_len=8388608 segment_size=65536 block_size=256",
        "0.05": "python modify_config.py server_data_len=8388608 segment_size=65536 block_size=256",
        "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.2": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.3": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.4": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.5": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256"
    },
    "cphy03-50m": {
        "0.01": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.05": "python modify_config.py server_data_len=285212672 segment_size=2228224 block_size=256",
        "0.1": "python modify_config.py server_data_len=553648128 segment_size=4325376 block_size=256",
        "0.2": "python modify_config.py server_data_len=1090519040 segment_size=8519680 block_size=256",
        "0.3": "python modify_config.py server_data_len=1644167168 segment_size=12845056 block_size=256",
        "0.4": "python modify_config.py server_data_len=2181038080 segment_size=17039360 block_size=256",
        "0.5": "python modify_config.py server_data_len=2734686208 segment_size=21364736 block_size=256"
    },
    "cphy04-50m": {
        "0.01": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.05": "python modify_config.py server_data_len=184549376 segment_size=1441792 block_size=256",
        "0.1": "python modify_config.py server_data_len=369098752 segment_size=2883584 block_size=256",
        "0.2": "python modify_config.py server_data_len=738197504 segment_size=5767168 block_size=256",
        "0.3": "python modify_config.py server_data_len=1090519040 segment_size=8519680 block_size=256",
        "0.4": "python modify_config.py server_data_len=1459617792 segment_size=11403264 block_size=256",
        "0.5": "python modify_config.py server_data_len=1828716544 segment_size=14286848 block_size=256"
    },
    "cphy05-50m": {
        "0.01": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.05": "python modify_config.py server_data_len=469762048 segment_size=3670016 block_size=256",
        "0.1": "python modify_config.py server_data_len=922746880 segment_size=7208960 block_size=256",
        "0.2": "python modify_config.py server_data_len=1845493760 segment_size=14417920 block_size=256",
        "0.3": "python modify_config.py server_data_len=2751463424 segment_size=21495808 block_size=256",
        "0.4": "python modify_config.py server_data_len=3674210304 segment_size=28704768 block_size=256",
        "0.5": "python modify_config.py server_data_len=4596957184 segment_size=35913728 block_size=256"
    },
    "cphy06-50m": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.1": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.2": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.3": "python modify_config.py server_data_len=134217728 segment_size=1048576 block_size=256",
        "0.4": "python modify_config.py server_data_len=167772160 segment_size=1310720 block_size=256",
        "0.5": "python modify_config.py server_data_len=218103808 segment_size=1703936 block_size=256"
    },
    "cphy07-50m": {
        "0.01": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.1": "python modify_config.py server_data_len=201326592 segment_size=1572864 block_size=256",
        "0.2": "python modify_config.py server_data_len=402653184 segment_size=3145728 block_size=256",
        "0.3": "python modify_config.py server_data_len=603979776 segment_size=4718592 block_size=256",
        "0.4": "python modify_config.py server_data_len=805306368 segment_size=6291456 block_size=256",
        "0.5": "python modify_config.py server_data_len=1006632960 segment_size=7864320 block_size=256"
    },
    "cphy08-50m": {
        "0.01": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=117440512 segment_size=917504 block_size=256",
        "0.1": "python modify_config.py server_data_len=234881024 segment_size=1835008 block_size=256",
        "0.2": "python modify_config.py server_data_len=469762048 segment_size=3670016 block_size=256",
        "0.3": "python modify_config.py server_data_len=704643072 segment_size=5505024 block_size=256",
        "0.4": "python modify_config.py server_data_len=939524096 segment_size=7340032 block_size=256",
        "0.5": "python modify_config.py server_data_len=1157627904 segment_size=9043968 block_size=256"
    },
    "cphy09-50m": {
        "0.01": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.05": "python modify_config.py server_data_len=201326592 segment_size=1572864 block_size=256",
        "0.1": "python modify_config.py server_data_len=385875968 segment_size=3014656 block_size=256",
        "0.2": "python modify_config.py server_data_len=771751936 segment_size=6029312 block_size=256",
        "0.3": "python modify_config.py server_data_len=1157627904 segment_size=9043968 block_size=256",
        "0.4": "python modify_config.py server_data_len=1526726656 segment_size=11927552 block_size=256",
        "0.5": "python modify_config.py server_data_len=1912602624 segment_size=14942208 block_size=256"
    },
    "cphy10-50m": {
        "0.01": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=167772160 segment_size=1310720 block_size=256",
        "0.1": "python modify_config.py server_data_len=335544320 segment_size=2621440 block_size=256",
        "0.2": "python modify_config.py server_data_len=654311424 segment_size=5111808 block_size=256",
        "0.3": "python modify_config.py server_data_len=973078528 segment_size=7602176 block_size=256",
        "0.4": "python modify_config.py server_data_len=1291845632 segment_size=10092544 block_size=256",
        "0.5": "python modify_config.py server_data_len=1610612736 segment_size=12582912 block_size=256"
    },
    "cphy02-100m": {
        "0.01": "python modify_config.py server_data_len=8388608 segment_size=65536 block_size=256",
        "0.05": "python modify_config.py server_data_len=8388608 segment_size=65536 block_size=256",
        "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.2": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.3": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.4": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.5": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256"
    },
    "msr_hm_0": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.2": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.3": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.4": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.5": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256"
    },
    "msr_prn_0": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.1": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.2": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.3": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.4": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.5": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256"
    },
    "msr_prn_1": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.1": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.2": "python modify_config.py server_data_len=117440512 segment_size=917504 block_size=256",
        "0.3": "python modify_config.py server_data_len=167772160 segment_size=1310720 block_size=256",
        "0.4": "python modify_config.py server_data_len=234881024 segment_size=1835008 block_size=256",
        "0.5": "python modify_config.py server_data_len=285212672 segment_size=2228224 block_size=256"
    },
    "msr_proj_0": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.2": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.3": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.4": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.5": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256"
    },
    "msr_proj_1": {
        "0.01": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.05": "python modify_config.py server_data_len=201326592 segment_size=1572864 block_size=256",
        "0.1": "python modify_config.py server_data_len=402653184 segment_size=3145728 block_size=256",
        "0.2": "python modify_config.py server_data_len=805306368 segment_size=6291456 block_size=256",
        "0.3": "python modify_config.py server_data_len=1191182336 segment_size=9306112 block_size=256",
        "0.4": "python modify_config.py server_data_len=1593835520 segment_size=12451840 block_size=256",
        "0.5": "python modify_config.py server_data_len=1979711488 segment_size=15466496 block_size=256"
    },
    "msr_proj_2": {
        "0.01": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.05": "python modify_config.py server_data_len=218103808 segment_size=1703936 block_size=256",
        "0.1": "python modify_config.py server_data_len=419430400 segment_size=3276800 block_size=256",
        "0.2": "python modify_config.py server_data_len=838860800 segment_size=6553600 block_size=256",
        "0.3": "python modify_config.py server_data_len=1258291200 segment_size=9830400 block_size=256",
        "0.4": "python modify_config.py server_data_len=1660944384 segment_size=12976128 block_size=256",
        "0.5": "python modify_config.py server_data_len=2080374784 segment_size=16252928 block_size=256"
    },
    "msr_proj_4": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.1": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.2": "python modify_config.py server_data_len=167772160 segment_size=1310720 block_size=256",
        "0.3": "python modify_config.py server_data_len=234881024 segment_size=1835008 block_size=256",
        "0.4": "python modify_config.py server_data_len=318767104 segment_size=2490368 block_size=256",
        "0.5": "python modify_config.py server_data_len=385875968 segment_size=3014656 block_size=256"
    },
    "msr_prxy_0": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.2": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.3": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.4": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.5": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256"
    },
    "msr_prxy_1": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.1": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.2": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.3": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.4": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.5": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256"
    },
    "msr_src1_0": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.1": "python modify_config.py server_data_len=150994944 segment_size=1179648 block_size=256",
        "0.2": "python modify_config.py server_data_len=301989888 segment_size=2359296 block_size=256",
        "0.3": "python modify_config.py server_data_len=436207616 segment_size=3407872 block_size=256",
        "0.4": "python modify_config.py server_data_len=587202560 segment_size=4587520 block_size=256",
        "0.5": "python modify_config.py server_data_len=738197504 segment_size=5767168 block_size=256"
    },
    "msr_src1_1": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.1": "python modify_config.py server_data_len=167772160 segment_size=1310720 block_size=256",
        "0.2": "python modify_config.py server_data_len=318767104 segment_size=2490368 block_size=256",
        "0.3": "python modify_config.py server_data_len=486539264 segment_size=3801088 block_size=256",
        "0.4": "python modify_config.py server_data_len=637534208 segment_size=4980736 block_size=256",
        "0.5": "python modify_config.py server_data_len=805306368 segment_size=6291456 block_size=256"
    },
    "msr_usr_1": {
        "0.01": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.05": "python modify_config.py server_data_len=184549376 segment_size=1441792 block_size=256",
        "0.1": "python modify_config.py server_data_len=369098752 segment_size=2883584 block_size=256",
        "0.2": "python modify_config.py server_data_len=721420288 segment_size=5636096 block_size=256",
        "0.3": "python modify_config.py server_data_len=1073741824 segment_size=8388608 block_size=256",
        "0.4": "python modify_config.py server_data_len=1442840576 segment_size=11272192 block_size=256",
        "0.5": "python modify_config.py server_data_len=1795162112 segment_size=14024704 block_size=256"
    },
    "msr_usr_2": {
        "0.01": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.1": "python modify_config.py server_data_len=201326592 segment_size=1572864 block_size=256",
        "0.2": "python modify_config.py server_data_len=385875968 segment_size=3014656 block_size=256",
        "0.3": "python modify_config.py server_data_len=570425344 segment_size=4456448 block_size=256",
        "0.4": "python modify_config.py server_data_len=771751936 segment_size=6029312 block_size=256",
        "0.5": "python modify_config.py server_data_len=956301312 segment_size=7471104 block_size=256"
    },
    "msr_web_2": {
        "0.01": "python modify_config.py server_data_len=16777216 segment_size=131072 block_size=256",
        "0.05": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.1": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.2": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.3": "python modify_config.py server_data_len=117440512 segment_size=917504 block_size=256",
        "0.4": "python modify_config.py server_data_len=150994944 segment_size=1179648 block_size=256",
        "0.5": "python modify_config.py server_data_len=184549376 segment_size=1441792 block_size=256"
    },
    "wiki_2016u-100m": {
        "0.01": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.1": "python modify_config.py server_data_len=201326592 segment_size=1572864 block_size=256",
        "0.2": "python modify_config.py server_data_len=385875968 segment_size=3014656 block_size=256",
        "0.3": "python modify_config.py server_data_len=570425344 segment_size=4456448 block_size=256",
        "0.4": "python modify_config.py server_data_len=771751936 segment_size=6029312 block_size=256",
        "0.5": "python modify_config.py server_data_len=956301312 segment_size=7471104 block_size=256"
    },
    "meta_kvcache_traces_1-100m": {
        "0.01": "python modify_config.py server_data_len=20971520 segment_size=327680 block_size=256",
        "0.05": "python modify_config.py server_data_len=92274688 segment_size=1441792 block_size=256",
        "0.1": "python modify_config.py server_data_len=184549376 segment_size=2883584 block_size=256",
        "0.2": "python modify_config.py server_data_len=364904448 segment_size=5701632 block_size=256",
        "0.3": "python modify_config.py server_data_len=545259520 segment_size=8519680 block_size=256",
        "0.4": "python modify_config.py server_data_len=729808896 segment_size=11403264 block_size=256",
        "0.5": "python modify_config.py server_data_len=910163968 segment_size=14221312 block_size=256"
    },
    "meta_kvcache_traces_2-100m": {
        "0.01": "python modify_config.py server_data_len=20971520 segment_size=327680 block_size=256",
        "0.05": "python modify_config.py server_data_len=92274688 segment_size=1441792 block_size=256",
        "0.1": "python modify_config.py server_data_len=184549376 segment_size=2883584 block_size=256",
        "0.2": "python modify_config.py server_data_len=364904448 segment_size=5701632 block_size=256",
        "0.3": "python modify_config.py server_data_len=549453824 segment_size=8585216 block_size=256",
        "0.4": "python modify_config.py server_data_len=729808896 segment_size=11403264 block_size=256",
        "0.5": "python modify_config.py server_data_len=910163968 segment_size=14221312 block_size=256"
    },
    "meta_kvcache_traces_3-100m": {
        "0.01": "python modify_config.py server_data_len=20971520 segment_size=327680 block_size=256",
        "0.05": "python modify_config.py server_data_len=92274688 segment_size=1441792 block_size=256",
        "0.1": "python modify_config.py server_data_len=180355072 segment_size=2818048 block_size=256",
        "0.2": "python modify_config.py server_data_len=360710144 segment_size=5636096 block_size=256",
        "0.3": "python modify_config.py server_data_len=541065216 segment_size=8454144 block_size=256",
        "0.4": "python modify_config.py server_data_len=721420288 segment_size=11272192 block_size=256",
        "0.5": "python modify_config.py server_data_len=901775360 segment_size=14090240 block_size=256"
    },
    "meta_kvcache_traces_4-100m": {
        "0.01": "python modify_config.py server_data_len=20971520 segment_size=327680 block_size=256",
        "0.05": "python modify_config.py server_data_len=92274688 segment_size=1441792 block_size=256",
        "0.1": "python modify_config.py server_data_len=180355072 segment_size=2818048 block_size=256",
        "0.2": "python modify_config.py server_data_len=356515840 segment_size=5570560 block_size=256",
        "0.3": "python modify_config.py server_data_len=532676608 segment_size=8323072 block_size=256",
        "0.4": "python modify_config.py server_data_len=708837376 segment_size=11075584 block_size=256",
        "0.5": "python modify_config.py server_data_len=884998144 segment_size=13828096 block_size=256"
    },
    "meta_kvcache_traces_5-100m": {
        "0.01": "python modify_config.py server_data_len=20971520 segment_size=327680 block_size=256",
        "0.05": "python modify_config.py server_data_len=92274688 segment_size=1441792 block_size=256",
        "0.1": "python modify_config.py server_data_len=180355072 segment_size=2818048 block_size=256",
        "0.2": "python modify_config.py server_data_len=356515840 segment_size=5570560 block_size=256",
        "0.3": "python modify_config.py server_data_len=532676608 segment_size=8323072 block_size=256",
        "0.4": "python modify_config.py server_data_len=708837376 segment_size=11075584 block_size=256",
        "0.5": "python modify_config.py server_data_len=884998144 segment_size=13828096 block_size=256"
    },
    "meta_kvcache_traces_1-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.05": "python modify_config.py server_data_len=268435456 segment_size=2097152 block_size=256",
        "0.1": "python modify_config.py server_data_len=536870912 segment_size=4194304 block_size=256",
        "0.2": "python modify_config.py server_data_len=1040187392 segment_size=8126464 block_size=256",
        "0.3": "python modify_config.py server_data_len=1577058304 segment_size=12320768 block_size=256",
        "0.4": "python modify_config.py server_data_len=2080374784 segment_size=16252928 block_size=256",
        "0.5": "python modify_config.py server_data_len=2617245696 segment_size=20447232 block_size=256"
    },
    "meta_kvcache_traces_2-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.05": "python modify_config.py server_data_len=268435456 segment_size=2097152 block_size=256",
        "0.1": "python modify_config.py server_data_len=536870912 segment_size=4194304 block_size=256",
        "0.2": "python modify_config.py server_data_len=1040187392 segment_size=8126464 block_size=256",
        "0.3": "python modify_config.py server_data_len=1577058304 segment_size=12320768 block_size=256",
        "0.4": "python modify_config.py server_data_len=2080374784 segment_size=16252928 block_size=256",
        "0.5": "python modify_config.py server_data_len=2583691264 segment_size=20185088 block_size=256"
    },
    "meta_kvcache_traces_3-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.05": "python modify_config.py server_data_len=268435456 segment_size=2097152 block_size=256",
        "0.1": "python modify_config.py server_data_len=536870912 segment_size=4194304 block_size=256",
        "0.2": "python modify_config.py server_data_len=1040187392 segment_size=8126464 block_size=256",
        "0.3": "python modify_config.py server_data_len=1577058304 segment_size=12320768 block_size=256",
        "0.4": "python modify_config.py server_data_len=2080374784 segment_size=16252928 block_size=256",
        "0.5": "python modify_config.py server_data_len=2583691264 segment_size=20185088 block_size=256"
    },
    "meta_kvcache_traces_4-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.05": "python modify_config.py server_data_len=268435456 segment_size=2097152 block_size=256",
        "0.1": "python modify_config.py server_data_len=536870912 segment_size=4194304 block_size=256",
        "0.2": "python modify_config.py server_data_len=1040187392 segment_size=8126464 block_size=256",
        "0.3": "python modify_config.py server_data_len=1543503872 segment_size=12058624 block_size=256",
        "0.4": "python modify_config.py server_data_len=2046820352 segment_size=15990784 block_size=256",
        "0.5": "python modify_config.py server_data_len=2550136832 segment_size=19922944 block_size=256"
    },
    "meta_kvcache_traces_5-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.05": "python modify_config.py server_data_len=268435456 segment_size=2097152 block_size=256",
        "0.1": "python modify_config.py server_data_len=536870912 segment_size=4194304 block_size=256",
        "0.2": "python modify_config.py server_data_len=1040187392 segment_size=8126464 block_size=256",
        "0.3": "python modify_config.py server_data_len=1543503872 segment_size=12058624 block_size=256",
        "0.4": "python modify_config.py server_data_len=2046820352 segment_size=15990784 block_size=256",
        "0.5": "python modify_config.py server_data_len=2550136832 segment_size=19922944 block_size=256"
    },
    "twitter007-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=134217728 segment_size=1048576 block_size=256",
        "0.05": "python modify_config.py server_data_len=402653184 segment_size=3145728 block_size=256",
        "0.1": "python modify_config.py server_data_len=805306368 segment_size=6291456 block_size=256",
        "0.2": "python modify_config.py server_data_len=1476395008 segment_size=11534336 block_size=256",
        "0.3": "python modify_config.py server_data_len=2147483648 segment_size=16777216 block_size=256",
        "0.4": "python modify_config.py server_data_len=2818572288 segment_size=22020096 block_size=256",
        "0.5": "python modify_config.py server_data_len=3489660928 segment_size=27262976 block_size=256"
    },
    "twitter020-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=117440512 segment_size=917504 block_size=256",
        "0.1": "python modify_config.py server_data_len=234881024 segment_size=1835008 block_size=256",
        "0.2": "python modify_config.py server_data_len=452984832 segment_size=3538944 block_size=256",
        "0.3": "python modify_config.py server_data_len=671088640 segment_size=5242880 block_size=256",
        "0.4": "python modify_config.py server_data_len=889192448 segment_size=6946816 block_size=256",
        "0.5": "python modify_config.py server_data_len=1107296256 segment_size=8650752 block_size=256"
    },
    "twitter024-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.05": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.1": "python modify_config.py server_data_len=201326592 segment_size=1572864 block_size=256",
        "0.2": "python modify_config.py server_data_len=352321536 segment_size=2752512 block_size=256",
        "0.3": "python modify_config.py server_data_len=503316480 segment_size=3932160 block_size=256",
        "0.4": "python modify_config.py server_data_len=654311424 segment_size=5111808 block_size=256",
        "0.5": "python modify_config.py server_data_len=805306368 segment_size=6291456 block_size=256"
    },
    "twitter030-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.05": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.1": "python modify_config.py server_data_len=150994944 segment_size=1179648 block_size=256",
        "0.2": "python modify_config.py server_data_len=301989888 segment_size=2359296 block_size=256",
        "0.3": "python modify_config.py server_data_len=452984832 segment_size=3538944 block_size=256",
        "0.4": "python modify_config.py server_data_len=603979776 segment_size=4718592 block_size=256",
        "0.5": "python modify_config.py server_data_len=704643072 segment_size=5505024 block_size=256"
    },
    "twitter042-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=83886080 segment_size=655360 block_size=256",
        "0.05": "python modify_config.py server_data_len=167772160 segment_size=1310720 block_size=256",
        "0.1": "python modify_config.py server_data_len=251658240 segment_size=1966080 block_size=256",
        "0.2": "python modify_config.py server_data_len=419430400 segment_size=3276800 block_size=256",
        "0.3": "python modify_config.py server_data_len=587202560 segment_size=4587520 block_size=256",
        "0.4": "python modify_config.py server_data_len=754974720 segment_size=5898240 block_size=256",
        "0.5": "python modify_config.py server_data_len=922746880 segment_size=7208960 block_size=256"
    },
    "twitter049-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=318767104 segment_size=2490368 block_size=256",
        "0.05": "python modify_config.py server_data_len=637534208 segment_size=4980736 block_size=256",
        "0.1": "python modify_config.py server_data_len=956301312 segment_size=7471104 block_size=256",
        "0.2": "python modify_config.py server_data_len=1593835520 segment_size=12451840 block_size=256",
        "0.3": "python modify_config.py server_data_len=2231369728 segment_size=17432576 block_size=256",
        "0.4": "python modify_config.py server_data_len=2868903936 segment_size=22413312 block_size=256",
        "0.5": "python modify_config.py server_data_len=3506438144 segment_size=27394048 block_size=256"
    },
    "twitter053-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=201326592 segment_size=1572864 block_size=256",
        "0.05": "python modify_config.py server_data_len=603979776 segment_size=4718592 block_size=256",
        "0.1": "python modify_config.py server_data_len=1207959552 segment_size=9437184 block_size=256",
        "0.2": "python modify_config.py server_data_len=2415919104 segment_size=18874368 block_size=256",
        "0.3": "python modify_config.py server_data_len=3623878656 segment_size=28311552 block_size=256",
        "0.4": "python modify_config.py server_data_len=4831838208 segment_size=37748736 block_size=256",
        "0.5": "python modify_config.py server_data_len=6039797760 segment_size=47185920 block_size=256"
    },
    "oG-twitter007-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.1": "python modify_config.py server_data_len=134217728 segment_size=1048576 block_size=256",
        "0.2": "python modify_config.py server_data_len=268435456 segment_size=2097152 block_size=256",
        "0.3": "python modify_config.py server_data_len=402653184 segment_size=3145728 block_size=256",
        "0.4": "python modify_config.py server_data_len=536870912 segment_size=4194304 block_size=256",
        "0.5": "python modify_config.py server_data_len=671088640 segment_size=5242880 block_size=256"
    },
    "oG-twitter020-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=117440512 segment_size=917504 block_size=256",
        "0.1": "python modify_config.py server_data_len=234881024 segment_size=1835008 block_size=256",
        "0.2": "python modify_config.py server_data_len=452984832 segment_size=3538944 block_size=256",
        "0.3": "python modify_config.py server_data_len=671088640 segment_size=5242880 block_size=256",
        "0.4": "python modify_config.py server_data_len=889192448 segment_size=6946816 block_size=256",
        "0.5": "python modify_config.py server_data_len=1107296256 segment_size=8650752 block_size=256"
    },
    "oG-twitter024-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.05": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.1": "python modify_config.py server_data_len=150994944 segment_size=1179648 block_size=256",
        "0.2": "python modify_config.py server_data_len=301989888 segment_size=2359296 block_size=256",
        "0.3": "python modify_config.py server_data_len=402653184 segment_size=3145728 block_size=256",
        "0.4": "python modify_config.py server_data_len=553648128 segment_size=4325376 block_size=256",
        "0.5": "python modify_config.py server_data_len=654311424 segment_size=5111808 block_size=256"
    },
    "oG-twitter030-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=33554432 segment_size=262144 block_size=256",
        "0.05": "python modify_config.py server_data_len=67108864 segment_size=524288 block_size=256",
        "0.1": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.2": "python modify_config.py server_data_len=167772160 segment_size=1310720 block_size=256",
        "0.3": "python modify_config.py server_data_len=234881024 segment_size=1835008 block_size=256",
        "0.4": "python modify_config.py server_data_len=301989888 segment_size=2359296 block_size=256",
        "0.5": "python modify_config.py server_data_len=402653184 segment_size=3145728 block_size=256"
    },
    "oG-twitter042-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.05": "python modify_config.py server_data_len=50331648 segment_size=393216 block_size=256",
        "0.1": "python modify_config.py server_data_len=100663296 segment_size=786432 block_size=256",
        "0.2": "python modify_config.py server_data_len=201326592 segment_size=1572864 block_size=256",
        "0.3": "python modify_config.py server_data_len=251658240 segment_size=1966080 block_size=256",
        "0.4": "python modify_config.py server_data_len=352321536 segment_size=2752512 block_size=256",
        "0.5": "python modify_config.py server_data_len=402653184 segment_size=3145728 block_size=256"
    },
    "oG-twitter049-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=285212672 segment_size=2228224 block_size=256",
        "0.05": "python modify_config.py server_data_len=570425344 segment_size=4456448 block_size=256",
        "0.1": "python modify_config.py server_data_len=855638016 segment_size=6684672 block_size=256",
        "0.2": "python modify_config.py server_data_len=1426063360 segment_size=11141120 block_size=256",
        "0.3": "python modify_config.py server_data_len=1996488704 segment_size=15597568 block_size=256",
        "0.4": "python modify_config.py server_data_len=2566914048 segment_size=20054016 block_size=256",
        "0.5": "python modify_config.py server_data_len=3137339392 segment_size=24510464 block_size=256"
    },
    "oG-twitter053-100m-real_size": {
        "0.01": "python modify_config.py server_data_len=184549376 segment_size=1441792 block_size=256",
        "0.05": "python modify_config.py server_data_len=553648128 segment_size=4325376 block_size=256",
        "0.1": "python modify_config.py server_data_len=1107296256 segment_size=8650752 block_size=256",
        "0.2": "python modify_config.py server_data_len=2214592512 segment_size=17301504 block_size=256",
        "0.3": "python modify_config.py server_data_len=3321888768 segment_size=25952256 block_size=256",
        "0.4": "python modify_config.py server_data_len=4429185024 segment_size=34603008 block_size=256",
        "0.5": "python modify_config.py server_data_len=5536481280 segment_size=43253760 block_size=256"
    }
}

mean_kv_size_map = {
    "meta_kvcache_traces_1-100m": 512,
    "meta_kvcache_traces_2-100m": 512,
    "meta_kvcache_traces_3-100m": 512,
    "meta_kvcache_traces_4-100m": 512,
    "meta_kvcache_traces_5-100m": 512,
    "twitter007-100m": 2048,
    "twitter020-100m": 256,
    "twitter024-100m": 768,
    "twitter030-100m": 768,
    "twitter042-100m": 1280,
    "twitter049-100m": 4864,
    "twitter053-100m": 3072,
    "oG-twitter007-100m": 512,
    "oG-twitter020-100m": 256,
    "oG-twitter024-100m": 768,
    "oG-twitter030-100m": 512,
    "oG-twitter042-100m": 768,
    "oG-twitter049-100m": 4352,
    "oG-twitter053-100m": 2816
}
    

cmake_templates = {
    'penalty_us': '-Dpenalty_us={}',
    'hash_bucket_assoc': '-Dhash_bucket_assoc={}',
    'hash_bucket_num': '-Dhash_bucket_num={}',
    'max_group_size': '-Dmax_group_size={}',
    'group_bytes': '-Dgroup_bytes={}',
    'eva_real_size': '-Deva_real_size={}',
    'num_thread_per_smm': '-Dnum_thread_per_smm={}',
    'alloc_retry': '-Dalloc_retry={}',
    'max_sample_num': '-Dmax_sample_num={}',
    'num_cn': '-Dnum_cn={}',
    'num_mn': '-Dnum_mn={}',
}

running_default_opt = {
    'penalty_us': 0,
    'max_group_size': 64,
    'eva_real_size': 'OFF',
    # If eviction by a single SMM master per CN becomes a bottleneck,
    # lower num_thread_per_smm to create more SMM masters per CN.
    'num_thread_per_smm': 128,
    'alloc_retry': 1024,
    'max_sample_num': 32,
    'num_cn': 1,
    'num_mn': 1,
}

def get_method_opts(method):
    if method in ['forge']:
        return {}
    raise ValueError(f"Unsupported method {method}; this artifact builds FORGE only")


def get_workload_opts(wl, size):
    hash_bucket_assoc = hash_bucket_assoc_setting
    hash_bucket_num = hash_bucket_num_setting[wl][size]
    return {
        'hash_bucket_assoc': f'{hash_bucket_assoc}',
        'hash_bucket_num': f'{hash_bucket_num}'
    }


def get_cmake_cmd(method, wl, size, running_opts):
    method_opts = get_method_opts(method)
    workload_opts = get_workload_opts(wl, size)
    running_opts = running_opts

    cmake_opt_list = []
    for opt in method_opts:
        cmake_opt_list.append(cmake_templates[opt].format(method_opts[opt]))
    for opt in workload_opts:
        cmake_opt_list.append(cmake_templates[opt].format(workload_opts[opt]))
    for opt in running_opts:
        cmake_opt_list.append(cmake_templates[opt].format(running_opts[opt]))
    cmake_opt_str = ' '.join(cmake_opt_list)
    return f"cmake .. {cmake_opt_str}"


def get_make_cmd(build_dir, method, wl, size, running_opts):
    cmake_cmd = get_cmake_cmd(method, wl, size, running_opts) + ' -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G "Unix Makefiles"'
    return f"rm -rf {build_dir} && mkdir -p {build_dir} && cd {build_dir} && {cmake_cmd} && make clean && make -j 8"  # Rerun cmake to update the Makefile. Use new Makefile to compile.


def get_cache_config_cmd(config_dir, wl, size):
    return f"cd {config_dir} && {cache_config_cmd_map[wl][size]}"


def get_mn_cpu_cmd(config_dir, num_cpu):
    assert (num_cpu > 0)
    return f"cd {config_dir} && python modify_config.py num_server_threads={num_cpu}"
