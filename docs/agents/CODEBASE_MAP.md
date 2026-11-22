# Codebase map            Mapped at: fd3fa03 (2026-11-21)

Read this before planning a change. Every `path:line` was opened during mapping; re-check lines
before relying on them if `git log fd3fa03..HEAD -- <area>` shows changes. Areas marked
"not read" were outside the survey. Structural graph: `graphify-out/` (see *Graph* below).

## Build & run
- Toolchain: ESP-IDF v6.1 via EIM; always go through `./tools/idf.ps1 <args>` (picks the newest
  6.1.x from `C:\Espressif\tools\eim_idf.json`, override `$env:GW_EIM_JSON` / `$env:GW_IDF_PATH`
  which must match an EIM-listed path — `tools/idf.ps1:13,22-30,44-46`). **Build verified
  2026-11-21** (`./tools/idf.ps1 build` exit 0). ⚠ The app is 0x4f52d0 B in a 0x500000 B OTA
  slot — **~43.3 KB (1 %) free**; the build already prints "smallest app partition is nearly
  full". Pre-existing warnings: `main/display/font/ttf_convert.cc:125`, `main/display/a2ui/a2ui_image.cc:490`.
- Project version: `PROJECT_VER` in `CMakeLists.txt:5`. `main/assets/fonts/font.fontpack` is
  flashed to `font_data` by `esptool_py_flash_to_partition` (`CMakeLists.txt:12-19`).
- Board variant (Kconfig `BOARD_TYPE`, `main/Kconfig.projbuild:3-15`): `METALIO_E_INK4_PLUS`
  (current `sdkconfig`) or `..._GLOBAL` (same sources; sets `en-US` default and forces Wi-Fi —
  `main/CMakeLists.txt:366-368`, `main/boards/common/board/dual_network_board.cc:32,63`).
- Host tests (reader text rules): `python tools/tests/test_text_break.py` → `text_break: PASS`
  (g++ ≥ C++17 on PATH or `CXX`; tests the header-only rules in `main/reader/vertical_text.h`
  and `book_screen/reader/book_margin_rules.h`). Verified 2026-11-21 with MinGW g++ 13.1.
- Host tests (reader dictionary): `python tools/tests/test_reader_dict.py` → `reader_dict: PASS`
  (word hit-test, lookup rules, StarDict reader; LVGL/FatFS replaced by `tools/tests/stubs/`) and
  `python tools/tests/test_prepare_stardict.py` → `prepare_stardict: PASS`.
- New `.cc` files under `main/` are found by `file(GLOB ...)` in `main/CMakeLists.txt` only at
  configure time: run `./tools/idf.ps1 reconfigure` before the next build, or they are silently
  not compiled (and only fail at link once something calls them).
- App slot headroom (2026-11-22): app 5,240,896 B in a 5 MiB OTA slot (~2 KB free) — any new
  feature needs space found first.
- Host tests (lifecycle): `python tools/tests/test_lifecycle_fixes.py`. Compiles C
  harnesses with MSVC; paths hard-coded to VS2022 BuildTools 14.44.35207 / SDK 10.0.26100.0
  (`tools/tests/test_lifecycle_fixes.py:12-13`). Covers Wi-Fi scan init cleanup and display feeder
  teardown via fault injection; no pytest. **Not run in this session.**
- Release image: `tools/firmware/merge_firmware.sh` (bash; needs python3, esptool.py, idf.py).
  ⚠ Defaults `--chip esp32s3` unless `ESPTOOL_CHIP` is set (`tools/firmware/merge_firmware.sh:244`)
  — set `ESPTOOL_CHIP=esp32s31` (or fix the default). Its IDF search paths are Linux v5.5.4-era
  (`:69-75`). Unverified on Windows.
- Partitions (`partitions/v1/16m.csv`): nvs 16K, otadata, phy, `model` 400K, `ota_0`/`ota_1`
  5 MB each, `resources` 400K, `font_data` 5 MB (subtype 0x40, mmapped), `coredump` 64K.

## Boot & runtime
1. `app_main` (`main/app_main.cc:17-38`): `power_hw_log_sleep_reset` →
   `epd_board_metalio_eink4_plus_boot_release_holds` → default event loop → NVS init (erase on
   version/no-pages) → `NtpServer::Initialize` → `BlePageTurner::Init(PagerKey_Receive)` →
   `Application::Start()`; `app_main` then returns.
2. `Application::Start` (`main/application.cc:1760`): chat cloud + language from NVS →
   `SdLogWriter_Start` → `Board::GetInstance()` (constructs the board, below) → `GetCamera()` →
   `GetDisplay()` → `fontpack_lv_ensure_ready()` (needs internal-DRAM stack) →
   `a2ui_img_cache_init` / `assistant_chat_store_init` → creates `main_event_loop`
   (3072 B, prio 3, `:1803`) → 1 s clock timer → `ota.MarkCurrentVersionValid()` →
   `TrySdCardFirmwareUpgrade()` (`:1819`) → `book_home_snapshot::HydrateFromNvs` (`:1823`) →
   `board.StartNetwork()` → later `CheckNewVersion` (`:1855`).
3. Board constructor `MetalioEInk4PlusBoard` (`main/boards/metalio-e-ink4-plus/metalio_e_ink4_plus_board.cc:60-100`),
   in order: `MetalioEpd_Init` (**returns early on failure — nothing below runs**, `:62-65`) →
   `MetalioTouch_UpgradeOnBoot` → `MetalioPower_Init` → frontlight (output deferred) + NVS restore →
   RTC → accel → `metalio_touch_init` → `MetalioDisplay_Create` → vibe → SD → `WifiConfigStorage_Init`
   → `Wallpaper_HydrateFromNvsNow` → keys → system monitor → `PowerPolicy::Init(this)` →
   `MetalioDisplay_SetSystemReady` → `MetalioAudio_Init` (async BT "mode 1"; releases frontlight
   deferral) → `PanelVcomInit`. Registered with `DECLARE_BOARD` (`main/boards/common/board/board.h:128-132`).

**Work dispatch.** `MainEventLoop` (`main/application.cc:2054`) only waits on event bits and
forwards. Business work goes through `Application::Schedule(cb, TaskDomain)`
(`main/application_tasks.cc:70`) to 5 lazily created workers, prio 3, internal-RAM stacks, retired
after 2 s idle unless voice/audio is active (`:20-29,41-66`):

| Worker | Stack | Queue | Domain |
|---|---|---|---|
| app_voice | 8192 | 32 | protocol / voice session (serial) |
| app_network | 8192 | 4 | networking, activation, firmware requests |
| app_storage | 4096 | 16 | storage / NVS |
| app_audio | 8192 | 16 | local audio service acquire/start/stop |
| app_cleanup | 4096 | 24 | network cancel, resource release |

`Schedule` never blocks; returns `false` on full queue or task-create failure — callers must
recover (`main/application_tasks.cc:70-100`).

**UI thread.** `lv_epdiy_tick` (16 KB, prio 2, core 1, 10 ms; `main/display/lv_adapter_epdiy.cc:116-137`)
runs LVGL and drains: `ScreenLvAsyncDrain`, `VkKey_DrainTouch`, `PagerKey_Drain`
(`lv_adapter_epdiy.cc:720-727`). UI mailboxes: static queues normal 80 / urgent 16
(`main/display/screen/screen_common_nav.cc:24-30`), non-blocking send, drain ≤ 8 msgs or ~5 ms per
pass (`:111-129`). Accepted message → callback owns ctx; rejected → sender still owns it.

**Other tasks (name — stack, prio, core — where)**
- `epd_refresh` — 6144, MAX-2, core 1 — `main/display/epd_refresh.cc:636`
- `epd_prep` ×2 — PSRAM stack, MAX-1, core i — EPDiy `render.c:428-437` (board display/)
- `touch_feed` 4096/5 — `metalio_touch.c:35,300`; `metalio_keys` 3072/5 — `metalio_keys.cc:213`;
  `metalio_accel` 3072/5; `sys_mon` 4096/1; `bt_mode_init` 4096/5/core0 — `metalio_audio.cc:233`;
  `panel_vcom` 6144/4/core0 — `panel_vcom.cc:322` (all under `main/boards/metalio-e-ink4-plus/`)
- Audio: `audio_input` prio 8, `audio_output` MAX-2, `opus_codec` MAX-2 — PSRAM stacks, created on
  start, deleted on `Stop()` (`main/audio/audio_service.cc:110-209`)
- Power/storage: `pwr_off`, `standby_lp` (4K/3/core0), `battery_off`, `cx25601n_vreg`,
  `usb_vd_worker` (12K/6), `sd_log` (4K/6/core0), `uart_rx_task` (4K/10,
  `main/boards/common/audio/simple_uart.hpp:60`), `pager_init`, NT26 `cell_recover`/`cell_csq`
- UI workers: `reader_page` (24 KB, `book_page_work.cc:145`), `book_font`, `fp_mmap`, `a2ui_img`
- Timers: `pwr_pol` 1 s (`main/boards/common/power_policy/power_policy.cc:703-711`),
  `audio_power_timer` 1 s / 15 s idle, frontlight / vibe esp_timers, app clock 1 s.

**Device state** (`main/device_state.h:4-16`): Unknown, Starting, WifiConfiguring, Idle, Connecting,
Listening, Speaking, Upgrading, Activating, AudioTesting, FatalError — set only via
`Application::SetDeviceState`.

## Modules

### Application core — `main/application.{h,cc}`, `main/application_tasks.cc`
Boot, state machine, voice session, OTA orchestration. 2437 lines. Entry: `Start` (:1760),
`MainEventLoop` (:2054), `Schedule`. Voice: `StartXiaozhiVoice` (:749) /
`StartXiaozhiVoiceOnTask` (:760), `StopXiaozhiVoice` (:1055), `StartListening` (:1477),
`DispatchVoiceEvents` (:1948, runs on a worker). Audio arbitration: `AudioClient` bits
Assistant=1, FactoryTest=2, AgingTest=4, WorkBuddy=8, AppPlayback=16 (`application.h:106-112`);
`AcquireAudioService`/`ReleaseAudioClients` (`application.cc:190-227`) — service stops when no
bits remain; release requires `audio_service_mutex_`.

### Audio — `main/audio/`, `main/boards/common/audio/`
`AudioService`: Opus 60 ms frames, 16 kHz mono encode, complexity 0 (`audio_service.cc:47-49`);
send/decode queues 40 packets. Board codec is `BTAudioCodecDuplex` — I2S duplex with the ESP as
**slave** to the external BT chip (`main/boards/common/audio/bt_audio_codec.h:9-36`).
`audio/codecs/*` are upstream codecs unused by this board. Current `sdkconfig`: wake word disabled,
`USE_AUDIO_PROCESSOR` off (no esp-sr on S31) → no-op processor. BT chip AT commands over UART1
(115200) live in `periph/metalio_audio.cc` and
`display/screen/bluetooth_screen/bluetooth_uart.cc`, sharing the `SimpleUart` singleton with
`panel_vcom.cc` (protocol: `docs/ble_chip-audio-protocol.md`).

### Protocols — `main/protocols/`, `main/chat_cloud.h`
`Protocol` base (`protocol.h:44`), `WebsocketProtocol`, `MqttProtocol` (UDP + AES via PSA,
`mqtt_protocol.cc:173-375`). Selection (`application.cc:484-491`): MQTT only if provider is not
Yunxin, ws `url` empty and mqtt `endpoint` set; else WebSocket. Provider enum Baiwen / Xiaozhi /
Yunxin (`chat_cloud.h:9`); default Yunxin for Global, Baiwen otherwise; it also picks NVS
namespaces (`chat_cloud.h:65-66`). Incoming JSON types handled (`application.cc:536-660`): tts,
stt, vision, llm (emotion), mcp, system (reboot), alert, custom (Kconfig-gated). Every callback
captures and checks `voice_generation_` (`:493-496`) — new handlers must too.

### Board (device-specific) — `main/boards/metalio-e-ink4-plus/`
`MetalioEInk4PlusBoard : DualNetworkBoard`. `config.h` (pins; revisions 1.2/1.3/1.4 noted at
`:58-61`), `bus/` (shared `I2C_NUM_1` + lock in `metalio_i2c1.c`, `pca9555.c`, `tps65185.c`),
`display/` (EPDiy fork: `epdiy.c`, `render.c`, `highlevel.c`, `output_lcd/lcd_driver.c`, LUTs;
board glue `epd_board_metalio_eink4_plus.c`; facade `metalio_epd.c`; touch `metalio_touch.c`,
`metalio_touch_upgrade.cc`), `input/` (keys, vibe), `periph/` (frontlight, accel, audio, power,
rtc, sd, sys_mon, panel_vcom). Overrides: `GetDisplay`, lazy `GetCamera`, static
`GetAudioCodec`, `GetBatteryLevel` (`metalio_e_ink4_plus_board.cc:105-154`). Usual change: pins in
`config.h`; new peripheral = `periph/*.cc` init + one ctor call, keeping ctor order.

### Board (shared) — `main/boards/common/`
- `board/`: `Board` base + `create_board()` factory (upstream-derived), `WifiBoard` (STA only,
  SoftAP web config removed, `wifi_board.cc:100-109`), `DualNetworkBoard` (Wi-Fi / NT26 4G over
  UART; NVS `network/type`, `dual_network_board.cc:36-47`).
- `power_policy/`: `PowerPolicy` tiers, standby, shutdown; `PowerNeed` enum
  (`power_policy.h:25`) acquired/released or via RAII `PowerNeedHold` — needs used by screens:
  ForegroundNetwork, UiKeepNet, StandbyInhibit, OtaDownload, BtAudio, LocalMic. `power_hw`,
  `s31_sleep`, standby LP task.
- `power/`: BQ27220 gauge, CX25601N charger. `sensor/`: PCF8563 RTC, SC7A20H accel.
- `storage/`: `SdCardManager`, `sd_access::Transaction` (all SD access), `sd_paths.h` (every SD
  path; mount `/sdcard`), `sd_log_writer`, `usb_virtual_disk` (TinyUSB MSC — mutually exclusive
  with local VFS), `wifi_config_storage` (mirrors creds to SD `settings/wificonfig.json`).
- `camera/`: `dvp_camera` (GC2145, PID check `dvp_camera.cc:128`). `bluetooth/`:
  `ble_page_turner` (NimBLE remote).
- Usual change: add a virtual with default to `Board`, override in the board class.

### Display core — `main/display/` (excluding `screen/`)
`Display` base (`display.h:35`, `DisplayLockGuard` 1 s timeout `:89`); `LvAdapterEpdiy : Display`
singleton (`lv_adapter_epdiy.h:17`, `Instance()` `.cc:111`) — LVGL 9 init, draw buffer
(PSRAM first, size max(full I1 frame, L8×64 rows), `.cc:395-412`), tick, standby park/wake,
wallpaper. `epd_refresh.{h,cc}`: refresh worker, `EpdSubmit`/`EpdWait`, profiles (NVS `screen`).
`epd_nvs_guard.cc`, `epd_flash_guard.h`: Flash → I2C → NVS lock order. `font/`: fontpack mmap
(`fontpack_lvgl.cc:297`), TTF, epdfont. `a2ui/`: A2UI JSON renderer + image cache
(`a2ui_init` from `assistant_fonts.cc:95`). `lvgl_display/`, `emote_display.h`: upstream leftovers.
Behaviour spec: `main/display/README.md`.

### Screens / apps — `main/display/screen/` (~300 files)
No base class: a screen is a static `X::Create()` returning `lv_obj_t*`; navigation is a
back-stack of factories — `ScreenNavigateTo/Back/GoHome` (`screen_common.cc:138-166`), deferred
until `EpdTryBeginUiRender()` succeeds (`:101-135`). Lifecycle via `VkKey_AttachScreen`
(`vk_key_handler.cc:302`) hooking `LV_EVENT_SCREEN_LOADED/UNLOADED`. Home app table:
`home_screen.cc:72-92`. Apps (folder `<name>_screen/`): home, task (checklist), assistant (Baiwen
AI; calls `StartXiaozhiVoice` on load / `StopXiaozhiVoice` on unload, `assistant_screen.cc:115,163`),
book (shelf, reader, settings, cover, cloud library; state singleton `Book_State()` →
`BookUiState`, `book_screen.cc:51`), wallpaper, standby (overlay, not a screen), cloud (transfer),
settings (+ `settings_test/` factory/aging), bluetooth, ota_upgrade / ota_confirm_dialog, pager,
app_store, workbuddy. Input: touch → `VkKey_PostTouch`; BOOT/POWER → `BootKey_On*` /
`PowerKey_OnClick`; BLE pager → `PagerKey_Receive` (16-event mailbox, stale epochs dropped).
Add an app: new folder with `Create()` + `VkKey_AttachScreen`, an `AppEntry` in home, free state on
unload. Largest: `workbuddy_screen.cc` 1894 lines (no sub-modules), `app_store_screen.cc` 1093,
`bluetooth_screen.cc` 1068.

### Reader engine — `main/reader/`
No UI. Opens TXT / EPUB / `.ebook` (chunked zlib, spec `tools/ebook/SPEC.md`; `ebook_document.h:12`).
Hub `BookSession` (`book_session.h:50`; `book_session.cc` 4194 lines): open, pagination, TXT
streaming index, progress, page-image cache. Creates no tasks — UI's `reader_page`/`book_font`
drive it. Locks: `layout_progress_mu_`, `chapter_io_mu_`, `page_image_cache_mu_`
(`book_session.h:389,484,499`), `book_file_mu`/`progress_write_mu` (`book_session.cc:673-675`).
Storage: books in `/sdcard/metalio/e-ink/books` (`book_session.h:32`); sidecars `.idx/.pos/.cpg`
(`book_session.cc:606-620`), invalidated by `FontFingerprint` (`book_session.h:365`); NVS
`reader` keys `rec0..3`, `hs_*` (`book_home_snapshot.cc:32-36`). PNG/JPEG decode in
`image_stream.cc`, `jpeg/` (PSRAM, internal fallback). `EBOOK_EMULATOR` guards let the PC emulator
build this code — keep them.
Line breaking (horizontal): one shared rule, `CanBreakBetween` / `VerticalBreakState::Observe(...,
words)` in `vertical_text.h` — CJK context keeps the per-character rule, Latin context (English,
Vietnamese) breaks only after spaces/hyphens; in Latin context spaces and combining marks hang at line
end (`HangsAtLineEnd`). Drawers seed the line's context from its first strong character
(`TextLineCjkContext`).
Justification stretches only `JustifySlot` gaps (word gaps in Latin text). Changing the rules
needs a bump of `kLayoutRulesVersion` (`book_session.cc`) so page caches rebuild. Alignment
(Justified/Left) is draw-time only (`TextStyle::justify`, NVS `justify`); side margins are one
symmetric value 5–40 px (`book_margin_rules.h`). Spec: `docs/specs/2026-11-21-reader-word-wrap-design.md`.
Horizontal page positions: `page_line_walk.h` (`ForEachPageTextLine`: rows, paragraph gaps, indent,
centred titles) and `text_glyph_pen.h` (`HorizontalGlyphPen`: per-glyph x with shrink/justify) are
shared by `RasterPageTextToL8` and the word hit-test — change layout there, not in one caller.
Dictionary lookup: `page_word_hit.*` (`PageWordAt`, `WordAround`), `dict_word_rules.h`
(`DictLookupCandidates`: case + English suffix stripping), `dictionary.*` (StarDict on SD under
`/sdcard/dict/<name>/`, binary search via our `.pos` table, files opened per lookup, caller holds
`sd_access::Transaction`). Data prep: `tools/dict/prepare_stardict.py`. UI: book_screen
`reader/book_dict_sheet.*` — long-press (`LV_EVENT_LONG_PRESSED`) on the page body; page taps use
`LV_EVENT_SHORT_CLICKED`; sheet is outside `ReadChrome` (`DictSheetVisible()` gates taps, keys,
auto-turn); one-shot `book_dict` task. Spec: `docs/specs/2026-11-22-reader-dictionary-design.md`.

### Services — top-level `main/`
- OTA: `ota.cc/.h` (`MarkCurrentVersionValid` `:344`, `StartUpgrade*` `:516-526`; writes next
  slot then sets boot partition `:506,:680`), `ota_ignore.*`. `CheckNewVersion`
  (`application.cc:1137`) skips when the Cloudzao endpoint is blank (`:1149`). SD upgrade:
  `/sdcard/Metalio-E-Ink4-Plus.bin` (`ota.h:14`), flow `application.cc:1234-1306`.
  **App rollback is disabled** (`CONFIG_APP_ROLLBACK_ENABLE` not set in `sdkconfig`), so
  `MarkCurrentVersionValid` is effectively a no-op.
- MCP: `mcp_server.cc` — tools registered at `:44-277`: get_device_status,
  audio_speaker.set_volume, screen.set_brightness, camera.take_photo, screen.set_theme, plus
  user-only get_system_info, reboot, upgrade_firmware, screen.get_info, screen.snapshot,
  screen.preview_image, assets.set_download_url. Add via `AddTool` / `AddUserOnlyTool`.
- Book cover on standby / power-off: Settings → Power choice per screen (NVS `wallpaper`
  `stb_cover` / `off_cover`, RAM cache in `wallpaper_active.cc`). `wallpaper_screen/book_cover_wallpaper.cc`
  prepares one panel-fit `SD_PATH_SCREEN_COVER` (+ tag file = book path) on a low-priority task after a
  book opens (`book_open_worker.cc`) and at boot (`application.cc`); standby (`standby_wallpaper.cc`) and
  power-off (`lv_adapter_epdiy.cc` `DecodeShutdownWallpaperTask`) try it first, then the old paths.
  Pure rules: `main/reader/screen_cover_rules.h`. Spec: `docs/specs/2026-11-21-book-cover-standby-design.md`.
- `settings.{h,cc}`: NVS wrapper (`Settings ns(...)`). `system_info`, `ntp_server`, `assets`,
  `haptic_feedback`, `device_state_event`.
- Cloud endpoints: hosts only in `cloudzao_endpoints.c` (empty strings in this checkout; Global
  variant constants at `:11`), URL builders in `api_endpoints.h` (`api::Url` `:44`). New call:
  extern in `cloudzao_endpoints.{c,h}` → wrapper in `api_endpoints.h` → call site.
- `app_store/`: ELF app installer/loader (`esp_elf`; RISC-V ELF32 validation
  `app_store.cc:991-995`; stored as `<sha256>.elf` `:262`; math symbol table from
  `app_runtime_math.def`). Sample apps + build: `tools/elf_app/`.
- `workbuddy/`: assistant client with exclusive voice capture. `yunxin_binding/`: QR binding for
  the international (Yunxin) cloud.

### Tools — `tools/` (host only)
- Run by CMake at build time: `generate_home_icon_pixels.py`, `generate_quick_control_icons.py`,
  `compress_binary.py` (4-byte LE size + zlib), `assets/pack_ap_fileman.py`
  (`main/CMakeLists.txt:393-458`).
- Manual: `fontpack/` (`build_fontpack.py`, `build_misans_mixed.sh` → `font.fontpack`; needs
  freetype-py, fonttools), `ebook/` (`convert_ebook.py` PDF/EPUB/MOBI/TXT → `.ebook`, web app,
  WASM emulator in `emulator/src` reusing firmware code), `a2i1/` (1bpp image tool), `epdfont/`,
  `elf_app/` (needs riscv32-esp-elf-gcc), `assets/spiffs_assets/` (assets.bin),
  `firmware/` (merge, release, `blank_cloudzao_endpoints.py`), `vcom/` (factory VCOM page),
  `git/git-commit.sh`. Per-tool `requirements.txt` and venvs; no root `.python-version`.

## Cross-cutting
- **Lock order: Flash → I2C → NVS.** Enforced by link-time `--wrap` of every `nvs_*` and FatFS/VFS
  call (`main/CMakeLists.txt:321-346`) plus `lv_async_call` wrapping. Never call NVS inside a
  waveform/refill callback or while holding the I2C lock (`main/display/README.md:113`).
- **Stacks:** NVS, fontpack mmap and `HydrateFromNvs` need internal-RAM stacks — not the LVGL /
  PSRAM-stack tasks (`book_home_snapshot.h:21`). Default malloc goes to PSRAM above 16 B
  (`sdkconfig.defaults.esp32s31`: `SPIRAM_MALLOC_ALWAYSINTERNAL=16`, 64 KB internal reserve);
  LVGL allocations and draw buffer in PSRAM.
- **NVS namespaces seen:** `power` (idle_mhz, net_grace_s), `wifi`, `network` (type), `board`
  (uuid), `audio` (output_volume), `display` (brightness, sd_log), `clock`, `pager`,
  `frontlight` (cct), `screen` (refresh profile), `reader`, `chat_cloud`, `websocket`, `mqtt`,
  `xz_*`, `yx_*`, `ui` (language).
- **SD layout:** all paths in `main/boards/common/storage/sd_paths.h`; app data under
  `/sdcard/metalio/e-ink/`; logs `.../logs/metalio_logs.txt`, coredump exported to SD on boot.
- **Logging:** `ESP_LOG*` with per-file `TAG`; common tags in `README.md` §15.1. `pm` lowered to
  WARN at boot (`app_main.cc:20`).
- **Kconfig switches:** `BOARD_TYPE_*`, `WAKE_WORD_*`, `USE_AUDIO_PROCESSOR` / `USE_DEVICE_AEC` /
  `USE_SERVER_AEC`, `METALIO_TOUCH_UPGRADE_ON_BOOT` (default y), `USE_AUDIO_DEBUGGER`
  (`main/Kconfig.projbuild`); `CONFIG_RECEIVE_CUSTOM_MESSAGE`; `CONFIG_PM_ENABLE`;
  `CONFIG_TINYUSB_MSC_ENABLED`.
- **Managed deps** (`main/idf_component.yml`): lvgl 9.5.0, esp_lvgl_adapter 0.6.3, opus encoder,
  esp_codec_dev, zlib, freetype, esp_new_jpeg, esp_tinyusb (S31), esp-sr (s3/p4 only).

## Hardware
Pins and revisions: `main/boards/metalio-e-ink4-plus/config.h`; overview tables in `README.md` §7.

| Peripheral | Driver | Bus / notes |
|---|---|---|
| ED047TC2 4.7" e-paper | EPDiy fork `board display/`, `metalio_epd.c` | parallel LCD + ISR bounce buffers (`lcd_driver.c:238-512`) |
| TPS65185 PMIC | `bus/tps65185.c` | I2C1, controlled through TCA9555 pins |
| TCA9555 / PCA9555 expander | `bus/pca9555.c`, IRQ in `epd_board_metalio_eink4_plus.c:93` | I2C1, addr 0x20; P12 shared reset (TP/4G/cam), P14 shared power rail (`config.h:101-104`) |
| FT6336U touch | `display/metalio_touch.c` (+ fw upgrade) | I2C1, IRQ `touch_isr` `:109` |
| BQ27220 gauge / CX25601N charger | `common/power/` | I2C1 |
| PCF8563 RTC / SC7A20H accel | `common/sensor/`, `periph/` | I2C1 |
| BT audio chip | `periph/metalio_audio.cc`, `bt_audio_codec` | UART1 AT (GPIO 39/38) + I2S (ESP slave) |
| GC2145 camera | `common/camera/dvp_camera.cc` | DVP, detected lazily |
| SD card / USB MSC | `common/storage/`, `periph/metalio_sd.cc` | SDMMC; TinyUSB MSC |
| NT26 4G modem | `common/board/nt26_board`, `uart_eth_modem` | UART |
| Frontlight, vibe, keys | `periph/`, `input/` | GPIO/PWM; BOOT ISR `metalio_keys.cc:137` |

## Conventions
- C++ (`.cc`) for app/services/screens; C (`.c`) for EPDiy, bus drivers and endpoint tables; C APIs
  wrapped in `#ifdef __cplusplus extern "C"`. Chinese comments are common; new code matches the
  surrounding language.
- Singletons via `GetInstance()` (`Application`, `Board`, `PowerPolicy`, `LvAdapterEpdiy::Instance`).
- Background work → `Application::Schedule`; anything touching LVGL → `ScreenLvAsync[Urgent]`;
  both can return `false` and the caller keeps ownership.
- Async results are validated with generation counters / cancellation flags (voice, pager, pages).
- Sources: `main/CMakeLists.txt` uses globs plus a hand-written list — new directories must be
  added there.

## Danger zones
- **Board ctor early return** if `MetalioEpd_Init` fails (`metalio_e_ink4_plus_board.cc:62-65`):
  power policy, SD, keys, audio never initialise; later code must tolerate `display_ == nullptr`.
- **Shared I2C1 + lock** (TPS, TCA, touch, gauge, RTC, accel) and the Flash → I2C → NVS order.
- **ISR / IRAM / timing:** EPDiy `lcd_driver.c`, `render_lcd.c`, `lut.c/.S`, `epd_prep` tasks,
  waveform phase timing (~85 Hz target, README §9.1); touch/BOOT/TCA ISRs.
- **Creating PSRAM-stack tasks while the flash cache is disabled**; audio `Stop()` ordering
  (`audio_service.cc:220-291`).
- **LVGL off the UI thread** is never allowed; back-stack `kMaxBack` drops oldest
  (`screen_common.cc:204-208`); standby is an overlay tracked separately.
- **Reader:** don't hold `layout_progress_mu_` while laying out or touching UI; `pages_` rebuild vs
  UI has a noted UAF hazard (`book_screen.cc:49`); layout changes invalidate `.cpg/.idx`.
- **SD:** always via `sd_access::Transaction`; USB MSC excludes local VFS.
- **Release:** `merge_firmware.sh` chip default (see Build).
- **Vendored / upstream:** `components/` (vendored, incl. epdiy component), `patches/esp-idf/`,
  managed components; upstream-shared xiaozhi code: `board/`, `audio/codecs/`, protocols,
  `display/lvgl_display/`. Prefer extending via the board layer.

## Graph (graphify)
- Config: `.graphifyrc` (`language.h=cpp`), `.graphifyignore` (components, build,
  managed_components, main/assets, tools/ttf-fonts, docs/datasheets, docs/images, vendored LVGL in
  `tools/ebook/emulator/lvgl/`, `.claude/`, `metalio-claude-kit/`). Output `graphify-out/` is
  git-ignored.
- Build: `graphify extract . --code-only` then `graphify cluster-only . --no-viz --no-label`
  (offline). At fd3fa03: 13,917 nodes, 35,745 edges, 507 communities (unnamed).
- Known gap: headers with split `#ifdef __cplusplus extern "C" {` … `}` blocks fail to parse as
  C++ (e.g. `metalio_epd.h`, `metalio_i2c1.h`, `cx25601n.h`), so their declarations are missing;
  the `.c` definitions are indexed.

## Open questions
- Build/size not run during mapping — record the first verified `idf.py build` / `size` result here.
- Does `esptool.py --chip esp32s3 merge_bin` produce a valid S31 image, or must `ESPTOOL_CHIP` be set?
- Which task runs reader pagination loops; where gray/AA refresh modes are chosen
  (`book_reader_body.cc`, `book_page_work.cc` likely) — not traced.
- `Display::Lock` implementation, GC16 vs DU waveform choice, A2UI streaming path — not read.
- App store sandbox beyond the symbol whitelist (`app_api.h`) — not read.
- `SetDeviceState` transition rules; `Settings`/`OtaIgnore` key names; 4G selection logic in
  `dual_network_board.cc`; BQ27220/CX25601N internals — not read.
- Upstream diff: which audio/protocol parts are Metalio-specific (providers, vision, generation
  counters look custom) — not diffed against xiaozhi-esp32.
- Host-test candidates (pure logic): `text_encoding`, `txt_chapter`, `html_content`,
  `image_geometry`, `.pos` parsing — unverified; the PC emulator already compiles reader code.
