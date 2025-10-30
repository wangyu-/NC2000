<template>
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
</template>

<script setup lang="ts">
import { ref } from 'vue'

interface LoadedFile {
    path: string;
    name: string;
    size: number;
}

const props = defineProps({
    wasmInstance: {
        type: Object,
        default: () => null,
    }
})

const selectedFilePath = ref('');
const uploadStatusText = ref('');
const fileList = ref<LoadedFile[]>([]);


// 填充文件列表
function populateFileList() {
    if (!props.wasmInstance) {
        console.warn('WASM实例未初始化，无法获取文件列表');
        return;
    }

    const FS = props.wasmInstance.FS;
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


// 格式化文件大小
function formatFileSize(bytes: number): string {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}


// 下载选中的文件
function downloadSelectedFile() {
    if (!props.wasmInstance) {
        alert('WASM实例未初始化，请稍后再试');
        return;
    }

    if (!selectedFilePath.value) {
        alert('请先选择一个文件');
        return;
    }

    try {
        const FS = props.wasmInstance.FS;

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
    if (!props.wasmInstance) {
        uploadStatusText.value = 'WASM实例未初始化，请稍后再试';
        return;
    }

    const FS = props.wasmInstance.FS;
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

</script>

<style scoped lang="less">
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
</style>