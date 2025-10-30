<template>
    <div ref="keyboardRef" class="virtual-keyboard">
        <div v-for="(row, index) in keyMatrix" :key="index" class="keyboard-row">
            <template v-for="key in row" :key="key.label">
                <div class="virtual-key" v-if="key.label" :class="key.classes"
                    @mousedown="simulateSDLKeyEvent(key.sdlKeys, true)"
                    @mouseup="simulateSDLKeyEvent(key.sdlKeys, false)"
                    @touchstart="simulateSDLKeyEvent(key.sdlKeys, true)"
                    @touchend="simulateSDLKeyEvent(key.sdlKeys, false)">

                    <span class="key-label">{{ key.label }}</span>
                    <span v-if="key.subscript" class="key-subscript">{{ key.subscript }}</span>
                    <span v-if="key.superscript" class="key-superscript">{{ key.superscript }}</span>
                </div>
                <div v-else class="empty-key">
                </div>
            </template>

        </div>
    </div>
</template>

<script setup lang="ts">

import { ref, defineProps } from 'vue';
const props = defineProps({
    wasmInstance: {
        type: Object,
        default: () => { },
    }
});


const keyboardRef = ref<HTMLDivElement | null>(null);

// 类型定义
interface KeyData {
    label: string;
    subscript: string;
    superscript: string;
    sdlKeys: string[];
    classes?: string[];
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

const emptyKey: KeyData = { label: '', subscript: '', superscript: '', sdlKeys: [], classes: ['empty-key'] };
// 键盘矩阵
const keyMatrix: (KeyData)[][] = [
    // Row 2
    [{ label: 'ON/OFF', subscript: '', superscript: '', sdlKeys: ['SDLK_F12'] },
        emptyKey, emptyKey, emptyKey, emptyKey, emptyKey,
    { label: 'F1', subscript: '', superscript: '插入', sdlKeys: ['SDLK_F1'] },
    { label: 'F2', subscript: '', superscript: '删除', sdlKeys: ['SDLK_F2'] },
    { label: 'F3', subscript: '', superscript: '查找', sdlKeys: ['SDLK_F3'] },
    { label: 'F4', subscript: '', superscript: '修改', sdlKeys: ['SDLK_F4'] }],
    // Row 3
    [{ label: '发音', subscript: '', superscript: '', sdlKeys: ['SDLK_SEMICOLON'], classes: ['fun-key'] },
    { label: '报时', subscript: '', superscript: '', sdlKeys: ['SDLK_QUOTE'], classes: ['fun-key'] },
        emptyKey,
    { label: '英汉', subscript: '', superscript: '汉英', sdlKeys: ['SDLK_F5'], classes: ['fun-key'] },
    { label: '名片', subscript: '', superscript: '通讯', sdlKeys: ['SDLK_F6'], classes: ['fun-key'] },
    { label: '计算', subscript: '', superscript: '换算', sdlKeys: ['SDLK_F7'], classes: ['fun-key'] },
    { label: '行程', subscript: '', superscript: '记事', sdlKeys: ['SDLK_F8'], classes: ['fun-key'] },
    { label: '资料', subscript: '', superscript: '游戏', sdlKeys: ['SDLK_F9'], classes: ['fun-key'] },
    { label: '时间', subscript: '', superscript: '其他', sdlKeys: ['SDLK_F10'], classes: ['fun-key'] },
    { label: '网络', subscript: '', superscript: '', sdlKeys: ['SDLK_F11'], classes: ['fun-key'] }],
    // Row 5 - Q row
    [{ label: 'Q', subscript: 'sin', superscript: 'sin-1', sdlKeys: ['SDLK_q'] },
    { label: 'W', subscript: 'cos', superscript: 'cos-1', sdlKeys: ['SDLK_w'] },
    { label: 'E', subscript: 'tan', superscript: 'tan-1', sdlKeys: ['SDLK_e'] },
    { label: 'R', subscript: '1/X', superscript: 'hyp', sdlKeys: ['SDLK_r'] },
    { label: 'T', subscript: '7', superscript: '', sdlKeys: ['SDLK_t', 'SDLK_7'], classes: ['num-key'] },
    { label: 'Y', subscript: '8', superscript: '', sdlKeys: ['SDLK_y', 'SDLK_8'], classes: ['num-key'] },
    { label: 'U', subscript: '9', superscript: '', sdlKeys: ['SDLK_u', 'SDLK_9'], classes: ['num-key'] },
    { label: 'I', subscript: '%', superscript: '', sdlKeys: ['SDLK_i'] },
    { label: 'O', subscript: '÷', superscript: '#', sdlKeys: ['SDLK_o'] },
    { label: 'P', subscript: 'MC', superscript: '☎', sdlKeys: ['SDLK_p'] }],
    // Row 6 - A row
    [{ label: 'A', subscript: 'log', superscript: '10x', sdlKeys: ['SDLK_a'] },
    { label: 'S', subscript: 'ln', superscript: 'ex', sdlKeys: ['SDLK_s'] },
    { label: 'D', subscript: 'Xʸ', superscript: 'y√x', sdlKeys: ['SDLK_d'] },
    { label: 'F', subscript: '√', superscript: 'X²', sdlKeys: ['SDLK_f'] },
    { label: 'G', subscript: '4', superscript: '', sdlKeys: ['SDLK_g', 'SDLK_4'], classes: ['num-key'] },
    { label: 'H', subscript: '5', superscript: '', sdlKeys: ['SDLK_h', 'SDLK_5'], classes: ['num-key'] },
    { label: 'J', subscript: '6', superscript: '', sdlKeys: ['SDLK_j', 'SDLK_6'], classes: ['num-key'] },
    { label: 'K', subscript: '±', superscript: '', sdlKeys: ['SDLK_k'] },
    { label: 'L', subscript: 'x', superscript: '*', sdlKeys: ['SDLK_l'] },
    { label: '输入', subscript: 'MR', superscript: '', sdlKeys: ['SDLK_RETURN'], classes: ['enter-key'] }
    ],
    // Row 7 - Z row
    [{ label: 'Z', subscript: '(', superscript: ')', sdlKeys: ['SDLK_z'] },
    { label: 'X', subscript: 'π', superscript: 'X!', sdlKeys: ['SDLK_x'] },
    { label: 'C', subscript: 'EXP', superscript: '。\'"', sdlKeys: ['SDLK_c'] },
    { label: 'V', subscript: 'C', superscript: '', sdlKeys: ['SDLK_v'] },
    { label: 'B', subscript: '1', superscript: '', sdlKeys: ['SDLK_b', 'SDLK_1'], classes: ['num-key'] },
    { label: 'N', subscript: '2', superscript: '', sdlKeys: ['SDLK_n', 'SDLK_2'], classes: ['num-key'] },
    { label: 'M', subscript: '3', superscript: '', sdlKeys: ['SDLK_m', 'SDLK_3'], classes: ['num-key'] },
    { label: '↟', subscript: '税', superscript: '', sdlKeys: ['SDLK_COMMA'] },
    { label: '▲', subscript: '-', superscript: '', sdlKeys: ['SDLK_UP'] },
    { label: '↡', subscript: 'M-', superscript: '', sdlKeys: ['SDLK_SLASH'] },],
    // Row 8 - Top function keys
    [{ label: '求助', subscript: '', superscript: '', sdlKeys: ['SDLK_LEFTBRACKET'], classes: ['fun-key'] },
    { label: '中英数', subscript: '', superscript: 'SHIFT', sdlKeys: ['SDLK_RIGHTBRACKET'] },
    { label: '输入法', subscript: '', superscript: '反查 CAPS', sdlKeys: ['SDLK_BACKSLASH'] },
    { label: '跳出', subscript: 'AC', superscript: '', sdlKeys: ['SDLK_ESCAPE'] },
    { label: '符号', subscript: '0', superscript: '继续', sdlKeys: ['SDLK_0'], classes: ['num-key', 'num-key-0'] },
    { label: '.', subscript: '.', superscript: '-', sdlKeys: ['SDLK_PERIOD'] },
    { label: '空格', subscript: '=', superscript: '✓', sdlKeys: ['SDLK_EQUALS', 'SDLK_SPACE'] },
    { label: '◀', subscript: '', superscript: '', sdlKeys: ['SDLK_LEFT'] },
    { label: '▼', subscript: '+', superscript: '', sdlKeys: ['SDLK_DOWN'] },
    { label: '▶', subscript: 'M+', superscript: '', sdlKeys: ['SDLK_RIGHT'] }],
];

// 模拟SDL按键事件
function simulateSDLKeyEvent(sdlKeyNames: string[], keyDown: boolean) {
    console.log("simulateSDLKeyEvent", sdlKeyNames, keyDown);
    // 检查模块和函数是否准备就绪
    if (!props.wasmInstance) {
        console.log('Module not ready yet');
        return;
    }

    // 尝试直接访问函数
    let injectKeyFunction: ((key: number, down: number) => void) | null = null;
    try {
        injectKeyFunction = props.wasmInstance.cwrap('injectVirtualKeyEvent', null, ['number', 'number']);
    } catch (e) {
        console.log('Could not wrap injectVirtualKeyEvent function:', e);
        // 尝试替代方法
        if (typeof props.wasmInstance._injectVirtualKeyEvent === 'function') {
            injectKeyFunction = (key, down) => props.wasmInstance._injectVirtualKeyEvent(key, down);
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
    const keyName = sdlKeyNames[0];
    if (!keyName) {
        console.warn(`Unknown SDL key: ${keyName}`);
        return;
    }
    const sdlKeyCode = SDLKeycodes[keyName];
    if (sdlKeyCode !== undefined) {
        injectKeyFunction(sdlKeyCode, keyDown ? 1 : 0);
    } else {
        console.warn(`Unknown SDL key: ${keyName}`);
    }
}

</script>

<style scoped lang="less">
.virtual-keyboard {
    display: flex;
    flex-direction: column;
    gap: 5px;
    margin-top: 15px;
    padding: 10px;
    background-color: #e0e0e0;
    border-radius: 8px;
    max-width: 960px;
    width: 100%;

    .keyboard-row {
        display: flex;
        justify-content: center;
        gap: 5px;
    }

    .virtual-key {
        width: 80px;
        height: 40px;
        padding: 8px;
        background-color: #fff;
        border: 2px solid #ccc;
        border-radius: 8px;
        display: flex;
        flex-direction: column;
        justify-content: center;
        align-items: center;
        font-size: 20px;
        cursor: pointer;
        user-select: none;
        position: relative;
        transition: all 0.1s;
        box-shadow: 0 2px 4px rgba(92, 92, 92, 0.2);

        &:active,
        &.pressed {
            transform: scale(0.9);
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
        }
    }

    .empty-key {
        background-color: transparent;
        border: none;
        width: 100px;
        height: 60px;
        cursor: default;
    }

    .enter-key {
        background-color: #f0b295;
    }

    .fun-key {
        background-color: #184B9C;
        color: white;
        display: flex;
        flex-direction: column;
        flex-flow: column-reverse;

        & .key-label {
            width: 100%;
            height: 100%;
            display: flex;
            justify-content: center;
            align-items: center;
        }

        & .key-superscript {
            font-size: 16px;
            height: 100%;
            width: 100%;
            display: flex;
            justify-content: center;
            align-items: center;
            position: relative;
        }
    }

    .num-key {
        display: flex;
        flex-direction: row;

        & .key-label {
            font-size: 24px;
            width: 100%;
            height: 100%;
            text-align: center;
            display: inline-flex;
            justify-content: center;
            align-items: center;
        }

        & .key-subscript {
            font-size: 24px;
            height: 80%;
            width: 100%;
            display: flex;
            justify-content: center;
            align-items: center;
            position: relative;
            background-color: #184B9C;
            border-radius: 8px;
            color: white;
        }
    }

    .num-key-0 {
        & .key-label {
            font-size: 16px;
        }
    }

    .key-label {
        font-weight: bold;
    }

    .key-subscript {
        font-size: 12px;
        position: absolute;
        top: 2px;
        left: 2px;
    }

    .key-superscript {
        font-size: 12px;
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

    .key-enter {}
}
</style>