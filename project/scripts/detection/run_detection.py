import argparse
import glob
import os
import pandas as pd

par = argparse.ArgumentParser()
par.add_argument("input")
args = par.parse_args()

import sys
if sys.platform.startswith("win") and "*" in args.input:
    input_paths = glob.glob(args.input)

if not isinstance(input_paths, list):
    input_paths = [i for i in input_paths]
input_paths = [i for i in input_paths if os.path.isdir(i)]


for input_path in input_paths:
    if not os.path.exists(input_path):
        raise FileNotFoundError()
    elif os.path.isdir(input_path):
        csv_file_path = os.path.join(input_path, "rpl-statistics.csv")
    else:
        csv_file_path = input_path
    
    df = pd.read_csv(csv_file_path)
    
    print(df)
