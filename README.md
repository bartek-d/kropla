# kropla
Kropki game engine

# Building

Clone the repo
```
git clone git@github.com:bartek-d/kropla.git
cd kropla
```

Init and fetch submodules
```
git submodule init
git submodule update
```

Dependencies

One needs HDF5 and googlemock. In Debian, these are provided
as `libhdf5-dev` and `libgmock-dev` packages, respectively.

Included: lz77.h from [Yet another LZ77](https://github.com/ivan-tkatchev/yalz77) (public domain), by Ivan Tkatchev.

Build
```
mkdir build
cd build
cmake ..
make kropla
```

# Running
```
./kropla
```
runs the program in an interactive mode.