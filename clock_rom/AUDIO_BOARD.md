# 整点提示音硬件

依据卖家 `S3天气预报电路图.pdf`，与已工作的显示接线一致。
原件SHA256：`840B5900D0BA32935F07A8D8FA5649377EF34DA617D5730C5B82056648DB7A76`。

| 信号 | GPIO |
|---|---|
| ES8311 I2C SDA / SCL | 40 / 39 |
| ES8311 I2C地址 | 0x18 |
| I2S MCLK / BCLK / WS | 38 / 16 / 7 |
| MCU输出→ES8311 DSDIN | 4 |
| NS4150B功放使能 | 8，高有效 |

播放使用16kHz、16位双声道同值PCM，880Hz正弦短音三声；PCM限幅、淡入淡出，停止后关闭功放。不驱动麦克风输入，不录音。0%静音；100%是软件受限幅值的上限，不代表功放满输出。

寄存器初始化参考Espressif esp-bsp的ES8311驱动（Apache-2.0）：
https://github.com/espressif/esp-bsp/blob/master/components/es8311/es8311.c

I2C/I2S失败时状态接口标记不可用；网页试听排队成功不等于已听到声音。原理图依据、编译成功与实际听感是不同验证范围，三声、音量和整点恢复效果需用户实机确认。
