#!/usr/bin/env python3

# To run this script: ./collect_combined_stats.py 
# This script assumes that you are running it inside of the combined/ folder which contains all of the logs folders 

import sys
import os
from datetime import datetime 
import pandas as pd 
import re 
import subprocess 

def usage(exitcode=0): 
    progname = os.path.basename(sys.argv[0])
    print(f'''Usage: {progname} [flags] [folders]
    ''')
    sys.exit(exitcode)

def create_statfile(statfile): 
    cmd = f'''./get_combined_stats.sh > {statfile}'''
    subprocess.check_output(cmd, shell=True)

def read_statfile(df, statfile): 
    statfile_lines = open(statfile, 'r')
    for line in statfile_lines.readlines(): 
        line = line.rstrip() 
        if line.startswith("logs"): 
            log_filename = line 
            nodes = line.split('_')[1][1:]
            scale = line.split('-')[3][1:]
        elif line.startswith("Mean"): 
            pass 
        elif len(line) < 1: 
            pass
        else: 
            data = {} 
            data['file'] = log_filename 
            data['nodes'] = nodes 
            data['scale'] = scale
            data = add_app_info(line, data)
            df = df.append(data, ignore_index=True)
    return df 


def add_app_info(line, data): 
    data['app'] = line.split(':')[0].strip() 
    data['mean'] = line.split(':')[1].split('+')[0].strip()
    data['mean_tolerance'] = line.split('-')[1].lstrip().split(' ')[0]
    data['min'] = line.split('/')[2].split(' ')[1]
    data['max'] = line.split('/')[3].split(' ')[0]
    data['unit'] = line.split(' ')[-1]
    return data 

# Main function
if __name__ == '__main__':
    
    num_flags = 0 
    # make sure num arguments is correct 
    arguments = sys.argv[1:]
    if len(arguments) < num_flags * 2: # 1 for each flag + 1 for the corresponding arg 
        usage(0)
    
    # set default vals 
    now = datetime.now().strftime("%Y_%m_%d.%H-%M-%S")
    filename = "combined_performance_stats_" + now + ".csv" 
    intermediate_statfile = "intermediate_statfile_" + now + ".txt" 
    
    # aggregate stats into one file 
    create_statfile(intermediate_statfile) # TODO: add an error check 

    # command line parsing 
    while arguments and arguments[0].startswith('-'):
        argument = arguments.pop(0)
        if argument == '-h':
            usage(0)
        else:
            usage(1) 
    # Tell user what's going on 
    print(f"Folders: {arguments}") 
    print(f"Updating file: {filename}")
    
    # create data frame 
    df = pd.DataFrame(columns=['file', 'app', 'nodes', 'unit', 'scale', 'mean', 'mean_tolerance', 'min', 'max']) 
    
    # read through stat file 
    df = read_statfile(df, intermediate_statfile)
    print(df)
    df.to_csv(f"{filename}")
    
