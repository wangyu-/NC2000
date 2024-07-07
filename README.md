# NC2000
文曲星nc2000/nc2600模拟器。跨平台，支持windows/linux/mac。

<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/05079aab-d3ae-4938-868c-c2eca7c58244">
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/4a75209e-a200-4250-bd3b-e9fc4d4ca390">  
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/f05a426f-d5e5-4190-880e-9fe40570d58f">

<br>
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/991604a8-2b8a-442f-885a-f94f3d40b868">
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/15f89720-2ba4-41b6-91f0-5041deb68c3b">
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/ce8f6dea-a2ab-46ac-bdc0-6a93b821640e">

<br>

<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/d2d0d8b7-a291-4778-9d2c-511ce0c4017e">
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/b699cf4a-4831-4203-9446-9b6f7f257caa">
<img width="270" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/07fc2e49-46f8-4d84-8402-7bbdbd33d239">


代码刚刚调通，非常乱，暂时不要尝试阅读，后面会整理。 nand代码非常tricky，主要是为了先跑通证明可行性，后面会重写。

后面会做发音。

# nand 和 nor
因为可能有版权问题，暂时没放到repo里。 可以从官方nc2000或nc2600模拟器里自取rom。

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

# 模拟命令行

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

#### Note

1. 如何切换当前目录呢？ 你在文曲星上打开资源管理器，进入哪个目录，哪个目录就是你的当前目录。 也就是说你文件上传到哪里，取决于模拟器内文曲星当前所在的文件夹。
2. 以上命令需要文曲星在进入系统以后才可以运行
3. 模拟器暂时不支持bin解密。上传bin文件需要在电脑上提前把bin文件解密好。
4. 下载功能只做了nc2600。nc2000目前未做，强行使用可能会死机。

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

1. 某些游戏速度不正常。 大概是某个timer没有正确实现。 可以暂时用speed命令调整速度，让游戏正常。

2. 在资源管理器里运行“空间整理”会死机。 nand模拟的小问题，以后会修。

# 代码基于以下项目

原版： https://github.com/banxian/Sim800   （初始code）

1改： https://github.com/hackwaly/jswqx   （用js重写，nc1020支持）

2改：https://github.com/hackwaly/NC1020  (上面版本用c++重写) （仅猜测，不确定，也有可能上一版本是参考这个。另外不确定URL是否是原作者repo） 

3改：https://github.com/Wang-Yue/NC1020 （上面版本移植到sdl） 

（所以本作算4改版本）
