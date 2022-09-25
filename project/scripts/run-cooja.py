#!/usr/bin/env python3

import argparse
import shutil
import sys
import os
import time
import traceback
import subprocess
from subprocess import PIPE, STDOUT, CalledProcessError

# Find path to this script
SELF_PATH = os.path.dirname(os.path.abspath(__file__))
# Find path to Contiki-NG relative to this script
CONTIKI_PATH = os.path.dirname(os.path.dirname(SELF_PATH))

cooja_jar = os.path.normpath(os.path.join(CONTIKI_PATH, "tools", "cooja", "dist", "cooja.jar"))
cooja_output = 'COOJA.testlog'
cooja_log = 'COOJA.log'


#######################################################
# Run a child process and get its output

def _run_command(command):
    try:
        proc = subprocess.run(command, stdout=PIPE, stderr=STDOUT, shell=True, universal_newlines=True)
        return proc.returncode, proc.stdout if proc.stdout else ''
    except CalledProcessError as e:
        print(f"Command failed: {e}", file=sys.stderr)
        return e.returncode, e.stdout if e.stdout else ''
    except (OSError, Exception) as e:
        traceback.print_exc()
        return -1, str(e)


def _remove_file(filename):
    try:
        os.remove(filename)
    except FileNotFoundError:
        pass


#############################################################
# Run a single instance of Cooja on a given simulation script

def run_simulation(cooja_file, output_path=None):
    # Remove any old simulation logs
    _remove_file(cooja_output)
    _remove_file(cooja_log)

    target_basename = cooja_file
    if target_basename.endswith('.csc.gz'):
        target_basename = target_basename[:-7]
    elif target_basename.endswith('.csc'):
        target_basename = target_basename[:-4]
    simulation_id = str(round(time.time() * 1000))
    if output_path is not None:
        target_basename = os.path.join(output_path, os.path.split(target_basename)[1])
    
    target_basename = os.path.abspath(target_basename)
    target_basename += '-dt-' + simulation_id
    target_basename_fail = target_basename + '-fail'
    target_event_output = target_basename + '/events.log'
    target_log_output = target_basename + '/cooja.log'

    # filename = os.path.join(SELF_PATH, cooja_file)
    # command = (f"java -Djava.awt.headless=true -jar {cooja_jar} -nogui={cooja_file} -contiki={CONTIKI_PATH}"
    #            f" -datatrace={target_basename}")
    # print(command)
    
    command = " ".join([
        "ant",
        "run_nogui",
        f"-Dargs={os.path.abspath(cooja_file)}",
        f"-Ddatatrace={target_basename}",
        "-buildfile",
        "/home/user/contiki-ng/tools/cooja/build.xml",
    ])
    
    sys.stdout.write(f"  Running Cooja:\n    {command}\n")

    start_time = time.time()
    (return_code, output) = _run_command(command)
    end_time = time.time()
    with open(cooja_log, 'a') as f:
        f.write(f'\nSimulation execution time: {end_time - start_time} s.\n')

    if not os.path.isdir(target_basename):
        os.mkdir(target_basename)
    
    has_cooja_output = True
    # has_cooja_output = os.path.isfile(cooja_output)
    # if has_cooja_output:
    #     os.rename(cooja_output, target_output)
    
    os.rename(cooja_log, target_log_output)

    if return_code != 0 or not os.path.exists(target_event_output):
        print(f"Failed, ret code={return_code}, output:", file=sys.stderr)
        print("-----", file=sys.stderr)
        print(output, file=sys.stderr, end='')
        print("-----", file=sys.stderr)
        if not has_cooja_output:
            print("No Cooja simulation script output!", file=sys.stderr)
        # os.rename(target_basename, target_basename_fail)
        shutil.rmtree(target_basename)
        return False

    # print("  Checking for output...")

    # is_done = False
    # with open(target_output, "r") as f:
    #     for line in f.readlines():
    #         line = line.strip()
    #         if line == "TEST OK":
    #             is_done = True
    #             continue

    # if not is_done:
    #     print("  test failed.")
    #     os.rename(target_basename, target_basename_fail)
    #     return False

    print(f"  test done in {round((end_time - start_time) / 1000000)} milliseconds.")
    return True


#######################################################
# Run the application

def main(parser=None):
    if not os.access(cooja_jar, os.R_OK):
        sys.exit(f'The file "{cooja_jar}" does not exist, did you build Cooja?')

    if not parser:
        parser = argparse.ArgumentParser()
    parser.add_argument('-o', dest='output_path')
    parser.add_argument('input', nargs='+')
    try:
        conopts = parser.parse_args(sys.argv[1:])
    except Exception as e:
        sys.exit(f"Illegal arguments: {e}")

    if conopts.output_path and not os.path.isdir(conopts.output_path):
        os.mkdir(conopts.output_path)

    os.environ['JAVA_HOME'] = "/usr/lib/jvm/java-11-openjdk-i386"

    for simulation_file in conopts.input:
        if not os.access(simulation_file, os.R_OK):
            print(f'Can not read simulation script "{simulation_file}"', file=sys.stderr)
            sys.exit(1)

        print(f'Running simulation "{simulation_file}"')
        while not run_simulation(simulation_file, conopts.output_path):
            pass
            # sys.exit(f'Failed to run simulation "{simulation_file}"')

    print('Done. No more simulation files specified.')


#######################################################

if __name__ == '__main__':
    main()
