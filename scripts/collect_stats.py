#!/usr/bin/env python3

# To run this script: ./collect_stats.py triangle_count/ pagerank/ connected_components/ ktruss/ 
# This script assumes that ^ these are the names of your folders that contain the .mwx, logs folders, and run script

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

def get_unit(app_folder): 
    if "pagerank" in app_folder: 
        return "MFLOPS" 
    elif "ktruss" in app_folder: 
        return "GTEPS" 
    elif "components" in app_folder: 
        return "GTEPS" 
    elif "triangle" in app_folder: 
        return "GTPPS" 
    else: 
        print(f"'pagerank', 'ktruss', 'triangle', nor 'components' are in {app_folder}. Don't know unit for stats.")
        exit(1) 

def make_entry(app_folder, stats):     
    print("There is only one log folder, so have to manually create entry")
    #print(f"app_folder: {app_folder}")
    cmd = f'''ls {app_folder} | grep logs_N*'''
    #print(f"Executing command: {cmd}") 
    log_folder = subprocess.check_output(cmd, shell=True) 
    log_folder = log_folder.decode().split('\n')[0]
    full_path = app_folder + log_folder 
    #print(f"full_path: {full_path}")
    cmd = f'''ls {full_path}'''
    outfile = subprocess.check_output(cmd, shell=True) 
    outfile = outfile.decode().split('\n')[0]
    #print(f"outfile: {outfile}")  
    entry = full_path + '/' + outfile + ':' + stats[0] 
    #print(f"entry: {entry}") 
    return [entry]


def dump_stats(app_folder): 
    unit = get_unit(app_folder) 
    cmd = f'''grep -oE "[0-9]+.[0-9]+ *{unit}" {app_folder}logs*/*'''
    #print(f"Executing command: {cmd}")
    stats = subprocess.check_output(cmd, shell=True)
    stats = stats.decode().split('\n') 
    stats = stats[0:-1]
    #print(f"Stats: {stats}")
    if(len(stats)==1): 
        stats = make_entry(app_folder, stats)
    return stats 

def make_stats_row(entry):
    #print(f"entry: {entry}")
    data = {} 
    try: 
        app_folder, log_folder, outfile_stat = entry.split('/')
        #print(f"app_folder: {app_folder}, log_folder: {log_folder}, outfile_stat: {outfile_stat}")
        outfile, stat = outfile_stat.split(':')
        #print(f"outfile: {outfile}, stat: {stat}")
        app, scale, nodes_out = outfile.split('-')
        nodes = nodes_out.split('.')[0][1:]
        stat, unit = stat.split(' ')
        #print(f"nodes: {nodes}, scale: {scale}")
        data['file'] = entry.split(':')[0]
        data['app'] = app 
        data['nodes'] = nodes 
        data['scale'] = scale[1:] 
        data['stat'] = stat
        data['unit'] = unit
    except: 
        pass 
    return data 
        

# Main function
if __name__ == '__main__':
    
    num_flags = 0.5 # need at least one folder (0.5 * 2 = 1 folder) 
    # make sure num arguments is correct 
    arguments = sys.argv[1:]
    if len(arguments) < num_flags * 2: # 1 for each flag + 1 for the corresponding arg 
        usage(0)
    
    # set default vals 
    now = datetime.now().strftime("%Y_%m_%d.%H-%M-%S")
    filename = "performance_stats_" + now + ".csv" 

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
    df = pd.DataFrame(columns=['file', 'app', 'nodes', 'unit', 'scale', 'stat']) 

    # collect stats 
    for folder in arguments: 
        stats = dump_stats(folder)
        for entry in stats: 
            data = make_stats_row(entry)
            df = df.append(data, ignore_index=True)
    print(df)
    df.to_csv(f"{filename}")
