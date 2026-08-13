# Artifact of *Heimdallr: Bounded-Time Byzantine Fault Detection and Recovery in Geo-Distributed Systems*


## 0. Contents
- [Artifact of *Heimdallr: Bounded-Time Byzantine Fault Detection and Recovery in Geo-Distributed Systems*](#artifact-of-heimdallr-bounded-time-byzantine-fault-detection-and-recovery-in-geo-distributed-systems)
  - [0. Contents](#0-contents)
  - [1. Setting up](#1-setting-up)
    - [Machine requirements](#machine-requirements)
    - [Setup docker](#setup-docker)
    - [Software and packages](#software-and-packages)
  - [2. Running everything at once](#2-running-everything-at-once)
  - [3. Where the results are](#3-where-the-results-are)
    - [Plots and tables — `outputs/`](#plots-and-tables--outputs)
    - [Generated data — `results/`](#generated-data--results)
  - [4. Running one figure at a time](#4-running-one-figure-at-a-time)


---

## 1. Setting up

### Machine requirements

| | requirements | notes |
|---|---|---|
| **OS** | Linux | We provide a docker image with the required environment |
| **CPU cores** | ≥ 26 (the more the better) | Figure 9's largest configuration runs 25 replicas + 1 client; TGS simulation is parallelized and benefits from more cores. |
| **Time** | — |  From several hours to days depending on #cores|


### Setup docker 
  ```bash
  # Install docker if needed (assuming Ubuntu)
  sudo apt update 
  sudo apt install docker.io -y

  # Pull
  sudo docker pull fyc1007261/heimdallr-artifact:v1.0 
  
  # Run
  sudo docker run --name heimdallr-artifact-docker -it fyc1007261/heimdallr-artifact:v1.0 # only need to run this once
  sudo docker start heimdallr-artifact-docker # run after reboot
  sudo docker exec -it heimdallr-artifact-docker bash # attach to the terminal inside 

  # Delete after use
  sudo docker rm heimdallr-artifact-docker
  ```



### Software and packages

```bash
cd ~
git clone git@github.com:phan-lab/Heimdallr.git
cd Heimdallr
./setup.sh            # reports what is present and what is missing;
                      # should return all OKs with the docker
```

---

## 2. Running everything at once

```bash
./run.sh all
```

This generates every dataset from source and plots the figures and tables.

Important remarks:

- **Figure 9 and Table 4 are hardware-dependent.** They measure CPU time. The
  paper used 2.4 GHz EPYC cores at one process per core. However, the trend should look the same.
- **Noises in TGS-related evaluations.** The numbers may not exactly match those in tha papers, due to the probabilistic nature of network delays and drops.
- **Some figures and results are not covered.** Figure 11 and the jitter results in Section 3.1 of the paper require a multi-machine setup in multiple geographic locations.
---

## 3. Where the results are

### Plots and tables — `outputs/`

```
outputs/figure-06.pdf   figure-06.png     … through figure-14
outputs/table-03.md     table-04.md
```

### Generated data — `results/`

The raw data your machine produced, and what the plots are built from.


## 4. Running one figure at a time

Every target has its own README covering the parameters, what is simulated or
measured, how it differs from the original code, and how to read the result.

```bash
./run.sh                       # list targets
./run.sh figure-06             # one target
```




