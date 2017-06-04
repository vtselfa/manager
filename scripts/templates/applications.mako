applications:
  nas:
    bt.B.x: &bt.B
      name: bt.B
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/bt.B.x

    cg.B.x: &cg.B
      name: cg.B
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/cg.B.x

    dc.B.x: &dc.B
      name: dc.B
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/dc.B.x

    ep.B.x: &ep.B
      name: ep.B
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/ep.B.x

    ft.B.x: &ft.B
      name: ft.B
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/ft.B.x

    lu.B.x: &lu.B
      name: lu.B
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/lu.B.x

    sp.B.x: &sp.B
      name: sp.B
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/sp.B.x

    ua.B.x: &ua.B
      name: ua.B
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/ua.B.x

    bt.C.x: &bt.C
      name: bt.C
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/bt.C.x

    cg.C.x: &cg.C
      name: cg.C
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/cg.C.x

    ep.C.x: &ep.C
      name: ep.C
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/ep.C.x

    ft.C.x: &ft.C
      name: ft.C
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/ft.C.x

    is.C.x: &is.C
      name: is.C
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/is.C.x

    lu.C.x: &lu.C
      name: lu.C
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/lu.C.x

    mg.C.x: &mg.C
      name: mg.C
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/mg.C.x

    sp.C.x: &sp.C
      name: sp.C
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/sp.C.x

    ua.C.x: &ua.C
      name: ua.C
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/ua.C.x

    bt.D.x: &bt.D
      name: bt.D
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/bt.D.x

    cg.D.x: &cg.D
      name: cg.D
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/cg.D.x

    ep.D.x: &ep.D
      name: ep.D
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/ep.D.x

    is.D.x: &is.D
      name: is.D
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/is.D.x

    lu.D.x: &lu.D
      name: lu.D
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/lu.D.x

    mg.D.x: &mg.D
      name: mg.D
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/mg.D.x

    sp.D.x: &sp.D
      name: sp.D
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/sp.D.x

    ua.D.x: &ua.D
      name: ua.D
      cmd: /home/benchmarks/NPB3.3.1/NPB3.3-SER/bin/ua.D.x


  lmbench:
    lat_mem_rd: &lat_mem_rd
      name: lat_mem_rd
      cmd: /home/benchmarks/lmbench/lat_mem_rd -P1 -N1 -t 1000 128


  cpu_spec_2006:
    400.perlbench: &perlbench
      name: perlbench
      skel: /home/benchmarks/spec2006/400.perlbench/data-ref
      cmd: /home/benchmarks/spec2006/400.perlbench/perlbench_base.i386 checkspam.pl 2500 5 25 11 150 1 1 1 1

    401.bzip2: &bzip2
      name: bzip2
      skel: /home/benchmarks/spec2006/401.bzip2/data-ref
      cmd: /home/benchmarks/spec2006/401.bzip2/bzip2_base.i386 input.source 280

    403.gcc: &gcc
      name: gcc
      skel: /home/benchmarks/spec2006/403.gcc/data-ref
      cmd: /home/benchmarks/spec2006/403.gcc/gcc_base.i386 g23.i -o -

    410.bwaves: &bwaves
      name: bwaves
      skel: /home/benchmarks/spec2006/410.bwaves/data-ref
      cmd: /home/benchmarks/spec2006/410.bwaves/bwaves_base.i386 

    416.gamess: &gamess
      name: gamess
      skel: /home/benchmarks/spec2006/416.gamess/data-ref
      cmd: /home/benchmarks/spec2006/416.gamess/gamess_base.i386 
      stdin: triazolium.config

    429.mcf: &mcf
      name: mcf
      skel: /home/benchmarks/spec2006/429.mcf/data-ref
      cmd: /home/benchmarks/spec2006/429.mcf/mcf_base.i386 inp.in

    433.milc: &milc
      name: milc
      skel: /home/benchmarks/spec2006/433.milc/data-ref
      cmd: /home/benchmarks/spec2006/433.milc/milc_base.i386 
      stdin: su3imp.in

    434.zeusmp: &zeusmp
      name: zeusmp
      skel: /home/benchmarks/spec2006/434.zeusmp/data-ref
      cmd: /home/benchmarks/spec2006/434.zeusmp/zeusmp_base.i386 

    435.gromacs: &gromacs
      name: gromacs
      skel: /home/benchmarks/spec2006/435.gromacs/data-ref
      cmd: /home/benchmarks/spec2006/435.gromacs/gromacs_base.i386 -silent -deffnm gromacs -nice 0

    436.cactusADM: &cactusADM
      name: cactusADM
      skel: /home/benchmarks/spec2006/436.cactusADM/data-ref
      cmd: /home/benchmarks/spec2006/436.cactusADM/cactusADM_base.i386 benchADM.par

    437.leslie3d: &leslie3d
      name: leslie3d
      skel: /home/benchmarks/spec2006/437.leslie3d/data-ref
      cmd: /home/benchmarks/spec2006/437.leslie3d/leslie3d_base.i386 
      stdin: leslie3d.in

    444.namd: &namd
      name: namd
      skel: /home/benchmarks/spec2006/444.namd/data-ref
      cmd: /home/benchmarks/spec2006/444.namd/namd_base.i386 --input namd.input --iterations 38

    445.gobmk: &gobmk
      name: gobmk
      skel: /home/benchmarks/spec2006/445.gobmk/data-ref
      cmd: /home/benchmarks/spec2006/445.gobmk/gobmk_base.i386 --quiet --mode gtp
      stdin: 13x13.tst

    447.dealII: &dealII
      name: dealII
      skel: /home/benchmarks/spec2006/447.dealII/data-ref
      cmd: /home/benchmarks/spec2006/447.dealII/dealII_base.i386 23

    450.soplex: &soplex
      name: soplex
      skel: /home/benchmarks/spec2006/450.soplex/data-ref
      cmd: /home/benchmarks/spec2006/450.soplex/soplex_base.i386 -s1 -e -m45000 pds-50.mps

    453.povray: &povray
      name: povray
      skel: /home/benchmarks/spec2006/453.povray/data-ref
      cmd: /home/benchmarks/spec2006/453.povray/povray_base.i386 SPEC-benchmark-ref.ini

    454.calculix: &calculix
      name: calculix
      skel: /home/benchmarks/spec2006/454.calculix/data-ref
      cmd: /home/benchmarks/spec2006/454.calculix/calculix_base.i386 -i hyperviscoplastic

    456.hmmer: &hmmer
      name: hmmer
      skel: /home/benchmarks/spec2006/456.hmmer/data-ref
      cmd: /home/benchmarks/spec2006/456.hmmer/hmmer_base.i386 --fixed 0 --mean 500 --num 500000 --sd 350 --seed 0 retro.hmm

    458.sjeng: &sjeng
      name: sjeng
      skel: /home/benchmarks/spec2006/458.sjeng/data-ref
      cmd: /home/benchmarks/spec2006/458.sjeng/sjeng_base.i386 ref.txt

    459.GemsFDTD: &GemsFDTD
      name: GemsFDTD
      skel: /home/benchmarks/spec2006/459.GemsFDTD/data-ref
      cmd: /home/benchmarks/spec2006/459.GemsFDTD/GemsFDTD_base.i386 

    462.libquantum: &libquantum
      name: libquantum
      skel: /home/benchmarks/spec2006/462.libquantum/data-ref
      cmd: /home/benchmarks/spec2006/462.libquantum/libquantum_base.i386 1397 8

    464.h264ref: &h264ref
      name: h264ref
      skel: /home/benchmarks/spec2006/464.h264ref/data-ref
      cmd: /home/benchmarks/spec2006/464.h264ref/h264ref_base.i386 -d sss_encoder_main.cfg

    465.tonto: &tonto
      name: tonto
      skel: /home/benchmarks/spec2006/465.tonto/data-ref
      cmd: /home/benchmarks/spec2006/465.tonto/tonto_base.i386 

    470.lbm: &lbm
      name: lbm
      skel: /home/benchmarks/spec2006/470.lbm/data-ref
      cmd: /home/benchmarks/spec2006/470.lbm/lbm_base.i386 3000 reference.dat 0 0 100_100_130_ldc.of

    471.omnetpp: &omnetpp
      name: omnetpp
      skel: /home/benchmarks/spec2006/471.omnetpp/data-ref
      cmd: /home/benchmarks/spec2006/471.omnetpp/omnetpp_base.i386 omnetpp.ini

    473.astar: &astar
      name: astar
      skel: /home/benchmarks/spec2006/473.astar/data-ref
      cmd: /home/benchmarks/spec2006/473.astar/astar_base.i386 rivers.cfg

    481.wrf: &wrf
      name: wrf
      skel: /home/benchmarks/spec2006/481.wrf/data-ref
      cmd: /home/benchmarks/spec2006/481.wrf/wrf_base.i386 namelist.input

    482.sphinx3: &sphinx3
      name: sphinx3
      skel: /home/benchmarks/spec2006/482.sphinx3/data-ref
      cmd: /home/benchmarks/spec2006/482.sphinx3/sphinx3_base.i386 ctlfile . args.an4

    483.xalancbmk: &xalancbmk
      name: xalancbmk
      skel: /home/benchmarks/spec2006/483.xalancbmk/data-ref
      cmd: /home/benchmarks/spec2006/483.xalancbmk/xalancbmk_base.i386 -v t5.xml xalanc.xsl
      stdout: /dev/null # Fuck you


  parsec3:
    <%text>
    blackscholes: &blackscholes
      name: blackscholes
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/blackscholes/obj/amd64-linux.gcc/blackscholes ${NTHREADS} in_10M.txt prices.txt
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/blackscholes/inputs/native

    bodytrack: &bodytrack
      name: bodytrack
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/bodytrack/obj/amd64-linux.gcc/bodytrack sequenceB_261 4 261 4000 5 0 ${NTHREADS}
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/bodytrack/inputs/native

    facesim: &facesim
      name: facesim
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/facesim/obj/amd64-linux.gcc/facesim -timing -threads ${NTHREADS} -lastframe 100
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/facesim/inputs/native

    ferret: &ferret
      name: ferret
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/ferret/obj/amd64-linux.gcc/ferret corel lsh queries 50 20 ${NTHREADS} output.txt
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/ferret/inputs/native

    fluidanimate: &fluidanimate
      name: fluidanimate
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/fluidanimate/obj/amd64-linux.gcc/fluidanimate ${NTHREADS} 500 in_500K.fluid out.fluid
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/fluidanimate/inputs/native

    freqmine: &freqmine
      name: freqmine
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/freqmine/obj/amd64-linux.gcc/freqmine webdocs_250k.dat 11000
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/freqmine/inputs/native

    raytrace: &raytrace
      name: raytrace
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/raytrace/obj/amd64-linux.gcc/raytrace thai_statue.obj -automove -nthreads ${NTHREADS} -frames 200 -res 1920 1080
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/raytrace/inputs/native

    swaptions: &swaptions
      name: swaptions
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/swaptions/obj/amd64-linux.gcc/swaptions -ns 128 -sm 1000000 -nt ${NTHREADS}

    vips: &vips
      name: vips
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/vips/obj/amd64-linux.gcc/vips im_benchmark orion_18000x18000.v output.v
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/vips/inputs/native

    x264: &x264
      name: x264
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/x264/obj/amd64-linux.gcc/x264 --quiet --qp 20 --partitions b8x8,i4x4 --ref 5 --direct auto --b-pyramid --weightb --mixed-refs --no-fast-pskip --me umh --subme 7 --analyse b8x8,i4x4 --threads ${NTHREADS} -o eledream.264 eledream_1920x1080_512.y4m
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/x264/inputs/native

    canneal: &canneal
      name: canneal
      cmd: /home/benchmarks/parsec-3.0/pkgs/kernels/canneal/obj/amd64-linux.gcc/canneal ${NTHREADS} 15000 2000 2500000.nets 6000
      skel: /home/benchmarks/parsec-3.0/pkgs/kernels/canneal/inputs/native

    dedup: &dedup
      name: dedup
      cmd: /home/benchmarks/parsec-3.0/pkgs/kernels/dedup/obj/amd64-linux.gcc/dedup -c -p -v -t ${NTHREADS} -i FC-6-x86_64-disc1.iso -o output.dat.ddp
      skel: /home/benchmarks/parsec-3.0/pkgs/kernels/dedup/inputs/native

    streamcluster: &streamcluster
      name: streamcluster
      cmd: /home/benchmarks/parsec-3.0/pkgs/kernels/streamcluster/obj/amd64-linux.gcc/streamcluster 10 20 128 1000000 200000 5000 none output.txt ${NTHREADS}
    </%text>

