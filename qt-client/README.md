# 网易云音乐 · Qt 桌面客户端

基于 Qt Quick/QML 的网易云音乐桌面客户端，页面布局和交互对照网页版（music.163.com/st/webplayer）：推荐、精选（歌单广场、排行榜、歌手、VIP）、播客、漫游、关注、歌单与歌手详情、分类搜索、带歌词和相似推荐的全屏播放页。默认浅色主题，可切换深色。与网页版的逐项对照见 `功能核对.md`。

## 启动

需要 Qt 6.9+（Quick、Quick Controls 2、Network、Multimedia 和 DBus）、CMake、Ninja 和 OpenSSL 开发库（Ubuntu：`libssl-dev`）。建议安装 Qt Image Formats 以解码 WebP 封面。在本目录执行：

```bash
qt-cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/record-room
```

可以在 Qt Creator 中打开 `CMakeLists.txt`。

客户端在进程内直接请求网易云音乐服务（`src/NeteaseApi.cpp`，请求加密在 `src/NeteaseCrypto.cpp`），不再需要 Node.js 或 `api-enhanced`。调试时仍可设置 `NCM_API_URL` 指向一个现成的 api-enhanced 服务。

- 单实例：重复启动只会唤起已打开的窗口。
- 系统媒体控制与桌面歌词：通过 MPRIS（`org.mpris.MediaPlayer2.RecordRoom`）发布播放状态，GNOME 顶栏媒体控件和键盘媒体键可以直接控制。当前歌曲的歌词以 LRC 文本放在 `xesam:asText` 里，GNOME 扩展 [Desktop Lyric](https://extensions.gnome.org/extension/4006/desktop-lyric/) 会直接显示；osdlyrics 等其他读 MPRIS 的歌词工具也能识别播放进度。
- 缓存：封面图存在 `~/.cache/RecordRoom/RecordRoom/images`（上限 512 MB），公开页面的接口数据按账号存在 `~/.cache/RecordRoom/RecordRoom/responses`（上限 64 MB）。启动时先显示上次的内容，再在后台刷新。
- 登录：扫码登录，会话保存在 `~/.config/RecordRoom/RecordRoom/session.json`（仅本人可读）。
- 反作弊口令：收藏 / 取消收藏歌单时，会像网页版一样实时生成网易易盾口令。做法是从网易 CDN 下载官方易盾 SDK，放进 Qt 自带的 JavaScript 引擎里运行（浏览器环境由 `assets/dun/browser-shim.js` 模拟，脚本只能访问网易易盾的域名）。生成失败时回退到固定口令。
