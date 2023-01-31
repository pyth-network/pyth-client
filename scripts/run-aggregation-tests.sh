set -eux

PYTH_DIR=$( cd "${1:-.}" && pwd)
C_DIR="$PYTH_DIR/program/c/"

cd "${C_DIR}/src/oracle/model"
./run_tests
cd "${C_DIR}/src/oracle/sort"
./run_tests
cd "${C_DIR}/src/oracle/util"
./run_tests