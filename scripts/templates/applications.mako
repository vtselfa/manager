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

  cpu_spec_2017:
    500.perlbench_r: &perlbench_r
      name: perlbench_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/500.perlbench_r/data/refrate/input, /home/benchmarks/spec2017/benchspec/CPU/500.perlbench_r/data/all/input]
      cmd: bash -c 'export SPECPERLLIB=/home/viselol/benchmarks/spec2017/bin/lib:/home/viselol/benchmarks/spec2017/bin; \
                    /home/benchmarks/spec2017/benchspec/CPU/500.perlbench_r/exe/perlbench_r_base.InitialTest-m64 -I./lib checkspam.pl 2500 5 25 11 150 1 1 1 1; \
                    /home/benchmarks/spec2017/benchspec/CPU/500.perlbench_r/exe/perlbench_r_base.InitialTest-m64 -I./lib diffmail.pl 4 800 10 17 19 300; \
                    /home/benchmarks/spec2017/benchspec/CPU/500.perlbench_r/exe/perlbench_r_base.InitialTest-m64 -I./lib splitmail.pl 6400 12 26 16 100 0'

    502.gcc_r: &gcc_r
      name: gcc_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/502.gcc_r/data/refrate/input]
      cmd: bash -c '/home/benchmarks/spec2017/benchspec/CPU/502.gcc_r/exe/cpugcc_r_base.InitialTest-m64 gcc-pp.c -O3 -finline-limit=0 -fif-conversion -fif-conversion2 -o gcc-pp.opts-O3_-finline-limit_0_-fif-conversion_-fif-conversion2.s; \
                    /home/benchmarks/spec2017/benchspec/CPU/502.gcc_r/exe/cpugcc_r_base.InitialTest-m64 gcc-pp.c -O2 -finline-limit=36000 -fpic -o gcc-pp.opts-O2_-finline-limit_36000_-fpic.s; \
                    /home/benchmarks/spec2017/benchspec/CPU/502.gcc_r/exe/cpugcc_r_base.InitialTest-m64 gcc-smaller.c -O3 -fipa-pta -o gcc-smaller.opts-O3_-fipa-pta.s; \
                    /home/benchmarks/spec2017/benchspec/CPU/502.gcc_r/exe/cpugcc_r_base.InitialTest-m64 ref32.c -O5 -o ref32.opts-O5.s; \
                    /home/benchmarks/spec2017/benchspec/CPU/502.gcc_r/exe/cpugcc_r_base.InitialTest-m64 ref32.c -O3 -fselective-scheduling -fselective-scheduling2 -o ref32.opts-O3_-fselective-scheduling_-fselective-scheduling2.s'

    503.bwaves_r: &bwaves_r
      name: bwaves_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/503.bwaves_r/data/refrate/input]
      cmd: bash -c '/home/benchmarks/spec2017/benchspec/CPU/503.bwaves_r/exe/bwaves_r_base.InitialTest-m64 <bwaves_1.in; \
                    /home/benchmarks/spec2017/benchspec/CPU/503.bwaves_r/exe/bwaves_r_base.InitialTest-m64 <bwaves_2.in; \
                    /home/benchmarks/spec2017/benchspec/CPU/503.bwaves_r/exe/bwaves_r_base.InitialTest-m64 <bwaves_3.in; \
                    /home/benchmarks/spec2017/benchspec/CPU/503.bwaves_r/exe/bwaves_r_base.InitialTest-m64 <bwaves_4.in'

    505.mcf_r: &mcf_r
      name: mcf_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/505.mcf_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/505.mcf_r/exe/mcf_r_base.InitialTest-m64 inp.in

    507.cactuBSSN_r: &cactuBSSN_r
      name: cactuBSSN_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/507.cactuBSSN_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/507.cactuBSSN_r/exe/cactusBSSN_r_base.InitialTest-m64 spec_ref.par

    508.namd_r: &namd_r
      name: namd_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/508.namd_r/data/refrate/input, /home/benchmarks/spec2017/benchspec/CPU/508.namd_r/data/all/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/508.namd_r/exe/namd_r_base.InitialTest-m64 --input apoa1.input --output apoa1.ref.output --iterations 65

    510.parest_r: &parest_r
      name: parest_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/510.parest_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/510.parest_r/exe/parest_r_base.InitialTest-m64 ref.prm

    511.povray_r: &povray_r
      name: povray_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/511.povray_r/data/refrate/input, /home/benchmarks/spec2017/benchspec/CPU/511.povray_r/data/all/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/511.povray_r/exe/povray_r_base.InitialTest-m64 SPEC-benchmark-ref.ini

    519.lbm_r: &lbm_r
      name: lbm_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/519.lbm_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/519.lbm_r/exe/lbm_r_base.InitialTest-m64 3000 reference.dat 0 0 100_100_130_ldc.of

    520.omnetpp_r: &omnetpp_r
      name: omnetpp_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/520.omnetpp_r/data/refrate/input, /home/benchmarks/spec2017/benchspec/CPU/520.omnetpp_r/data/all/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/520.omnetpp_r/exe/omnetpp_r_base.InitialTest-m64 -c General -r 0

    521.wrf_r: &wrf_r
      name: wrf_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/521.wrf_r/data/refrate/input, /home/benchmarks/spec2017/benchspec/CPU/521.wrf_r/data/all/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/521.wrf_r/exe/wrf_r_base.InitialTest-m64

    523.xalancbmk_r: &xalancbmk_r
      name: xalancbmk_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/523.xalancbmk_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/523.xalancbmk_r/exe/cpuxalan_r_base.InitialTest-m64 -v t5.xml xalanc.xsl

    525.x264_r: &x264_r
      name: x264_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/525.x264_r/data/refrate/input]
      cmd: bash -c '/home/benchmarks/spec2017/benchspec/CPU/525.x264_r/exe/x264_r_base.InitialTest-m64 --pass 1 --stats x264_stats.log --bitrate 1000 --frames 1000 -o BuckBunny_New.264 BuckBunny.yuv 1280x720; \
                    /home/benchmarks/spec2017/benchspec/CPU/525.x264_r/exe/x264_r_base.InitialTest-m64 --pass 2 --stats x264_stats.log --bitrate 1000 --dumpyuv 200 --frames 1000 -o BuckBunny_New.264 BuckBunny.yuv 1280x720; \
                    /home/benchmarks/spec2017/benchspec/CPU/525.x264_r/exe/x264_r_base.InitialTest-m64 --seek 500 --dumpyuv 200 --frames 1250 -o BuckBunny_New.264 BuckBunny.yuv 1280x720'

    526.blender_r: &blender_r
      name: blender_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/526.blender_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/526.blender_r/exe/blender_r_base.InitialTest-m64 sh3_no_char.blend --render-output sh3_no_char_ --threads 1 -b -F RAWTGA -s 849 -e 849 -a

    527.cam4_r: &cam4_r
      name: cam4_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/527.cam4_r/data/refrate/input, /home/benchmarks/spec2017/benchspec/CPU/527.cam4_r/data/all/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/527.cam4_r/exe/cam4_r_base.InitialTest-m64

    531.deepsjeng_r: &deepsjeng_r
      name: deepsjeng_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/531.deepsjeng_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/531.deepsjeng_r/exe/deepsjeng_r_base.InitialTest-m64 ref.txt

    538.imagick_r: &imagick_r
      name: imagick_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/538.imagick_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/538.imagick_r/exe/imagick_r_base.InitialTest-m64 -limit disk 0 refrate_input.tga -edge 41 -resample 181% -emboss 31 -colorspace YUV -mean-shift 19x19+15% -resize 30% refrate_output.tga

    541.leela_r: &leela_r
      name: leela_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/541.leela_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/541.leela_r/exe/leela_r_base.InitialTest-m64 ref.sgf

    544.nab_r: &nab_r
      name: nab_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/544.nab_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/544.nab_r/exe/nab_r_base.InitialTest-m64 1am0 1122214447 122

    548.exchange2_r: &exchange2_r
      name: exchange2_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/548.exchange2_r/data/refrate/input, /home/benchmarks/spec2017/benchspec/CPU/548.exchange2_r/data/all/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/548.exchange2_r/exe/exchange2_r_base.InitialTest-m64 6

    549.fotonik3d_r: &fotonik3d_r
      name: fotonik3d_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/549.fotonik3d_r/data/refrate/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/549.fotonik3d_r/exe/fotonik3d_r_base.InitialTest-m64

    554.roms_r: &roms_r
      name: roms_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/554.roms_r/data/refrate/input, /home/benchmarks/spec2017/benchspec/CPU/554.roms_r/data/all/input]
      cmd: /home/benchmarks/spec2017/benchspec/CPU/554.roms_r/exe/roms_r_base.InitialTest-m64
      stdin: ocean_benchmark2.in.x

    557.xz_r: &xz_r
      name: xz_r
      skel: [/home/benchmarks/spec2017/benchspec/CPU/557.xz_r/data/refrate/input, /home/benchmarks/spec2017/benchspec/CPU/557.xz_r/data/all/input]
      cmd: bash -c '/home/benchmarks/spec2017/benchspec/CPU/557.xz_r/exe/xz_r_base.InitialTest-m64 cld.tar.xz 160 19cf30ae51eddcbefda78dd06014b4b96281456e078ca7c13e1c0c9e6aaea8dff3efb4ad6b0456697718cede6bd5454852652806a657bb56e07d61128434b474 59796407 61004416 6; \
                    /home/benchmarks/spec2017/benchspec/CPU/557.xz_r/exe/xz_r_base.InitialTest-m64 cpu2006docs.tar.xz 250 055ce243071129412e9dd0b3b69a21654033a9b723d874b2015c774fac1553d9713be561ca86f74e4f16f22e664fc17a79f30caa5ad2c04fbc447549c2810fae 23047774 23513385 6e; \
                    /home/benchmarks/spec2017/benchspec/CPU/557.xz_r/exe/xz_r_base.InitialTest-m64 input.combined.xz 250 a841f68f38572a49d86226b7ff5baeb31bd19dc637a922a972b2e6d1257a890f6a544ecab967c313e370478c74f760eb229d4eef8a8d2836d233d3e9dd1430bf 40401484 41217675 7'


  parsec3:
    <%text>
    blackscholes: &blackscholes
      name: blackscholes
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/blackscholes/inst/amd64-linux.gcc/bin/blackscholes ${NTHREADS} in_10M.txt prices.txt
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/blackscholes/inputs/native

    bodytrack: &bodytrack
      name: bodytrack
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/bodytrack/inst/amd64-linux.gcc/bin/bodytrack sequenceB_261 4 261 4000 5 0 ${NTHREADS}
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/bodytrack/inputs/native

    facesim: &facesim
      name: facesim
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/facesim/inst/amd64-linux.gcc/bin/facesim -timing -threads ${NTHREADS} -lastframe 100
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/facesim/inputs/native

    ferret: &ferret
      name: ferret
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/ferret/inst/amd64-linux.gcc/bin/ferret corel lsh queries 50 20 ${NTHREADS} output.txt
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/ferret/inputs/native

    fluidanimate: &fluidanimate
      name: fluidanimate
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/fluidanimate/inst/amd64-linux.gcc/bin/fluidanimate ${NTHREADS} 500 in_500K.fluid out.fluid
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/fluidanimate/inputs/native

    freqmine: &freqmine
      name: freqmine
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/freqmine/inst/amd64-linux.gcc/bin/freqmine webdocs_250k.dat 11000
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/freqmine/inputs/native

    raytrace: &raytrace
      name: raytrace
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/raytrace/inst/amd64-linux.gcc/bin/rtview thai_statue.obj -automove -nthreads ${NTHREADS} -frames 200 -res 1920 1080
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/raytrace/inputs/native

    swaptions: &swaptions
      name: swaptions
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/swaptions/inst/amd64-linux.gcc/bin/swaptions -ns 128 -sm 1000000 -nt ${NTHREADS}

    vips: &vips
      name: vips
      cmd: env IM_CONCURRENCY=${NTHREADS} /home/benchmarks/parsec-3.0/pkgs/apps/vips/inst/amd64-linux.gcc/bin/vips im_benchmark orion_18000x18000.v output.v
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/vips/inputs/native

    x264: &x264
      name: x264
      cmd: /home/benchmarks/parsec-3.0/pkgs/apps/x264/inst/amd64-linux.gcc/bin/x264 --quiet --qp 20 --partitions b8x8,i4x4 --ref 5 --direct auto --weightb --mixed-refs --no-fast-pskip --me umh --subme 7 --analyse b8x8,i4x4 --threads ${NTHREADS} -o eledream.264 eledream_1920x1080_512.y4m
      skel: /home/benchmarks/parsec-3.0/pkgs/apps/x264/inputs/native

    canneal: &canneal
      name: canneal
      cmd: /home/benchmarks/parsec-3.0/pkgs/kernels/canneal/inst/amd64-linux.gcc/bin/canneal ${NTHREADS} 15000 2000 2500000.nets 6000
      skel: /home/benchmarks/parsec-3.0/pkgs/kernels/canneal/inputs/native

    dedup: &dedup
      name: dedup
      cmd: /home/benchmarks/parsec-3.0/pkgs/kernels/dedup/inst/amd64-linux.gcc/bin/dedup -c -p -v -t ${NTHREADS} -i FC-6-x86_64-disc1.iso -o output.dat.ddp
      skel: /home/benchmarks/parsec-3.0/pkgs/kernels/dedup/inputs/native

    streamcluster: &streamcluster
      name: streamcluster
      cmd: /home/benchmarks/parsec-3.0/pkgs/kernels/streamcluster/inst/amd64-linux.gcc/bin/streamcluster 10 20 128 1000000 200000 5000 none output.txt ${NTHREADS}
    </%text>

