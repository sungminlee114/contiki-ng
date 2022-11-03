from msilib.schema import Feature
import matplotlib
import pandas as pd
import os
import datetime
import numpy as np

import matplotlib.pyplot as plt

class DIR:
    BASE = os.path.dirname(os.path.abspath(__file__))
    DATA = os.path.join(BASE, "data")
    RAW_DATA = os.path.join(DATA, "raw")
    ALL_DATA = os.path.join(RAW_DATA, "all")
    PROCESSED_DATA = os.path.join(DATA, "processed")
    RESULTS = os.path.join(BASE, "results")

class LOG_LEVEL:
    SILENT = 0
    VERBOSE = 1

    level = VERBOSE

    @staticmethod
    def print_if_verbose(*args):
        if LOG_LEVEL.level == LOG_LEVEL.VERBOSE:
            print(*args)

class Dataset_RPL:
    @staticmethod
    def gather_raw_data():
        raw_datas = [[os.path.join(path, name) for name in os.listdir(path)] for path in
                     [os.path.join(DIR.RAW_DATA, dir) for dir in os.listdir(DIR.RAW_DATA)]]
        raw_datas = raw_datas[0] + raw_datas[1]

        import shutil
        for raw_data in raw_datas:
            print(os.path.join(DIR.ALL_DATA, raw_data.split("\\")[-1]))
            shutil.copy(raw_data, os.path.join(DIR.ALL_DATA, raw_data.split("\\")[-1]))

    @staticmethod
    def get_meta_data_from_file_name(file_name: str):
        # format : rpl-udp-base-5-1-attack-blackhole-rpl-statistics
        _, protocol, _, n_motes, n_exp_iter, _, scenario, _, _ = file_name.split("-")

        LOG_LEVEL.print_if_verbose(protocol, n_motes, n_exp_iter, scenario)
        return protocol, n_motes, n_exp_iter, scenario

    @staticmethod
    def build_file_name_from_metadata(n_motes, n_exp_iter, scenario, protocol="udp"):
        # format : rpl-udp-base-5-1-attack-blackhole-rpl-statistics
        file_name = f"rpl-{protocol}-base-{n_motes}-{n_exp_iter}-attack-{scenario}-rpl-statistics.csv"
        LOG_LEVEL.print_if_verbose(file_name)
        return file_name


def read_a_data(data_dir, file_name):
    print(file_name)
    _, n_motes, n_exp_iter, scenario = Dataset_RPL.get_meta_data_from_file_name(file_name)
    print(n_motes, n_exp_iter, scenario)
    n_motes, n_exp_iter = int(n_motes), int(n_exp_iter)

    data_path = os.path.join(data_dir, file_name)
    df = pd.read_csv(data_path)

    # 시뮬레이션이니까 각 stage 마다 모든 mote가 돌거고
    # 때문에 각 stage마다 끊어서 print
    # grouped_df = df.groupby(df.index // n_motes)
    # for key, item in grouped_df:
    #     stage_df = grouped_df.get_group(key)
    #     print(stage_df)
    #     print("\n\n")

    return df

    # motes = df["Mote"].unique()
    # print(sorted(motes), len(motes))

# --*-- #
def saveFig(saveDir, title, dpi='figure', tight_layout=True):
    LOG_LEVEL.print_if_verbose(f"saveFig {title}")
    format = ".png"

    if tight_layout:
        plt.tight_layout()
    if DIR.RESULTS is not None:
        plt.savefig(os.path.join(saveDir, title) + format, dpi=dpi)
    else:
        plt.show()

    plt.cla()
    plt.clf()

# --*-- #

LOG_LEVEL.level = LOG_LEVEL.SILENT
# LOG_LEVEL.level = LOG_LEVEL.VERBOSE

pd.set_option('display.max_rows', None)
matplotlib.rcParams.update({'font.size': 5})
    # df.to_csv(os.path.join(DIR.RESULTS, datetime.datetime.now().strftime("%m%d%H%M%S") + ".csv"))

# data_dir = DIR.ALL_DATA
# data_dir = os.path.join(DIR.RAW_DATA, "dis_flooding")
# data_name = os.listdir(data_dir)[0]

# for n_motes in range(10, 11):
#     for n_iter in range(0, 1):
#         plot_rank_stage_per_mote(n_motes, n_iter)
# FEATURES = ["Rank", "Seq", "DIS-R", "DIS-S", "DIO-R", "DIO-S", "DAO-R", "RPL-total-sent"]
FEATURES = ["DIS-R", "DIS-S", "DIO-R", "DIO-S", "DAO-R", "RPL-total-sent"]
cmap = matplotlib.cm.get_cmap('jet')
attacker_idx = 7 - 7 - 1
ylims = [[0, 10], [0, 4], [0, 100], [0, 100], [0, 200], [0, 150]]
def plot(csv_file_path, scenario):
    print(csv_file_path ,scenario)
    df = pd.read_csv(csv_file_path)
    
    grouped_df = df.groupby("Mote")
    # grouped_df = sorted(grouped_df)
    
    keys = [4, 5, 6, 7, 8, 9, 10, 11, 2, 3]
    lines = {}
    for key in keys:
        stage_df = grouped_df.get_group(key)
        x = stage_df["Time"]
        for j, feature in enumerate(FEATURES):
            ax = plt.subplot(len(FEATURES), 1, j + 1)
            y = stage_df[feature]
            lines[key] = ax.plot(x, y, label=f"M {key}")[0]
            ax.set_ylabel(feature)
            ax.yaxis.label.set_fontsize(7)
            ax.set_ylim(ylims[j])
            
            if j == 0:
                ax.set_title(scenario)
                ax.title.set_size(10)

            if j == len(FEATURES) -1:
                ax.set_xlabel("Time(ms)")
                
    
    title = scenario
    plt.legend(handles=[lines[k] for k in sorted(keys)],
               loc="lower right", bbox_to_anchor=(1.1, 0))
    # plt.suptitle(title)
    saveFig(csv_file_path[:csv_file_path.rfind("\\")] + "/..", title+"_ymatch", dpi=500, tight_layout=False)
    
if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser()
    parser.add_argument('input')
    parser.add_argument('scenario')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.input):
        raise FileNotFoundError()
    elif os.path.isdir(args.input):
        csv_file_path = os.path.join(args.input, "rpl-statics.csv")
    else:
        csv_file_path = args.input
        
    plot(csv_file_path, args.scenario)