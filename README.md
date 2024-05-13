# TapeSorter

Tape Sorter implementation. 

# How to compile and run

For now, you also need to change main.cpp global static veriables to the correct paths:

for example

```cpp
const char* IN_PATH      = "w:\\tapes\\in_tape";
const char* OUT_PATH     = "w:\\tapes\\out_tape";
const char* TMP_DIR_PATH = "w:\\tapes\\tmp\\";
```

Use any c++ compiler supportig c++17 standard. For example clang++:

```
clang++ main.cpp -std=c++17
```

and then run the executable
