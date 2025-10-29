<script setup lang="js">
import { onMounted } from 'vue';
import WqxsimModule from '@/assets/wqxsim.js'
onMounted(() => {
  // 移动设备检测和抽屉控制
  function setupMobileUI() {
    // 检测是否为移动设备
    const isMobile = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);

    // 添加设备类型类到body
    document.body.classList.add(isMobile ? 'mobile-device' : 'desktop-device');

    // 获取抽屉元素
    const drawer = document.getElementById('control-drawer');
    const drawerToggle = document.getElementById('drawer-toggle');
    const drawerOverlay = document.getElementById('drawer-overlay');

    // 抽屉开关函数
    function toggleDrawer() {
      drawer.classList.toggle('open');
      drawerOverlay.classList.toggle('show');
    }

    // 关闭抽屉函数
    function closeDrawer() {
      drawer.classList.remove('open');
      drawerOverlay.classList.remove('show');
    }

    // 抽屉开关按钮点击事件
    drawerToggle.addEventListener('click', toggleDrawer);

    // 遮罩层点击事件
    drawerOverlay.addEventListener('click', closeDrawer);

    // 在移动设备上默认关闭抽屉，桌面设备上默认打开
    if (isMobile) {
      closeDrawer();
    } else {
      toggleDrawer();
    }

    // 在移动设备上优化虚拟键盘
    if (isMobile) {
      const virtualKeyboard = document.getElementById('virtual-keyboard');
      if (virtualKeyboard) {
        virtualKeyboard.style.maxWidth = '100%';
      }
    }

    // 返回是否为移动设备
    return isMobile;
  }

  var statusElement = document.getElementById('status');
  var progressElement = document.getElementById('progress');
  var spinnerElement = document.getElementById('spinner');
  var canvasElement = document.getElementById('canvas');
  var outputElement = document.getElementById('output');
  var keyboardElement = document.getElementById('virtual-keyboard');
  if (outputElement) outputElement.value = ''; // clear browser cache

  // 初始化移动设备UI
  const isMobileDevice = setupMobileUI();

  // 添加触摸手势支持
  function setupTouchGestures() {
    if (!isMobileDevice) return;

    const canvas = document.getElementById('canvas');
    const zoomContainer = document.getElementById('zoom-container');
    let initialDistance = 0;
    let initialScale = 1;

    // 计算两点之间的距离
    function getDistance(touches) {
      const dx = touches[0].clientX - touches[1].clientX;
      const dy = touches[0].clientY - touches[1].clientY;
      return Math.sqrt(dx * dx + dy * dy);
    }

    // 处理双指缩放
    canvas.addEventListener('touchstart', function (e) {
      if (e.touches.length === 2) {
        e.preventDefault();
        initialDistance = getDistance(e.touches);
        const transform = window.getComputedStyle(zoomContainer).transform;
        if (transform !== 'none') {
          const matrix = transform.match(/matrix.*\((.+)\)/)[1].split(', ');
          initialScale = parseFloat(matrix[0]);
        } else {
          initialScale = 1;
        }
      }
    });

    canvas.addEventListener('touchmove', function (e) {
      if (e.touches.length === 2) {
        e.preventDefault();
        const currentDistance = getDistance(e.touches);
        const scale = (currentDistance / initialDistance) * initialScale;

        // 限制缩放范围
        const clampedScale = Math.min(Math.max(0.2, scale), 3);
        zoomContainer.style.transform = `scale(${clampedScale})`;

        // 更新缩放滑块
        const zoomSlider = document.getElementById('zoom-slider');
        const zoomValue = document.getElementById('zoom-value');
        if (zoomSlider && zoomValue) {
          zoomSlider.value = clampedScale;
          zoomValue.textContent = Math.round(clampedScale * 100) + '%';
        }
      }
    });

    // 处理双击缩放
    let lastTap = 0;
    canvas.addEventListener('touchend', function (e) {
      const currentTime = new Date().getTime();
      const tapLength = currentTime - lastTap;
      if (tapLength < 500 && tapLength > 0) {
        // 双击检测到
        e.preventDefault();
        const currentScale = parseFloat(window.getComputedStyle(zoomContainer).transform.match(/matrix.*\((.+)\)/)?.[1]?.split(', ')?.[0] || 1);

        // 如果当前缩放不是1，则重置为1；否则放大到1.5倍
        const targetScale = currentScale !== 1 ? 1 : 1.5;
        zoomContainer.style.transform = `scale(${targetScale})`;

        // 更新缩放滑块
        const zoomSlider = document.getElementById('zoom-slider');
        const zoomValue = document.getElementById('zoom-value');
        if (zoomSlider && zoomValue) {
          zoomSlider.value = targetScale;
          zoomValue.textContent = Math.round(targetScale * 100) + '%';
        }
      }
      lastTap = currentTime;
    });

    // 添加滑动关闭抽屉的手势
    let touchStartX = 0;
    let touchStartY = 0;
    const drawer = document.getElementById('control-drawer');

    document.addEventListener('touchstart', function (e) {
      touchStartX = e.touches[0].clientX;
      touchStartY = e.touches[0].clientY;
    });

    document.addEventListener('touchend', function (e) {
      if (!drawer.classList.contains('open')) return;

      const touchEndX = e.changedTouches[0].clientX;
      const touchEndY = e.changedTouches[0].clientY;
      const diffX = touchEndX - touchStartX;
      const diffY = touchEndY - touchStartY;

      // 检测是否为从左向右的滑动（关闭抽屉）
      if (Math.abs(diffX) > Math.abs(diffY) && diffX > 50) {
        // 从左向右滑动，关闭抽屉
        drawer.classList.remove('open');
        document.getElementById('drawer-overlay').classList.remove('show');
      }
    });
  }

  // 初始化触摸手势
  setupTouchGestures();

  // As a default initial behavior, pop up an alert when webgl context is lost. To make your
  // application robust, you may want to override this behavior before shipping!
  // See http://www.khronos.org/registry/webgl/specs/latest/1.0/#5.15.2
  canvasElement.addEventListener('webglcontextlost', (e) => {
    alert('WebGL context lost. You will need to reload the page.');
    e.preventDefault();
  }, false);

  // Define SDL keycode mappings
  const SDLKeycodes = {
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

  // Define key matrix based on key_new.cpp
  const keyMatrix = [
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
    // { label: 'reset', subscript: '', superscript: '', sdlKeys: ['SDLK_F6'] },
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
    // Row 8 - Top function keys (物理键盘的最上面一行)
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

  // Create virtual keyboard
  function createVirtualKeyboard() {
    keyboardElement.innerHTML = '';

    // Only render rows that have keys
    const visibleRows = keyMatrix.filter(row => row.some(cell => cell !== null));

    visibleRows.forEach(row => {
      const rowElement = document.createElement('div');
      rowElement.className = 'keyboard-row';

      row.forEach(keyData => {
        if (keyData) {
          const keyElement = document.createElement('div');
          keyElement.className = 'virtual-key';

          // Add key label
          const labelElement = document.createElement('div');
          labelElement.className = 'key-label';
          labelElement.innerHTML = keyData.label.replace('\n', '<br>');
          keyElement.appendChild(labelElement);

          // Add subscript if exists
          if (keyData.subscript) {
            const subscriptElement = document.createElement('div');
            subscriptElement.className = 'key-subscript';
            subscriptElement.textContent = keyData.subscript;
            keyElement.appendChild(subscriptElement);
          }

          // Add superscript if exists
          if (keyData.superscript) {
            const superscriptElement = document.createElement('div');
            superscriptElement.className = 'key-superscript';
            superscriptElement.textContent = keyData.superscript;
            keyElement.appendChild(superscriptElement);
          }

          // Add event listeners
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

          // Touch events for mobile
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
          // Add empty space for alignment
          const emptyElement = document.createElement('div');
          emptyElement.style.width = '80px';
          rowElement.appendChild(emptyElement);
        }
      });

      keyboardElement.appendChild(rowElement);
    });
  }

  // Simulate SDL key event
  function simulateSDLKeyEvent(sdlKeyNames, keyDown) {
    // Check if Module and the function are ready
    if (!Module) {
      console.log('Module not ready yet');
      return;
    }

    // Try to access the function directly
    let injectKeyFunction = null;
    try {
      injectKeyFunction = Module.cwrap('injectVirtualKeyEvent', null, ['number', 'number']);
    } catch (e) {
      console.log('Could not wrap injectVirtualKeyEvent function:', e);
      // Try alternative method
      if (typeof Module._injectVirtualKeyEvent === 'function') {
        injectKeyFunction = (key, down) => Module._injectVirtualKeyEvent(key, down);
      }
    }

    if (!injectKeyFunction) {
      console.log('SDL key simulation not ready yet - function not found');
      return;
    }

    // For each SDL key in the list, simulate the event
    sdlKeyNames.forEach(keyName => {
      const sdlKeyCode = SDLKeycodes[keyName];
      if (sdlKeyCode !== undefined) {
        // console.log(`Simulating SDL key ${keyName} (${sdlKeyCode}) ${keyDown ? 'down' : 'up'}`);
        // Call the C++ function
        injectKeyFunction(sdlKeyCode, keyDown ? 1 : 0);
      } else {
        console.warn(`Unknown SDL key: ${keyName}`);
      }
    });
  }

  // ROM configurations
  const romConfigs = {
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
    // 注意：其他ROM版本暂时不可用，因为缺少对应的ROM文件
    // 如果需要添加更多ROM版本，请先将对应的ROM文件添加到build/roms目录
  };

  // Current selected ROM
  let currentRom = 'nc2000';
  let wasmInstance = null;

  // 缩放功能
  let currentZoom = 1.0;
  const zoomContainer = document.getElementById('zoom-container');
  const zoomSlider = document.getElementById('zoom-slider');
  const zoomValue = document.getElementById('zoom-value');
  const zoomReset = document.getElementById('zoom-reset');
  const autoFitCheckbox = document.getElementById('auto-fit');
  let isFitMode = false;
  let autoFitEnabled = false;

  // 应用缩放
  function applyZoom(zoomLevel) {
    currentZoom = parseFloat(zoomLevel);
    zoomContainer.style.transform = `scale(${currentZoom})`;
    zoomValue.textContent = `${Math.round(currentZoom * 100)}%`;
    zoomSlider.value = currentZoom;
  }

  // 重置缩放
  function resetZoom() {
    applyZoom(1.0);
  }

  // 适应屏幕缩放
  function fitToScreen() {
    // 获取视口高度和宽度
    const viewportHeight = window.innerHeight;
    const viewportWidth = window.innerWidth;

    // 获取缩放容器的原始尺寸
    const containerRect = zoomContainer.getBoundingClientRect();
    const containerHeight = containerRect.height / currentZoom; // 当前缩放下的实际高度
    const containerWidth = containerRect.width / currentZoom; // 当前缩放下的实际宽度

    // 计算可用空间（减去顶部控件的高度）
    const availableHeight = viewportHeight - 250; // 估算的顶部控件高度
    const availableWidth = viewportWidth - 20; // 留出一些边距

    // 计算缩放比例
    const heightScale = availableHeight / containerHeight;
    const widthScale = availableWidth / containerWidth;

    // 使用较小的缩放比例，确保内容完全可见
    const fitZoom = Math.min(heightScale, widthScale, 3.0); // 最大不超过2.0
    const minZoom = Math.max(fitZoom, 0.2); // 最小不小于0.5

    applyZoom(minZoom);
  }

  // 设置缩放事件监听器
  function setupZoomControls() {
    // 滑块变化事件
    zoomSlider.addEventListener('input', (e) => {
      applyZoom(e.target.value);
      isFitMode = false;
      autoFitCheckbox.checked = false;
      autoFitEnabled = false;
    });

    // 重置按钮点击事件
    zoomReset.addEventListener('click', () => {
      resetZoom();
      isFitMode = false;
      autoFitCheckbox.checked = false;
      autoFitEnabled = false;
    });

    // 自动适应屏幕复选框事件
    autoFitCheckbox.addEventListener('change', (e) => {
      autoFitEnabled = e.target.checked;
      if (autoFitEnabled) {
        fitToScreen();
        isFitMode = true;
      } else {
        isFitMode = false;
      }
    });

    // 窗口大小变化时，如果是适应屏幕模式或自动适应模式，则重新计算
    window.addEventListener('resize', () => {
      if (isFitMode || autoFitEnabled) {
        fitToScreen();
      }
    });
  }

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

  var Module = {
    print(...args) {
      console.log(...args);
      if (outputElement) {
        var text = args.join(' ');
        outputElement.value += text + "\n";
        outputElement.scrollTop = outputElement.scrollHeight; // focus on bottom
      }
    },
    canvas: canvasElement,
    setStatus(text) {
      console.log('setStatus', text);
      if (!Module.setStatus.last) Module.setStatus.last = { time: Date.now(), text: '' };
      if (text === Module.setStatus.last.text) return;
      var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
      var now = Date.now();
      if (m && now - Module.setStatus.last.time < 30) return; Module.setStatus.last.time = now;
      Module.setStatus.last.text = text; if (m) {
        text = m[1]; progressElement.value = parseInt(m[2]) * 100;
        progressElement.max = parseInt(m[4]) * 100; progressElement.hidden = false; spinnerElement.hidden = false;
      } else {
        progressElement.value = null; progressElement.max = null; progressElement.hidden = true; if (!text)
          spinnerElement.style.display = 'none';
      } statusElement.innerHTML = text;
    }, totalDependencies: 0,
    monitorRunDependencies(left) {
      console.log('monitorRunDependencies', left)
      this.totalDependencies = Math.max(this.totalDependencies, left); Module.setStatus(left ? 'Preparing... (' +
        (this.totalDependencies - left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
    },
    onRuntimeInitialized() {
      console.log('onRuntimeInitialized');
      // Don't create virtual keyboard here, wait for main() to be called
    }
  };
  Module.setStatus('Downloading...'); window.onerror = (event) => {
    Module.setStatus('Exception thrown, see JavaScript console');
    spinnerElement.style.display = 'none';
    Module.setStatus = (text) => {
      if (text) console.error('[post-exception status] ' + text);
    };
  };



  // ROM选择事件处理函数
  function setupRomSelector() {
    const romSelect = document.getElementById('rom-select');
    const applyButton = document.getElementById('apply-rom');
    const romStatus = document.getElementById('rom-status');

    // 设置默认值为当前ROM
    romSelect.value = currentRom;

    // 应用按钮点击事件
    applyButton.addEventListener('click', async () => {
      const selectedRom = romSelect.value;

      // if (selectedRom === currentRom) {
      //     romStatus.textContent = '已经是当前ROM，无需更改';
      //     return;
      // }

      if (!wasmInstance) {
        romStatus.textContent = 'WASM模块尚未加载完成，请稍后再试';
        return;
      }

      try {
        // romStatus.textContent = '正在切换ROM...';
        currentRom = selectedRom;

        // 重新加载WASM
        await restartWithNewRom();
        // romStatus.textContent = `已切换到: ${romConfigs[selectedRom].name}`;
      } catch (error) {
        console.error('切换ROM时出错:', error);
        romStatus.textContent = `切换ROM失败: ${error.message}`;
      }
    });
  }

  // 使用新ROM重新启动
  async function restartWithNewRom() {
    if (!wasmInstance) {
      throw new Error('WASM实例未初始化');
    }

    const FS = wasmInstance.FS;
    const romConfig = romConfigs[currentRom];

    // // 下载ROM文件
    // const romFiles = romConfig.files;
    // const fetchPromises = romFiles.map(async (file) => {
    //     const response = await fetch(file.url);
    //     if (!response.ok) {
    //         throw new Error(`Failed to fetch ${file.url}: ${response.statusText}`);
    //     }
    //     const data = await response.arrayBuffer();
    //     return { data: new Uint8Array(data), path: file.vfsPath };
    // });

    // const loadedFiles = await Promise.all(fetchPromises);

    // // 将ROM文件写入VFS
    // for (const file of loadedFiles) {
    //     const lastSlashIndex = file.path.lastIndexOf('/');
    //     const dirPath = lastSlashIndex > 0 ? file.path.substring(0, lastSlashIndex) : '.';
    //     FS.mkdirTree(dirPath);
    //     FS.writeFile(file.path, file.data);
    //     console.log(`VFS: Wrote ${file.path}`);
    // }

    // 重新调用main函数
    console.log("使用新ROM重新启动...");
    wasmInstance.callMain(romConfig.args);
    console.log("wqxsim 已使用新ROM启动。");
  }

  function setUpScreenFit() {
    // 设置缩放控制
    setupZoomControls();
    // 延迟执行自动适应屏幕，确保所有元素都已渲染
    setTimeout(() => {
      fitToScreen();
      isFitMode = true;
      autoFitCheckbox.checked = true;
      autoFitEnabled = true;
    }, 100);
  }

  // 6. [核心] 异步函数，用于加载和运行
  async function loadAndRun() {
    try {
      // 6a. 加载 Wasm 模块
      const instance = await WqxsimModule(Module);
      wasmInstance = instance; // 保存WASM实例

      console.log("Wasm 模块已加载，准备文件系统...", instance);

      // 6b. 获取 FS API
      const FS = instance.FS;

      // 6c. (并行) 下载所有文件 (保持不变)
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

      // 6d. 将所有已下载的文件写入 VFS (保持不变)
      Module.setStatus('Files downloaded. Writing to virtual file system...');
      for (const file of loadedFiles) {
        const lastSlashIndex = file.path.lastIndexOf('/');
        const dirPath = lastSlashIndex > 0 ? file.path.substring(0, lastSlashIndex) : '.';
        FS.mkdirTree(dirPath);
        FS.writeFile(file.path, file.data);
        console.log(`VFS: Wrote ${file.path}`);
      }

      // 6e. [完成] 手动调用 main() (使用当前ROM配置)
      console.log("文件系统已准备就绪。即将启动 main()...");
      Module.setStatus(' ');

      // 使用当前选择的ROM配置
      const romConfig = romConfigs[currentRom];
      instance.callMain(romConfig.args);
      console.log("wqxsim 已启动。");

      // 设置ROM选择器
      setupRomSelector();



      // 设置文件下载控制
      setupFileDownloadControls();



    } catch (err) {
      Module.setStatus('Error during startup. See console.');
      console.error(err);
      spinnerElement.style.display = 'none';
    }
  }
  createVirtualKeyboard();

  // 文件下载功能
  function populateFileList() {
    if (!wasmInstance) {
      console.warn('WASM实例未初始化，无法获取文件列表');
      return;
    }

    const FS = wasmInstance.FS;
    const fileSelect = document.getElementById('file-select');

    // 清空现有选项
    fileSelect.innerHTML = '<option value="">选择文件...</option>';

    try {
      // 首先检查根目录内容，用于调试
      console.log('开始遍历文件系统...');
      const rootEntries = FS.readdir('/');
      console.log('根目录内容:', rootEntries);

      // 递归遍历文件系统
      function traverseDirectory(path, prefix = '') {
        const items = [];

        // 跳过系统目录，避免errno 63错误
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
                // 递归处理子目录，但跳过系统目录
                if (!fullPath.startsWith('/proc') && !fullPath.startsWith('/dev')) {
                  const subItems = traverseDirectory(fullPath, prefix + entry + '/');
                  items.push(...subItems);
                }
              }
            } catch (e) {
              // 忽略errno 63 (ENOSYS) 和其他权限错误
              if (e.errno !== 63 && e.errno !== 44) {
                console.warn(`无法访问 ${fullPath}:`, e);
              }
            }
          }
        } catch (e) {
          // 忽略errno 63 (ENOSYS) 和其他权限错误
          if (e.errno !== 63 && e.errno !== 44) {
            console.warn(`无法读取目录 ${path}:`, e);
          }
        }
        return items
      }

      // 从根目录开始遍历，但跳过系统目录
      const files = traverseDirectory('/');

      // 按文件名排序
      files.sort((a, b) => a.name.localeCompare(b.name));

      console.log(`总共找到 ${files.length} 个文件:`, files);

      // 添加到选择框
      for (const file of files) {
        const option = document.createElement('option');
        option.value = file.path;
        option.textContent = `${file.name} (${formatFileSize(file.size)})`;
        fileSelect.appendChild(option);
      }

      // 如果没有找到文件，显示提示
      if (files.length === 0) {
        const option = document.createElement('option');
        option.value = '';
        option.textContent = '暂无文件可下载，点击刷新按钮重试';
        option.disabled = true;
        fileSelect.appendChild(option);
        console.log('虚拟文件系统中没有找到可下载的文件');

        // 显示友好的提示信息
        const infoOption = document.createElement('option');
        infoOption.value = '';
        infoOption.textContent = '系统将在几秒内自动创建示例文件...';
        infoOption.disabled = true;
        fileSelect.appendChild(infoOption);
      }

    } catch (e) {
      console.error('遍历文件系统时出错:', e);

      // 显示错误信息给用户
      const option = document.createElement('option');
      option.value = '';
      option.textContent = '文件系统错误，请稍后重试';
      option.disabled = true;
      fileSelect.appendChild(option);
    }
  }

  // 格式化文件大小
  function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  }

  // 下载选中的文件
  function downloadSelectedFile() {
    if (!wasmInstance) {
      alert('WASM实例未初始化，请稍后再试');
      return;
    }

    const fileSelect = document.getElementById('file-select');
    const selectedPath = fileSelect.value;

    if (!selectedPath) {
      alert('请先选择一个文件');
      return;
    }

    try {
      const FS = wasmInstance.FS;

      // 首先检查文件是否存在且可读
      try {
        const stat = FS.stat(selectedPath);
        if (!FS.isFile(stat.mode)) {
          alert('选择的路径不是文件');
          return;
        }
      } catch (e) {
        alert('无法访问文件，可能不存在或没有权限');
        return;
      }

      // 读取文件内容
      const data = FS.readFile(selectedPath);

      // 获取文件名
      const fileName = selectedPath.split('/').pop();

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
      if (e.errno === 63) {
        alert('下载文件失败: 文件系统权限错误 (errno 63)');
      } else if (e.errno === 44) {
        alert('下载文件失败: 文件不存在 (errno 44)');
      } else {
        alert(`下载文件失败: ${e.message}`);
      }
    }
  }
  // 设置文件下载事件监听器
  function setupFileDownloadControls() {
    const downloadButton = document.getElementById('download-files');
    const fileSelect = document.getElementById('file-select');
    const refreshButton = document.getElementById('refresh-files');
    const uploadButton = document.getElementById('upload-files');
    const fileInput = document.getElementById('file-upload');
    const uploadStatus = document.getElementById('upload-status');

    // 下载按钮点击事件
    downloadButton.addEventListener('click', downloadSelectedFile);

    // 刷新按钮点击事件
    refreshButton.addEventListener('click', function () {
      console.log('手动刷新文件列表...');
      console.log('WASM实例状态:', wasmInstance ? '已初始化' : '未初始化');
      if (wasmInstance) {
        console.log('文件系统可用:', wasmInstance.FS ? '是' : '否');
      }
      populateFileList();
    });

    // 上传按钮点击事件 - 触发文件选择
    uploadButton.addEventListener('click', function () {
      fileInput.click();
    });

    // 文件选择事件
    fileInput.addEventListener('change', async function (event) {
      const files = event.target.files;
      if (files.length === 0) return;

      await uploadFilesToRoms(files, uploadStatus);

      // 清空文件输入，允许重复选择相同文件
      fileInput.value = '';
    });

    // 文件选择框获得焦点时更新文件列表（更友好的触发方式）
    // fileSelect.addEventListener('focus', populateFileList);

    // // 鼠标悬停时也更新文件列表（可选）
    // fileSelect.addEventListener('mouseenter', populateFileList);

    // 文件选择框变更事件
    fileSelect.addEventListener('change', function () {
      if (this.value) {
        console.log(`用户选择了文件: ${this.value}`);
      }
    });

    // 页面加载完成后延迟创建示例文件，然后更新文件列表
    setTimeout(function () {
      console.log('开始初始化文件下载功能...');
      populateFileList();
    }, 2000);


  }

  // 上传文件到ROMs目录
  async function uploadFilesToRoms(files, statusElement) {
    if (!wasmInstance) {
      statusElement.textContent = 'WASM实例未初始化，请稍后再试';
      statusElement.style.color = '#e74c3c';
      return;
    }

    const FS = wasmInstance.FS;
    let uploadedCount = 0;
    let failedCount = 0;

    statusElement.textContent = '开始上传文件...';
    statusElement.style.color = '#666';

    for (const file of files) {
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
        statusElement.textContent = `上传进度: ${uploadedCount}/${files.length}`;

      } catch (error) {
        console.error(`上传文件 ${file.name} 失败:`, error);
        failedCount++;
        statusElement.textContent = `上传失败: ${file.name}`;
        statusElement.style.color = '#e74c3c';
      }
    }

    // 显示最终结果
    if (failedCount === 0) {
      statusElement.textContent = `成功上传 ${uploadedCount} 个文件`;
      statusElement.style.color = '#27ae60';
    } else {
      statusElement.textContent = `上传完成: ${uploadedCount} 成功, ${failedCount} 失败`;
      statusElement.style.color = failedCount === files.length ? '#e74c3c' : '#f39c12';
    }

    // 刷新文件列表
    setTimeout(() => {
      populateFileList();
    }, 500);
  }

  // 启动整个过程
  loadAndRun();

  setUpScreenFit();
})
</script>

<template>
  <!-- 抽屉式控件容器 -->
  <div class="drawer" id="control-drawer">
    <h2 style="margin-top: 0; color: #333; border-bottom: 1px solid #eee; padding-bottom: 10px;">控制面板</h2>

    <!-- ROM Selection -->
    <div id="rom-selection" style="margin-bottom: 20px;">
      <h3 style="margin-top: 0; margin-bottom: 10px; font-size: 16px;">ROM选择</h3>
      <select id="rom-select"
        style="width: 100%; padding: 8px; margin-bottom: 10px; border-radius: 4px; border: 1px solid #ccc;">
        <option value="nc2000">nc2000/2600 官方3.5</option>
        <option value="nc2600c_fc42">nc2600c 非常4.2 by 41824984 - 24MB扩容</option>
      </select>
      <button id="apply-rom"
        style="width: 100%; padding: 8px; background-color: #3498db; color: white; border: none; border-radius: 4px; cursor: pointer;">应用</button>
      <div id="rom-status" style="margin-top: 5px; font-size: 12px; color: #666;"></div>
    </div>

    <!-- 缩放控制 -->
    <div id="zoom-controls" style="margin-bottom: 20px;">
      <h3 style="margin-top: 0; margin-bottom: 10px; font-size: 16px;">缩放控制</h3>
      <div style="margin-bottom: 10px;">
        <label for="zoom-slider" style="display: block; margin-bottom: 5px;">缩放:</label>
        <input type="range" id="zoom-slider" min="0.2" max="3" step="0.1" value="1" style="width: 100%;">
        <div style="display: flex; justify-content: space-between; margin-top: 5px;">
          <span id="zoom-value" style="font-weight: bold;">100%</span>
          <button id="zoom-reset"
            style="padding: 5px 10px; background-color: #3498db; color: white; border: none; border-radius: 3px; cursor: pointer;">重置</button>
        </div>
      </div>
      <label style="display: flex; align-items: center;">
        <input type="checkbox" id="auto-fit" style="margin-right: 8px;">
        <span>自适应屏幕</span>
      </label>
    </div>

    <!-- 文件管理 -->
    <div id="file-management" style="margin-bottom: 20px;">
      <h3 style="margin-top: 0; margin-bottom: 10px; font-size: 16px;">文件管理</h3>

      <!-- 文件下载控制 -->
      <div class="file-download-section" style="margin-bottom: 15px;">
        <button id="download-files"
          style="width: 100%; padding: 8px; background: linear-gradient(135deg, #27ae60, #2ecc71); color: white; border: none; border-radius: 4px; cursor: pointer; font-weight: bold; margin-bottom: 10px;">下载文件</button>
        <select id="file-select"
          style="width: 100%; padding: 8px; border-radius: 4px; border: 1px solid #ccc; margin-bottom: 10px;">
          <option value="">点击此处刷新文件列表...</option>
        </select>
        <button id="refresh-files"
          style="width: 100%; padding: 6px 12px; background: linear-gradient(135deg, #3498db, #2980b9); color: white; border: none; border-radius: 4px; cursor: pointer;">刷新</button>
      </div>

      <!-- 文件上传控制 -->
      <div class="file-upload-section">
        <input type="file" id="file-upload" style="display: none;" multiple accept="*/*">
        <button id="upload-files"
          style="width: 100%; padding: 8px; background: linear-gradient(135deg, #27ae60, #2ecc71); color: white; border: none; border-radius: 4px; cursor: pointer; font-weight: bold;">上传文件到ROMs</button>
        <div id="upload-status" style="margin-top: 8px; font-size: 12px; color: #666;"></div>
      </div>
    </div>
  </div>

  <!-- 抽屉遮罩层 -->
  <div class="drawer-overlay" id="drawer-overlay"></div>

  <!-- 抽屉开关按钮 -->
  <button class="drawer-toggle" id="drawer-toggle">☰</button>

  <div class="header">
    <h1>WQXSIM</h1>
    <div class="status">
      <div class="spinner" id='spinner'></div>
      <div class="emscripten" id="status">Downloading...</div>
    </div>
  </div>

  <div class="emscripten">
    <progress value="0" max="100" id="progress" hidden=1></progress>
  </div>

  <!-- 缩放容器，包含屏幕和虚拟键盘 -->
  <div id="zoom-container" style="transform-origin: top center; transition: transform 0.3s ease;">
    <div class="emscripten_border">
      <canvas class="emscripten" id="canvas" oncontextmenu="event.preventDefault()" tabindex=-1></canvas>
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
    <textarea style="display: none;" id="output" rows="8"></textarea>

    <!-- Virtual Keyboard Container -->
    <div id="virtual-keyboard">
      <!-- Virtual keyboard will be generated here by JavaScript -->
    </div>
  </div>
</template>


<style scoped>
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

/* 移动设备检测 */
.mobile-device {
  --drawer-width: 280px;
}

/* 桌面设备样式 */
.desktop-device {
  --drawer-width: 350px;
}

/* 抽屉样式 */
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
}

.drawer.open {
  transform: translateX(0);
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
}

.drawer-overlay.show {
  display: block;
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
}

.drawer-toggle:active {
  background-color: #2980b9;
}

/* 移动设备特定样式 */
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
    /* border-radius: 8px; */
    box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
    touch-action: manipulation;
  }

  #virtual-keyboard {
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
  }

  .screen_num span {
    font-size: 14px;
  }
}

.header {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.header h1 {
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 10px;
  margin: 0;
}

.header #rom-selection {
  margin: 10px;
  display: flex;
  justify-content: center;
  align-items: center;
}

#rom-selection #rom-select {
  padding: 5px;
  border-radius: 3px;
  border: 1px solid #ccc;
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
}

.spinner:before {
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

@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}

#controls {
  margin: 10px 0;
  display: flex;
  gap: 15px;
  align-items: center;
}

#output {
  width: 100%;
  max-width: 800px;
  height: 150px;
  margin-top: 10px;
  font-family: monospace;
  resize: none;
}

#zoom-controls {
  padding: 10px 0;
}

/* Virtual keyboard styles */
#virtual-keyboard {
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
}

.virtual-key:active,
.virtual-key.pressed {
  background-color: #3498db;
  color: white;
  border-color: #2980b9;
  transform: translateY(2px);
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

.file-manager-section {
  display: flex;
  flex-direction: rom;
  margin-bottom: 10px;
}

/* 文件下载样式 */
.file-download-section {
  display: inline-block;
  vertical-align: middle;
}

#download-files {
  padding: 6px 12px;
  background: linear-gradient(135deg, #27ae60, #2ecc71);
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-weight: bold;
  transition: all 0.3s ease;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}

#download-files:hover {
  background: linear-gradient(135deg, #229954, #27ae60);
  transform: translateY(-1px);
  box-shadow: 0 4px 8px rgba(0, 0, 0, 0.15);
}

#download-files:active {
  transform: translateY(0);
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}

#file-select {
  margin-left: 10px;
  padding: 6px 8px;
  border-radius: 4px;
  border: 1px solid #ccc;
  background-color: white;
  font-size: 14px;
  min-width: 200px;
  transition: border-color 0.3s ease;
}

#file-select:hover {
  border-color: #3498db;
}

#file-select:focus {
  outline: none;
  border-color: #2980b9;
  box-shadow: 0 0 0 2px rgba(52, 152, 219, 0.2);
}

#file-select option {
  padding: 8px;
}
</style>
