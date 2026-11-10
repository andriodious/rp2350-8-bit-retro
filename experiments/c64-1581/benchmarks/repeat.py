#!/usr/bin/env python3
"""Rotate native benchmark order; run alone without competing benchmarks.
Build directories must first be produced by firmware/test.py --benchmark --crt.
Native timings rank this host only; they do not predict Cortex-M33/XIP timing.
"""
from pathlib import Path
import argparse, subprocess, re, json, statistics
p=argparse.ArgumentParser()
p.add_argument('--builds',type=Path,nargs='+',required=True)
p.add_argument('--rom',type=Path,required=True)
p.add_argument('--d81',type=Path,required=True)
p.add_argument('--crt',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--rounds',type=int,default=5)
a=p.parse_args();results=[]
for run in range(a.rounds):
 offset=run%len(a.builds)
 for build in a.builds[offset:]+a.builds[:offset]:
  build=build.resolve()
  for mode,media in [('game',a.d81),('crt',a.crt)]:
   r=subprocess.run([str(build/'test'),str(a.rom.resolve()),str(media.resolve()),str(build),mode],capture_output=True,text=True,check=True)
   nums={int(f):(int(t),int(d)) for f,t,d in re.findall(r'BENCH frame=(\d+) us=(\d+) drive_us=(\d+)',r.stdout)}
   row=dict(variant=build.name,run=run,mode=mode,total_us=nums[10000][0],first_3000_us=nums[3000][0],steady_us=nums[10000][0]-nums[3000][0],steady_drive_us=nums[10000][1]-nums[3000][1])
   results.append(row);print(row,flush=True)
a.output.write_text(json.dumps(results,indent=2)+'\n')
for build in a.builds:
 for mode in ['game','crt']:
  rows=[r['steady_us'] for r in results if r['variant']==build.name and r['mode']==mode]
  print(build.name,mode,'median_us',statistics.median(rows),'range_us',min(rows),max(rows))
