[![CMake](https://github.com/Frodez/spinet/actions/workflows/cmake.yml/badge.svg)](https://github.com/Frodez/spinet/actions/workflows/cmake.yml)
![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/Frodez/spinet)

# **spinet**

A simple network library powered by epoll and proactor pattern.

# **Installation**

**Required**  
1. cmake version 3.10 or above  
2. c++ standard 17 or above  

```
git clone https://github.com/Frodez/spinet
cmake -B spinet/build -S spinet/ -DCMAKE_BUILD_TYPE=Release
cmake --build spinet/build --config Release --target install
```

The default installation path is `spinet/release`.

# **Third-party dependencies**

1. [fmt 8.0.1](https://github.com/fmtlib/fmt/releases/tag/8.0.1)  
2. [robin-map v0.6.3](https://github.com/Tessil/robin-map/releases/tag/v0.6.3)  
