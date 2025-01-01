# NC2000
Retro device wqx nc2000/nc2600 emulator, which emulates a 6502 SoC and peripherals. The emulator runs the offical firmware.

Cross-platform, supports running on windows/linux/mac.

nc2000/nc2600 is a series of electric dictionary:
<img width="700" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/e9d5bca8-2824-442b-9e22-b13e5e4a0eb6"> 
<br>
[image source](https://www.bilibili.com/video/BV1EW411C77i)

The device has following hardware:

* 5MHZ 6502 CPU, integrated inside SPDC1024 SoC    (implemented in `cpu.cpp` and `cpu.h`)
* 512K x 8 Bit Nor Flash with Bus Flash interface, named SPR4096 (`nor.cpp` `nor.h`)
* 32M x 8 Bit NAND Flash Memory, supplied by either Samsung or Toshiba (`nand.cpp` and `nand.h`)
* 24K internal ram, 32K external ram, 4k addition ram built inside SPR4096  (`ram.cpp` and `ram.h`)
* 00h~03Fh as special registers of SoC, also called IO port (`io.cpp` `io.h`)
* memory mapping from 0000h~FFFFh controlled by `00h` as "bank switch", `0Ah[0:3]` as "BIOS bank switch", `0Ah[8]` as "ROM/RAM select", `0D[0:1]` as "volume select", `0D[2]` as "ramb select" (`mem.cpp` `mem.h`)
* 160*80 LCD with SPLD803A as LCD driver (currently as part of `main.cpp` and `nc2000.cpp`)
* QWERT keyboard (currently as part of `main.cpp` and `nc2000.cpp`)

PCB layout:

<img width="600" alt="image" src="https://github.com/user-attachments/assets/43768f20-fb8a-4ea3-a1d3-6bf1794c4a7a">
<br>
<br>

The rest of Readme is going to be in Chinese， I will put english materials in [wiki](https://github.com/wangyu-/NC2000/wiki)

# Screenshots

<img width="405" alt="image" src="https://github.com/user-attachments/assets/fc9514bc-4733-40a4-966d-10c6021a8bb1">
<img width="405" alt="image" src="https://github.com/user-attachments/assets/f6449eb2-d1df-4922-ad62-55eb23b158a2">

<br>

<img width="270" alt="image" src="https://github.com/user-attachments/assets/03c426b6-b2bc-434d-adf6-97dbe8f60144">
<img width="270" alt="image" src="https://github.com/user-attachments/assets/3ff956a8-83b9-42bf-a57d-63398c73c981">
<img width="270" alt="image" src="https://github.com/user-attachments/assets/b95e775c-2e61-40f8-bd37-905af0d6c772">


<br>
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/991604a8-2b8a-442f-885a-f94f3d40b868">
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/15f89720-2ba4-41b6-91f0-5041deb68c3b">
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/ce8f6dea-a2ab-46ac-bdc0-6a93b821640e">

<br>

<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/d2d0d8b7-a291-4778-9d2c-511ce0c4017e">
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/b699cf4a-4831-4203-9446-9b6f7f257caa">
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/07fc2e49-46f8-4d84-8402-7bbdbd33d239">

# NC2000

文曲星nc2000/nc2600模拟器。跨平台，可以运行在windows/linux/mac。

支持以下feature：
* 下载, 上传文件
* 保存状态
* 发音/beeper
* 4灰度
* 液晶格栅效果, 液晶残影

# nand 和 nor
因为可能有版权问题，暂时没放到repo里。 需要使用从真机dump的rom。

nand命名为：`nand.bin`

nor命名为：`nor.bin`

# 按键

特殊键：

`英汉 名片 计算 行程 时间 游戏 网络` ：`F5~F11`

`on/off` ： `F12`

`求助 中英数 输入法` ： `[ ] \`

`跳出` ： `ESC`

`翻页上 翻页下` ： `, ?`

其他的键都跟直觉相符，与电脑上的同名键对应

另外按TAB可以切换快进模式

# 模拟器命令行

模拟器启动后会监听在udp 9000端口，可以接受外部发来的命令。 实现下载，调整模拟器速度等功能。 

推荐用netcat向模拟器发送命令， 比如：

```
nc -u 127.0.0.1 9000 <回车>
speed 0.5 <回车>        //把模拟器速度调为0.5倍
create_dir XXXX <回车>    //在模拟器创建一个名为XXXX的目录
....                     //只要nc和模拟器不关，可以继续发送命令
```


### 下载相关命令

`create_dir XXXX`： 在文曲星当前目录内创建一个名为XXXX的目录。 

`put aaa.bin bbb.bin` ：把本地的aaa.bin上传到文曲星的当前目录，命名为bbb.bin

`put 1.txt` 把本地的1.txt上传到文曲星的当前目录，命名为1.txt (也就是上一个命令省略了一个参数的形式)

`get aaa.bin bbb.bin` ：把文曲星的当前目录的aaa.bin下载到电脑，命名为bbb.bin


#### Note

1. 如何切换当前目录呢？ 你在文曲星上打开资源管理器，进入哪个目录，哪个目录就是你的当前目录。 也就是说你文件上传到哪里，取决于模拟器内文曲星当前所在的文件夹。
2. 以上命令需要文曲星在进入系统以后才可以运行
3. 模拟器暂时不支持bin解密。上传bin文件需要在电脑上提前把bin文件解密好。

### flash保存

`save_flash`:   把模拟器对nand和nor的修改保存到硬盘。  默认模拟器是不会写硬盘的。

### 其他命令

`speed 2` :  把模拟器运行速度调为2倍

`file_manager` : 强制文曲星打开资源管理器。 主要是为了管理文件方便，免得手工一层一层得进入。

`dump 4000 100` : 从地址0x4000开始，dump 100个字节的内存打印出来。

`ec 4000 00 27 05 60`: 把16进制数据 0x00 0x27 0x05 0x60写入 地址0x4000开始的内存， 有多少参数就写多少字节。

`wqxhex` : 强制模拟器打开wqxhex, 方便调试bug。 需要本地目录有wqxhex.bin文件。

### 更改监听端口

如果9000端口被占用，或者是需要多开模拟器，可以修改监听的端口。

比如 `a.exe 8000` 就是监听在8000端口。

### 已知问题

1. 超级玛丽、淘金者等游戏按键不能用
2. nc2000报时报不出来

### TODO

1. 模拟更多的I/O。

### Note on compile

```
git clone https://github.com/wangyu-/NC2000.git
cd NC2000
git clone https://github.com/wangyu-/wqxdsp.git dsp
cmake .
make
```

# 代码基于以下项目

**这个项目本质上是sim800和wayback的fork：**

https://github.com/banxian/Sim800 sim800: cc800模拟器。 作者：曾半仙

https://github.com/banxian/Wayback800iOS wayback800: cc800/pc1000模拟器。（sim800作者的新版）

**CPU[1]、DSP功能、部分IO，复用或者参考了：**

Pc1000emux，nc3000emux旧版。 作者：Lee。

nc3000emux新版(没有源码)

**早期代码复用或参考了：**

https://github.com/hackwaly/NC1020 nc1020模拟器，应该是基于sim800开发。 (不确定URL是否是原作者）

https://github.com/Wang-Yue/NC1020 nc1020模拟器SDL版。应该是上一个项目的fork。

[1] 支持两个cpu实现，其中一个是wayback中的handypsp实现，另一个是pc1000mux的c6502.cpp实现。 可以在编译时切换。
