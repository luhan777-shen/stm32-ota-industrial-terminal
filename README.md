# 基于 STM32 + FreeRTOS 的 OTA 工业智能终端

> 一款运行在 **STM32F103C8T6** 上的多协议工业网关，集成 **Modbus RTU / CANopen / MQTT** 三种现场总线与物联网协议，并通过 **Cat.1 4G 模块**接入 ThingsBoard 物联网平台，实现 **远程固件升级（OTA）**。
> 在 64KB Flash / 20KB RAM 的极限资源下，自建 **Bootloader + 外部 W25Q32 固件仓库** 的双分区方案，实现端到端、断电安全的可靠升级。

---

## 📋 目录

- [项目简介](#-项目简介)
- [功能特性](#-功能特性)
- [系统架构](#-系统架构)
- [内存分区设计](#-内存分区设计)
- [OTA 升级全流程](#-ota-升级全流程)
- [任务模型](#-任务模型)
- [硬件资源](#-硬件资源)
- [目录结构](#-目录结构)
- [关键技术点](#-关键技术点)
- [编译与烧录](#-编译与烧录)
- [使用说明](#-使用说明)
- [已知限制](#-已知限制)

---

## 🎯 项目简介

工业现场的传感器/PLC 设备往往部署在难以到达的位置，**一旦固件有 bug，派人去现场刷机成本极高**。本项目的目标是给一台低成本的工业网关插上「远程升级」的能力。

核心难点在于资源约束：

| 约束 | 数值 | 带来的问题 |
|---|---|---|
| 内部 Flash | **64 KB** | 装不下「旧固件 + 新固件」两份，无法双备份升级 |
| 内部 RAM | **20 KB** | 任务栈 + 两套堆 + 全局缓冲，每一字节都要精算 |
| 无操作系统级保护 | — | 升级过程断电必须能自恢复，不能变砖 |

**解决方案**：外挂一颗 4MB 的 SPI NOR Flash（W25Q32）作为**固件仓库**，内部 Flash 划分为 8KB Bootloader + 56KB 应用程序。升级时先把新固件下载到外部仓库并校验，**校验通过后写「升级标志」并复位**；Bootloader 上电检测到标志，才执行「擦除旧固件 → 搬运新固件 → 清标志 → 跳转」的动作。

这样，**唯一会擦写内部 Flash 的代码是 Bootloader 本身，而它永远不会被升级覆盖**——从根本上杜绝了刷砖。

> ✅ 已通过 **断电安全测试**：下载中断电 → 重新下载即可；搬运中断电 → 重新搬运即可。

---

## ✨ 功能特性

### 通信协议
- **Modbus RTU 从站**：基于 FreeModbus，运行于 RS485（USART2 + 方向控制引脚）
- **CANopen 从站**：基于 CanFestival，支持 NMT / SDO / PDO / 心跳 / EMCY
- **MQTT 客户端**：通过 ML307 Cat.1 4G 模块接入 ThingsBoard 平台，支持遥测上报与 RPC 控制

### 数据采集
- **AHT20**：温湿度（I2C）
- **INA226**：电压 / 电流 / 功率（I2C）
- **ADC1**：模拟量采集
- **OLED**：0.96" 显示屏，实时显示采集数据与 OTA 进度

### 远程升级（本项目核心）
- ThingsBoard 平台上传固件 → 自动推送 `attributes` → 设备自动下载
- 分块（256 字节）下载 + **CRC32 增量校验**
- 外部仓库暂存 + 内部 Flash 搬运的双分区方案
- **断电安全**：任意时刻断电均可自恢复
- **版本签名**：OLED 显示版本号 + 开机 LED 闪灯次数，用于肉眼区分新旧固件

### 远程控制
- 通过 ThingsBoard RPC 控制 **LED1 / LED2 / 蜂鸣器 / 继电器**

---

## 🏗 系统架构

```
┌───────────────────────────────────────────────────────────────────────┐
│                        ThingsBoard 物联网平台                          │
│              (RPC 指令 / 遥测数据 / OTA 固件包)                         │
└───────────────────────────────┬───────────────────────────────────────┘
                                │ MQTT over 4G
┌───────────────────────────────▼───────────────────────────────────────┐
│  ML307 Cat.1 4G 模组                              USART3 (DMA + IDLE)  │
└───────────────────────────────┬───────────────────────────────────────┘
                                │
        ┌───────────────────────▼────────────────────────┐
        │              STM32F103C8T6 @ 72MHz             │
        │  ┌──────────────────────────────────────────┐  │
        │  │            FreeRTOS  (CMSIS-RTOS)         │  │
        │  ├──────────────────────────────────────────┤  │
        │  │ ModbusTask │ MQTTTask │ CANopenTask │     │  │
        │  │   256 字   │  256 字  │   256 字    │Downlink│ │
        │  │            │          │             │ 768 字 │ │
        │  └─────┬──────┴────┬─────┴──────┬──────┴───┬───┘  │
        │        │           │            │          │      │
        │  ┌─────▼───────────▼────────────▼──────────▼───┐ │
        │  │        REG_HOLD_BUF[10]  共享寄存器中台      │ │
        │  │        （三协议共用同一份数据 + 位域控制）    │ │
        │  └─────┬───────────┬────────────┬──────────────┘ │
        └────────┼───────────┼────────────┼────────────────┘
                 │           │            │
        ┌────────▼──┐  ┌─────▼────┐  ┌────▼──────────────────┐
        │ RS485     │  │ CAN 总线  │  │ I2C1 / ADC / SPI1     │
        │ USART2    │  │ PA11/PA12│  │ 传感器 + OLED + W25Q32 │
        │ Modbus RTU│  │ CANopen  │  │                       │
        └───────────┘  └──────────┘  └───────────────────────┘
```

**设计要点**：三种协议共享同一个 `REG_HOLD_BUF[10]` 寄存器中台。例如上位机通过 Modbus 改地址 0 的值、通过 CANopen SDO 改同一个对象、通过 MQTT RPC 控制灯——**最终都落到同一份内存**，保证三个通道看到的状态永远一致。

---

## 💾 内存分区设计

### 内部 Flash（64 KB）

```
0x08000000 ┌─────────────────────────┐
           │  Bootloader  (8 KB)      │  ← 永不升级；只做「搬运 + 跳转」
           │  寄存器级实现，无 HAL     │     上电若检测到升级标志才动作
0x08002000 ├─────────────────────────┤
           │  应用程序 APP (56 KB)    │  ← 日常业务逻辑；OTA 升级时被整块替换
           │  FreeRTOS + 三协议 + ... │     向量表经 SCB->VTOR 重定位到此
0x0800FFFF └─────────────────────────┘
```

### 外部 W25Q32（4 MB）

```
0x000000   ┌─────────────────────────┐
           │  固件仓库 (64 KB)        │  ← OTA 下载暂存区，边收边写
0x010000   ├─────────────────────────┤
           │  空闲 (~3.9 MB)          │  ← 可扩展为多版本/日志/配置区
0x3FF000   ├─────────────────────────┤
           │  升级状态扇区 (4 KB)     │  ← 唯一的「升级意图」标记
0x3FFFFF   └─────────────────────────┘
```

**状态扇区结构（8 字节）**：

| 偏移 | 长度 | 含义 |
|---|---|---|
| `[0..3]` | 4 B | 升级标志：`0xAAAAAAAA` = 有待搬运的新固件 |
| `[4..7]` | 4 B | 固件长度（小端序） |

> **为什么把状态放在外部 Flash 而不放内部？**
> 因为内部 Flash 的擦写会中断代码执行（擦到自己的代码就跑飞了）。而外部 Flash 通过 SPI 访问，擦写它不影响 CPU 取指，可以随时擦随时写。

---

## 🔄 OTA 升级全流程

```
 ①  平台上传 app.bin → 分配给设备 → 平台自动推送 attributes
        fw_title / fw_version / fw_size / fw_checksum
        （checksum = CRC32 后按字节反转）

 ②  设备 DownlinkTask 解析 attributes → 置位 MQTT_OTA_FLAG

 ③  MQTTTask 检测到标志 → 擦除 W25Q 固件仓库（16 个 4K 扇区）
         → 逐块请求 v2/fw/request/{rid}/chunk/{n}

 ④  平台回 v2/fw/response/{rid}/chunk/{n}，256 字节/块
         → 边收边写 W25Q 仓库；同时增量计算 CRC32
         → OLED 实时显示下载进度百分比

 ⑤  收完后比对本地 CRC32 与平台下发的 checksum
         ├─ 不一致 → 上报 fw_state = FAILED，清理标志，等待重试
         └─ 一致   → ⑤'

 ⑤' 写升级状态扇区：[0..3] = 0xAAAAAAAA，[4..7] = 固件长度
         → 上报 fw_state = UPDATED
         → HAL_NVIC_SystemReset()    ← 主动复位

 ⑥  ┌──── 复位后，CPU 从 0x08000000 起跑，进入 Bootloader ────┐
     │  读 W25Q 状态扇区                                          │
     │  标志 != 0xAAAAAAAA → 直接跳转 APP（正常启动路径）         │
     │  标志 == 0xAAAAAAAA 且长度合法 →                           │
     │      擦除内部 APP 区（按长度擦）                            │
     │      SPI 读取 W25Q 仓库 → 半字编程写入内部 Flash            │
     │      ⚠ 搬运完成「之后」才清除标志 ← 断电安全的关键          │
     │      SCB->VTOR = 0x08002000                                │
     │      取出 APP 向量的 SP/PC，__set_MSP + 跳转                │
     └────────────────────────────────────────────────────────────┘

 ⑦  新固件自举 → OLED 显示新版本号 + LED1 闪 N 下（版本签名）

 ⑧  断电安全：
       下载阶段断电  → 状态扇区还是旧的/空的，重新走 ①~⑤
       搬运阶段断电  → 升级标志仍在 → 下次上电重新搬运（幂等）
       清标志后断电  → 正常启动新固件
```

---

## 🧵 任务模型

| 任务 | 栈大小 | 优先级 | 职责 |
|---|---|---|---|
| `ModbusTask` | 256 字 (1 KB) | Normal | FreeModbus 轮询 + 寄存器解析 |
| `MQTTTask` | 256 字 (1 KB) | Normal | 连接服务器、周期上报遥测、**执行 OTA 下载状态机** |
| `CANopenTask` | 256 字 (1 KB) | Normal | CanFestival 初始化，事件由 TIM2 中断驱动 |
| `DownlinkTask` | **768 字 (3 KB)** | Normal | **解析 ThingsBoard 下行**（RPC / attributes / OTA 固件块） |

**为什么 DownlinkTask 的栈要大 3 倍？**

因为它要跑 `cJSON_Parse` 解析 JSON，而 `printf` 的 `%d`/`%s` 格式化（cJSON 内部大量使用）**吃栈极深**。实测 128 字栈会直接溢出崩溃，256 字仍不稳定，最终给到 768 字才通过 `uxTaskGetStackHighWaterMark()` 实测验证。

### 内存分配总账

| 项目 | 大小 | 位置 | 用途 |
|---|---|---|---|
| FreeRTOS 堆 | 7 KB | `configTOTAL_HEAP_SIZE` | 4 个任务的栈（heap_4 方案） |
| MicroLIB 堆 | 3 KB | 启动文件 `Heap_Size = 0xC00` | **cJSON 的 `malloc`** |
| 主栈 | 1 KB | 启动文件 `Stack_Size = 0x400` | `main()` 与中断上下文 |

> ⚠️ **这里有个坑**：cJSON 内部用的是 **C 标准库的 `malloc`**，也就是 MicroLIB 堆，而**不是** FreeRTOS 堆。MicroLIB 默认堆只有 512 字节，解析稍大的 attributes JSON（需要约 1KB）会**静默失败**——`cJSON_Parse` 返回 NULL 但不报错。必须把 `Heap_Size` 调到 3KB 才够。

---

## 🔌 硬件资源

**主控**：STM32F103C8T6（LQFP48，Cortex-M3 @ 72MHz，64KB Flash / 20KB RAM）

| 外设 | 引脚 | 用途 |
|---|---|---|
| **SPI1** | PA4(CS) / PA5(SCK) / PA6(MISO) / PA7(MOSI) | W25Q32 外部 Flash（9 MHz，Mode 0） |
| **USART1** | PA9(TX) / PA10(RX) | 调试串口（`printf` 输出 + 数据转发） |
| **USART2** | PA2(TX) / PA3(RX) | Modbus RTU（RS485，PB2 为收发方向控制） |
| **USART3** | PB10(TX) / PB11(RX) | ML307 Cat.1 4G 模组（DMA + 空闲中断接收） |
| **CAN** | PA11(RX) / PA12(TX) | CANopen 总线 |
| **I2C1** | PB6(SCL) / PB7(SDA) | OLED + AHT20 + INA226 |
| **TIM1** | — | HAL 时基（1ms tick） |
| **TIM2** | — | CanFestival 定时器 |
| **TIM4** | — | USART3 帧间隔检测（10ms 超时判定一帧结束） |
| **ADC1** | — | 模拟量采集 |

| GPIO | 引脚 | 用途 |
|---|---|---|
| LED1 | PB9 | 状态指示 / 版本签名 |
| LED2 | PB8 | 状态指示 |
| BEEP | PC13 | 蜂鸣器 |
| RELAY | PB1 | 继电器 |
| RS485_WR | PB2 | RS485 收发方向 |
| KEY_OK/UP/DOWN/LEFT/RIGHT/ESC | PB12/PB13/PB14/PB5/PB15/PA8 | 按键 |

**外部 Flash**：W25Q32JVSSIQ（4MB SPI NOR，JEDEC ID = `EF 40 16`）

---

## 📁 目录结构

```
07-canopen-freertos/
├── Core/                          # ★ 项目核心代码
│   ├── Inc/
│   │   ├── main.h                 # 引脚定义与全局声明
│   │   ├── mqtt.h                 # ★ ThingsBoard 主题、token、OTA 信息结构体
│   │   ├── w25q32.h               # ★ W25Q32 指令集与 OTA 分区地址
│   │   ├── wifi4g.h               # ★ 4G 模组通信与下行投递变量
│   │   ├── fifo.h                 # 接收环形队列
│   │   ├── FreeRTOSConfig.h       # ★ FreeRTOS 裁剪配置（堆 7KB）
│   │   └── ...
│   └── Src/
│       ├── boot.c                 # ★ OTA Bootloader（8KB，寄存器级裸奔）
│       ├── main.c                 # ★ 入口、VTOR 重定位、版本签名
│       ├── freertos.c             # ★ 4 个任务的定义与创建
│       ├── mqtt.c                 # ★ ThingsBoard 协议 + OTA 下载状态机
│       ├── wifi4g.c               # ★ 4G 模组 AT 指令 + 中断→任务的投递链
│       ├── spi.c                  # ★ SPI1 驱动（W25Q32 专用）
│       ├── w25q32.c               # ★ W25Q32 驱动（读/页写/扇区擦除）
│       ├── crc32.c                # ★ OTA 固件增量校验
│       ├── stm32f1xx_it.c         # 中断向量与空闲中断处理
│       ├── usart.c                # 三路串口 + DMA 配置
│       └── ...
├── MDK-ARM/
│   ├── 07-canopen-freertos.uvprojx    # Keil 工程文件
│   ├── 07-canopen-freertos.sct        # ★ 分散加载文件（OTA 双分区核心）
│   ├── startup_stm32f103xb.s          # ★ 启动文件（含 BOOT 引导段）
│   └── 07-canopen-freertos/           # 构建输出（.gitignore 已排除）
├── Canfestival/                   # CANopen 协议栈
├── FreeModbus/                    # Modbus RTU 从站协议栈
├── Middlewares/                   # FreeRTOS 内核
├── Drivers/                       # STM32 HAL 库 + CMSIS
└── OD/                            # CANopen 对象字典
```

★ = 本项目中自己编写 / 深度修改的文件

---

## 🔧 关键技术点

### 1. 分散加载：把启动文件「劈成两半」

`startup_stm32f103xb.s` 里有**两个 `AREA`**：`BOOT` 和 `RESET`。通过分散加载文件把它们送到两个不同的地址：

```
MDK-ARM/07-canopen-freertos.sct

LR_BOOT  0x08000000 0x2000  {          ; 8KB Bootloader 区
  ER_BOOT 0x08000000 0x2000 {
    startup_stm32f103xb.o (BOOT, +First)   ; ← 只取 BOOT 段（2 个字）
    boot.o (+RO)                           ; ← Bootloader 代码
  }
}
LR_IROM1 0x08002000 0xE000  {          ; 56KB APP 区
  ER_IROM1 0x08002000 0xE000 {
    *.o (RESET, +First)                    ; ← 真正的向量表放这
    .ANY (+RO +XO)
  }
  RW_IRAM1 0x20000000 0x5000 { .ANY (+RW +ZI) }
}
```

`BOOT` 段在 `0x08000000` 处只放两个 32 位字——**这是 Cortex-M 的硬件要求**：复位时 CPU 从向量表第 1 个字取 MSP、第 2 个字取 PC。所以第二个字被设成 `__BootReset`，它立刻 `BX` 到 `boot_main()`。

> 💡 **踩坑记录**：Keil 的 `fromelf --bin` 遇到**多加载区**会报错 `Q0147E`，生成的 `.bin` 是坏的（一直是旧版本）。所以本项目的 app 镜像改用 `pyelftools` 从 `.axf` 中直接提取 ELF 段（跳过前 8KB boot 区）。**调试时务必以 `.axf` 为准，不要相信 `.bin`。**

### 2. Bootloader 为什么要「裸奔」

`boot.c` 完全不用 HAL / FreeRTOS / 标准库，全部直接操作寄存器：

```c
SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE | (2UL << 3);
```

原因：
- 运行在 **HSI 8MHz**（内部 RC 振荡器），不依赖外部晶振——**晶振起振可能失败，而 Bootloader 必须永远能跑起来**
- 未调用 `HAL_Init()`，中断全程关闭，SysTick 未配置
- **`__main` 的分散加载过程根本没执行过**，所以 RW/ZI 段是未初始化的垃圾值

> ⚠️ **因此 `boot.c` 里绝对不能用带初值的全局/静态变量**——它们会被分配到 `RW_IRAM1`（位于 APP 区），而复制初值的代码没跑过，读出来就是随机值。代码里全程只用局部变量，`uart_hex8()` 里的表也是 `const`（属于 RO，会被 `LR_BOOT` 正确加载）。

### 3. 内部 Flash 只能「半字编程」

STM32F1 的 Flash **不支持 8 位写入**，必须 16 位对齐写入。且写之前必须先擦除（Flash 只能把 1 变成 0，擦除才能全变回 1）：

```c
static void flash_program_half(uint32_t addr, uint16_t h)
{
    FLASH->CR |= FLASH_CR_PG;
    *(volatile uint16_t *)addr = h;      // ← 必须 uint16_t* 转换 + volatile
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_PG;
}
```

- `(uint16_t *)` 强制转换：让编译器生成 16 位的 `STRH` 指令，否则 Flash 控制器会忽略这次写入
- `volatile`：禁止编译器优化掉这条「写了没人读」的语句

Bootloader 搬运时把外部 Flash 的字节流两两拼成半字：

```c
uint16_t h = (uint16_t)buf[i] | ((uint16_t)buf[i + 1] << 8);
```

### 4. ⭐ 断电安全：把「完成标记」放在最后

```c
/* 搬运完成 -> 才清除升级标志 */
for (...) { /* 搬运 56KB */ }
w25q_sector_erase(W25Q_STATE_SECTOR);     // ← 这一步必须是最后
```

推演一下「如果反过来先清标志再搬运」会发生什么：

| 断电时机 | 先清标志（❌ 错误） | 后清标志（✅ 本项目） |
|---|---|---|
| 搬运到一半 | 标志没了 → 下次直接跳进**半个固件** → **变砖** | 标志还在 → 重新搬运 → 恢复正常 |
| 搬运途中任意时刻 | 同上，必砖 | 幂等重试，结果永远正确 |

**核心思想：让危险操作可以安全地重复执行（幂等），并把「标记完成」作为最后一步。**

这就是「标志位」在此处的真正含义——它不是「要不要升级」，而是**「上次搬运还没干完」**。

### 5. SPI1 的 DMA 被迫让路给 USART3

```c
/* spi.c 注释 */
/* 注意：SPI1_TX 的 DMA1_Channel3 与 USART3_RX 冲突，
   所以本驱动只用轮询模式，不用 DMA。 */
```

STM32F1 的 DMA1 通道与外设请求是**硬件固定映射**的：

| DMA 通道 | 可选外设请求 |
|---|---|
| DMA1_Channel2 | SPI1_RX、USART3_TX |
| **DMA1_Channel3** | **SPI1_TX、USART3_RX** |

4G 模组接在 USART3 上，数据量大且突发，**必须用 DMA 接收**（代码里确实是 `hdma_usart3_rx.Instance = DMA1_Channel3`）。所以 SPI1 只能放弃 DMA，改用轮询。

**这不是软件能力问题，是硬件连线表不给你选**——典型的嵌入式资源冲突案例。

### 6. W25Q32 驱动的三个铁律

**① 每次写/擦除前必须 Write Enable**

```c
W25Q_WriteEnable();        // 0x06 命令
W25Q_CS_LOW();
...
```

芯片内部的 WEL 位在**每次写/擦完成后会被硬件自动清零**，所以每次都要重新使能，不能「开一次用到底」。这是防止程序跑飞误擦 Flash 的保险设计。

**② 每次写/擦后必须等 BUSY 清零**

4KB 扇区擦除要 **45ms**，全片擦除要 **20~40 秒**。期间芯片忙，新命令会被**静默忽略**——不报错，只是数据悄悄不对，这类 bug 极难排查。

**③ 页写不能跨 256 字节页边界**

W25Q 内部按 256 字节一页写入。如果一次写入跨越了页边界，**多余的字节会绕回本页开头覆盖**（不是写到下一页）。本项目通过「整条链路都是 256 字节对齐」从设计上规避了这个问题。

### 7. 二进制固件的传输链路必须「二进制安全」

固件块里含 `0x00` 字节，而链路上到处都是「文本处理」的陷阱：

| 环节 | 陷阱 | 解决 |
|---|---|---|
| 中断 → 任务投递 | `strcpy`/`strlen` 遇 `0x00` 截断 | 改用 `memcpy` + **显式长度变量** |
| 环形队列长度 | `uint8_t` 存不下 256+ | 提升为 `uint16_t` |
| 块消息解析 | 主题后还有长度字段 `,256,` | 解析时**跳过长度字段** |
| 接收缓冲 | 512 字节 DMA 缓冲不够放整条消息 | `NSize` / `DMA_BUF_SIZE` 统一放大到 512 |

### 8. 4G 模组裸命令载荷不能含逗号

ML307 的 `MQPUB,1,<topic>,<data>` 是**按逗号切分字段**的。如果 `<data>` 里含逗号（比如一次发 7 个遥测键值对 `{"TP":25,"RH":60,...}`），字段会被切错。

解法：**遥测改为逐键分条发布**，每周期发 7 条，每条只含一个键；而且**必须间隔 300ms 慢发**——连发 7 条会打崩 MQTT 会话（触发 DISCONNECT 循环）。

### 9. ThingsBoard attributes 是「一批多条消息」

平台推送属性时，一次会发**两条** MQTT 消息（`deleted` 清空 + 新属性）。解析器如果只处理第一条，真正的 OTA 信息就被吞掉了。所以 `CAT1_Parse_ATComander()` 用了**循环扫描**，每次找三条下行链中最靠前的那个标记，处理完继续扫，直到扫不到为止。

同一 token 同时只允许一个 MQTT 会话——**调试工具必须使用另一个设备的 token**，否则会和设备互踢。

---

## 🛠 编译与烧录

### 环境要求

- **Keil MDK-ARM v5**（本工程使用 ARM Compiler 5，`<uAC6>0</uAC6>`）
- **STM32F1xx Device Family Pack**
- 已勾选 **Use MicroLIB**（`<useUlib>1</useUlib>`）——cJSON 依赖
- 烧录工具：ST-Link / J-Link

### 编译

```
打开 MDK-ARM/07-canopen-freertos.uvprojx
→ Project → Build Target (F7)
```

### 烧录（关键！）

**Bootloader 和 APP 是两个独立的镜像，必须分别烧录：**

| 镜像 | 烧录地址 | 来源 |
|---|---|---|
| **Bootloader** (8KB) | `0x08000000` | Keil 直接下载即可（首次必须烧） |
| **APP** (56KB) | `0x08002000` | Keil 直接下载即可 |

> 日常调试直接用 Keil 的 Download 即可，不需要关心地址——**分散加载文件已经帮你把两个区放好了**。

### 生成 OTA 升级用的固件包

⚠️ **不要使用 Keil 生成的 `.bin`**（多加载区会导致 fromelf 报错 `Q0147E`，产物是坏的）。正确做法是从 `.axf` 提取：

```python
from elftools.elf.elffile import ELFFile

with open('MDK-ARM/07-canopen-freertos/07-canopen-freertos.axf', 'rb') as f:
    elf = ELFFile(f)
    data = elf.get_section_by_name('.text').data()   # 或按地址筛 0x08002000 起 56KB
    open('app.bin', 'wb').write(data)
```

提取出的 `app.bin` 直接上传到 ThingsBoard 的 OTA 更新页面即可。

---

## 📖 使用说明

### 1. 平台配置

在 `Core/Inc/mqtt.h` 中配置你的 ThingsBoard 服务器与设备令牌：

```c
#define THINGSBOARD_HOST   "your-server.com"
#define THINGSBOARD_PORT   "1883"
#define THINGSBOARD_TOKEN  "YOUR_DEVICE_TOKEN"
```

> 🔒 本仓库中的 token 为**私有部署环境**使用。如果你 fork 本项目，请替换为你自己的设备令牌。

### 2. 触发 OTA 升级

1. 修改代码后**务必升级版本号**（见下），否则新旧固件无法区分
2. 编译 → 用脚本从 `.axf` 提取 `app.bin`
3. 平台 → **OTA 更新** → 新建包 → 上传 `app.bin` → **分配给设备**
4. 平台自动推送 attributes → 设备自动开始下载（OLED 显示进度条）
5. 下载完成 → 设备自动复位 → Bootloader 搬运（LED1 亮约 3 秒）→ 新固件启动

### 3. 版本签名（重要）

每次改完固件，**必须同步升级这三处版本号**，否则无法判断设备跑的是新版还是旧版：

| 位置 | 文件 | 当前值 |
|---|---|---|
| 串口打印 | `Core/Src/main.c` | `printf("OTA-FW v9\r\n")` |
| OLED 首行 | `Core/Src/main.c` | `OLED_ShowStr(0,0,"OTA-FW v9",2)` |
| 开机闪灯 | `Core/Src/main.c` | `for(int _k=0;_k<3;_k++)` LED1 闪 3 下 |

### 4. 远程控制（RPC）

平台 → 设备 → RPC，支持以下方法：

| RPC 方法 | 参数 | 作用 |
|---|---|---|
| `led1Set` | `1` / `0` | 控制 LED1 |
| `led2Set` | `1` / `0` | 控制 LED2 |
| `beepSet` | `1` / `0` | 控制蜂鸣器 |
| `relaySet` | `1` / `0` | 控制继电器 |
| `led1Status` 等 | — | 查询当前状态 |

---

## ⚠️ 已知限制

1. **传感器接触不良**：AHT20 / INA226 使用杜邦线连接，偶发接触不良导致读数为 0 或极值，属硬件问题
2. **串口字节交错**：`printf`（HAL 阻塞发送）与中断上下文的数据转发共用 USART1，偶有字节级交错，**不影响功能**，属显示层问题
3. **固件仓库仅 64KB**：够放单份固件，不支持多版本回滚。W25Q32 还有 ~3.9MB 空闲，可扩展
4. **OTA 升级期间 MQTTTask 阻塞**：下载状态机采用忙等（10 秒超时/块），期间该任务不处理其他事务；但其他任务（Modbus/CANopen）不受影响
5. **平台 OTA 包「删旧换新」后需重新分配设备**，否则推送的仍是旧包

---

## 📄 许可

本项目为个人学习与作品展示用途。使用的第三方组件遵循其各自许可：

- **FreeRTOS** — MIT License
- **FreeModbus** — BSD License
- **CanFestival** — LGPL v2.1
- **cJSON** — MIT License
- **STM32 HAL / CMSIS** — STMicroelectronics 许可
