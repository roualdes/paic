# paic

This repository attempts to implement the method [Posterior Averaging Information Criterion](https://www.mdpi.com/1099-4300/25/3/468) by [Dr. Shouhao Zhou](https://cancer.psu.edu/researchers/individual/-/researcher/8BD247A26C38BD2CE053C90D1DACECEA/shouhao-zhou-phd).

Posterior Averaging Information Criterion (PAIC) requires some things
which a standard [Stan](https://mc-stan.org/) program offers

* posterior draws of a Stan program, and
* MAP estimates,

and some things which a standard Stan program does not offer

* gradients of the per observation log posterior density, and
* per observation log-likelihood evaluations at posterior draws.

To obtain the latter required components of PAIC, we write custom C++
functions using [Stan Math](https://github.com/stan-dev/math).  The
components necessary for PAIC are then merged in a single R program.

## Requirements

As this repository heavily builds off Stan, we share (core) Stan
requirements and add a few more.

* [C++ toolchain](https://mc-stan.org/docs/cmdstan-guide/installation.html#cpp-toolchain)
* [CMake](https://cmake.org/)
* [R](https://cran.r-project.org/)
* [CmdStanr](https://mc-stan.org/cmdstanr/)
* [Eigen](https://libeigen.gitlab.io/eigen/docs-nightly/)

Eigen should be dealth with automatically via CMake.

## Building

I'm certainly no CMake expert.  For me the following commands build a
shared object file for each of the subdirectories (that aren't named
`build`).

```bash
# clone this repository
cd paic
cmake -S . -B build
cmake --build build
```

## Running

After building, there should be at least the file
`build/lib/logistic_r.so`; if the extension is instead `.dylib` great!
Things worked and your a Windows machine.

Open the file `paic.R`. Change the filepath extensions as
appropriate. The R code should run without issue.
