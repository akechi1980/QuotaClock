# 第三方来源与发布注意

- `clock_rom/clock_rom.ino` 来源于 SmallDesktopDisplay / SDD V1.4，保留 Misaka、微车游的原作者署名与说明。当前资料中未找到覆盖整个V1.4工程的明确开源许可证；**公开再分发前需要确认授权，不能擅自把全工程标成MIT/Apache。**
- 原工程字库、天气图片/动画与其他资源同样需要核实再分发许可。
- `ui_glyphs.h`由本机Windows字体生成；公开发布前复核字体/生成字形授权，必要时改用可再分发字体重新生成。
- 配套库放在 `vendor/clock_rom/libraries/`，版本见 `ARCHIVE_INFO.json`；保留各库已有许可证/版权声明。缺失或不明确的许可需补查上游，不以打包操作代替授权。
- ES8311初始化参考Espressif esp-bsp（Apache-2.0）；说明与许可证在 `clock_rom/NOTICE.audio.txt`、`clock_rom/LICENSE.audio-Apache-2.0.txt`。

本目录仅完成开发资料整理与敏感数据排除，未替你作出第三方许可结论，也未执行Git初始化、推送或发布。
