// 键盘矩阵定义
export interface KeyData {
  label: string;
  subscript?: string;
  superscript?: string;
  sdlKeys: string[];
}

// ROM 配置
export interface RomConfig {
  name: string;
  args: string[];
  files: RomFile[];
}

export interface RomFile {
  url: string;
  vfsPath: string;
}

// 文件系统项
export interface FileSystemItem {
  path: string;
  name: string;
  size: number;
  isDirectory?: boolean;
}

// 缩放配置
export interface ZoomConfig {
  level: number;
  autoFit: boolean;
}

// 应用状态
export interface AppState {
  currentRom: string;
  isWasmLoaded: boolean;
  isLoading: boolean;
  statusMessage: string;
  isMobile: boolean;
  isDrawerOpen: boolean;
  zoom: ZoomConfig;
}

// WASM 模块接口
export interface WasmModule {
  FS: any;
  callMain: (args?: string[]) => void;
  cwrap: (name: string, returnType: string, argTypes: string[]) => Function;
  _injectVirtualKeyEvent?: (key: number, down: number) => void;
}

// SDL 键码映射
export interface SDLKeycodes {
  [key: string]: number;
}