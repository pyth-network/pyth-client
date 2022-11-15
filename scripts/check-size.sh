ORACLE_SIZE=$(wc -c ./target/deploy/pyth_oracle.so | awk '{print $1}')
if [ $ORACLE_SIZE -lt 81760 ]
then
    echo "The program fits on mainnet"
    echo $ORACLE_SIZE
else
    echo "Program is too big"
    echo $ORACLE_SIZE
    exit 1
fi
