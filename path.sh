#!/bin/bash

# Creation of  an associative array
declare -A PATHS

# Put values into the array  
# key = name of benchmark
# value = path to benchmark

#SPEC 
PATHS[perlbench_base.i386]=/home/lupones/benchmarks/spec2006/400.perlbench
PATHS[bzip2_base.i386]=/home/lupones/benchmarks/spec2006/401.bzip2
PATHS[gcc_base.i386]=/home/lupones/benchmarks/spec2006/403.gcc
PATHS[bwaves_base.i386]=/home/lupones/benchmarks/spec2006/410.bwaves
PATHS[gamess_base.i386]=/home/lupones/benchmarks/spec2006/416.gamess
PATHS[mcf_base.i386]=/home/lupones/benchmarks/spec2006/429.mcf
PATHS[milc_base.i386]=/home/lupones/benchmarks/spec2006/433.milc
PATHS[zeusmp_base.i386]=/home/lupones/benchmarks/spec2006/434.zeusmp
PATHS[gromacs_base.i386]=/home/lupones/benchmarks/spec2006/435.gromacs
PATHS[cactusADM_base.i386]=/home/lupones/benchmarks/spec2006/436.cactusADM
PATHS[leslie3d_base.i386]=/home/lupones/benchmarks/spec2006/437.lesliesd
PATHS[namd_base.i386]=/home/lupones/benchmarks/spec2006/444.namd
PATHS[gobmk_base.i386]=/home/lupones/benchmarks/spec2006/445.gobmk
PATHS[dealII_base.i386]=/home/lupones/benchmarks/spec2006/447.dealII
PATHS[soplex_base.i386]=/home/lupones/benchmarks/spec2006/450.soplex
PATHS[povray_base.i386]=/home/lupones/benchmarks/spec2006/453.povray
PATHS[calculix_base.i386]=/home/lupones/benchmarks/spec2006/454.calculix
PATHS[hmmer_base.i386]=/home/lupones/benchmarks/spec2006/456.hmmer
PATHS[sjeng_base.i386]=/home/lupones/benchmarks/spec2006/458.sjeng
PATHS[GemsFDTD_base.i386]=/home/lupones/benchmarks/spec2006/459.GemsFDTD
PATHS[libquantum_base.i386]=/home/lupones/benchmarks/spec2006/462.libquantum
PATHS[h264ref_base.i386]=/home/lupones/benchmarks/spec2006/464.h264ref
PATHS[tonto_base.i386]=/home/lupones/benchmarks/spec2006/465.tonto
PATHS[lbm_base.i386]=/home/lupones/benchmarks/spec2006/470.lbm
PATHS[omnetpp_base.i386]=/home/lupones/benchmarks/spec2006/471.omnetpp
PATHS[astar_base.i386]=/home/lupones/benchmarks/spec2006/473.astar
PATHS[wrf_base.i386]=/home/lupones/benchmarks/spec2006/481.wrf
PATHS[sphinx3_base.i386]=/home/lupones/benchmarks/spec2006/482.sphinx3
PATHS[xalancbmk_base.i386]=/home/lupones/benchmarks/spec2006/483.xalancbmk
xPATHS[specrand_base.i386]=/home/lupones/benchmarks/spec2006/998.specrand

#NAS

