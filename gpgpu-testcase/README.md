# README

## usage
1. 编译汇编测例
```bash
make TEST=[testcasename]
```
2. 运行spike测试
```bash
make spike-sim TEST=[testcasename]
```
PS:
- make spike-sim 同样会对测例进行编译
- testcasename为测例名称，如loop
- spike-sim 还有可选参数，包括
  - P: hart数，默认为1
  - VARCH: rvv架构的vlen，elen参数，默认为 vlen:256,elen:32
  - DEBUG: 是否debug, 默认为-d
  - ISA： 支持指令集, 默认为rv64gv
3. 清空编译文件
```bash
make clean
```

## 已充分测试的测例
rvv_vadd, csr, branch-basic, branch-nested

## 测试注意事项
barrier测例wid是在软件上写死的，真的测多warp的时候记得把代码里标注的两行去掉