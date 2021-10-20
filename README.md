[![CMake](https://github.com/Frodez/spinet/actions/workflows/cmake.yml/badge.svg)](https://github.com/Frodez/spinet/actions/workflows/cmake.yml)
![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Frodez/spinet)

# **spinet**

A simple network library powered by epoll.

# **Installation**

**Required**  
1.cmake version 3.10 or above  
2.c++ standard 17 or above  

```
git clone https://github.com/Frodez/spinet
mkdir spinet/build
cd spinet/build
cmake ..
make
```

The library path is spinet/build/lib, and the example path is spinet/build/bin.

# **Third-party dependencies**

1.[fmt 8.0.1](https://github.com/fmtlib/fmt)  
2.[robin-map v0.6.3](https://github.com/Tessil/robin-map)  