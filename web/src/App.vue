<script setup lang="ts">
import { ref, onMounted } from 'vue';
import WqxsimModule from '@/assets/wqxsim.js';
import VirtualKeyboard from '@/components/VirtualKeyboard.vue';
import FileManager from '@/components/FileManager.vue';

interface RomConfig {
  name: string;
  args: string[];
  files: Array<{
    url: string;
    vfsPath: string;
  }>;
}

// ROM 配置
const romConfigs: Record<string, RomConfig> = {
  'nc2000': {
    name: 'nc2000/2600 官方3.5',
    args: ['--nc2000', '--rom', 'roms/nc2000'],
    files: [
      { url: 'roms/nc2000.nand', vfsPath: '/roms/nc2000.nand' },
      { url: 'roms/nc2000.nand0', vfsPath: '/roms/nc2000.nand0' },
      { url: 'roms/nc2000.nor', vfsPath: '/roms/nc2000.nor' }
    ]
  },
  'nc2600c_fc42': {
    name: 'nc2600c 非常4.2 by 41824984 - 24MB扩容',
    args: ['--nc2000', '--rom', 'roms/fc42'],
    files: [
      { url: 'roms/fc42.nand', vfsPath: '/roms/fc42.nand' },
      { url: 'roms/fc42.nand0', vfsPath: '/roms/fc42.nand0' },
      { url: 'roms/fc42.nor', vfsPath: '/roms/fc42.nor' }
    ]
  }
};

// 需要加载的文件
const filesToLoad = [
  // Resource files
  { url: 'resource/lcdstripe_slice_w938.json', vfsPath: '/resource/lcdstripe_slice_w938.json' },
  { url: 'resource/lcdstripe_slice_w1313.json', vfsPath: '/resource/lcdstripe_slice_w1313.json' },
  { url: 'resource/lcdstripe_w938.bmp', vfsPath: '/resource/lcdstripe_w938.bmp' },
  { url: 'resource/lcdstripe_w1313.bmp', vfsPath: '/resource/lcdstripe_w1313.bmp' },
  // ROM files
  { url: 'roms/nc2000.nand', vfsPath: '/roms/nc2000.nand' },
  { url: 'roms/nc2000.nand0', vfsPath: '/roms/nc2000.nand0' },
  { url: 'roms/nc2000.nor', vfsPath: '/roms/nc2000.nor' },
  { url: 'roms/fc42.nand0', vfsPath: '/roms/fc42.nand0' },
  { url: 'roms/fc42.nor', vfsPath: '/roms/fc42.nor' },
  { url: 'roms/fc42.nand', vfsPath: '/roms/fc42.nand' },
];

const isDrawerOpen = ref(false);
const currentRom = ref('nc2000');
const currentZoom = ref(1.0);
const isFitMode = ref(false);
const autoFitEnabled = ref(false);
const statusText = ref('Downloading...');
const progressValue = ref(0);
const progressMax = ref(0);
const showProgress = ref(false);
const showSpinner = ref(true);
const outputText = ref('');
const romStatusText = ref('');


// DOM 元素引用
const canvasRef = ref<HTMLCanvasElement | null>(null);
const zoomContainerRef = ref<HTMLDivElement | null>(null);
const outputRef = ref<HTMLTextAreaElement | null>(null);

// WASM 实例
let wasmInstance: any = null;


// 切换抽屉状态
function toggleDrawer() {
  isDrawerOpen.value = !isDrawerOpen.value;
}

// 关闭抽屉
function closeDrawer() {
  isDrawerOpen.value = false;
}

// 应用缩放
function applyZoom(zoomLevel: number) {
  currentZoom.value = parseFloat(zoomLevel.toString());
  if (zoomContainerRef.value) {
    zoomContainerRef.value.style.transform = `scale(${currentZoom.value})`;
  }
}

// 重置缩放
function resetZoom() {
  applyZoom(1.0);
  isFitMode.value = false;
  autoFitEnabled.value = false;
}

// 适应屏幕缩放
function fitToScreen() {
  if (!zoomContainerRef.value) return;

  // 获取视口高度和宽度
  const viewportHeight = document.body.clientHeight;
  const viewportWidth = document.body.clientWidth;
  console.log('viewportWidth', viewportWidth);

  // 获取缩放容器的原始尺寸
  const containerRect = zoomContainerRef.value.getBoundingClientRect();
  const containerHeight = containerRect.height;
  const containerWidth = containerRect.width;

  // 计算可用空间
  const availableHeight = viewportHeight - 250;
  const availableWidth = viewportWidth - 20;

  // 计算缩放比例
  const heightScale = availableHeight / containerHeight;
  const widthScale = availableWidth / containerWidth;

  // 使用较小的缩放比例
  const fitZoom = Math.min(heightScale, widthScale);
  // const minZoom = Math.max(fitZoom, 0.2);

  console.log('fitZoom', fitZoom, availableWidth, containerWidth);

  applyZoom(fitZoom);
}

// 设置缩放控制
function setupZoomControls() {
  // 窗口大小变化时，如果是适应屏幕模式或自动适应模式，则重新计算
  window.addEventListener('resize', () => {
    if (isFitMode.value || autoFitEnabled.value) {
      fitToScreen();
    }
  });
}

// 应用ROM更改
async function applyRomChange() {
  if (!wasmInstance) {
    romStatusText.value = 'WASM模块尚未加载完成，请稍后再试';
    return;
  }

  try {
    romStatusText.value = '正在切换ROM...';

    // 重新加载WASM
    await restartWithNewRom();
    const romConfig = romConfigs[currentRom.value];
    romStatusText.value = romConfig ? `已切换到: ${romConfig.name || ''}` : `已切换到未知 ROM`;
  } catch (error) {
    console.error('切换ROM时出错:', error);
    romStatusText.value = `切换ROM失败: ${(error as Error).message}`;
  }
}

// 使用新ROM重新启动
async function restartWithNewRom() {
  if (!wasmInstance) {
    throw new Error('WASM实例未初始化');
  }

  const romConfig = romConfigs[currentRom.value];

  // 重新调用main函数
  console.log("使用新ROM重新启动...");
  wasmInstance.callMain(romConfig?.args || []);
  console.log("wqxsim 已使用新ROM启动。");
}

// 设置屏幕适应
function setUpScreenFit() {
  // 设置缩放控制
  setupZoomControls();

  // 延迟执行自动适应屏幕
  setTimeout(() => {
    fitToScreen();
    isFitMode.value = true;
    autoFitEnabled.value = true;
  }, 100);
}




// 异步加载和运行WASM
async function loadAndRun() {
  try {
    // 加载 Wasm 模块
    const Module = {
      print: (...args: any[]) => {
        console.log(...args);
        if (outputRef.value) {
          const text = args.join(' ');
          outputText.value += text + "\n";
          outputRef.value.scrollTop = outputRef.value.scrollHeight;
        }
      },
      canvas: canvasRef.value,
      setStatus: (text: string) => {
        console.log('setStatus', text);
        if (text.includes('loading...')) {
          showSpinner.value = true;
        } else {
          showSpinner.value = false;
        }

        statusText.value = text;
      },
      totalDependencies: 0,
      monitorRunDependencies: function (left: number) {
        console.log('monitorRunDependencies', left);
        this.totalDependencies = Math.max(this.totalDependencies, left);
        this.setStatus(left ? 'Preparing... (' + (this.totalDependencies - left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
      },
      onRuntimeInitialized: function () {
        console.log('onRuntimeInitialized');
      }
    };

    Module.setStatus('Downloading...');

    window.onerror = () => {
      Module.setStatus('Exception thrown, see JavaScript console');
      showSpinner.value = false;
      Module.setStatus = (text) => {
        if (text) console.error('[post-exception status] ' + text);
      };
      return false;
    };

    const instance = await WqxsimModule(Module);
    wasmInstance = instance;

    console.log("Wasm 模块已加载，准备文件系统...", instance);

    // 获取 FS API
    const FS = instance.FS;

    // 下载所有文件
    Module.setStatus(`Downloading ${filesToLoad.length} asset(s)...`);
    const fetchPromises = filesToLoad.map(async (file) => {
      const response = await fetch(file.url);
      if (!response.ok) {
        throw new Error(`Failed to fetch ${file.url}: ${response.statusText}`);
      }
      const data = await response.arrayBuffer();
      return { data: new Uint8Array(data), path: file.vfsPath };
    });

    const loadedFiles = await Promise.all(fetchPromises);

    // 将所有已下载的文件写入 VFS
    Module.setStatus('Files downloaded. Writing to virtual file system...');
    for (const file of loadedFiles) {
      const lastSlashIndex = file.path.lastIndexOf('/');
      const dirPath = lastSlashIndex > 0 ? file.path.substring(0, lastSlashIndex) : '.';
      FS.mkdirTree(dirPath);
      FS.writeFile(file.path, file.data);
      console.log(`VFS: Wrote ${file.path}`);
    }


    console.log("文件系统已准备就绪。即将启动 main()...");
    Module.setStatus(' ');

    // 使用当前选择的ROM配置
    const romConfig = romConfigs[currentRom.value] || { args: [] };
    // 手动调用 main()
    instance.callMain(romConfig.args);
    console.log("wqxsim 已启动。");

  } catch (err) {
    statusText.value = 'Error during startup. See console.';
    console.error(err);
    showSpinner.value = false;
  }
}

// 处理自动适应屏幕变化
function handleAutoFitChange(event: Event) {
  const target = event.target as HTMLInputElement;
  autoFitEnabled.value = target.checked;
  if (autoFitEnabled.value) {
    fitToScreen();
    isFitMode.value = true;
  } else {
    isFitMode.value = false;
  }
}

// 处理缩放滑块变化
function handleZoomSliderChange(event: Event) {
  const target = event.target as HTMLInputElement;
  applyZoom(parseFloat(target.value));
  isFitMode.value = false;
  autoFitEnabled.value = false;
}

// 生命周期钩子
onMounted(() => {

  // 设置屏幕适应
  setUpScreenFit();

  // 加载和运行WASM
  loadAndRun();

  // // 设置WebGL上下文丢失处理
  // if (canvasRef.value) {
  //   canvasRef.value.addEventListener('webglcontextlost', (e) => {
  //     alert('WebGL context lost. You will need to reload the page.');
  //     e.preventDefault();
  //   }, false);
  //   // 确保canvas有正确的尺寸
  //   canvasRef.value.width = 935;
  //   canvasRef.value.height = 400;
  // }


});
</script>

<template>

  <div class="app">


    <!-- 抽屉式控件容器 -->
    <div class="drawer" :class="{ open: isDrawerOpen }">
      <h2>- </h2>

      <!-- ROM Selection -->
      <div class="control-section">
        <h3>ROM选择</h3>
        <select v-model="currentRom" class="full-width">
          <option v-for="(config, key) in romConfigs" :key="key" :value="key">
            {{ config.name }}
          </option>
        </select>
        <button @click="applyRomChange" class="primary-button full-width">应用</button>
        <div class="status-text">{{ romStatusText }}</div>
      </div>

      <!-- 缩放控制 -->
      <div class="control-section">
        <h3>缩放控制</h3>
        <div class="zoom-controls">
          <label for="zoom-slider">缩放:</label>
          <input type="range" id="zoom-slider" min="0.2" max="3" step="0.1" :value="currentZoom"
            @input="handleZoomSliderChange" class="full-width">
          <div class="zoom-display">
            <span>{{ Math.round(currentZoom * 100) }}%</span>
            <button @click="resetZoom" class="secondary-button">重置</button>
          </div>
        </div>
        <label class="checkbox-label">
          <input type="checkbox" id="auto-fit" :checked="autoFitEnabled" @change="handleAutoFitChange">
          <span>自适应屏幕</span>
        </label>
      </div>
      <FileManager :wasmInstance="wasmInstance" />

    </div>

    <!-- 抽屉遮罩层 -->
    <div class="drawer-overlay" :class="{ show: isDrawerOpen }" @click="closeDrawer"></div>

    <!-- 抽屉开关按钮 -->
    <button class="drawer-toggle" @click="toggleDrawer">☰</button>

    <div class="header">
      <h1>WQXSIM</h1>
      <div class="status">
        <div class="spinner" v-show="showSpinner"></div>
        <div class="emscripten">{{ statusText }}</div>
      </div>
    </div>

    <div class="emscripten">
      <progress :value="progressValue" :max="progressMax" v-show="showProgress"></progress>
    </div>

    <!-- 缩放容器，包含屏幕和虚拟键盘 -->
    <div ref="zoomContainerRef" class="zoom-container">
      <div class="emscripten_border">
        <canvas ref="canvasRef" class="emscripten" oncontextmenu="event.preventDefault()" tabindex="-1" width="972"
          height="435"></canvas>
        <div class="screen_num">
          <span v-for="n in [1, 2, 3, 4, 5, 6, 7, 8, 9]">{{ n }}</span>
        </div>
      </div>
      <textarea ref="outputRef" v-model="outputText" rows="8" style="display: none;"></textarea>
      <VirtualKeyboard :wasmInstance="wasmInstance" />
    </div>

  </div>

</template>

<style lang="less">
// 基础样式
body {
  font-family: Arial, sans-serif;
  margin: 0;
  padding: 0;
  // display: flex;
  // flex-direction: column;
  // align-items: center;
  // background-color: #f0f0f0;
  // overflow-x: hidden;

  select {
    padding: 8px;
    margin-bottom: 10px;
    border-radius: 4px;
    border: 1px solid #ccc;
  }
}

.app {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
}

// 移动设备检测
.mobile-device {
  --drawer-width: 280px;
}

// 桌面设备样式
.desktop-device {
  --drawer-width: 350px;
}

// 抽屉样式
.drawer {
  position: fixed;
  top: 0;
  left: 0;
  width: var(--drawer-width);
  height: 100vh;
  background-color: #fff;
  box-shadow: 2px 0 10px rgba(0, 0, 0, 0.1);
  z-index: 1000;
  transform: translateX(-100%);
  transition: transform 0.3s ease-out;
  overflow-y: auto;
  padding: 20px;
  box-sizing: border-box;

  h2 {
    margin-top: 0;
    color: #333;
    border-bottom: 1px solid #eee;
    padding-bottom: 10px;
  }

  h3 {
    margin-top: 0;
    margin-bottom: 10px;
    font-size: 16px;
  }

  .control-section {
    margin-bottom: 20px;
  }

  .full-width {
    width: 100%;
  }

  .primary-button {
    padding: 8px;
    background-color: #3498db;
    color: white;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    margin-bottom: 10px;

    &:hover {
      background-color: #2980b9;
    }

    &:disabled {
      background-color: #bdc3c7;
      cursor: not-allowed;
    }
  }

  .secondary-button {
    padding: 5px 10px;
    background-color: #3498db;
    color: white;
    border: none;
    border-radius: 3px;
    cursor: pointer;

    &:hover {
      background-color: #2980b9;
    }
  }

  .status-text {
    margin-top: 5px;
    font-size: 12px;
    color: #666;
  }

  .zoom-controls {
    margin-bottom: 10px;

    label {
      display: block;
      margin-bottom: 5px;
    }

    .zoom-display {
      display: flex;
      justify-content: space-between;
      margin-top: 5px;

      span {
        font-weight: bold;
      }
    }
  }

  .checkbox-label {
    display: flex;
    align-items: center;

    input {
      margin-right: 8px;
    }
  }

  &.open {
    transform: translateX(0);
  }
}

.drawer-overlay {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-color: rgba(0, 0, 0, 0.5);
  z-index: 999;
  display: none;

  &.show {
    display: block;
  }
}

.drawer-toggle {
  position: fixed;
  top: 5px;
  left: 15px;
  z-index: 1001;
  background-color: #3498db;
  color: white;
  border: none;
  border-radius: 50%;
  width: 50px;
  height: 50px;
  display: flex;
  justify-content: center;
  align-items: center;
  cursor: pointer;
  box-shadow: 0 2px 5px rgba(0, 0, 0, 0.2);
  font-size: 24px;

  &:active {
    background-color: #2980b9;
  }
}

.header {
  display: flex;
  flex-direction: column;
  align-items: center;

  h1 {
    display: flex;
    justify-content: center;
    align-items: center;
    padding: 10px;
    margin: 0;
  }
}

.emscripten_border {
  border: 1px solid #ccc;
  margin: 10px 0;
  background-color: #fff;
  display: inline-block;
}

.screen_num {
  padding-left: 218px;
  display: flex;
  justify-content: flex-start;
  gap: 70px;
}

.emscripten {
  display: block;
}

canvas.emscripten {
  border: none;
  background-color: black;
  // width: 935px;
  // height: 400px;
}

#status {
  font-size: 14px;
  text-align: center;
  margin-top: 10px;
}

.spinner {
  margin: 10px auto;
  width: 30px;
  height: 30px;
  position: relative;

  &:before {
    content: "";
    display: block;
    position: absolute;
    width: 100%;
    height: 100%;
    border-radius: 50%;
    border: 3px solid rgba(0, 0, 0, 0.1);
    border-top-color: #3498db;
    animation: spin 1s linear infinite;
  }
}

@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}

#output {
  width: 100%;
  max-width: 800px;
  height: 150px;
  margin-top: 10px;
  font-family: monospace;
  resize: none;
}


.zoom-container {
  transform-origin: top center;
  // transition: transform 0.3s ease;
}
</style>