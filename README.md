# pyth-client
client API for on-chain pyth programs

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
docker build . --platform linux/amd64 -f mac/Dockerfile -t pyth-client
```

You can then open a shell in the image by running:

```
docker run -it --platform linux/amd64 pyth-client
```
