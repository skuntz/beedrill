#!/bin/bash 

line=152 # summary start line 
# 166 for 8 nodes 
# 152 for 1 node 

grep "0=0x0" logs*/* | cut -d : -f 1 > valid_files

while IFS= read -r string 
do 
	echo "$string" 
	cat $string | tail -n +$line | head -n 5
	echo " " 	
done < valid_files 

