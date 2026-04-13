# NC2000-wasm
Run NC2000 in browser

文曲星NC2000模拟器，原版 SDL2 在 [https://github.com/wangyu-/NC2000](https://github.com/wangyu-/NC2000), 本项目在其基础上，使用 Emscripten 编译为 WebAssembly，在浏览器中运行。

可以在 [https://fwindpeak.github.io/NC2000-wasm/build/index.html](https://fwindpeak.github.io/NC2000-wasm/build/index.html) 在线运行。
目前只集成了 NC2000 3.5版的 ROM。


## 编译说明

1. 安装 Emscripten 环境，参考 [https://emscripten.org/docs/getting_started/index.html](https://emscripten.org/docs/getting_started/index.html)
2. 编译
   ```
   emcmake cmake -B build 
    emmake make -C build
   ```
3. 运行
   ```
   python -m http.server --directory build
   ```
   然后在浏览器中打开 [http://localhost:8000/wqxsim.html](http://localhost:8000/wqxsim.html) 即可运行。
   
   或者可以使用一些热更新的http服务，比如node的live-server、vite等。
   比如用[bun](https://bun.sh/)运行 `live-server`：
   ```
   bunx live-server build
   ```
   然后在浏览器中打开 [http://127.0.0.1:8080/wqxsim.html](http://127.0.0.1:8080/wqxsim.html) 即可运行。
   
# NC2000
Retro device wqx nc2000/nc2600 emulator, which emulates a 6502 SoC and peripherals. The emulator runs firmware dumped from physical device.

Cross-platform, supports running on windows/linux/mac.

nc2000/nc2600 is a series of 6502-based portable computer:
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
* 160*80 LCD with SPLD803A as LCD driver (`display.cpp` and `display.h`)
* SPDS104A DSP with speaker (`sound.cpp` and `sound.h` and `dsp` folder)
* QWERT keyboard (`key.cpp` and `key.h`)
* IrDA and Serial port (UART protocol) (`iv_uart.cpp` and `iv_uart.h`)

PCB layout:

<img width="600" alt="image" src="https://github.com/user-attachments/assets/43768f20-fb8a-4ea3-a1d3-6bf1794c4a7a">
<br>
<br>

The rest of Readme is going to be in Chinese, there are a few more english materials in [wiki](https://github.com/wangyu-/NC2000/wiki).

# Screenshots
<img width="812" alt="image" src="https://github.com/user-attachments/assets/541a872a-c93b-438b-9c44-86c8df19c784" />

<img width="405" alt="image" src="https://github.com/user-attachments/assets/8946b77e-9426-4a52-8a29-f0f7e988c189" />

<img width="405" alt="image" src="https://github.com/user-attachments/assets/13b3e1d3-822f-42a0-845d-19e1e366426f" />

<img width="405" alt="image" src="https://github.com/user-attachments/assets/a0c4e8f3-09b6-4a2f-b8f2-1998a0172351" />

<img width="405" alt="image" src="https://github.com/user-attachments/assets/4ca56d3b-24ce-427c-9646-4f319dbfbe06" />

<img width="816" alt="image" src="https://github.com/user-attachments/assets/0d281a72-f8f2-424e-b0ac-105ed01478a6" />

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

# NC2000/NC1020模拟器

文曲星nc2000/nc2600/nc1020模拟器。跨平台，可以运行在windows/linux/mac。

支持以下feature：
* 运行从真机dump的rom
* 兼容真机软件，和各种自制内核
* 下载, 上传文件, 保存
* 发音，蜂鸣器
* 4灰度
* 液晶格栅效果, 液晶残影, 液晶屏侧面的小图标
* 热键唤醒, 闹铃唤醒
* [红外/串口通信](https://github.com/wangyu-/NC2000/wiki/%E7%BA%A2%E5%A4%96-%E4%B8%B2%E5%8F%A3%E9%80%9A%E4%BF%A1)
* [即时存档](https://github.com/wangyu-/NC2000/wiki/%E5%8D%B3%E6%97%B6%E5%AD%98%E6%A1%A3)
* 超频

经测试支持的rom:
* nc2000c `官方3.5`
* nc2600c `官方3.2`, `3.5内核 by Sun`,`4.1内核 by qiqi`, `4.1内核保留剑桥版 by qiqi`, `非常4.2内核 by 41824984`, `5.0内核 by epc`
* 哈电族nc2000 `官方3.6B`
* nc1020 `官方3.6` `4.3内核by SAILOR-HB` `4.9内核 by ZHY` `5.3内核 by 逍遥人`
* 哈电族nc1020 `官方5.2`
  
# 按键

特殊键：

```
英汉 名片 计算 行程 测验 时间 网络 ：F5~F11
on/off ： F12

发音 报时：; '
求助 中英数 输入法 ： [ ] \
红外：Alt

跳出 ： ESC
翻页上 翻页下 ： , ?
```
其他的键都跟直觉相符，与电脑上的同名键对应

另:
* `TAB`可以切换快进模式
* <code>\`</code> 可以进入模拟器内置的命令行
* SHIFT + <code>\`</code> 可以切至其它键位

具体见wiki[模拟器键位](https://github.com/wangyu-/NC2000/wiki/%E6%A8%A1%E6%8B%9F%E5%99%A8%E9%94%AE%E4%BD%8D)

# 模拟器参数

### 切换型号和rom
```
nc2000 官方3.5:
 nc2000.exe --nc2000 --rom roms/nc2000  (默认参数)
nc2600 官方3.2:
 nc2000.exe --nc2000 --rom roms/nc2600
nc1020 官方3.6:
 nc2000.exe --nc1020 --rom roms/nc1020
```
nc2000/2600官方内核文件系统较慢，推荐使用下面两个"极速"内核：
```
nc2600 3.5:
 nc2000.exe --nc2000 --rom roms/35  (集成了“极速”文件系统，同时支持新旧网络，兼容性较好，单词可以发音)
nc2600 非常4.2:
 nc2000.exe --nc2000 --rom roms/fc42  (24MB扩容内核，集成了“极速”文件系统，同时支持新旧网络，兼容性较好，但是单词不能发音。里面预先下载了很多软件)
```
更多见wiki[切换不同机型和内核](https://github.com/wangyu-/NC2000/wiki/%E5%88%87%E6%8D%A2%E4%B8%8D%E5%90%8C%E6%9C%BA%E5%9E%8B%E5%92%8C%E5%86%85%E6%A0%B8)

### 超频

```
nc2000.exe --oc 2 （超频到2倍速）
```

### 调整屏幕

#### 调整背景颜色
```
nc2000.exe --rgb-scale 1,1,0.92
```
屏幕背景色为红1.0绿1.0蓝0.92，也就是淡黄色背景，效果：

<img width="500" alt="image" src="https://github.com/user-attachments/assets/e6bc079c-eb38-4bae-a706-3b9cd9108615" />

#### 调整屏幕尺寸
```
nc2000.exe --pixel-size 3 --gap-size 1 --lcd-scale 1      
```
每个wqx像素对应3个屏幕像素，wqx像素间隔对应1个屏幕像素，缩放1倍）

### 完整参数

见wiki[模拟器参数](https://github.com/wangyu-/NC2000/wiki/%E6%A8%A1%E6%8B%9F%E5%99%A8%E5%8F%82%E6%95%B0)

# 模拟器命令行

按<code>`</code>进入模拟的内置的命令行后，可以输入各种命令，如图：

<img width="500" alt="image" src="https://github.com/user-attachments/assets/ab0955ee-38ec-4757-87fb-f99eb03c6210" />

(另外还有其它方法运行命令，具体见wiki)

### 下载相关命令

`create_dir XXXX`： 在文曲星当前目录内创建一个名为XXXX的目录。 

`put aaa.bin bbb.bin` ：把本地的aaa.bin下载到文曲星的当前目录，命名为bbb.bin

`put 1.txt` 把本地的1.txt下载到文曲星的当前目录，命名为1.txt (也就是上一个命令省略了一个参数的形式)

`get aaa.bin bbb.bin` ：把文曲星的当前目录的aaa.bin上传到电脑，命名为bbb.bin


#### Note

1. 如何切换当前目录呢？ 你在文曲星上打开资源管理器，进入哪个目录，哪个目录就是你的当前目录。 也就是说你文件上传到哪里，取决于模拟器内文曲星当前所在的文件夹。
2. 以上命令需要文曲星在进入系统以后才可以运行
3. 模拟器不自带bin解密功能。下载bin文件需要在电脑上提前把bin文件解密好。

### flash保存

`save_flash`:  把模拟器对nand和nor的修改保存到硬盘。  默认模拟器是不会写硬盘的。

另外还支持ram保存，这样玩宠物猫等游戏退出模拟器后进度不会丢失，具体使用见wiki。

### 其他命令

完整命令见wiki [模拟器命令](https://github.com/wangyu-/NC2000/wiki/%E6%A8%A1%E6%8B%9F%E5%99%A8%E5%91%BD%E4%BB%A4)

# 编译

见wiki [How to compile](https://github.com/wangyu-/NC2000/wiki/How-to-compile)

# 代码基于以下项目

**这个项目本质上是sim800和wayback的fork：**

[sim800](https://github.com/banxian/Sim800 ): cc800模拟器。 作者：曾半仙

[wayback800](https://github.com/banxian/Wayback800iOS): cc800/pc1000模拟器。（sim800作者的新版）

**DSP功能基于：**

Pc1000emux。 作者：Lee。

**CPU[1], IO，中断处理参考、复用了：**

Pc1000emux，nc3000emux旧版，nc3000emux新版(没有源码) 作者：Lee。

[1] 软件默认运行wayback的handypsp cpu实现，不过也支持pc1000emux的cpu实现用做对比查错。用 `--cpu 2`可以切换至pc1000emux的cpu

**早期代码基于：**

[nc1020模拟器SDL版](https://github.com/Wang-Yue/NC1020) 作者：Wang-Yue。 貌似此版本是基于"nc1020模拟器c语言版"做了SDL移植。 

"nc1020模拟器c语言版"作者不详，代码最早应该也是基于sim800。 另外"nc1020模拟器c语言版"貌似跟[nc1020模拟器js版](https://github.com/hackwaly/jswqx)也有关系, 具体不详。

**感谢**

除了感谢以上作者外，诗诺比对本项目提供了很多帮助，特此感谢。

# Wiki

更多信息见wiki: https://github.com/wangyu-/NC2000/wiki/

