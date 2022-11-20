# AutoSpike使用方法

## 编译
```bash
g++ autospike.cpp -o autospike
```

## 输入文件

### 汇编代码

格式如下

```assembly
# name为测试的名字, 会作为各种输出文件的一部分, 也是主程序入口
.globl {name}, {label1}, {...}
{name}:
    {assembly code} # 多行, 可加注释
{label1}:
    {assembly code} # 多行, 可加注释
... ...
```
文件命名不要是`{name}.s`, 它会被输出文件占用, 可以用类似`{name}_.s`这种名字作个区分.

以下预定义的CSR直接使用即可, 它们的定义会直接插入到输出汇编文件的开头:
* `CSR_NUMW`
* `CSR_NUMT`
* `CSR_TID`
* `CSR_WID`
* `CSR_GDS`
* `CSR_LDS`

此外, 对`CSR_GDS`的操作会自动替换为加载`global_data`地址. 因此汇编文件中不要进行对CSR_GDS的写入操作.

### 数据文件
与Chisel仿真使用的文件相同, 一行一个int型16进制表示, 无需"0x"前导符.

## 使用
```bash
# 自动生成汇编, 生成Makefile, 运行spike, 切分和处理log文件
autospike -s 汇编文件 -d 数据文件 -w warp数目 -t 每warp线程数目 [-x 位宽(默认32)]
```
在./下生成Makefile, 合成的汇编源码是{name}.s

log文件输出到`./log`下, 每个warp一份, 命名为`{name}.{#warp}.log`. 完整的log也保留了, 命名为{name}.log.

单独运行仿真(仍然需要使用autospike预先生成汇编和Makefile):
```bash
make spike-sim
```