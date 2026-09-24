# M3508_PSI_Monitor

STM32H723VGTx 单电机控制固件：通过 FDCAN1 驱动一台达妙（DAMIAO）DM 电机做 MIT 模式正弦位置轨迹运动，用板载按键切换使能 / 失能。

代码基于达妙官方 `DM-H7 FDCAN` 裸机 demo（见 [USAGE.md](USAGE.md)），在其分层框架上做了应用层改动：控制对象从"上电直给固定目标"改为"实时正弦轨迹 + 按键使能切换"。

---

## 1. 硬件与开发环境

| 项目 | 值 |
| --- | --- |
| MCU | `STM32H723VGTx`（LQFP100） |
| 板卡 | CtrBoard（`MDK-ARM/DebugConfig/CtrBoard_STM32H723VGTx_1.0.2.dbgconf`） |
| 调试器 | J-Link（`MDK-ARM/JLinkSettings.ini`、根目录 `monitor.jdebug`） |
| HSE | 24 MHz 外部晶振 |
| 系统时钟 | SYSCLK 480 MHz / HCLK 240 MHz / APB1·APB2 120 MHz（VOS0 + Flash Latency 3） |
| 工程文件 | `M3508_PSI_Monitor.ioc`（STM32CubeMX） |
| Keil 工程 | `MDK-ARM/M3508_PSI_Monitor.uvprojx`，Target `J4310P-MIT` |
| 输出名 | `M3508_PSI_Monitor`（生成 `.axf` 与 `.hex`） |

> 工程名里的 `J4310P` 指达妙 J4310P 电机，`M3508` 是本次改造对应的目标机构/台架名；代码中不包含 DJI M3508 + C620 相关协议。

## 2. 外设与引脚

| 外设 | 引脚 | 配置 | 用途 |
| --- | --- | --- | --- |
| FDCAN1 | `PD0` / `PD1` | AF9，经典 CAN 1 Mbps | 与电机通信 |
| TIM3 | — | PSC=239、ARR=999 → **1 kHz** 更新中断 | 控制帧周期调度 |
| TIM4 | — | 同 TIM3 参数 | 已初始化，**当前未启动、未使用** |
| USART1 | `PA9` / `PA10` | 921600 8N1 | 已初始化，**当前代码未收发** |
| GPIO 输出 | `PC14` | 推挽输出，宏 `power(x)` | 板级电源 / 使能控制 |
| GPIO 输入 | `PA15` | `KEY_Pin`，上拉关闭、**低电平有效** | `KEY0_Instance` 按键 |

FDCAN1 初始化（`Core/Src/fdcan.c`）按 FDCAN 全功能帧配置（StdFiltersNbr=4、FIFO0 深度 10、TxFifoQueue 模式），**运行期实际链路格式由 `bsp_fdcan_set_baud()` 覆盖**为 `FDCAN_FRAME_CLASSIC`。

接收滤波器（`User/bsp_fdcan.c: can_filter_init`）：

- Filter 0：标准 ID、掩码模式、`FilterID1 = FilterID2 = 0x00` → 掩码全 0，**所有标准 ID 都进 FIFO0**；
- 全局滤波 `FDCAN_REJECT`：不匹配的标准/扩展 ID 与远程帧一律拒收；
- 中断：`RX_FIFO0_WATERMARK`（水位 1）+ TX 完成 / TX FIFO 空 / Bus-Off / 协议错误 / Error Passive / Warning。

## 3. 目录结构

```text
M3508_PSI_Monitor
├─ M3508_PSI_Monitor.ioc     # CubeMX 工程
├─ README.md                 # 本文件
├─ USAGE.md                  # 达妙官方 demo 说明（协议与移植指导，仍然适用）
├─ .gitignore                # 忽略编译中间产物，保留 .axf
├─ Core                      # CubeMX 生成的 HAL 外设与中断入口
│  ├─ Inc/  (main.h 含 power(x) 与 KEY_Pin 定义)
│  └─ Src/  (main.c 应用入口、fdcan.c、tim.c、usart.c、gpio.c、stm32h7xx_it.c)
├─ Drivers                   # STM32H7 HAL + CMSIS
├─ MDK-ARM                   # Keil 工程与构建产物（产物目录 M3508_PSI_Monitor/）
└─ User                      # 用户代码（分层见下）
```

用户代码分层：

| 文件 | 职责 |
| --- | --- |
| `User/bsp_fdcan.c/.h` | FDCAN 滤波、启动、收发、波特率与帧格式切换；`HAL_FDCAN_RxFifo0Callback` → `fdcan1_rx_callback()` |
| `User/dm_motor_drv.c/.h` | 电机协议层：使能/失能/清错、MIT·POS·SPD·PSI 控制帧、反馈解析、45 项寄存器读写保存 |
| `User/dm_motor_ctrl.c/.h` | 电机对象 `motor[]` 初始化、寄存器轮询流程、按反馈 ID 分发到各电机 |
| `User/keys.c/.h` | 按键扫描状态机（`Key_Scan`），另有未启用的 ADC 五向键版本 `KeyA_Scan` |
| `User/delay.c/.h` | 微秒级延时 |

## 4. 运行流程

`main()` （`Core/Src/main.c`）：

1. `MX_GPIO_Init` → `MX_FDCAN1_Init` → `MX_TIM3_Init` → `MX_USART1_UART_Init` → `MX_TIM4_Init`；
2. `power(1)` 拉高 `PC14`，延时 1 s；
3. `bsp_fdcan_set_baud(&hfdcan1, CAN_CLASS, CAN_BR_1M)`：经典 CAN、1 Mbps；
4. `bsp_can_init()`：配滤波器、`HAL_FDCAN_Start`、开中断；
5. `dm_motor_init()` 初始化 `motor[Motor1]` / `motor[Motor2]`；
6. 按 `crtl_mode` 覆盖 `Motor1` 的控制模式与速度给定（默认 `MIT_MODE`）：`mit_mode` + `vel_set = 0.0f`；若把 `crtl_mode` 改成 `POS_MODE`，则走 `pos_mode` + `vel_set = 2.2f`；
7. `dm_motor_enable(&hfdcan1, &motor[Motor1])` 发送使能帧；
8. 写 MIT 增益 `kp_set = 10.0f`、`kd_set = 0.08f`，延时 1 s；
9. `HAL_TIM_Base_Start_IT(&htim3)` 启动 1 kHz 调度。

主循环 `while(1)`：

```c
Key_Scan(&KEY0_Instance);                     /* 10 ms 周期扫描按键 */
motor[Motor1].ctrl.pos_set = 0.2f * sinf(2π * 1.0f * t);   /* t = HAL_GetTick()/1000 */
HAL_Delay(10);
```

`HAL_TIM_PeriodElapsedCallback`（TIM3，1 kHz）：

```c
if (KEY0_Instance.key_flag) {                 /* 按下并释放后置位 */
    KEY0_Instance.key_flag = 0;
    ctrl_enable = !ctrl_enable;
    if (ctrl_enable) dm_motor_enable(&hfdcan1, &motor[Motor1]);
    else             disable_motor_mode(&hfdcan1, motor[Motor1].id, MIT_MODE);
}
dm_motor_ctrl_send(&hfdcan1, &motor[Motor1]); /* 每 1 ms 发一帧 MIT 控制帧 */
```

接收路径：`FDCAN1_IT0_IRQHandler` → `HAL_FDCAN_IRQHandler` → `HAL_FDCAN_RxFifo0Callback` → `fdcan1_rx_callback()`（`User/dm_motor_ctrl.c`）→ 按标准 ID 分发：

```c
case 0x13: dm_motor_fbdata(&motor[Motor1], rx_data); break;
/* case 0x12: ... motor[Motor2] ... 已注释 */
```

## 5. 电机参数

`dm_motor_init()` 中的默认配置（`User/dm_motor_ctrl.c`）：

| 字段 | Motor1 | Motor2 |
| --- | --- | --- |
| 电机 CAN ID | `0x03` | `0x02` |
| 反馈 / Master ID | `0x13` | `0x12` |
| `ctrl.mode` | `pos_mode`（**被 `main.c` 覆盖为 `mit_mode`**） | `psi_mode` |
| `ctrl.vel_set` | `2.2f`（**被 `main.c` 覆盖：MIT 下为 `0.0f`**） | `1.0f` |
| `PMAX` / `VMAX` / `TMAX` | 12.56 / 50.0 / 5.0 | 12.5 / 50.0 / 10.0 |
| 状态 | **实际控制对象** | 仅初始化，收发均已注释 |

运行期 `Motor1` 的 MIT 帧实际载荷：

| 字段 | 来源 | 值 |
| --- | --- | --- |
| `pos` | `main.c` 主循环，0.2 rad 幅值、1 Hz | `0.2·sin(2πt)` |
| `vel` | `main.c` 初始化分支 | `0.0`（`crtl_mode` 为 `POS_MODE` 时为 `2.2`） |
| `kp` / `kd` | `main.c` | 10.0 / 0.08 |
| `tor` | `dm_motor_init()` | 0.0 |

反馈由 `dm_motor_fbdata()` 按 `PMAX`/`VMAX`/`TMAX` 反算，得到 `para.id`、`para.state`、`para.pos`、`para.vel`、`para.tor`、`para.Tmos`、`para.Tcoil`。

## 6. CAN 协议速查

控制帧 ID = `电机 ID + 模式偏移`：

| 模式 | 偏移 | 帧格式 |
| --- | ---: | --- |
| MIT | `0x000` | `[pos16][vel12\|kp12][kd12\|tor12]`（`PMAX`/`VMAX`/`TMAX`/`KP_MAX=500`/`KD_MAX=5` 线性映射） |
| POS | `0x100` | `[float pos][float vel]` |
| SPD | `0x200` | `[float vel]` |
| PSI | `0x300` | `[float pos][u16 vel×100][u16 cur×10000]` |

特殊帧（`User/dm_motor_drv.c`）：

| 动作 | ID | 数据 |
| --- | --- | --- |
| 使能 | `id + mode` | `FF FF FF FF FF FF FF FC` |
| 失能 | `id + mode` | `FF FF FF FF FF FF FF FD` |
| 清除错误 | `id + mode` | `FF FF FF FF FF FF FF FB` |
| 读寄存器 | `0x7FF` | `[id低4位][id高4位][0x33][rid]` |
| 写寄存器 | `0x7FF` | `[id低8位][id高3位][0x55][rid][d0][d1][d2][d3]` |
| 保存参数 | `0x7FF` | `[id低8位][id高3位][0xAA][0x01]` |

寄存器 `RID_*` 完整列表（45 项，含 UV/KT/ACC/DEC/PMAX/VMAX/TMAX/CAN 波特率等）见 `User/dm_motor_drv.h`；`read_all_motor_data()` 按 `read_flag = 1..45` 顺序轮询，收到 `RID_X_OUT` 后把 `read_flag` 清零。

## 7. 编译与烧录

```bash
# Keil MDK（uVision）打开工程，Target 选 J4310P-MIT，Build / Rebuild
MDK-ARM/M3508_PSI_Monitor.uvprojx
```

产物输出到 `MDK-ARM/M3508_PSI_Monitor/`：

| 文件 | 说明 | 是否入库 |
| --- | --- | --- |
| `M3508_PSI_Monitor.axf` | 调试用固件（含符号） | **是** |
| `M3508_PSI_Monitor.hex` | 烧录用固件 | 否（`.gitignore` 白名单只放行 `.axf`，需要请在 `.gitignore` 中加 `!` 例外） |
| `*.o` `*.d` `*.map` `*.lnp` `*.sct` `*.htm` `*.dep` | 编译中间产物 | 否 |

命令行构建可用 Keil 的 `UV4.exe`：

```bash
UV4.exe -j0 -b MDK-ARM/M3508_PSI_Monitor.uvprojx -t J4310P-MIT
```

## 8. 已知注意事项

1. **控制帧未被 `ctrl_enable` 门控**：TIM3 回调里的 `if (ctrl_enable)` 已注释，因此按下 `KEY0` 触发"失能"后，1 kHz 的 MIT 控制帧仍在持续发送（此时 `kp=10`、`kd=0.08` 依然生效），电机不会真正停止跟随。要真正停止需放开注释：把 `dm_motor_ctrl_send()` 包进 `if (ctrl_enable)`。
2. **模式覆盖只作用于 `Motor1`**：`main.c:151-160` 现在同时设置 `motor[Motor1].ctrl.mode` 与 `vel_set`，`dm_motor_init()` 中为 `Motor1` 写的 `pos_mode` / `vel_set = 2.2f` 只是初值、会被覆盖；`Motor2` 的 `psi_mode` 仍只在 `dm_motor_init()` 中设置，且其收发均未启用。
3. **`crtl_mode` 拼写**：源码中该变量名少一个 `o`（`crtl_mode`），已按原样保留。
4. **发送侧帧格式固定为 FD**：`fdcanx_send_data()` 无论链路模式都把 Tx header 填成 `FDFormat=FDCAN_FD_CAN`、`BitRateSwitch=ON`；实际链路格式由 `bsp_fdcan_set_baud()` 的 `FrameFormat` 决定（当前为经典 CAN）。
5. **源码注释为 GBK 编码**：`User/dm_motor_drv.c`、`dm_motor_drv.h`、`dm_motor_ctrl.c` 中的中文注释在 UTF-8 编辑器下会显示乱码。
6. **TIM4 / USART1 已初始化但未使用**：留着供后续扩展（例如 USART1 输出监控数据，项目名里的 "Monitor" 目前只有代码框架、没有上报实现）。
7. `read_all_motor_data()` / `receive_motor_data()` 的参数轮询流程保留完整，但 `main.c` 中的调用已注释，实际不会读取寄存器；`mst_id` 也**不参与**接收分发，分发完全由 `fdcan1_rx_callback()` 的 `switch (rec_id)` 决定。

## 9. 二次开发入口

| 想做的事 | 改哪里 |
| --- | --- |
| 改轨迹（幅值 / 频率 / 换模式） | `Core/Src/main.c` 顶部 `pos_amp`、`pos_freq`、`crtl_mode` |
| 调 MIT 增益 | `Core/Src/main.c` 中 `kp_set` / `kd_set` |
| 改 CAN ID、反馈 ID、映射范围 | `User/dm_motor_ctrl.c` 的 `dm_motor_init()` 与 `fdcan1_rx_callback()` |
| 新增第二台电机 | 参考 `USAGE.md` 第 2 节；注意 `Motor2` 的初始化和回调目前都是注释状态 |
| 改波特率 / 换成 CAN FD | `Core/Src/main.c` 的 `bsp_fdcan_set_baud(&hfdcan1, CAN_FD_BRS, CAN_BR_5M)` |

> 注意：**修改波特率前必须先确认电机侧的波特率设置**，两者不一致时总线无响应且会累积错误帧。demo 里 `write_motor_data(id, 35, CAN_BR_5M, 0, 0, 0)` + `save_motor_data(id, 10)` 的写寄存器序列（`main.c:162-170`）就是为此准备的，当前处于注释状态。
