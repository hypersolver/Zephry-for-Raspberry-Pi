# Project overview: Zyphry for Raspberry Pi and Zavira GUI

## 開發動機

RPiduino 想讓 Raspberry Pi 既能執行即時控制，也能以 HDMI 顯示裝置狀態與操作介面。第一步是建立可重現的 Zephyr microSD 開機及 UART 驗證流程。第二步是用 LVGL 建立可獨立更新的開機畫面、桌面與應用程式。長期目標是讓原生 Zephyr 應用與 Arduino Sketch 使用同一套 GUI 服務，並讓 XDOS 等平台透過各自的適配層重用 Zavira 的主題和元件。

## 系統架構

| 層級 | 職責 |
| --- | --- |
| Raspberry Pi 韌體 | 從 microSD 載入 Zephyr 核心與 ZAPP 開機資料。 |
| Zyphry for Raspberry Pi | 維護 Zephyr 板卡適配、顯示與 UART 驅動整合、開機封裝及實機驗證。 |
| 共用 LVGL、GUI 執行緒與 LLEXT | GUI 執行緒集中處理畫面操作。LLEXT 依宿主與模組的 ABI 載入 ZAPP。 |
| Zavira Desktop 與應用 | `desktop.zapp` 提供紫藍旋轉風車、版本與 uptime、Launcher。Launcher 可啟動 `lvgl-demo.zapp`。 |
| 應用程式 API（規劃） | 為原生應用提供共用介面，再加上供 `setup()`／`loop()` 使用的 Arduino C++ 包裝。 |

Pi 4B 原型以 `zapps.bin` 封裝兩個 ZAPP，由韌體在開機時載入。桌面與 Demo 的畫面配置在各自的模組，顯示渲染使用核心中的同一份 LVGL。COM3 提供心跳、`DEBUG`、`RESET`，並可用 `OPEN LVGL` 和 `DESKTOP` 切換畫面。[執行架構圖與 GitHub 架構圖](architecture.md)

## 開發流程

1. **維護基礎系統。** Raspberry Pi fork 的 `main` 跟隨 Zephyr 上游。透過版本固定的驗證分支建置各板卡的基礎包，檢查 microSD 冷開機、UART 心跳、`DEBUG` 與 `RESET`。
2. **獨立開發 Zavira。** 規劃以 `zavira-gui` 的 `main` 管理 Core、Theme、Desktop、Demo 和平台適配。使用短期功能分支與 PR，先在模擬環境驗證 UI 再合併。Zavira 自行標記版本。
3. **整合與驗收。** 在 `rpiduino/zavira-gui` 鎖定相容的 Zephyr、LVGL 與 Zavira 版本。先驗證 QEMU 開機、風車動畫、桌面、Demo 切換與 UART，再在 Pi 4B 核對 HDMI、冷開機及 RESET 後能否回到桌面。
4. **按用途發布。** Raspberry Pi fork 的 Release 提供基礎開機包與指定板卡的 GUI 整合包。每個包標註適用板卡、版本、驗證範圍與 SHA-256。Pi 3B 和 3B+ 共用基礎包，Pi 4B 使用獨立組態，Pi 5 完成實測後再納入。

## 未來應用

| 應用 | 第一個可驗證成果 |
| --- | --- |
| 嵌入式控制面板 | 在 Pi HDMI 顯示感測值、裝置狀態與告警，透過 UART 查看診斷資訊。 |
| Arduino 圖形應用 | 讓一支 Sketch 在 Zephyr 上利用 Zavira 元件更新畫面，再驗證原生模組載入、卸載與 GUI 資源清理。 |
| 設備診斷與展示 | 用獨立 ZAPP 提供裝置自檢、系統資訊和互動展示。 |
| 跨平台 GUI | 讓 PC 模擬器和 XDOS 使用獨立平台適配，重用風車、視覺主題與桌面元件。 |
| Raspberry Pi 5 | 先驗證基礎開機與 UART，再評估顯示與輸入整合。 |

目前基礎開機與 UART 心跳已在 Pi 3B、3B+ 和 Pi 4B 實測。Zavira 的動畫、桌面、Demo 切換、心跳與 `DEBUG` 已在 QEMU 驗證。Pi 4B Zavira 實機 HDMI／RESET、獨立 Zavira 倉庫、Arduino API、USB 輸入與 Pi 5 仍在規劃或驗證中。基礎版 GitHub Release 仍是草稿。
