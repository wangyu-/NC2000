<template>
    <div class="file-manager">
        <h3>文件管理</h3>

        <!-- 路径导航 -->
        <div class="path-navigator">
            <button @click="navigateTo('/')" class="path-item home">/</button>
            <template v-for="(segment, index) in currentPathSegments" :key="index">
                <span class="path-separator">/</span>
                <button 
                    @click="navigateTo(buildPath(currentPathSegments, index + 1))" 
                    class="path-item"
                >
                    {{ segment }}
                </button>
            </template>
        </div>

        <!-- 工具栏 -->
        <div class="toolbar">
            <button @click="refreshCurrentDirectory" class="toolbar-button" title="刷新">
                🔄 刷新
            </button>
            <button @click="createNewDirectory" class="toolbar-button" title="新建目录">
                📁 新建目录
            </button>
            <button @click="triggerFileUpload" class="toolbar-button primary" title="上传文件">
                📤 上传文件
            </button>
            <input 
                type="file" 
                ref="fileInput"
                style="display: none;" 
                multiple 
                accept="*/*" 
                @change="handleFileUpload"
            >
        </div>

        <!-- 文件列表 -->
        <div class="file-list-container">
            <table class="file-list">
                <thead>
                    <tr>
                        <th>名称</th>
                        <th>大小</th>
                        <th>操作</th>
                    </tr>
                </thead>
                <tbody>
                    <!-- 返回上一级目录 -->
                    <tr v-if="currentPath !== '/'">
                        <td colspan="3">
                            <button @click="navigateUp" class="nav-item">
                                📁 ..
                            </button>
                        </td>
                    </tr>
                    
                    <!-- 目录列表 -->
                    <tr v-for="dir in directories" :key="dir.path" class="dir-item">
                        <td>
                            <button @click="navigateTo(dir.path)" class="nav-item">
                                📁 {{ dir.name }}
                            </button>
                        </td>
                        <td>-</td>
                        <td>
                            <button @click="renameItem(dir)" class="action-button" title="重命名">
                                重命名
                            </button>
                            <button @click="deleteDirectory(dir.path)" class="action-button delete" title="删除">
                                删除
                            </button>
                        </td>
                    </tr>
                    
                    <!-- 文件列表 -->
                    <tr v-for="file in files" :key="file.path" class="file-item">
                        <td>{{ file.name }}</td>
                        <td>{{ formatFileSize(file.size || 0) }}</td>
                        <td>
                            <button @click="downloadFile(file)" class="action-button" title="下载">
                                下载
                            </button>
                            <button @click="renameItem(file)" class="action-button" title="重命名">
                                重命名
                            </button>
                            <button @click="deleteFile(file.path)" class="action-button delete" title="删除">
                                删除
                            </button>
                            <button @click="moveItem(file)" class="action-button" title="移动">
                                移动
                            </button>
                        </td>
                    </tr>
                </tbody>
            </table>
            
            <div v-if="directories.length === 0 && files.length === 0 && currentPath !== '/loading'" class="empty-message">
                目录为空
            </div>
        </div>

        <!-- 状态信息 -->
        <div class="status-text" v-if="statusMessage">{{ statusMessage }}</div>

        <!-- 重命名对话框 -->
        <div v-if="showRenameDialog" class="modal-overlay" @click="closeRenameDialog">
            <div class="modal-content" @click.stop>
                <h4>重命名 {{ renamingItem?.name }}</h4>
                <input 
                    v-model="newName" 
                    type="text" 
                    class="rename-input"
                    @keyup.enter="confirmRename"
                    ref="renameInput"
                >
                <div class="modal-buttons">
                    <button @click="closeRenameDialog" class="modal-button secondary">取消</button>
                    <button @click="confirmRename" class="modal-button primary">确认</button>
                </div>
            </div>
        </div>

        <!-- 新建目录对话框 -->
        <div v-if="showCreateDirDialog" class="modal-overlay" @click="closeCreateDirDialog">
            <div class="modal-content" @click.stop>
                <h4>新建目录</h4>
                <input 
                    v-model="newDirName" 
                    type="text" 
                    class="rename-input"
                    @keyup.enter="confirmCreateDir"
                    ref="newDirInput"
                    placeholder="输入目录名称"
                >
                <div class="modal-buttons">
                    <button @click="closeCreateDirDialog" class="modal-button secondary">取消</button>
                    <button @click="confirmCreateDir" class="modal-button primary">确认</button>
                </div>
            </div>
        </div>

        <!-- 移动对话框 -->
        <div v-if="showMoveDialog" class="modal-overlay" @click="closeMoveDialog">
            <div class="modal-content move-modal" @click.stop>
                <h4>移动 {{ movingItem?.name }}</h4>
                <div class="move-dialog-content">
                    <div class="path-navigator">
                        <button @click="navigateMoveTo('/')" class="path-item home">/</button>
                        <template v-for="(segment, index) in movePathSegments" :key="index">
                            <span class="path-separator">/</span>
                            <button 
                                @click="navigateMoveTo(buildPath(movePathSegments, index + 1))" 
                                class="path-item"
                            >
                                {{ segment }}
                            </button>
                        </template>
                    </div>
                    
                    <div class="move-directories">
                        <!-- 返回上一级目录 -->
                        <button v-if="movePath !== '/'" @click="navigateMoveUp" class="nav-item">
                            📁 ..
                        </button>
                        
                        <!-- 目录列表 -->
                        <button 
                            v-for="dir in moveDirectories" 
                            :key="dir.path" 
                            @click="navigateMoveTo(dir.path)" 
                            class="nav-item"
                        >
                            📁 {{ dir.name }}
                        </button>
                    </div>
                    
                    <div class="modal-buttons">
                        <button @click="closeMoveDialog" class="modal-button secondary">取消</button>
                        <button @click="confirmMove" class="modal-button primary">移动到此处</button>
                    </div>
                </div>
            </div>
        </div>
    </div>
</template>

<script setup lang="ts">
import { ref, computed, nextTick } from 'vue'

interface FileSystemItem {
    path: string;
    name: string;
    size?: number;
    isDirectory?: boolean;
}

const props = defineProps({
    wasmInstance: {
        type: Object,
        default: () => null,
    }
})

// 当前路径和内容
const currentPath = ref('/');
const files = ref<FileSystemItem[]>([]);
const directories = ref<FileSystemItem[]>([]);
const statusMessage = ref('');

// 对话框状态
const showRenameDialog = ref(false);
const showCreateDirDialog = ref(false);
const showMoveDialog = ref(false);
const renamingItem = ref<FileSystemItem | null>(null);
const movingItem = ref<FileSystemItem | null>(null);
const newName = ref('');
const newDirName = ref('');

// 移动对话框相关
const movePath = ref('/');
const moveDirectories = ref<FileSystemItem[]>([]);

// 引用
const fileInput = ref<HTMLInputElement | null>(null);
const renameInput = ref<HTMLInputElement | null>(null);
const newDirInput = ref<HTMLInputElement | null>(null);

// 计算当前路径的分段
const currentPathSegments = computed(() => {
    if (currentPath.value === '/') return [];
    return currentPath.value.split('/').filter(Boolean);
});

// 计算移动对话框中的路径分段
const movePathSegments = computed(() => {
    if (movePath.value === '/') return [];
    return movePath.value.split('/').filter(Boolean);
});

// 构建路径
function buildPath(segments: string[], length: number): string {
    if (length === 0) return '/';
    return '/' + segments.slice(0, length).join('/');
}

// 加载当前目录内容
function loadCurrentDirectory() {
    if (!props.wasmInstance) {
        statusMessage.value = 'WASM实例未初始化，无法访问文件系统';
        return;
    }

    const FS = props.wasmInstance.FS;
    files.value = [];
    directories.value = [];
    statusMessage.value = '';

    try {
        // 检查当前路径是否存在
        try {
            const stat = FS.stat(currentPath.value);
            if (!FS.isDir(stat.mode)) {
                statusMessage.value = '当前路径不是目录';
                return;
            }
        } catch (e) {
            statusMessage.value = `无法访问目录 ${currentPath.value}: ${(e as Error).message}`;
            return;
        }

        // 读取目录内容
        const entries = FS.readdir(currentPath.value);

        for (const entry of entries) {
            if (entry === '.' || entry === '..') continue;

            const fullPath = currentPath.value === '/' ? `/${entry}` : `${currentPath.value}/${entry}`;
            try {
                const stat = FS.stat(fullPath);
                if (FS.isFile(stat.mode)) {
                    files.value.push({
                        path: fullPath,
                        name: entry,
                        size: stat.size,
                        isDirectory: false
                    });
                } else if (FS.isDir(stat.mode)) {
                    directories.value.push({
                        path: fullPath,
                        name: entry,
                        isDirectory: true
                    });
                }
            } catch (e) {
                // 忽略权限错误和系统文件错误
                if ((e as any).errno !== 63 && (e as any).errno !== 44) {
                    console.warn(`无法访问 ${fullPath}:`, e);
                }
            }
        }

        // 按名称排序
        directories.value.sort((a, b) => a.name.localeCompare(b.name));
        files.value.sort((a, b) => a.name.localeCompare(b.name));

    } catch (e) {
        console.error('加载目录内容时出错:', e);
        statusMessage.value = `加载目录失败: ${(e as Error).message}`;
    }
}

// 刷新当前目录
function refreshCurrentDirectory() {
    loadCurrentDirectory();
}

// 导航到指定路径
function navigateTo(path: string) {
    // 跳过系统目录
    if (path.startsWith('/proc') || path.startsWith('/dev')) {
        statusMessage.value = '无法访问系统目录';
        return;
    }
    currentPath.value = path;
    loadCurrentDirectory();
}

// 返回上一级目录
function navigateUp() {
    if (currentPath.value === '/') return;
    const parentPath = currentPath.value.split('/').slice(0, -1).join('/') || '/';
    navigateTo(parentPath);
}

// 格式化文件大小
function formatFileSize(bytes: number): string {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

// 下载文件
function downloadFile(file: FileSystemItem) {
    if (!props.wasmInstance) {
        statusMessage.value = 'WASM实例未初始化，请稍后再试';
        return;
    }

    try {
        const FS = props.wasmInstance.FS;

        // 读取文件内容
        const data = FS.readFile(file.path);

        // 创建Blob并下载
        const blob = new Blob([data], { type: 'application/octet-stream' });
        const url = URL.createObjectURL(blob);

        // 创建临时下载链接
        const a = document.createElement('a');
        a.href = url;
        a.download = file.name;
        document.body.appendChild(a);
        a.click();

        // 清理
        setTimeout(() => {
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        }, 100);

        statusMessage.value = `已下载文件: ${file.name}`;
        setTimeout(() => statusMessage.value = '', 3000);

    } catch (e) {
        console.error('下载文件时出错:', e);
        statusMessage.value = `下载文件失败: ${(e as Error).message}`;
    }
}

// 触发文件上传
function triggerFileUpload() {
    fileInput.value?.click();
}

// 处理文件上传
async function handleFileUpload(event: Event) {
    const target = event.target as HTMLInputElement;
    if (target.files && target.files.length > 0) {
        await uploadFilesToCurrentDirectory(target.files);
        // 清空文件输入，允许重复选择相同文件
        target.value = '';
    }
}

// 上传文件到当前目录
async function uploadFilesToCurrentDirectory(filesToUpload: FileList) {
    if (!props.wasmInstance) {
        statusMessage.value = 'WASM实例未初始化，请稍后再试';
        return;
    }

    const FS = props.wasmInstance.FS;
    let uploadedCount = 0;
    let failedCount = 0;

    statusMessage.value = '开始上传文件...';

    for (let i = 0; i < filesToUpload.length; i++) {
        const file = filesToUpload[i] as File;
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

            // 构建目标路径
            const targetPath = currentPath.value === '/' 
                ? `/${file.name}` 
                : `${currentPath.value}/${file.name}`;

            // 写入文件到虚拟文件系统
            FS.writeFile(targetPath, uint8Array);
            console.log(`已上传文件: ${targetPath} (${formatFileSize(file.size)})`);

            uploadedCount++;
            statusMessage.value = `上传进度: ${uploadedCount}/${filesToUpload.length}`;

        } catch (error) {
            console.error(`上传文件 ${file.name} 失败:`, error);
            failedCount++;
            statusMessage.value = `上传失败: ${file.name}`;
        }
    }

    // 显示最终结果
    if (failedCount === 0) {
        statusMessage.value = `成功上传 ${uploadedCount} 个文件`;
    } else {
        statusMessage.value = `上传完成: ${uploadedCount} 成功, ${failedCount} 失败`;
    }

    // 刷新文件列表
    setTimeout(() => {
        loadCurrentDirectory();
        statusMessage.value = '';
    }, 1000);
}

// 打开重命名对话框
function renameItem(item: FileSystemItem) {
    renamingItem.value = item;
    newName.value = item.name;
    showRenameDialog.value = true;
    nextTick(() => {
        renameInput.value?.select();
        renameInput.value?.focus();
    });
}

// 关闭重命名对话框
function closeRenameDialog() {
    showRenameDialog.value = false;
    renamingItem.value = null;
    newName.value = '';
}

// 确认重命名
function confirmRename() {
    if (!renamingItem.value || !newName.value.trim()) {
        statusMessage.value = '名称不能为空';
        return;
    }

    if (!props.wasmInstance) {
        statusMessage.value = 'WASM实例未初始化，请稍后再试';
        closeRenameDialog();
        return;
    }

    try {
        const FS = props.wasmInstance.FS;
        const oldPath = renamingItem.value.path;
        const parentPath = oldPath.substring(0, oldPath.lastIndexOf('/')) || '/';
        const newPath = parentPath === '/' ? `/${newName.value.trim()}` : `${parentPath}/${newName.value.trim()}`;

        // 检查目标是否已存在
        try {
            FS.stat(newPath);
            statusMessage.value = '该名称已存在';
            return;
        } catch (e) {
            // 文件不存在，继续
        }

        // 执行重命名
        FS.rename(oldPath, newPath);
        statusMessage.value = `已重命名为: ${newName.value.trim()}`;
        closeRenameDialog();
        loadCurrentDirectory();
        setTimeout(() => statusMessage.value = '', 3000);

    } catch (e) {
        console.error('重命名失败:', e);
        statusMessage.value = `重命名失败: ${(e as Error).message}`;
    }
}

// 删除文件
function deleteFile(filePath: string) {
    if (!confirm('确定要删除这个文件吗？此操作无法撤销。')) {
        return;
    }

    if (!props.wasmInstance) {
        statusMessage.value = 'WASM实例未初始化，请稍后再试';
        return;
    }

    try {
        const FS = props.wasmInstance.FS;
        FS.unlink(filePath);
        statusMessage.value = '文件已删除';
        loadCurrentDirectory();
        setTimeout(() => statusMessage.value = '', 3000);

    } catch (e) {
        console.error('删除文件失败:', e);
        statusMessage.value = `删除文件失败: ${(e as Error).message}`;
    }
}

// 删除目录
function deleteDirectory(dirPath: string) {
    if (!confirm('确定要删除这个目录吗？此操作无法撤销。')) {
        return;
    }

    if (!props.wasmInstance) {
        statusMessage.value = 'WASM实例未初始化，请稍后再试';
        return;
    }

    try {
        const FS = props.wasmInstance.FS;
        
        // 检查目录是否为空
        const entries = FS.readdir(dirPath);
        if (entries.length > 2) { // 包含 . 和 ..
            if (!confirm('目录不为空，确定要删除所有内容吗？')) {
                return;
            }
            // 递归删除非空目录
            FS.rmdir(dirPath, { recursive: true });
        } else {
            // 删除空目录
            FS.rmdir(dirPath);
        }
        
        statusMessage.value = '目录已删除';
        loadCurrentDirectory();
        setTimeout(() => statusMessage.value = '', 3000);

    } catch (e) {
        console.error('删除目录失败:', e);
        statusMessage.value = `删除目录失败: ${(e as Error).message}`;
    }
}

// 打开新建目录对话框
function createNewDirectory() {
    newDirName.value = '';
    showCreateDirDialog.value = true;
    nextTick(() => {
        newDirInput.value?.focus();
    });
}

// 关闭新建目录对话框
function closeCreateDirDialog() {
    showCreateDirDialog.value = false;
    newDirName.value = '';
}

// 确认创建目录
function confirmCreateDir() {
    if (!newDirName.value.trim()) {
        statusMessage.value = '目录名称不能为空';
        return;
    }

    if (!props.wasmInstance) {
        statusMessage.value = 'WASM实例未初始化，请稍后再试';
        closeCreateDirDialog();
        return;
    }

    try {
        const FS = props.wasmInstance.FS;
        const dirPath = currentPath.value === '/' 
            ? `/${newDirName.value.trim()}` 
            : `${currentPath.value}/${newDirName.value.trim()}`;

        // 检查目录是否已存在
        try {
            FS.stat(dirPath);
            statusMessage.value = '该目录已存在';
            return;
        } catch (e) {
            // 目录不存在，继续
        }

        // 创建目录
        FS.mkdir(dirPath);
        statusMessage.value = `已创建目录: ${newDirName.value.trim()}`;
        closeCreateDirDialog();
        loadCurrentDirectory();
        setTimeout(() => statusMessage.value = '', 3000);

    } catch (e) {
        console.error('创建目录失败:', e);
        statusMessage.value = `创建目录失败: ${(e as Error).message}`;
    }
}

// 打开移动对话框
function moveItem(item: FileSystemItem) {
    movingItem.value = item;
    movePath.value = '/';
    loadMoveDirectories();
    showMoveDialog.value = true;
}

// 关闭移动对话框
function closeMoveDialog() {
    showMoveDialog.value = false;
    movingItem.value = null;
}

// 加载移动对话框中的目录列表
function loadMoveDirectories() {
    if (!props.wasmInstance) {
        return;
    }

    const FS = props.wasmInstance.FS;
    moveDirectories.value = [];

    try {
        const entries = FS.readdir(movePath.value);

        for (const entry of entries) {
            if (entry === '.' || entry === '..') continue;

            const fullPath = movePath.value === '/' ? `/${entry}` : `${movePath.value}/${entry}`;
            try {
                const stat = FS.stat(fullPath);
                // 确保不包含正在移动的项目本身
                if (FS.isDir(stat.mode) && fullPath !== movingItem?.value?.path) {
                    moveDirectories.value.push({
                        path: fullPath,
                        name: entry,
                        isDirectory: true
                    });
                }
            } catch (e) {
                // 忽略错误
            }
        }

        // 按名称排序
        moveDirectories.value.sort((a, b) => a.name.localeCompare(b.name));

    } catch (e) {
        console.error('加载目录列表时出错:', e);
    }
}

// 在移动对话框中导航
function navigateMoveTo(path: string) {
    // 跳过系统目录
    if (path.startsWith('/proc') || path.startsWith('/dev')) {
        return;
    }
    movePath.value = path;
    loadMoveDirectories();
}

// 在移动对话框中返回上一级
function navigateMoveUp() {
    if (movePath.value === '/') return;
    const parentPath = movePath.value.split('/').slice(0, -1).join('/') || '/';
    navigateMoveTo(parentPath);
}

// 确认移动
function confirmMove() {
    if (!movingItem.value) {
        return;
    }

    if (!props.wasmInstance) {
        statusMessage.value = 'WASM实例未初始化，请稍后再试';
        closeMoveDialog();
        return;
    }

    try {
        const FS = props.wasmInstance.FS;
        const oldPath = movingItem.value.path;
        const newPath = movePath.value === '/' 
            ? `/${movingItem.value.name}` 
            : `${movePath.value}/${movingItem.value.name}`;

        // 检查目标是否已存在
        try {
            FS.stat(newPath);
            statusMessage.value = '目标位置已存在同名项目';
            return;
        } catch (e) {
            // 文件不存在，继续
        }

        // 执行移动
        FS.rename(oldPath, newPath);
        statusMessage.value = `已移动到: ${movePath.value}`;
        closeMoveDialog();
        loadCurrentDirectory();
        setTimeout(() => statusMessage.value = '', 3000);

    } catch (e) {
        console.error('移动失败:', e);
        statusMessage.value = `移动失败: ${(e as Error).message}`;
    }
}

// 组件挂载时加载根目录
loadCurrentDirectory();
</script>

<style scoped lang="less">
.file-manager {
    max-width: 100%;
    margin: 0 auto;
    padding: 15px;
    background-color: #f9f9f9;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}

h3 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #333;
}

.path-navigator {
    display: flex;
    align-items: center;
    margin-bottom: 15px;
    padding: 10px;
    background-color: #fff;
    border-radius: 4px;
    flex-wrap: wrap;
}

.path-item {
    background: none;
    border: none;
    color: #2c3e50;
    cursor: pointer;
    padding: 2px 8px;
    border-radius: 3px;
    font-size: 14px;
    transition: background-color 0.2s;

    &:hover {
        background-color: #e3f2fd;
    }

    &.home {
        font-weight: bold;
    }
}

.path-separator {
    color: #7f8c8d;
    margin: 0 4px;
}

.toolbar {
    display: flex;
    gap: 10px;
    margin-bottom: 15px;
    flex-wrap: wrap;
}

.toolbar-button {
    padding: 8px 12px;
    background-color: #ecf0f1;
    border: 1px solid #bdc3c7;
    border-radius: 4px;
    cursor: pointer;
    font-size: 14px;
    transition: all 0.2s;

    &:hover {
        background-color: #d5dbdb;
    }

    &.primary {
        background-color: #3498db;
        color: white;
        border-color: #2980b9;

        &:hover {
            background-color: #2980b9;
        }
    }
}

.file-list-container {
    background-color: #fff;
    border-radius: 4px;
    overflow: hidden;
    margin-bottom: 15px;
    max-height: 400px;
    overflow-y: auto;
}

.file-list {
    width: 100%;
    border-collapse: collapse;
}

.file-list th,
.file-list td {
    padding: 10px 15px;
    text-align: left;
    border-bottom: 1px solid #eee;
}

.file-list th {
    background-color: #f5f5f5;
    font-weight: 600;
    color: #333;
    position: sticky;
    top: 0;
}

.file-list tr:hover {
    background-color: #f9f9f9;
}

.nav-item {
    background: none;
    border: none;
    cursor: pointer;
    padding: 5px 10px;
    text-align: left;
    width: 100%;
    transition: background-color 0.2s;

    &:hover {
        background-color: #e3f2fd;
        border-radius: 3px;
    }
}

.action-button {
    padding: 4px 8px;
    margin-right: 5px;
    font-size: 12px;
    background-color: #ecf0f1;
    border: 1px solid #bdc3c7;
    border-radius: 3px;
    cursor: pointer;
    transition: all 0.2s;

    &:hover {
        background-color: #d5dbdb;
    }

    &.delete {
        background-color: #e74c3c;
        color: white;
        border-color: #c0392b;

        &:hover {
            background-color: #c0392b;
        }
    }
}

.empty-message {
    padding: 20px;
    text-align: center;
    color: #7f8c8d;
    font-style: italic;
}

.status-text {
    padding: 10px;
    background-color: #e8f4f8;
    border-left: 4px solid #3498db;
    border-radius: 3px;
    margin-top: 10px;
    font-size: 14px;
}

/* 模态框样式 */
.modal-overlay {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background-color: rgba(0, 0, 0, 0.5);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 1000;
}

.modal-content {
    background-color: white;
    border-radius: 6px;
    padding: 20px;
    width: 90%;
    max-width: 400px;
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
}

.move-modal {
    max-width: 600px;
}

.move-dialog-content {
    max-height: 300px;
    overflow-y: auto;
}

.move-directories {
    margin: 15px 0;
    padding: 10px;
    background-color: #f9f9f9;
    border-radius: 4px;
    max-height: 200px;
    overflow-y: auto;
}

.modal-content h4 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #333;
}

.rename-input {
    width: 100%;
    padding: 10px;
    border: 1px solid #ddd;
    border-radius: 4px;
    margin-bottom: 15px;
    font-size: 14px;
    box-sizing: border-box;
}

.modal-buttons {
    display: flex;
    justify-content: flex-end;
    gap: 10px;
}

.modal-button {
    padding: 8px 16px;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    font-size: 14px;
    transition: background-color 0.2s;

    &.primary {
        background-color: #3498db;
        color: white;

        &:hover {
            background-color: #2980b9;
        }
    }

    &.secondary {
        background-color: #ecf0f1;
        color: #2c3e50;

        &:hover {
            background-color: #d5dbdb;
        }
    }
}

/* 响应式样式 */
@media (max-width: 768px) {
    .file-manager {
        padding: 10px;
    }

    .toolbar {
        flex-direction: column;
    }

    .toolbar-button {
        width: 100%;
    }

    .file-list th,
    .file-list td {
        padding: 8px;
        font-size: 12px;
    }

    .action-button {
        padding: 2px 4px;
        font-size: 10px;
        margin-right: 2px;
    }
}
</style>