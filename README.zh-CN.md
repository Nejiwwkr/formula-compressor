# Formula Compressor（公式压缩器）

[![Build](https://github.com/Nejiwwkr/formula-compressor/actions/workflows/build.yml/badge.svg)](https://github.com/Nejiwwkr/formula-compressor/actions/workflows/build.yml)

**把压缩曲线写成公式。**

传统压缩器的静态传输曲线通常写作 `f(x) = min(x, ax + b)`。Formula Compressor
把这条曲线交给用户：你可以只使用主输入电平 `x`，也可以同时使用最多四路外置侧链
`s1`..`s4` 以及它们的总和 `s` 来写 `f(x, s1..s4, s)`。插件将曲线作用在检波包络上，
再配合 Attack / Release / Hold 做动态处理。

![界面](Docs/ui-level.png)

Windows x64 构建发布在
[Releases](https://github.com/Nejiwwkr/formula-compressor/releases/latest) 页面，
包含 VST3、独立程序、安装说明与 SHA-256 校验文件。二进制未做代码签名，
SmartScreen 可能提示未知发布者。

## 工作原理

```text
主输入 ── Input Trim ── x 检波 ─────────────────┐
                                               ├── f(x, s1..s4, s) ── 增益 ── Mix/Makeup ── Monitor ── 输出
SC1 ─┐                                         │
SC2 ─┼── 共用一个 HPF ── s1..s4 检波 ───────────┘
SC3 ─┤        s = s1..s4 的线性幅度之和
SC4 ─┘
```

- `x` 是主输入的检波电平。
- `s1`..`s4` 是四路独立的外置侧链检波电平。SIDECHAIN 开关只有 **Off / On**；
  开启后，宿主启用了哪几路 aux 总线，哪几路就进入对应的变量。
- `s` 是侧链总电平：四路检波**线性幅度**之和，再换算到当前域（两路 -6 dBFS 的
  侧链相加是 0 dBFS，而不是 -12 dBFS）。
- 所有检波共用 Peak/RMS 与 Attack/Release/Hold；四路侧链共用一个 HPF 频率，
  但滤波器状态与包络各自独立。
- `x`、`s1`..`s4` 与 `s` 的域由公式框旁的 `x:` 开关决定：

```text
AMP    x、s1..s4、s 都是线性幅度；f(...) 返回线性幅度
       gain = f(...) / x

dBFS   x、s1..s4、s 都是 dBFS；f(...) 返回 dBFS
       gain dB = f(...) - x
```

公式描述静态传输，时间参数决定检波包络如何到达该曲线。它不是逐样本波形整形，
因此没有混叠，也没有额外延迟。

公式使用**当前侧链电平**求值：函数图显示 `f(x, s1..s4, s)` 在当前值处的切片；提示行
右侧的读数显示四路与总和。侧链关闭、或没有启用任何 aux 总线时，AMP 下
`s1..s4` 与 `s` 均为 0，dBFS 下均为 -120。

AMP 模式下 `f(...)` 的符号承载极性：负输出表示**负增益**，两个声道都会反相
（曲线画成虚线）；`f(...) = 0` 是真正的静音。dBFS 模式是幅度 dB，`f(...) = 0`
表示 0 dB（unity），不是静音；需要静音时写 `-120`。两种模式增益幅度上限都是
+24 dB。Mix 小于 100% 时，反相增益会与干声部分抵消。

默认以 **dBFS** 模式启动，公式是阈值 -12 dBFS、4:1 的经典压缩；在打开侧链或把
`s` 写进公式前，它不会使用侧链：

```text
min(x, -12 + (x + 12) / 4)      // x: dBFS
```

公式标题内的 `x:` 开关同时切换 x、s1..s4 与 s 的域；公式与模式都随工程状态保存。

### 侧链输入与 HPF

SIDECHAIN 卡是 **Off / On** 开关。开启后，宿主启用的所有可选 stereo aux 输入
同时参与；不再需要选择编号。在 FL Studio 中，需要在 wrapper 的
**Processing → VFX** 页面把工程的 sidechain 槽映射到插件输入 3/4、5/6、7/8、9/10，
分别对应 s1..s4。插件无法读取 FL 的轨道名或 sidechain 编号，只能看到哪些 aux
总线被启用。只暴露一个侧链输入的宿主对应 s1，公式仍可使用，s2..s4 为静音。

`SC HPF` 是作用于四路侧链检波的 12 dB/oct 高通；它不改变主音频，也不改变主 `x`
检波，每路保留独立的滤波状态。

提示行读数的形式是 `S1 -6.0 S2 -12.3 S3 -- S4 -- S -3.1`。`--` 表示宿主没有启用
该 aux 总线；`-120.0` 表示总线已启用但当前静音。总 `S` 是线性求和后换算到当前域的值。
侧链关闭时整个读数隐藏，但公式文本保持不变，并以静音哨兵值求值。

### 从 1.3 升级

1.3 保存的是“选中的一路”（`scInput` 0–4）。1.4 载入时按 **Off / On** 处理：
0 → Off，1–4 → On，同时保留公式、HPF、LINK、Monitor 与四路 aux 总线。对于路由了
不止一路的旧工程，有两处行为变化：

- 选中编号的身份不再恢复。On 表示所有已启用的 aux 总线都参与，四路输入都会生效。
- 使用 `s` 的公式以前跟随“选中的一路”，现在跟随四路总和。只路由一路的工程听感不变。
  如需保持逐路行为，把公式里的 `s` 换成 `s1`/`s2`/`s3`/`s4`。

旧的 `scInput` 自动化轨道不再读取；请改为自动化 `scEnabled` 参数。

### 立体声 LINK

`LINK` 为 0–100%。100% 时两个声道共用联动检波（默认，也就是 1.2 的行为）；
0% 时左右各自检波、各自计算增益。GR 表和时域图显示压缩最强的声道。四路侧链检波
及其总和始终是联动的。

### 监听

`MONITOR` 切换输出：

- **Off** — 正常压缩输出。
- **Delta** — `mix * (dry - dry * formulaGain)`，不含 Makeup；听被公式移除的部分。
- **Sidechain** — 四路侧链经过 HPF 后按输出声道求和的实际信号；没有 aux 总线时静音。
  求和可能相位抵消，也可能超过 0 dBFS，不做自动归一化。

Bypass 优先，始终输出干声。

### 时间视图

右侧面板是约 3 秒的滚动电平图，使用与传输图**相同的 dBFS 纵轴**。
**仅公式增益后**（不含 Mix / Makeup）的输入/输出电平以色块呈现：

- **石墨灰**：两者公共主体
- **vermilion**：原始独有，即被公式移除的部分
- **teal**：输出独有，即被公式增加的部分
- **cobalt**：同一时间窗内的增益曲线（`f(x,...)/x`，dB）

电平面积不描边，仅靠颜色区分。**点击时间图可冻结**传输图、时间图与电平表；
再次点击恢复实时显示。时间图始终显示公式增益，不显示 Monitor 输出。

### 三种视图

曲线面板可在三种**等刻度**视图之间切换：

| 视图 | 坐标轴 | 含义 |
|---|---|---|
| **LEVEL** | 输入 / 输出电平 dB | 传统压缩传输曲线；`f(x,...)=x` 是 45° 对角线 |
| **GAIN** | 输入电平 dB / 增益 dB | `f(x,...)/x`；恒定增益是水平线（`0.2x` = -14 dB） |
| **LINEAR** | 线性幅度 / 线性幅度 | 普通坐标；斜率就是增益倍数 |

函数图是当前 `(s1..s4, s)` 的切片。公式不含侧链变量时曲线静止；含侧链变量时，
任意一路或总和变化都会重建曲线，读数显示当前切片使用的值。

截图：[LEVEL](Docs/ui-level.png) · [GAIN](Docs/ui-gain.png) ·
[LINEAR](Docs/ui-linear.png) · [反相](Docs/ui-inverted.png) ·
[dBFS](Docs/ui-dbfs.png) · [侧链开启](Docs/ui-sidechain.png)

## 公式语法

`x` 是主检波电平；`s1`–`s4` 是四路侧链检波电平，`s` 是它们的线性幅度之和。这些
变量都按公式框旁的 `x:` 开关解释为**线性幅度**或**dBFS**。`P1`–`P4` 是四个可调
旋钮（范围 -1..+1）。大小写不敏感，可写 `f(x) = ...`、`f(x, s1, s2, s3, s4, s) = ...`
或 `y = ...` 前缀——前缀参数表只需括号平衡，不要求固定写法。

```text
运算    + - * / % ^          ^ 为幂，右结合
比较    < <= > >= == !=      结果为 0 或 1
逻辑    && || !              条件 cond ? a : b
变量    x, s, s1..s4, P1..P4, pi, e
函数    abs min max clamp(v,lo,hi) pow(a,b) sqrt cbrt
        exp log/ln log10 log2 floor ceil round trunc sign
        sin cos tan asin acos atan atan2 sinh cosh tanh
        step(edge,x) smoothstep(e0,e1,x) mix(a,b,t)
        db(v) lin(d) rand() if(c,a,b)
```

`db(v) = 20*log10(|v|)`，`lin(d) = 10^(d/20)`。侧链关闭或没有启用 aux 总线时，
`s1..s4` 与 `s` 在 AMP 下为 0，dBFS 下为 -120。dBFS 下 `s` 是负数，所以
“侧链越响、压得越多”应写成 `x - max(0, s + threshold) / ratio`；直接写 `x - s/10`
在侧链安静时会变成提升。AMP 模式下单路常用 `x * (1 - 0.8 * min(s1, 1))`。

语法错误时旧曲线继续运行，状态行显示错误列号；公式产生非有限值时曲线对应位置标红
并被引擎跳过。增益幅度上限 +24 dB；AMP 模式下负值是反相、零是静音。

示例（AMP）：

```text
min(x, P1 + (x - P1) * P2)                  经典向下压缩
lin(min(db(x), -12 + (db(x) + 12) / 4))     用 dB 书写的同一曲线
x < P1 ? x : P1 + (x - P1) * P2             硬拐点版本
tanh(x * (1 + P1 * 4)) / tanh(1 + P1 * 4)   饱和型曲线
x < P1 ? x * P2 : x                         向下扩展 / 噪声门
-0.5 * x                                    -6 dB 并反相
x * (1 - 0.8 * min(s, 1))                   使用总 s 的侧链 duck
x - s / 10                                  轻微的侧链 duck
x * (1 - 0.8 * min(s1, 1))                  只用 SC1 的 duck
x - (s1 + s2) / 10                          SC1 + SC2 之和
x - max(s1, s2) / 10                        只跟随较响的 SC1 / SC2
```

示例（dBFS，x、s1..s4、s 与 f 都是 dB）：

```text
min(x, -12 + (x + 12) / 4)                  经典 4:1
x - max(0, s + 60) / 4                      侧链高于 -60 dBFS 时 4:1 下压
x - max(0, s + 40)                          侧链高于 -40 dBFS 时 1:1 下压
x - max(0, max(s1, s2) + 40) / 2            SC1/SC2 中较响者高于 -40 dBFS 时下压
x < -30 ? x : mix(x, -24 + (x + 24) / 4, smoothstep(-30, -24, x))
x < -36 ? x + 12 : x                        向上压缩
x < -40 ? -120 : x                          -40 dBFS 以下静音
```

## 参数

| 控件 | 范围 | 默认 | 说明 |
|---|---|---|---|
| Attack | 0.1–200 ms | 10 ms | 包络上升 |
| Release | 5–2000 ms | 150 ms | 包络下降 |
| Hold | 0–500 ms | 0 ms | 回落后保持压缩的时间 |
| Mix | 0–100% | 100% | 干湿混合 |
| Input Gain | -24..+24 dB | 0 dB | 主检波前增益；外置侧链不受影响 |
| Makeup | -12..+24 dB | 0 dB | 输出补偿 |
| Detector | Peak / RMS | Peak | 同时作用于 x 和 s1..s4；RMS 使用 10 ms 一阶均方时间常数 |
| SC | Off / On | Off | On 时所有已启用的 aux 总线进入 s1–s4 |
| SC HPF | 0–500 Hz | 0 (Off) | 四路侧链共用的 12 dB/oct 高通 |
| Stereo Link | 0–100% | 100% | 100% 联动，0% 双单声道 |
| Monitor | Off / Delta / Sidechain | Off | Delta 不含 Makeup |
| P1–P4 | -1..+1 | 0.25, 0.25, 0, 0 | 公式变量 |
| Bypass | 开 / 关 | 关 | 宿主 Bypass 参数 |

公式文本与 `x` 模式随工程状态保存，但不是可自动化参数；SC 开关（`scEnabled`）、HPF、
LINK 和 MONITOR 是标准 VST3 参数。

## 预设

内置（线性）：Classic 4:1、Soft Knee、Hard Limit、Upward Compress、
Expander / Gate、Tanh Comp、Log Comp、Two Stage。
内置（dBFS）：Classic 4:1 (dB)、Soft Knee (dB)、Upward Compress (dB)、Gate (dB)。
侧链示例：Sidechain Duck (dB)、Sidechain Duck (AMP)。加载预设时会恢复对应的
`x` 模式；侧链开关、HPF、LINK 与 MONITOR 保持不变，方便保留路由。

用户预设保存完整状态到 `%APPDATA%\FormulaCompressor\Presets`，通过顶部的预设菜单
加载，或点 **SAVE** 命名保存。

## 构建与测试

需要 Visual Studio 2022（Desktop C++）与 CMake 3.24+。CMake 会自动下载固定版本的
JUCE 8.0.15；离线时可加 `-DFORMULA_JUCE_PATH=C:\path\to\JUCE`，或保留本地
`ThirdParty/JUCE-8.0.15`。

```powershell
cmake -S . -B Build -G "Visual Studio 17 2022" -A x64
cmake --build Build --config Release --target FormulaCompressor_VST3 FormulaCompressor_Standalone FormulaCompressorTests --parallel
.\Build\FormulaCompressorTests_artefacts\Release\FormulaCompressorTests.exe
powershell -ExecutionPolicy Bypass -File Tools\run_pluginval.ps1 -SkipGuiTests
```

- 测试覆盖表达式解析/求值/错误、`s1`..`s4` 与总和 `s`、Attack/Release/Hold、
  立体声 LINK、四路侧链路由与 HPF、Monitor、Mix/Makeup/Bypass、极端公式、
  状态与预设往返、两种窗口尺寸的 UI 布局、多种采样率与块大小。
- `FormulaCompressorTests.exe --screenshot Docs/ui-level.png level|gain|linear ["公式"] [amp|db] [--size 900x680] [--sidechain]`
  无需 DAW 即可渲染指定视图，例如
  `--screenshot Docs/ui-dbfs.png level "min(x, -12 + (x + 12) / 4)" db`；
  `--sidechain` 会启用两路不同电平的 aux，使 S1/S2/S 读数可见。
- 安装 VST3：`powershell -ExecutionPolicy Bypass -File Tools\install_vst3.ps1`
  （加 `-System` 安装到公共目录）。

## 许可证

Formula Compressor 使用 GNU Affero General Public License v3.0（AGPL-3.0）。
项目以 JUCE 8 的 AGPLv3 方式分发，不包含 JUCE 商业许可；每个发布版本对应的源码
就是本仓库的相应 tag。JUCE 与随附依赖的声明见
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)，完整许可证文本见
[LICENSE](LICENSE)。

## 说明

- 四路侧链依赖宿主提供四路 aux 输入总线。只有一路 aux 时，多路公式退化为 `s1`，
  `s2`..`s4` 保持静音。
- 没有单独的 `x = 侧链` 检波模式；经典侧链压缩通过包含 `s` 或 `s1`..`s4` 的公式表达。
- Monitor 的 **Sidechain** 是四路 post-HPF 音频的原始求和，可能相位抵消或超过
  0 dBFS，不做自动归一化。
- 不含 lookahead，延迟为 0，瞬态响应只由 Attack 决定。
- Makeup 为手动；没有跟随 P1–P4 的自动补偿。
- 公式作用于检波增益，而不是逐样本波形整形。
