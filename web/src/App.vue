<script setup lang="ts">
import { ref, onMounted } from 'vue';
import WqxsimModule from '@/assets/wqxsim.js';

// 类型定义
interface KeyData {
  label: string;
  subscript: string;
  superscript: string;
  sdlKeys: string[];
}

interface RomConfig {
  name: string;
  args: string[];
  files: Array<{
    url: string;
    vfsPath: string;
  }>;
}

// interface FileData {
//   data: Uint8Array;
//   path: string;
// }

interface LoadedFile {
  path: string;
  name: string;
  size: number;
}

// SDL 键码映射
const SDLKeycodes: Record<string, number> = {
  SDLK_F1: 0x4000003A,
  SDLK_F2: 0x4000003B,
  SDLK_F3: 0x4000003C,
  SDLK_F4: 0x4000003D,
  SDLK_F5: 0x4000003E,
  SDLK_F6: 0x4000003F,
  SDLK_F7: 0x40000040,
  SDLK_F8: 0x40000041,
  SDLK_F9: 0x40000042,
  SDLK_F10: 0x40000043,
  SDLK_F11: 0x40000044,
  SDLK_F12: 0x40000045,
  SDLK_ESCAPE: 0x0000001B,
  SDLK_1: 0x00000031,
  SDLK_2: 0x00000032,
  SDLK_3: 0x00000033,
  SDLK_4: 0x00000034,
  SDLK_5: 0x00000035,
  SDLK_6: 0x00000036,
  SDLK_7: 0x00000037,
  SDLK_8: 0x00000038,
  SDLK_9: 0x00000039,
  SDLK_0: 0x00000030,
  SDLK_MINUS: 0x0000002D,
  SDLK_EQUALS: 0x0000003D,
  SDLK_BACKSPACE: 0x00000008,
  SDLK_TAB: 0x00000009,
  SDLK_q: 0x00000071,
  SDLK_w: 0x00000077,
  SDLK_e: 0x00000065,
  SDLK_r: 0x00000072,
  SDLK_t: 0x00000074,
  SDLK_y: 0x00000079,
  SDLK_u: 0x00000075,
  SDLK_i: 0x00000069,
  SDLK_o: 0x0000006F,
  SDLK_p: 0x00000070,
  SDLK_LEFTBRACKET: 0x0000005B,
  SDLK_RIGHTBRACKET: 0x0000005D,
  SDLK_RETURN: 0x0000000D,
  SDLK_a: 0x00000061,
  SDLK_s: 0x00000073,
  SDLK_d: 0x00000064,
  SDLK_f: 0x00000066,
  SDLK_g: 0x00000067,
  SDLK_h: 0x00000068,
  SDLK_j: 0x0000006A,
  SDLK_k: 0x0000006B,
  SDLK_l: 0x0000006C,
  SDLK_SEMICOLON: 0x0000003B,
  SDLK_QUOTE: 0x00000027,
  SDLK_BACKSLASH: 0x0000005C,
  SDLK_z: 0x0000007A,
  SDLK_x: 0x00000078,
  SDLK_c: 0x00000063,
  SDLK_v: 0x00000076,
  SDLK_b: 0x00000062,
  SDLK_n: 0x0000006E,
  SDLK_m: 0x0000006D,
  SDLK_COMMA: 0x0000002C,
  SDLK_PERIOD: 0x0000002E,
  SDLK_SLASH: 0x0000002F,
  SDLK_SPACE: 0x00000020,
  SDLK_UP: 0x40000052,
  SDLK_DOWN: 0x40000051,
  SDLK_LEFT: 0x40000050,
  SDLK_RIGHT: 0x4000004F,
  SDLK_LSHIFT: 0x00000100,
  SDLK_RSHIFT: 0x00000101
};

// 键盘矩阵
const keyMatrix: (KeyData | null)[][] = [
  // Row 2
  [{ label: 'ON/OFF', subscript: '', superscript: '', sdlKeys: ['SDLK_F12'] },
    null, null, null, null, null,
  { label: 'F1', subscript: '', superscript: '插入', sdlKeys: ['SDLK_F1'] },
  { label: 'F2', subscript: '', superscript: '删除', sdlKeys: ['SDLK_F2'] },
  { label: 'F3', subscript: '', superscript: '查找', sdlKeys: ['SDLK_F3'] },
  { label: 'F4', subscript: '', superscript: '修改', sdlKeys: ['SDLK_F4'] }],
  // Row 3
  [{ label: '发音', subscript: '', superscript: '', sdlKeys: ['SDLK_SEMICOLON'] },
  { label: '报时', subscript: '', superscript: '', sdlKeys: ['SDLK_QUOTE'] },
    null,
  { label: '英汉', subscript: '', superscript: '汉英', sdlKeys: ['SDLK_F5'] },
  { label: '名片', subscript: '', superscript: '通讯', sdlKeys: ['SDLK_F6'] },
  { label: '计算', subscript: '', superscript: '换算', sdlKeys: ['SDLK_F7'] },
  { label: '行程', subscript: '', superscript: '记事', sdlKeys: ['SDLK_F8'] },
  { label: '资料', subscript: '', superscript: '游戏', sdlKeys: ['SDLK_F9'] },
  { label: '时间', subscript: '', superscript: '其他', sdlKeys: ['SDLK_F10'] },
  { label: '网络', subscript: '', superscript: '', sdlKeys: ['SDLK_F11'] }],
  // Row 5 - Q row
  [{ label: 'Q', subscript: 'sin', superscript: 'sin-1', sdlKeys: ['SDLK_q'] },
  { label: 'W', subscript: 'cos', superscript: 'cos-1', sdlKeys: ['SDLK_w'] },
  { label: 'E', subscript: 'tan', superscript: 'tan-1', sdlKeys: ['SDLK_e'] },
  { label: 'R', subscript: '1/X', superscript: 'hyp', sdlKeys: ['SDLK_r'] },
  { label: 'T', subscript: '7', superscript: '', sdlKeys: ['SDLK_t', 'SDLK_7'] },
  { label: 'Y', subscript: '8', superscript: '', sdlKeys: ['SDLK_y', 'SDLK_8'] },
  { label: 'U', subscript: '9', superscript: '', sdlKeys: ['SDLK_u', 'SDLK_9'] },
  { label: 'I', subscript: '%', superscript: '', sdlKeys: ['SDLK_i'] },
  { label: 'O', subscript: '÷', superscript: '#', sdlKeys: ['SDLK_o'] },
  { label: 'P', subscript: 'MC', superscript: '☎', sdlKeys: ['SDLK_p'] }],
  // Row 6 - A row
  [{ label: 'A', subscript: 'log', superscript: '10x', sdlKeys: ['SDLK_a'] },
  { label: 'S', subscript: 'ln', superscript: 'ex', sdlKeys: ['SDLK_s'] },
  { label: 'D', subscript: 'Xʸ', superscript: 'y√x', sdlKeys: ['SDLK_d'] },
  { label: 'F', subscript: '√', superscript: 'X²', sdlKeys: ['SDLK_f'] },
  { label: 'G', subscript: '4', superscript: '', sdlKeys: ['SDLK_g', 'SDLK_4'] },
  { label: 'H', subscript: '5', superscript: '', sdlKeys: ['SDLK_h', 'SDLK_5'] },
  { label: 'J', subscript: '6', superscript: '', sdlKeys: ['SDLK_j', 'SDLK_6'] },
  { label: 'K', subscript: '±', superscript: '', sdlKeys: ['SDLK_k'] },
  { label: 'L', subscript: 'x', superscript: '*', sdlKeys: ['SDLK_l'] },
  { label: '输入', subscript: 'MR', superscript: '', sdlKeys: ['SDLK_RETURN'] }
  ],
  // Row 7 - Z row
  [{ label: 'Z', subscript: '(', superscript: ')', sdlKeys: ['SDLK_z'] },
  { label: 'X', subscript: 'π', superscript: 'X!', sdlKeys: ['SDLK_x'] },
  { label: 'C', subscript: 'EXP', superscript: '。\'"', sdlKeys: ['SDLK_c'] },
  { label: 'V', subscript: 'C', superscript: '', sdlKeys: ['SDLK_v'] },
  { label: 'B', subscript: '1', superscript: '', sdlKeys: ['SDLK_b', 'SDLK_1'] },
  { label: 'N', subscript: '2', superscript: '', sdlKeys: ['SDLK_n', 'SDLK_2'] },
  { label: 'M', subscript: '3', superscript: '', sdlKeys: ['SDLK_m', 'SDLK_3'] },
  { label: '⇞', subscript: '税', superscript: '', sdlKeys: ['SDLK_COMMA'] },
  { label: '▲', subscript: '-', superscript: '', sdlKeys: ['SDLK_UP'] },
  { label: '⇟', subscript: 'M-', superscript: '', sdlKeys: ['SDLK_SLASH'] },],
  // Row 8 - Top function keys
  [{ label: '求助', subscript: '', superscript: '', sdlKeys: ['SDLK_LEFTBRACKET'] },
  { label: '中英数', subscript: '', superscript: 'SHIFT', sdlKeys: ['SDLK_RIGHTBRACKET'] },
  { label: '输入法', subscript: '', superscript: '反查 CAPS', sdlKeys: ['SDLK_BACKSLASH'] },
  { label: '跳出', subscript: 'AC', superscript: '', sdlKeys: ['SDLK_ESCAPE'] },
  { label: '符\n号', subscript: '0', superscript: '继续', sdlKeys: ['SDLK_0'] },
  { label: '.', subscript: '.', superscript: '-', sdlKeys: ['SDLK_PERIOD'] },
  { label: '空格', subscript: '=', superscript: '✓', sdlKeys: ['SDLK_EQUALS', 'SDLK_SPACE'] },
  { label: '◀', subscript: '', superscript: '', sdlKeys: ['SDLK_LEFT'] },
  { label: '▼', subscript: '+', superscript: '', sdlKeys: ['SDLK_DOWN'] },
  { label: '▶', subscript: 'M+', superscript: '', sdlKeys: ['SDLK_RIGHT'] }],
  // Row 9 - Function keys
  [null, null,
    null, null, null, null,
    null, null]
];

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

// 响应式状态
const isMobileDevice = ref(false);
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
const uploadStatusText = ref('');
const fileList = ref<LoadedFile[]>([]);
const selectedFilePath = ref('');

// DOM 元素引用
const canvasRef = ref<HTMLCanvasElement | null>(null);
const zoomContainerRef = ref<HTMLDivElement | null>(null);
const keyboardRef = ref<HTMLDivElement | null>(null);
const outputRef = ref<HTMLTextAreaElement | null>(null);

// WASM 实例
let wasmInstance: any = null;

// 检测是否为移动设备
function detectMobileDevice(): boolean {
  return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
}

// 初始化移动设备UI
function setupMobileUI(): boolean {
  const isMobile = detectMobileDevice();
  isMobileDevice.value = isMobile;

  // 添加设备类型类到body
  document.body.classList.add(isMobile ? 'mobile-device' : 'desktop-device');

  // 在移动设备上默认关闭抽屉，桌面设备上默认打开
  if (isMobile) {
    isDrawerOpen.value = false;
  } else {
    isDrawerOpen.value = true;
  }

  return isMobile;
}

// 切换抽屉状态
function toggleDrawer() {
  isDrawerOpen.value = !isDrawerOpen.value;
}

// 关闭抽屉
function closeDrawer() {
  isDrawerOpen.value = false;
}

// 创建虚拟键盘
function createVirtualKeyboard() {
  if (!keyboardRef.value) return;

  keyboardRef.value.innerHTML = '';

  // 只渲染有按键的行
  const visibleRows = keyMatrix.filter(row => row.some(cell => cell !== null));

  visibleRows.forEach(row => {
    const rowElement = document.createElement('div');
    rowElement.className = 'keyboard-row';

    row.forEach(keyData => {
      if (keyData) {
        const keyElement = document.createElement('div');
        keyElement.className = 'virtual-key';

        // 添加按键标签
        const labelElement = document.createElement('div');
        labelElement.className = 'key-label';
        labelElement.innerHTML = keyData.label.replace('\n', '<br>');
        keyElement.appendChild(labelElement);

        // 添加下标
        if (keyData.subscript) {
          const subscriptElement = document.createElement('div');
          subscriptElement.className = 'key-subscript';
          subscriptElement.textContent = keyData.subscript;
          keyElement.appendChild(subscriptElement);
        }

        // 添加上标
        if (keyData.superscript) {
          const superscriptElement = document.createElement('div');
          superscriptElement.className = 'key-superscript';
          superscriptElement.textContent = keyData.superscript;
          keyElement.appendChild(superscriptElement);
        }

        // 添加事件监听器
        keyElement.addEventListener('mousedown', (e) => {
          e.preventDefault();
          keyElement.classList.add('pressed');
          simulateSDLKeyEvent(keyData.sdlKeys, true);
        });

        keyElement.addEventListener('mouseup', () => {
          keyElement.classList.remove('pressed');
          simulateSDLKeyEvent(keyData.sdlKeys, false);
        });

        keyElement.addEventListener('mouseleave', () => {
          keyElement.classList.remove('pressed');
          simulateSDLKeyEvent(keyData.sdlKeys, false);
        });

        // 触摸事件
        keyElement.addEventListener('touchstart', (e) => {
          e.preventDefault();
          keyElement.classList.add('pressed');
          simulateSDLKeyEvent(keyData.sdlKeys, true);
        });

        keyElement.addEventListener('touchend', () => {
          keyElement.classList.remove('pressed');
          simulateSDLKeyEvent(keyData.sdlKeys, false);
        });

        rowElement.appendChild(keyElement);
      } else {
        // 添加空白占位
        const emptyElement = document.createElement('div');
        emptyElement.style.width = '80px';
        rowElement.appendChild(emptyElement);
      }
    });

    keyboardRef.value?.appendChild(rowElement);
  });
}

// 模拟SDL按键事件
function simulateSDLKeyEvent(sdlKeyNames: string[], keyDown: boolean) {
  // 检查模块和函数是否准备就绪
  if (!wasmInstance) {
    console.log('Module not ready yet');
    return;
  }

  // 尝试直接访问函数
  let injectKeyFunction: ((key: number, down: number) => void) | null = null;
  try {
    injectKeyFunction = wasmInstance.cwrap('injectVirtualKeyEvent', null, ['number', 'number']);
  } catch (e) {
    console.log('Could not wrap injectVirtualKeyEvent function:', e);
    // 尝试替代方法
    if (typeof wasmInstance._injectVirtualKeyEvent === 'function') {
      injectKeyFunction = (key, down) => wasmInstance._injectVirtualKeyEvent(key, down);
    }
  }

  if (!injectKeyFunction) {
    console.log('SDL key simulation not ready yet - function not found');
    return;
  }

  // 为每个SDL键模拟事件
  sdlKeyNames.forEach(keyName => {
    const sdlKeyCode = SDLKeycodes[keyName];
    if (sdlKeyCode !== undefined) {
      injectKeyFunction(sdlKeyCode, keyDown ? 1 : 0);
    } else {
      console.warn(`Unknown SDL key: ${keyName}`);
    }
  });
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
  const viewportHeight = window.innerHeight;
  const viewportWidth = window.innerWidth;

  // 获取缩放容器的原始尺寸
  const containerRect = zoomContainerRef.value.getBoundingClientRect();
  const containerHeight = containerRect.height / currentZoom.value;
  const containerWidth = containerRect.width / currentZoom.value;

  // 计算可用空间
  const availableHeight = viewportHeight - 250;
  const availableWidth = viewportWidth - 20;

  // 计算缩放比例
  const heightScale = availableHeight / containerHeight;
  const widthScale = availableWidth / containerWidth;

  // 使用较小的缩放比例
  const fitZoom = Math.min(heightScale, widthScale, 3.0);
  const minZoom = Math.max(fitZoom, 0.2);

  applyZoom(minZoom);
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

// 格式化文件大小
function formatFileSize(bytes: number): string {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

// 填充文件列表
function populateFileList() {
  if (!wasmInstance) {
    console.warn('WASM实例未初始化，无法获取文件列表');
    return;
  }

  const FS = wasmInstance.FS;
  // const files: LoadedFile[] = [];

  try {
    // 递归遍历文件系统
    function traverseDirectory(path: string, prefix = ''): LoadedFile[] {
      const items: LoadedFile[] = [];

      // 跳过系统目录
      if (path.startsWith('/proc') || path.startsWith('/dev')) {
        return items;
      }

      try {
        const entries = FS.readdir(path);
        console.log(`目录 ${path} 包含:`, entries);

        for (const entry of entries) {
          if (entry === '.' || entry === '..') continue;

          const fullPath = path + '/' + entry;
          try {
            const stat = FS.stat(fullPath);
            if (FS.isFile(stat.mode)) {
              console.log(`找到文件: ${fullPath}, 大小: ${stat.size}`);
              items.push({
                path: fullPath,
                name: prefix + entry,
                size: stat.size
              });
            } else if (FS.isDir(stat.mode)) {
              console.log(`找到目录: ${fullPath}`);
              // 递归处理子目录
              if (!fullPath.startsWith('/proc') && !fullPath.startsWith('/dev')) {
                const subItems = traverseDirectory(fullPath, prefix + entry + '/');
                items.push(...subItems);
              }
            }
          } catch (e) {
            // 忽略errno 63 (ENOSYS) 和其他权限错误
            if ((e as any).errno !== 63 && (e as any).errno !== 44) {
              console.warn(`无法访问 ${fullPath}:`, e);
            }
          }
        }
      } catch (e) {
        // 忽略errno 63 (ENOSYS) 和其他权限错误
        if ((e as any).errno !== 63 && (e as any).errno !== 44) {
          console.warn(`无法读取目录 ${path}:`, e);
        }
      }
      return items;
    }

    // 从根目录开始遍历
    const foundFiles = traverseDirectory('/');

    // 按文件名排序
    foundFiles.sort((a, b) => a.name.localeCompare(b.name));

    console.log(`总共找到 ${foundFiles.length} 个文件:`, foundFiles);
    fileList.value = foundFiles;

    // 如果没有找到文件，显示提示
    if (foundFiles.length === 0) {
      console.log('虚拟文件系统中没有找到可下载的文件');
    }

  } catch (e) {
    console.error('遍历文件系统时出错:', e);
  }
}

// 下载选中的文件
function downloadSelectedFile() {
  if (!wasmInstance) {
    alert('WASM实例未初始化，请稍后再试');
    return;
  }

  if (!selectedFilePath.value) {
    alert('请先选择一个文件');
    return;
  }

  try {
    const FS = wasmInstance.FS;

    // 检查文件是否存在且可读
    try {
      const stat = FS.stat(selectedFilePath.value);
      if (!FS.isFile(stat.mode)) {
        alert('选择的路径不是文件');
        return;
      }
    } catch (e) {
      alert('无法访问文件，可能不存在或没有权限');
      return;
    }

    // 读取文件内容
    const data = FS.readFile(selectedFilePath.value);

    // 获取文件名
    const fileName = selectedFilePath.value.split('/').pop() || 'file';

    // 创建Blob并下载
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);

    // 创建临时下载链接
    const a = document.createElement('a');
    a.href = url;
    a.download = fileName;
    document.body.appendChild(a);
    a.click();

    // 清理
    setTimeout(() => {
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }, 100);

    console.log(`已下载文件: ${fileName} (${formatFileSize(data.length)})`);

  } catch (e) {
    console.error('下载文件时出错:', e);
    if ((e as any).errno === 63) {
      alert('下载文件失败: 文件系统权限错误 (errno 63)');
    } else if ((e as any).errno === 44) {
      alert('下载文件失败: 文件不存在 (errno 44)');
    } else {
      alert(`下载文件失败: ${(e as Error).message}`);
    }
  }
}

// 上传文件到ROMs目录
async function uploadFilesToRoms(files: FileList) {
  if (!wasmInstance) {
    uploadStatusText.value = 'WASM实例未初始化，请稍后再试';
    return;
  }

  const FS = wasmInstance.FS;
  let uploadedCount = 0;
  let failedCount = 0;

  uploadStatusText.value = '开始上传文件...';

  for (let i = 0; i < files.length; i++) {
    const file = files[i] as File;
    try {
      // 验证文件
      if (file.size === 0) {
        console.warn(`跳过空文件: ${file.name}`);
        failedCount++;
        continue;
      }

      // 读取文件内容
      const arrayBuffer = await file.arrayBuffer();
      const uint8Array = new Uint8Array(arrayBuffer);

      // 确保ROMs目录存在
      try {
        FS.mkdirTree('/roms');
      } catch (e) {
        // 目录可能已存在，忽略错误
      }

      // 构建目标路径
      const targetPath = `/roms/${file.name}`;

      // 写入文件到虚拟文件系统
      FS.writeFile(targetPath, uint8Array);
      console.log(`已上传文件: ${targetPath} (${formatFileSize(file.size)})`);

      uploadedCount++;
      uploadStatusText.value = `上传进度: ${uploadedCount}/${files.length}`;

    } catch (error) {
      console.error(`上传文件 ${file.name} 失败:`, error);
      failedCount++;
      uploadStatusText.value = `上传失败: ${file.name}`;
    }
  }

  // 显示最终结果
  if (failedCount === 0) {
    uploadStatusText.value = `成功上传 ${uploadedCount} 个文件`;
  } else {
    uploadStatusText.value = `上传完成: ${uploadedCount} 成功, ${failedCount} 失败`;
  }

  // 刷新文件列表
  setTimeout(() => {
    populateFileList();
  }, 500);
}

// 处理文件上传
function handleFileUpload(event: Event) {
  const target = event.target as HTMLInputElement;
  if (target.files && target.files.length > 0) {
    uploadFilesToRoms(target.files);
    // 清空文件输入，允许重复选择相同文件
    target.value = '';
  }
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

    // 手动调用 main()
    console.log("文件系统已准备就绪。即将启动 main()...");
    Module.setStatus(' ');

    // 使用当前选择的ROM配置
    const romConfig = romConfigs[currentRom.value] || { args: [] };
    instance.callMain(romConfig.args);
    console.log("wqxsim 已启动。");

    // 设置文件下载控制
    setupFileDownloadControls();

  } catch (err) {
    statusText.value = 'Error during startup. See console.';
    console.error(err);
    showSpinner.value = false;
  }
}

// 设置文件下载控制
function setupFileDownloadControls() {
  // 页面加载完成后延迟创建示例文件，然后更新文件列表
  setTimeout(() => {
    console.log('开始初始化文件下载功能...');
    populateFileList();
  }, 2000);
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
  // 初始化移动设备UI
  setupMobileUI();

  // 创建虚拟键盘
  createVirtualKeyboard();

  // 设置屏幕适应
  setUpScreenFit();

  // 加载和运行WASM
  loadAndRun();

  // 设置WebGL上下文丢失处理
  if (canvasRef.value) {
    canvasRef.value.addEventListener('webglcontextlost', (e) => {
      alert('WebGL context lost. You will need to reload the page.');
      e.preventDefault();
    }, false);
    // 确保canvas有正确的尺寸
    canvasRef.value.width = 935;
    canvasRef.value.height = 400;
  }


});
</script>

<template>
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

    <!-- 文件管理 -->
    <div class="control-section">
      <h3>文件管理</h3>

      <!-- 文件下载控制 -->
      <div class="file-section">
        <button @click="downloadSelectedFile" class="primary-button full-width" :disabled="!selectedFilePath">
          下载文件
        </button>
        <select v-model="selectedFilePath" class="full-width">
          <option value="" disabled>选择文件...</option>
          <option v-for="file in fileList" :key="file.path" :value="file.path">
            {{ file.name }} ({{ formatFileSize(file.size) }})
          </option>
        </select>
        <button @click="populateFileList" class="secondary-button full-width">刷新</button>
      </div>

      <!-- 文件上传控制 -->
      <div class="file-section">
        <input type="file" id="file-upload" style="display: none;" multiple accept="*/*" @change="handleFileUpload">
        <label for="file-upload" class="primary-button full-width upload-label">
          上传文件到ROMs
        </label>
        <div class="status-text">{{ uploadStatusText }}</div>
      </div>
    </div>
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
      <canvas ref="canvasRef" class="emscripten" oncontextmenu="event.preventDefault()" tabindex="-1"></canvas>
      <div class="screen_num">
        <span>1</span>
        <span>2</span>
        <span>3</span>
        <span>4</span>
        <span>5</span>
        <span>6</span>
        <span>7</span>
        <span>8</span>
        <span>9</span>
      </div>
    </div>
    <textarea ref="outputRef" v-model="outputText" rows="8" style="display: none;"></textarea>

    <!-- Virtual Keyboard Container -->
    <div ref="keyboardRef" class="virtual-keyboard">
      <!-- Virtual keyboard will be generated here by JavaScript -->
    </div>
  </div>
</template>

<style lang="less">
// 基础样式
body {
  font-family: Arial, sans-serif;
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  background-color: #f0f0f0;
  overflow-x: hidden;
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

  .file-section {
    margin-bottom: 15px;

    select {
      padding: 8px;
      margin-bottom: 10px;
      border-radius: 4px;
      border: 1px solid #ccc;
    }

    .upload-label {
      display: block;
      text-align: center;
      padding: 8px;
      background: linear-gradient(135deg, #27ae60, #2ecc71);
      color: white;
      border: none;
      border-radius: 4px;
      cursor: pointer;
      font-weight: bold;
      margin-bottom: 10px;

      &:hover {
        background: linear-gradient(135deg, #229954, #27ae60);
      }
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
  top: 15px;
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

// 移动设备特定样式
@media (max-width: 768px) {
  body {
    padding: 0;
    overflow-x: hidden;
  }

  .drawer-toggle {
    width: 45px;
    height: 45px;
    font-size: 20px;
    top: 10px;
    left: 10px;
  }

  .header {
    padding: 5px;
    margin-bottom: 5px;
  }

  h1 {
    font-size: 1.5rem;
    margin: 5px 0;
  }

  .emscripten_border {
    margin: 60px 5px 5px;
    border: none;
    background-color: transparent;
  }

  canvas.emscripten {
    max-width: 100%;
    height: auto;
    width: 100% !important;
    box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
    touch-action: manipulation;
  }

  .virtual-keyboard {
    width: 100%;
    max-width: 100%;
    margin: 10px 0 0 0;
    padding: 5px;
    font-size: 14px;
    box-sizing: border-box;
    touch-action: manipulation;
    background-color: #f5f5f5;
    border-radius: 8px;
  }

  .keyboard-row {
    margin-bottom: 5px;
    gap: 5px;
    justify-content: center;
  }

  .virtual-key {
    min-width: 28px;
    height: 36px;
    font-size: 12px;
    padding: 4px;
    border-radius: 4px;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.2);
    touch-action: manipulation;
  }

  .virtual-key:active {
    transform: scale(0.95);
    box-shadow: 0 1px 2px rgba(0, 0, 0, 0.2);
  }

  .key-label {
    font-size: 12px;
  }

  .key-subscript,
  .key-superscript {
    font-size: 8px;
  }

  .key-wide {
    min-width: 60px;
  }

  .key-extra-wide {
    min-width: 90px;
  }

  .key-space {
    min-width: 150px;
  }

  .screen_num {
    padding-left: 0;
    justify-content: center;
    gap: 20px;
    margin-top: 5px;

    span {
      font-size: 14px;
    }
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
  width: 935px;
  height: 400px;
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

// Virtual keyboard styles
.virtual-keyboard {
  display: flex;
  flex-direction: column;
  gap: 5px;
  margin-top: 15px;
  padding: 10px;
  background-color: #e0e0e0;
  border-radius: 8px;
  max-width: 900px;
  width: 100%;
}

.keyboard-row {
  display: flex;
  justify-content: center;
  gap: 5px;
}

.virtual-key {
  width: 60px;
  height: 40px;
  padding: 8px;
  background-color: #fff;
  border: 2px solid #ccc;
  border-radius: 4px;
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  font-size: 20px;
  cursor: pointer;
  user-select: none;
  position: relative;
  transition: all 0.1s;

  &:active,
  &.pressed {
    background-color: #3498db;
    color: white;
    border-color: #2980b9;
    transform: translateY(2px);
  }
}

.key-label {
  font-weight: bold;
}

.key-subscript {
  font-size: 10px;
  position: absolute;
  top: 2px;
  left: 2px;
}

.key-superscript {
  font-size: 10px;
  position: absolute;
  top: 2px;
  right: 2px;
}

.key-wide {
  min-width: 80px;
}

.key-extra-wide {
  min-width: 120px;
}

.key-space {
  min-width: 200px;
}

.zoom-container {
  transform-origin: top center;
  transition: transform 0.3s ease;
}
</style>