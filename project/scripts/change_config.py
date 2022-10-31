import argparse
import math
import os
import random
import sys
from coojasim import Cooja, UDGMRadioMedium
import re

def main():
    p = argparse.ArgumentParser()
    p.add_argument('-i', dest='input', required=True, type=str, nargs='+')
    p.add_argument('-o', dest='output', required=True)
    # p.add_argument('-c', dest='count', type=int, default=1)
    # p.add_argument('--generated-seed', dest='generated_seed', action='store_true', default=False)
    # p.add_argument('--topology', dest='topology', default=None)
    # p.add_argument('--min-distance', dest='min_distance', type=int, default=0)
    p.add_argument('--trxr', dest='trx_ratio', type=float, default=1.0, nargs='+')
    p.add_argument('--randseed', dest='rand_seed', type=int, default=123456, nargs='+')
    
    try:
        conopts = p.parse_args()
    except Exception as e:
        sys.exit(f"Illegal arguments: {str(e)}")
    
    assert(os.path.isdir(conopts.output))
    
    import sys, glob
    if sys.platform.startswith("win") and len(conopts.input)==1 and "*" in conopts.input[0]:
        conopts.input = glob.glob(conopts.input[0])
    
    for input_path in conopts.input:
        for trx_ratio in conopts.trx_ratio:
            assert(0.0 <= trx_ratio <= 1.0)
            for rand_seed in conopts.rand_seed:

                c = Cooja(input_path)
                output_file = os.path.join(conopts.output, os.path.split(input_path)[1])
                
                # -- radio
                radio_medium = c.sim.radio_medium
                
                radio_medium.success_ratio_tx = trx_ratio
                radio_medium.success_ratio_rx = trx_ratio
                output_file = re.sub(r'-base', f'-trxr {trx_ratio}-rseed {rand_seed}-base', output_file)
                
                assert(100000 <= rand_seed <= 999999)

                c.sim.random_seed = rand_seed
                
                # --
                output_file = output_file.replace("-base", "")
                c.save(output_file)

if __name__ == "__main__":
    main()