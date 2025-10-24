# NC2000-wasm
Run NC2000 in browser

文曲星NC2000模拟器，原版 SDL2 在 [https://github.com/wangyu-/NC2000](https://github.com/wangyu-/NC2000), 本项目在其基础上，使用 Emscripten 编译为 WebAssembly，在浏览器中运行。

可以在 [https://fwindpeak.github.io/NC2000-wasm/build/wqxsim.html](https://fwindpeak.github.io/NC2000-wasm/build/wqxsim.html) 在线运行。
目前只集成了 NC2000 3.5版的 ROM，后续会考虑集成其他版本的 ROM。


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
