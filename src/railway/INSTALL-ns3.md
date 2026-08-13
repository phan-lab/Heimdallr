# Building ns-3 for Figure 10

Figure 10 is the only target that needs anything outside this repository: an
ns-3 build to compile the ETCS-3 railway scenario against. Everything else —
the scenario source, its configs, and the four contrib modules ns-3 needs — is
vendored here.

Once built, point the artifact at it:

```bash
NS3_ROOT=/opt/ns-3.38 ./run.sh figure-10
```

`NS3_ROOT` must contain `build/include/ns3/` and `build/lib/libns3.38-*.so`.
`./setup.sh` reports whether it was found.

---

## What is required, and why

| requirement | reason |
|---|---|
| **ns-3.38 exactly** | the libraries are versioned in their filenames (`libns3.38-core-default.so`) and the scenario links against those names |
| **C++17** | ns-3.38's `Singleton<T>` declares its deleted copy constructor as `Singleton<T>(const Singleton<T>&)`, which is ill-formed in C++20. Use GCC 11 or 12; GCC 13+ also needs extra `<cstdint>` includes that ns-3.38 predates |
| **the `lte` module** | the scenario models train↔eNB radio links |
| **four contrib modules** | `multisig`, `seconomist-message`, `secure-log`, `task-schedules` — HEIMDALLR's own ns-3 modules, not part of stock ns-3. Vendored in `ns3-contrib/` |
| **libpbc** | `multisig` links against the Stanford Pairing-Based Crypto library. **Not packaged in Debian/Ubuntu — must be built from source.** |
| **libgmp, OpenSSL** | `libpbc` needs GMP; the contrib modules use `openssl/sha.h` and link `ssl`/`crypto` |

The dependency that catches people out is **libpbc**: `apt install` will not
find it, and without it the `multisig` module fails to link.

---

## Dockerfile

Ubuntu 22.04 is the recommended base — it ships GCC 11 and CMake 3.22, both
known-good for ns-3.38.

```dockerfile
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# --- toolchain and libraries -------------------------------------------------
# g++/cmake/ninja  : ns-3's build system
# libgmp-dev       : required by libpbc
# libssl-dev       : openssl/sha.h and -lssl -lcrypto, used by the contrib modules
# libsodium-dev    : the localhost benchmarks (Figure 9, Table 4)
# flex/bison       : required to build libpbc from source
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential g++ make cmake ninja-build \
        python3 python3-pip python3-dev \
        git wget ca-certificates tar \
        libgmp-dev libssl-dev libsodium-dev \
        flex bison \
    && rm -rf /var/lib/apt/lists/*

# --- libpbc (Stanford Pairing-Based Crypto) ----------------------------------
# Not in the Ubuntu archive.  ns-3's `multisig` contrib module links -lpbc.
RUN cd /tmp \
    && wget -q https://crypto.stanford.edu/pbc/files/pbc-0.5.14.tar.gz \
    && tar xzf pbc-0.5.14.tar.gz \
    && cd pbc-0.5.14 \
    && ./configure --prefix=/usr/local \
    && make -j"$(nproc)" && make install \
    && ldconfig \
    && rm -rf /tmp/pbc-0.5.14*

# --- ns-3.38 -----------------------------------------------------------------
RUN cd /opt \
    && wget -q https://www.nsnam.org/releases/ns-allinone-3.38.tar.bz2 \
    && tar xjf ns-allinone-3.38.tar.bz2 \
    && mv ns-allinone-3.38/ns-3.38 /opt/ns-3.38 \
    && rm -rf ns-allinone-3.38 ns-allinone-3.38.tar.bz2

# --- HEIMDALLR's contrib modules ---------------------------------------------
# Vendored in the artifact under src/railway/ns3-contrib/.  Adjust the source
# path to wherever you copy the artifact in.
COPY artifact-reproduction/src/railway/ns3-contrib/ /opt/ns-3.38/contrib/

# --- configure and build -----------------------------------------------------
# Module list is from railway-ns3/default-configure.sh.  ns-3 pulls in the
# transitive dependencies (core, network, internet, mobility, spectrum, ...)
# on its own, which is why the build produces ~23 libraries.
RUN cd /opt/ns-3.38 \
    && ./ns3 configure \
         --enable-modules=lte,fd-net-device,olsr,multisig,seconomist-message,task-schedules,secure-log \
         --build-profile=default \
         --disable-werror \
    && ./ns3 build -j"$(nproc)"

ENV NS3_ROOT=/opt/ns-3.38
ENV LD_LIBRARY_PATH=/usr/local/lib:/opt/ns-3.38/build/lib

# --- the artifact ------------------------------------------------------------
COPY artifact-reproduction/ /root/artifact-reproduction/
WORKDIR /root/artifact-reproduction
RUN pip3 install --no-cache-dir -r requirements.txt

# Sanity check at image build time, so a broken ns-3 fails the build rather
# than the review.
RUN ./setup.sh && make -C src/railway && rm -f src/railway/sim

CMD ["/bin/bash"]
```

Build and run:

```bash
# from the directory containing artifact-reproduction/
docker build -t heimdallr-ae -f Dockerfile .
docker run --rm -it heimdallr-ae
# inside:
./run.sh all
```

**Expect the ns-3 build to take 20–40 minutes** and about 4 GB of disk; the LTE
module is most of it. Everything else in the image builds in under a minute.

---

## Doing it without Docker

Same steps, minus the container:

```bash
# 1. Dependencies (Debian/Ubuntu)
sudo apt-get install -y build-essential cmake ninja-build python3 \
                        libgmp-dev libssl-dev libsodium-dev flex bison wget

# 2. libpbc from source
wget https://crypto.stanford.edu/pbc/files/pbc-0.5.14.tar.gz
tar xzf pbc-0.5.14.tar.gz && cd pbc-0.5.14
./configure --prefix=/usr/local && make -j"$(nproc)" && sudo make install
sudo ldconfig && cd ..

# 3. ns-3.38
wget https://www.nsnam.org/releases/ns-allinone-3.38.tar.bz2
tar xjf ns-allinone-3.38.tar.bz2
sudo mv ns-allinone-3.38/ns-3.38 /opt/ns-3.38

# 4. HEIMDALLR's contrib modules, from this repo
sudo cp -r src/railway/ns3-contrib/* /opt/ns-3.38/contrib/

# 5. Configure and build
cd /opt/ns-3.38
./ns3 configure \
    --enable-modules=lte,fd-net-device,olsr,multisig,seconomist-message,task-schedules,secure-log \
    --build-profile=default --disable-werror
./ns3 build -j"$(nproc)"

# 6. Point the artifact at it
cd -                       # back to artifact-reproduction/
export NS3_ROOT=/opt/ns-3.38
./setup.sh                 # should report "ok  ns-3 for Figure 10 at /opt/ns-3.38"
./run.sh figure-10
```

---

## Verifying the build

```bash
ls $NS3_ROOT/build/lib/libns3.38-lte-default.so        # LTE built
ls $NS3_ROOT/build/lib/libns3.38-multisig-default.so   # contrib built
ldd $NS3_ROOT/build/lib/libns3.38-multisig-default.so | grep -E 'pbc|gmp|crypto'
```

The last command should show `libpbc.so.1`, `libgmp.so.10` and `libcrypto.so`
resolved — not "not found". Then:

```bash
make -C src/railway         # ~20 s; compiles the scenario against that build
./run.sh figure-10          # ~16 min: three 400 s simulations
./compare.py railway        # how close the traces are to the paper's
```

A correct build reproduces the leading train's trajectory exactly (0 m
deviation at every sampled timestamp); see
[../../targets/figure-10/README.md](../../targets/figure-10/README.md).

---

## Troubleshooting

| symptom | cause |
|---|---|
| `cannot find -lpbc` while building `multisig` | libpbc not installed, or `/usr/local/lib` not in the linker path — run `ldconfig` |
| `expected unqualified-id before 'const'` in `singleton.h` | the scenario is being compiled as C++20. `src/railway/Makefile` pins `-std=c++17`; do not override `CXXFLAGS` |
| `fatal error: openssl/sha.h` | `libssl-dev` missing |
| ns-3 configure ignores the contrib modules | they must be under `$NS3_ROOT/contrib/<name>/`, each with its own `CMakeLists.txt`; re-run `./ns3 configure` after copying |
| `no prebuilt ns-3 under .../build` from `run_scenarios.py` | `NS3_ROOT` unset or pointing somewhere without `build/include/ns3/` |
| GCC 13+ errors about `uint8_t` / `size_t` | ns-3.38 predates those `<cstdint>` include changes; use GCC 11 or 12 |

## Why not rebuild the tree that shipped with the paper

`railway-ns3/cmake-cache/CMakeCache.txt` records absolute paths from the machine
it was built on (`/home/.../seconomist-simulation`), so `./ns3 configure` there
discards the prebuilt libraries and rebuilds all of ns-3 anyway. Building a
clean ns-3.38 as above is the same amount of work and gives a tree whose paths
match where it actually lives.
