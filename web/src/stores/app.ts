import { defineStore } from 'pinia';
import { ref, computed } from 'vue';
import type { RomConfig, WasmModule, FileSystemItem, ZoomConfig } from '../types';

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

export const useAppStore = defineStore('app', () => {
  // 状态
  const currentRom = ref<string>('nc2000');
  const isWasmLoaded = ref<boolean>(false);
  const isLoading = ref<boolean>(false);
  const statusMessage = ref<string>('Downloading...');
  const wasmStatus = ref<string>('');
  const isMobile = ref<boolean>(false);
  const isDrawerOpen = ref<boolean>(false);
  const showVirtualKeyboard = ref<boolean>(true);
  const wasmInstance = ref<WasmModule | null>(null);
  const zoom = ref<ZoomConfig>({
    level: 1.0,
    autoFit: false
  });
  
  // 计算属性
  const currentRomConfig = computed(() => romConfigs[currentRom.value]);
  const romOptions = computed(() => 
    Object.entries(romConfigs).map(([key, config]) => ({
      value: key,
      label: config.name
    }))
  );
  const zoomLevel = computed(() => zoom.value.level);

  // 方法
  const setWasmLoaded = (loaded: boolean) => {
    isWasmLoaded.value = loaded;
  };

  const setLoading = (loading: boolean) => {
    isLoading.value = loading;
  };

  const setStatusMessage = (message: string) => {
    statusMessage.value = message;
  };

  const setMobile = (mobile: boolean) => {
    isMobile.value = mobile;
  };

  const toggleDrawer = () => {
    isDrawerOpen.value = !isDrawerOpen.value;
  };

  const setDrawerOpen = (open: boolean) => {
    isDrawerOpen.value = open;
  };

  const setWasmInstance = (instance: WasmModule | null) => {
    wasmInstance.value = instance;
  };

  const setCurrentRom = (rom: string) => {
    if (romConfigs[rom]) {
      currentRom.value = rom;
    }
  };

  const setZoomLevel = (level: number) => {
    zoom.value.level = Math.min(Math.max(0.2, level), 3.0);
  };

  const setAutoFit = (autoFit: boolean) => {
    zoom.value.autoFit = autoFit;
  };

  const resetZoom = () => {
    zoom.value.level = 1.0;
    zoom.value.autoFit = false;
  };

  const updateFileSystemItems = (items: FileSystemItem[]) => {
    fileSystemItems.value = items;
  };

  const setWasmStatus = (status: string) => {
    statusMessage.value = status;
  };

  const setEmulatorRunning = (running: boolean) => {
    isLoading.value = running;
  };
  
  const fileSystemItems = ref<FileSystemItem[]>([]);

  const initializeApp = () => {
    // 检测是否为移动设备
    const mobile = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
    setMobile(mobile);
    
    // 在移动设备上默认关闭抽屉，桌面设备上默认打开
    setDrawerOpen(!mobile);
  };

  return {
    // 状态
    currentRom,
    isWasmLoaded,
    isLoading,
    statusMessage,
    wasmStatus,
    isMobile,
    isDrawerOpen,
    showVirtualKeyboard,
    wasmInstance,
    zoom,
    fileSystemItems,
    
    // 计算属性
    currentRomConfig,
    romOptions,
    zoomLevel,
    
    // 方法
    setWasmLoaded,
    setLoading,
    setStatusMessage,
    setMobile,
    toggleDrawer,
    setDrawerOpen,
    setWasmInstance,
    setCurrentRom,
    setZoomLevel,
    setAutoFit,
    resetZoom,
    updateFileSystemItems,
    setWasmStatus,
    setEmulatorRunning,
    initializeApp,
    
    // 常量
    romConfigs
  };
});