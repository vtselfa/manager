git clone git@github.com:louisdx/cxx-prettyprint.git &&
git clone git@github.com:fmtlib/fmt.git &&
cd fmt && 
cmake . &&
make &&
cd ..
git clone git@github.com:anrieff/libcpuid.git libcpuid
cd libcpuid
libtoolize && autoreconf --install && ./configure && make -j 10
cd ..
git submodule init &&
git submodule update &&
echo "You should be able to do a 'make' now to build the project..."
