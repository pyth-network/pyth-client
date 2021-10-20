# pyth-client
client API for on-chain pyth programs

### Build Instructions

```
# depends on openssl
apt install libssl-dev

# depends on libz
apt install zlib1g zlib1g-dev

# depends on libzstd
apt install libzstd-dev

# uses cmake to build
apt install cmake

# default is release build
mkdir build
cd build
cmake ..
make

# run unit tests
ctest
```


### M1 Mac Build Instructions

We have a dockerfile that allows you to build the on-chain program on an M1 Mac. 
You can build the docker image using the following command:

```
docker build . --platform linux/amd64 -f docker/mac/Dockerfile -t pyth-client
```

You can then open a shell in the image by running:

```
docker run -it --platform linux/amd64 pyth-client
```


### Fuzzing

Build a docker image for running fuzz tests:

```
docker build . --platform linux/amd64 -f docker/fuzz/Dockerfile -t pyth-fuzz
```

Each fuzz test has a command-line program in the `fuzz/` directory,
and a corresponding set of test cases in the subdirectory with the same name as the program.
You can run these tests using a command like:

```
docker run -t \
  --platform linux/amd64 \
  -v "$(pwd)"/findings:/home/pyth/pyth-client/findings \
  pyth-fuzz \
  sh -c "./afl/afl-fuzz -i ./pyth-client/pyth/tests/fuzz/add/testcases -o ./pyth-client/findings ./pyth-client/build/fuzz add"
```

This command will run the `add` program on the tests cases in `pyth/tests/fuzz/add/testcases`, saving any outputs to `findings/`.
Note that `findings/` is shared between the host machine and the docker container, so you can inspect any error cases
by looking in that subdirectory on the host.

If you find an error case that you want to investigate further, you can run the program on the failing input using something like:

```
docker run -t \
  --platform linux/amd64 \
  -v "$(pwd)"/findings:/home/pyth/pyth-client/findings \
  pyth-fuzz \
  sh -c "./pyth-client/build/fuzz add < ./pyth-client/findings/crashes/id\:000000\,sig\:06\,src\:000000\,op\:flip1\,pos\:0"
```

in this example, `id\:000000\,sig\:06\,src\:000000\,op\:flip1\,pos\:0` is the file containing the failing input.
