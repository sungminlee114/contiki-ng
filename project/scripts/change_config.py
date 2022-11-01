import argparse
import os
from coojasim import Cooja
import re

RSs = [
    "123456",
    "654321",
    "123123",
    "456456",
    "102030",
    "123523",
    "734563",
    "934562",
    "261032",
    "589012",
]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('input', type=str, nargs='+')
    p.add_argument('output', type=str)
    # p.add_argument('-c', dest='count', type=int, default=1)
    # p.add_argument('--generated-seed', dest='generated_seed', action='store_true', default=False)
    # p.add_argument('--topology', dest='topology', default=None)
    # p.add_argument('--min-distance', dest='min_distance', type=int, default=0)
    p.add_argument('--trxr', dest='trx_ratio', type=float, default=1.0, nargs='+')
    p.add_argument('--rnds', dest='rand_seed', type=int, default=0)
    
    try:
        conopts = p.parse_args()
    except Exception as e:
        sys.exit(f"Illegal arguments: {str(e)}")
    
    
    if not os.path.isdir(conopts.output):
        os.mkdir(conopts.output) 
    
    import sys, glob
    if sys.platform.startswith("win") and len(conopts.input)==1 and "*" in conopts.input[0]:
        conopts.input = glob.glob(conopts.input[0])
    
    for input_path in conopts.input:
        for trx_ratio in conopts.trx_ratio:
            assert(0.0 <= trx_ratio <= 1.0)

            c = Cooja(input_path)
            output_file = os.path.join(conopts.output, os.path.split(input_path)[1])
            
            # -- radio
            radio_medium = c.sim.radio_medium
            
            radio_medium.success_ratio_tx = trx_ratio
            radio_medium.success_ratio_rx = trx_ratio
            
            # -- random seed
            assert(0 <= conopts.rand_seed <= len(RSs))
            for rs in range(conopts.rand_seed):
                c.sim.random_seed.set_seed(RSs[rs])
                
                # -- save
                _output_file = re.sub(r'-base', f'-trxr_{trx_ratio}-rs_{rs}', output_file)
                c.save(_output_file)

if __name__ == "__main__":
    main()