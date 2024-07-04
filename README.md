# NC2000
文曲星nc2000/nc2600模拟器

<img width="327" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/05079aab-d3ae-4938-868c-c2eca7c58244">
<img width="327" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/4a75209e-a200-4250-bd3b-e9fc4d4ca390">  
<br>
<img width="327" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/f05a426f-d5e5-4190-880e-9fe40570d58f">
<img width="323" alt="image" src="https://github.com/wangyu-/NC2000/assets/4922024/ce8f6dea-a2ab-46ac-bdc0-6a93b821640e">

代码刚刚调通，非常乱，暂时不要尝试阅读，后面会整理。 nand代码非常tricky，主要是为了先跑通证明可行性，后面会重写。

后面会做下载和发音。

# nand 和 nor
因为可能有版权问题，暂时没放到repo里。 可以从官方2600模拟器里自取rom。

nand命名为：`2600nand.bin`

nor命名为：`2600nor.bin`


# 代码基于以下项目

原版： https://github.com/banxian/Sim800   （初始code）

1改： https://github.com/hackwaly/jswqx   （用js重写，nc1020支持）

2改：https://github.com/hackwaly/NC1020  (上面版本用c++重写) （仅猜测，不确定，也有可能上一版本是参考这个。另外不确定URL是否是原作者repo） 

3改：https://github.com/Wang-Yue/NC1020 （上面版本移植到sdl） 

（所以本作算4改版本）
