# memchk
嵌入式arm平台内存泄漏检测工具。主要因为valgrind在嵌入式平台跑起来难度很大

# 编译
- 修改CMakeLists.txt,将arm-linux-gnueabihf改为自己平台的工具链前缀，注意没有-
- mkdir build
- cd build
- cmake ..
- make

# 使用
- 将sample拷贝到开发板某个路径下
- 将libmemchk.so拷贝到开发板某个路径下，比如/tmp/libmemchk.so
- LD_PRELOAD=/tmp/libmemchk.so ./sample
- 开启一个新的终端,执行killall -USR1 sample
- 查看是哪个符号出现的内存泄漏：addr2line 0xxxx -f -e program

# 作者
- treeswayinwind@gmail.com
- 企鹅：279191230
