git clone git@github.com:louisdx/cxx-prettyprint.git &&
git clone git@github.com:fmtlib/fmt.git &&
cd fmt && 
cmake . &&
make &&
cd ..
git submodule init &&
git submodule update &&
echo "You should be able to do a 'make' now to build the project..."
