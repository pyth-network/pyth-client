set -eux

PYTH_DIR=$( cd "${1:-.}" && pwd)

#find the makefile in pyth-client
#ASSUMES THAT there is only one makefile there 
C_DIR="$( find $PYTH_DIR | grep makefile)"
C_DIR=$(dirname $C_DIR)

cd "${C_DIR}/src/oracle/model"
./run_tests
cd "${C_DIR}/src/oracle/sort"
./run_tests
cd "${C_DIR}/src/oracle/util"
./run_tests