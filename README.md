# STM32WL 窄带通信实验

本仓库整理了基于 STM32WLE5 的窄带通信实验代码，主要内容包括温度采集、光照采集以及温度/光照双向 LoRa 通信。工程使用 STM32CubeMX 生成初始化代码，使用 Keil MDK-ARM 编译和下载。

仓库同时包含实验报告模板、完整实验报告以及各实验对应的程序流程图，便于复现实验和撰写课程报告。

## 硬件与环境

- 主控芯片：STM32WLE5CCU6
- 开发板：CT127C STM32WL 物联网实训平台
- 开发工具：STM32CubeMX、Keil MDK-ARM V5
- 主要外设：ADC、I2C、GPIO、TIM2、USART2、SUBGHZ
- 显示模块：I2C OLED
- 通信方式：STM32WLE5 内置 Sub-GHz LoRa

## 目录结构

```text
STM32WL/
├── diagram/                       # draw.io 流程图源文件和导出的 PNG 图片
│   ├── 实验2/                     # 温度发送/接收流程图
│   ├── 实验3/                     # 光照发送/接收流程图
│   └── 实验4/                     # 温度节点/光照节点双向通信流程图
├── 实验2/
│   └── Temperature/              # 温度采集与 LoRa 传输实验
├── 实验3/
│   └── Light/                    # 光照采集与 LoRa 传输实验
├── 实验4/
│   └── Light and Temperature/    # 温度与光照双向通信综合实验
├── 窄带实验报告模板.doc
└── 窄带实验报告1.doc              # 完整实验报告
```

每个实验工程都保留了 CubeMX 工程文件和 Keil 工程文件：

```text
Demo9_1SPI.ioc
MDK-ARM/Demo9_1SPI.uvprojx
```

## 实验2：温度数据采集与通信

工程路径：

```text
实验2/Temperature
```

实验内容：

- 通过 STS30 温度传感器采集环境温度。
- 使用 I2C2 与温度传感器通信。
- 将温度值转换为扩大 10 倍的整数，例如 `253` 表示 `25.3C`。
- 通过 LoRa 将温度数据发送到另一块板。
- 接收端解析温度帧并通过 OLED 显示。

温度 LoRa 数据帧格式：

```text
0xAA  0x01  data_H  data_L  check  0x55
```

其中 `0x01` 表示温度数据，`check = type ^ data_H ^ data_L`。

角色切换位置：

```c
// 实验2/Temperature/BSP/task.h
#define APP_ROLE_TX_SENSOR 1
```

- `1`：温度发送端
- `0`：温度接收端

相关流程图：

```text
diagram/实验2/lora_temperature_program_flowchart.drawio
diagram/实验2/lora_temperature_program_flowchart-LoRa 温度发送程序流程图.drawio.png
diagram/实验2/lora_temperature_program_flowchart-LoRa 温度接收程序流程图.drawio.png
```

## 实验3：光照数据采集与通信

工程路径：

```text
实验3/Light
```

实验内容：

- 通过光照模块采集模拟电压。
- 光照模块连接到 `PB3`，对应 `ADC_CHANNEL_2`。
- ADC 为 12 位分辨率，采样值范围为 `0~4095`。
- 该值表示相对光照强弱，不是标准 lux 值。
- 通过 LoRa 发送光照 ADC 原始值，接收端解析后在 OLED 显示。

光照 LoRa 数据帧格式：

```text
0xAA  0x02  data_H  data_L  check  0x55
```

其中 `0x02` 表示光照数据，`check = type ^ data_H ^ data_L`。

角色切换位置：

```c
// 实验3/Light/BSP/task.h
#define APP_ROLE_TX_SENSOR 0
```

- `1`：光照发送端
- `0`：光照接收端

相关流程图：

```text
diagram/实验3/lora_light_program_flowchart.drawio
diagram/实验3/lora_temperature_program_flowchart-LoRa 光照发送程序流程图.drawio.png
diagram/实验3/lora_temperature_program_flowchart-LoRa 光照接收程序流程图.drawio.png
```

## 实验4：温度与光照双向通信

工程路径：

```text
实验4/Light and Temperature
```

实验内容：

- 第一块板采集温度，并发送给第二块板。
- 第二块板采集光照，并发送给第一块板。
- 两块板都同时具备 LoRa 接收和发送能力。
- 温度节点显示本地温度和远端光照。
- 光照节点显示本地光照和远端温度。

统一数据帧格式：

```text
帧头  类型  数据高字节  数据低字节  校验  帧尾
0xAA  type  data_H      data_L      check 0x55
```

数据类型：

```text
0x01：温度数据
0x02：光照数据
```

节点角色切换位置：

```c
// 实验4/Light and Temperature/BSP/task.h
#define APP_NODE_ROLE APP_NODE_TEMP
```

- `APP_NODE_TEMP`：温度节点，采集温度并接收光照
- `APP_NODE_LIGHT`：光照节点，采集光照并接收温度

OLED 显示示例：

```text
温度节点：
TX T:25.3C
RX L:1234

光照节点：
TX L:1234
RX T:25.3C
```

相关流程图：

```text
diagram/实验4/lora_dual_node_program_flowchart.drawio
diagram/实验4/lora_temperature_program_flowchart-2.1 温度节点程序流程图.drawio.png
diagram/实验4/lora_temperature_program_flowchart-2.2 光照节点程序流程图.drawio.png
```

## 实验报告与流程图

根目录包含两份 Word 文档：

```text
窄带实验报告模板.doc    # 报告模板
窄带实验报告1.doc       # 已完成的实验报告
```

`diagram/` 目录保存了流程图资料，其中 `.drawio` 文件可继续用 draw.io 编辑，`.png` 文件可直接插入实验报告或 README。

## 编译与下载

1. 打开对应实验目录下的 Keil 工程：

```text
MDK-ARM/Demo9_1SPI.uvprojx
```

2. 根据需要修改节点角色宏。

3. 在 Keil 中执行 `Rebuild`。

4. 连接开发板，通过 DAP-Link 下载程序。

5. 复位开发板，观察 OLED 显示和 LoRa 通信结果。

## 主要代码文件

```text
BSP/task.c     # 实验主逻辑：采集、显示、收发、协议封装
BSP/task.h     # 任务声明、周期配置、节点角色宏
BSP/lora.c     # LoRa 初始化、发送、接收、射频开关控制
BSP/oled.c     # OLED 显示驱动
Core/Src/main.c # 系统初始化和主循环
Core/Src/adc.c  # ADC 初始化
Core/Src/i2c.c  # I2C 初始化
```

## 注意事项

- Keil 生成的中间文件和编译产物较多，上传 GitHub 时建议通过 `.gitignore` 排除 `*.o`、`*.d`、`*.axf`、`*.hex`、`*.map` 等文件。
- 如果修改了节点角色宏，需要重新 `Rebuild` 后再下载。
- 光照采集值是 ADC 原始值，范围约为 `0~4095`。
- 两块板进行 LoRa 通信时，必须使用相同的 LoRa 参数和相同的数据帧格式。
