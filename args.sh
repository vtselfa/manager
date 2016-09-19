#!/bin/bash

# Creation of  an associative array
declare -A ARGS

# Put values into the array  
# key = name of benchmark
# value = params 
ARGS[perlbench_base.i386]="checkspam.pl 2500 5 25 11 150 1 1 1 1" 
ARGS[bzip2_base.i386]="input.source 280"
ARGS[gcc_base.i386]="166.i -o 166.s"
ARGS[bwaves_base.i386]=""
ARGS[gamess_base.i386]="<triazolium.config"
ARGS[mcf_base.i386]="<inp.in"
ARGS[milc_base.i386]="<su3imp.in"
ARGS[zeusmp_base.i386]=""
ARGS[gromacs_base.i386]="-silent -deffnm gromacs -nice 0"
ARGS[cactusADM_base.i386]="benchADM.par"
ARGS[leslie3d_base.i386]="<leslie3d.in"
ARGS[namd_base.i386]="--input namd.input --iterations 38 --output namd.out"
ARGS[gobmk_base.i386]="--quiet --mode gtp <trevord.tst"
ARGS[dealII_base.i386]="23"
ARGS[soplex_base.i386]="-m3500 ref.mps"
ARGS[povray_base.i386]="SPEC-benchmark-ref.ini"
ARGS[calculix_base.i386]="-i hyperviscoplastic"
ARGS[hmmer_base.i386]="--fixed 0 --mean 500 --num 500000 --sd 350 --seed 0 retro.hmm"
ARGS[sjeng_base.i386]="ref.txt"
ARGS[GemsFDTD_base.i386]=""
ARGS[libquantum_base.i386]="1397 8"
ARGS[h264ref_base.i386]="-d sss_encoder_main.cfg"
ARGS[tonto_base.i386]=""
ARGS[lbm_base.i386]="reference.dat 0 0 100_100_130_ldc.of"
ARGS[omnetpp_base.i386]="omnetpp.ini"
ARGS[astar_base.i386]="rivers.cfg"
ARGS[wrf_base.i386]=""
ARGS[sphinx3_base.i386]="ctlfile . args.an4"
ARGS[xalancbmk_base.i386]="-v t5.xml xalanc.xsl"
ARGS[specrand_base.i386]="1255432124 234923"


