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
./scripts/build.sh
```

You may want to build this repository inside a linux docker container for various reasons, e.g., if building on an M1 mac, or to run BPF binaries.
You can run the following command to open a shell in a linux docker container with the pyth-client directory mounted:

```
export PYTH_REPO=/path/to/host/pyth-client

// Use the latest pythd image from https://hub.docker.com/r/pythfoundation/pyth-client/tags?name=pythd
export IMAGE=...

docker run -it \
  --volume "${HOME}:/home/pyth/home" \
  --volume "${HOME}/.config:/home/pyth/.config" \
  --mount "type=bind,src=${PYTH_REPO},target=/home/pyth/pyth-client" \
  --userns=host \
  --user="$( id -ur ):$( id -gr )" \
  --platform linux/amd64 \
  $IMAGE \
  /bin/bash -l
```

This command runs a recent pyth-client docker image that already has the necessary dependencies installed.
Therefore, once the container is running, all you have to do is run `cd pyth-client && ./scripts/build.sh`.
Note that updates to the `pyth-client` directory made inside the docker container will be persisted to the host filesystem (which is probably desirable).

### Local development

First, make sure you're building on the x86_64 architecture.
On a mac, this command will switch your shell to x86_64:

`env /usr/bin/arch -x86_64 /bin/bash --login`

then in the `program/c` directory, run:

```
make
make cpyth-bpf
make cpyth-native
```

then in the `program/rust` directory, run:

```
cargo build-bpf
cargo test
```

Note that the tests depend on the bpf build!

### Fuzzing

Build a docker image for running fuzz tests:

```
docker build . --platform linux/amd64 -f docker/fuzz/Dockerfile -t pyth-fuzz
```

Each fuzz test is invoked via an argument to the `fuzz` command-line program,
and has a corresponding set of test cases in the subdirectory with the same name as the test.
You can run these tests using a command like:

```
docker run -t \
  --platform linux/amd64 \
  -v "$(pwd)"/findings:/home/pyth/pyth-client/findings \
  pyth-fuzz \
  sh -c "./afl/afl-fuzz -i ./pyth-client/pyth/tests/fuzz/add/testcases -o ./pyth-client/findings ./pyth-client/build/fuzz add"
```

This command will run the `add` fuzz test on the tests cases in `pyth/tests/fuzz/add/testcases`, saving any outputs to `findings/`.
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

## Development Setup Using VS Code

First create a docker container in daemon as your working container (`IMAGE` and `PYTH_REPO` same as above):

```
docker run --name pyth-dev -d \\
  --volume "${HOME}:/home/pyth/home" \\
  --volume "${HOME}/.config:/home/pyth/.config" \\
  --volume "${HOME}/.ssh:/home/pyth/.ssh" \\ # Github access
  --mount "type=bind,src=${PYTH_REPO},target=/home/pyth/pyth-client" \\
  --platform linux/amd64 \\
  $IMAGE \\
  /bin/bash -c "while [ true ]; do sleep 1000; done"
```

Default user in the image is `pyth` which may not have access to your directories. Assign your user id and group id to it to enable access.
```
host@host$ id $USER # Shows user_id, group_id, and group names
host@host$ docker exec -ti pyth-dev bash
pyth@pyth-dev$ sudo su
root@pyth-dev# groupadd -g 1004 1004
root@pyth-dev# usermod -u 1002 -g 1004 -s /bin/bash pyth
```

Finally, in docker extension inside VS Code click right and choose "Attach VS Code". If you're using a remote host in VS Code make sure to let this connection be open.

### pre-commit hooks
pre-commit is a tool that checks and fixes simple issues (formatting, ...) before each commit. You can install it by following [their website](https://pre-commit.com/). In order to enable checks for this repo run `pre-commit install` from command-line in the root of this repo.

The checks are also performed in the CI to ensure the code follows consistent formatting. Formatting is only currently enforced in the `program/` directory.
You might also need to install the nightly toolchain to run the formatting by running `rustup toolchain install nightly`.
