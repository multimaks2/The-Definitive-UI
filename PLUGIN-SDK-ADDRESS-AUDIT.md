# План: хардкод-адреса ↔ Plugin-SDK

Аудит адресов в **The-Definitive-UI** (SA 1.0 US) против Plugin-SDK (`plugin_sa`) + сверка с gta-reversed.

Только игровые / RenderWare адреса. Цвета ARGB не входят.

Файл лежит рядом с `The-Definitive-UI.sln`. Пути в таблице — от корня проекта.

## Важно про хуки

Даже если функция есть в SDK (например `CHud::DrawRadar`), адрес для SafetyHook / call-site патча всё равно нужен как число.

| Метка | Смысл |
|-------|--------|
| **→ SDK** | Можно заменить вызов/global на API Plugin-SDK |
| **Частично** | Класс в SDK есть, нужного метода в заголовке нет |
| **Оставить** | В Plugin-SDK нет (call-site, RW/D3D9, platform, statics) |

Plugin-SDK внутри часто пишет те же `0x……` — API читаемее, кросс-версии без `ADDRESS_BY_VERSION` / meta не даёт.

## Сводка

### Легко вытянуть из SDK

`CRadar::*`, `CHud::*`, `CMenuManager::*` / `FrontEndMenuManager`, `CPad::UpdatePads`, `CGame::Process`, `gGameState`, `CPostEffects::HeatHazeFX` / `SpeedFX`, `FindPlayerCoors`, `TheText.Get`, `CGenericGameStorage::ms_Slots`, `CRadar::DrawBlips`.

### Нельзя убрать hex

Call-sites GPS / CopNThreat, GPS path-pool patches, почти весь `WindowMode` / RwD3D9, `FrontendIdle` / `Idle`, DI mouse, `GetVideoModeList`, radio `0x4E9E50`, YouAreHere statics, `gGxtString` (`0xC1B100`).

### Частично

| Адрес | Что | Почему |
|-------|-----|--------|
| `0x6190A0` | `C_PcSave` generate filename | `C_PcSave` есть; этого метода в SDK нет |
| `0x408340` | `CTxdStore::GetTxd` | `CTxdStore` есть; `GetTxd` не экспонирован |
| `0x531F20` / `0x531C90` / `0x52F590` / `0x530490` | controller config methods | класс контроллеров в SDK неполный |

---

## Таблица

| Файл | Адрес | Что это | Замена из SDK | Статус | Комментарий |
|------|-------|---------|---------------|--------|-------------|
| `source/Help/HelpGxt.cpp` | `0x6A0050` | `CText::Get` | `TheText.Get(key)` | → SDK | `CText.h` |
| `source/Help/HelpGxt.cpp` | `0x588BE0` | Set help message | `CHud::SetHelpMessage(...)` | → SDK | `CHud.h` |
| `source/Help/HelpGxt.cpp` | `0x588E30` | Set help with number | `CHud::SetHelpMessageWithNumber(...)` | → SDK | `CHud.h` |
| `source/Help/HelpGxt.cpp` | `0xC1B100` | `gGxtString[552]` | — | Оставить | есть в gta-reversed, нет в Plugin-SDK |
| `source/Radar/mapmanager/StockRadarDraw.cpp` | `0x583480` | Transform radar point | `CRadar::TransformRadarPointToScreenSpace` | → SDK | hook entry = тот же addr |
| `source/Radar/mapmanager/StockRadarDraw.cpp` | `0x5832F0` | Limit radar point | `CRadar::LimitRadarPoint` | → SDK | `CRadar.h` |
| `source/Radar/mapmanager/StockRadarDraw.cpp` | `0x583350` | Limit to map | `CRadar::LimitToMap` | → SDK | `CRadar.h` |
| `source/Radar/mapmanager/StockRadarDraw.cpp` | `0x584960` | Draw you-are-here | `CRadar::DrawYouAreHereSprite` | → SDK | `CRadar.h` |
| `source/Radar/mapmanager/StockRadarDraw.cpp` | `0x585FF0` | Draw radar sprite | `CRadar::DrawRadarSprite` | → SDK | `CRadar.h` |
| `source/Radar/mapmanager/StockRadarDraw.cpp` | `0x587D20` | Setup airstrip blips | `CRadar::SetupAirstripBlips` | → SDK | `CRadar.h` |
| `StockRadarDraw` / `pMainMenu` | `0x56E400` | Find player coors | `FindPlayerCoors(playerId)` | → SDK | `common.h` |
| `source/Radar/mapmanager/StockRadarDraw.cpp` | `0xBAA358` | map you-are-here timer | — | Оставить | static внутри стока |
| `source/Radar/mapmanager/StockRadarDraw.cpp` | `0x8D0930` | map you-are-here display | — | Оставить | static bool |
| comment | `0x586B00` | Draw map | `CRadar::DrawMap()` | → SDK | в коде только комментарий |
| `source/Radar/mapmanager/RadarOverlayCompat.cpp` | `0x588050` | Draw blips | `CRadar::DrawBlips()` | → SDK | вызов/trampoline; **не** хукать prologue |
| `source/Radar/mapmanager/RadarOverlayCompat.cpp` | `0x5869BF` | HUD call-site drawRadarOverlay | — | Оставить | GPS Redux |
| `source/Radar/mapmanager/RadarOverlayCompat.cpp` | `0x5759E4` | Pause call-site drawRadarOverlay | — | Оставить | call-site |
| `source/Radar/mapmanager/RadarOverlayCompat.cpp` | `0x58AA2D` | HUD call-site drawBlips (H_JUMP) | — | Оставить | CopNThreat |
| `source/Radar/mapmanager/RadarOverlayCompat.cpp` | `0x575B44` | Pause call-site drawBlips (H_CALL) | — | Оставить | CopNThreat |
| `source/Radar/render/GpsRender.cpp` | `0x44DE3C` … `0x4519B0` | path-node buffer / limit patches | — | Оставить | inline patch sites |
| `source/Config/Config.cpp` | `0x701780` | Heat haze FX | `CPostEffects::HeatHazeFX` | → SDK | `CPostEffects.h` |
| `source/Config/Config.cpp` | `0x7030A0` | Speed FX | `CPostEffects::SpeedFX` | → SDK | `CPostEffects.h` |
| `source/HookManager/HookManager.h` | `0x57C290` | Draw front-end | `FrontEndMenuManager.DrawFrontEnd()` | → SDK | адрес хука тот же |
| `source/HookManager/HookManager.h` | `0x57B440` | Menu Process | `FrontEndMenuManager.Process()` | → SDK | `CMenuManager.h` |
| `source/HookManager/HookManager.h` | `0x57FD70` | Menu UserInput | `FrontEndMenuManager.UserInput()` | → SDK | `CMenuManager.h` |
| `source/HookManager/HookManager.h` | `0xC8D4C0` | game state | `gGameState` | → SDK | `CGame.h` |
| `source/HookManager/HookManager.h` | `0x53BEE0` | Game process | `CGame::Process` | → SDK | hook target = addr |
| `source/HookManager/HookManager.h` | `0x541DD0` | Update pads | `CPad::UpdatePads()` | → SDK | `CPad.h` |
| `source/HookManager/HookManager.h` | `0x58A330` | Draw radar HUD | `CHud::DrawRadar()` | → SDK | `CHud.h` |
| `source/HookManager/HookManager.h` | `0x58B6E0` | Draw help text | `CHud::DrawHelpText()` | → SDK | `CHud.h` |
| `source/HookManager/HookManager.h` | `0x53E82D` / `0x53EB8C` | CALL DrawFrontEnd sites | — | Оставить | патч call-сайтов |
| `source/HookManager/HookManager.h` | `0x53E770` | FrontendIdle | — | Оставить | app-level |
| `source/HookManager/HookManager.h` | `0x53E920` | Idle (main loop) | — | Оставить | нет символа в Plugin-SDK |
| `HookManager` / `WindowMode` | `0x746F70` / `0x7469A0` | DIReleaseMouse / diMouseInit | — | Оставить | WinInput |
| `source/HookManager/HookManager.h` | `0x4E9E50` | Display radio station name | — | Оставить | AudioEngine-обёртка = другой addr |
| `GameSettings` / `WindowMode` / `MainMenu` | `0x745AF0` | GetVideoModeList | — | Оставить | platform VideoMode |
| `source/Game/WindowMode.cpp` | `0x745C70` | SetVideoMode | — | Оставить | platform |
| `source/Game/WindowMode.cpp` | `0x745CA0` | IsVideoModeExclusive | — | Оставить | platform |
| `source/Game/WindowMode.cpp` | `0xC97C1C` … `0xC97C54` | RwD3D9 HWND / device / RT / DS / modes | — | Оставить | D3D9 RW globals |
| `source/Game/WindowMode.cpp` | `0xC9C040` / `0xC9BCE0` / `0xC980B0` | Present / adapter / restore cb | — | Оставить | нет в Plugin-SDK |
| `source/Game/WindowMode.cpp` | `0xC920CC` / `0x8D6220` | windowed flag / cur video mode | — | Оставить | platform globals |
| `WindowMode` / `Draw` / `RenderTarget` | `0x7F5F20` / `0x7F5EF0` | RwD3D9 SetRenderTarget / SetDepthStencil | — | Оставить | нет обёрток в shared RW |
| `source/Game/WindowMode.cpp` | `0x7F7F70` / `0x4CC970` / `0x7F58D0` … | ReleaseVram / RasterRestore / Im2D/Im3D | — | Оставить | device-lost path |
| comment | `0x72FC70` | CameraSize | — | Оставить | только комментарий |
| `source/Game/GameSettings.cpp` | `0x531F20` / `0x531C90` / `0x52F590` / `0x530490` | controller config methods | класс есть, методы нет | Частично | хедер SDK неполный |
| `source/SaveSlots/SaveSlots.cpp` | `0xC16EBC` | save slot states | `CGenericGameStorage::ms_Slots` | → SDK | `CGenericGameStorage.h` |
| `source/SaveSlots/SaveSlots.cpp` | `0x6190A0` | generate save filename | `C_PcSave` есть, метода нет | Частично | SaveSlot/DeleteSlot ≠ этот addr |
| `source/TxdManager/TxdManager.cpp` | `0x408340` | Get TXD by slot | `CTxdStore` есть, `GetTxd` нет | Частично | CallAndReturn или дописать SDK |
| `source/TxdManager/TxdManager.cpp` | `0xB4E9E0` | RwD3D9RasterExtOffset | — | Оставить | RW plugin offset |
| `source/Utils/Utils.cpp` | `0x8E2440` … `0x8E2450` | RwD3D9 FVF/state cache invalidate | — | Оставить | gta-reversed: last FVF/state |
| `source/MainMenu/MainMenu.cpp` | `0x8E2430` | SelectedMultisamplingLevels | — | Оставить | D3D MSAA global |

Дубли RwD3D9 RT/DS (`0xC97C30`, `0xC97C2C`) и set-surface (`0x7F5F20`, `0x7F5EF0`): `source/Draw/Draw.cpp`, `source/Radar/render/RenderTarget.cpp`.

GPS patch-сайты подробно: `0x44DE3C`, `0x450D03`, `0x451782`, `0x451904`, `0x451AC3`, `0x451B33`, `0x4518F8`, `0x4519B0` — в `source/Radar/render/GpsRender.cpp`.

---

## Рекомендуемый порядок работ

1. **Низкий риск** — заменить прямые `plugin::Call<0x…>` / raw cast на SDK API: `CRadar`, `CHud`, `CMenuManager` / `FrontEndMenuManager`, `FindPlayerCoors`, `gGameState`, `ms_Slots`, post-FX.
2. **Средний риск** — partial: тонкие обёртки `CallMethod` с пометкой «нет в SDK», либо локальные хелперы.
3. **Не трогать без нужды** — call-sites оверлеев/блипов, GpsRender patches, блок WindowMode/RwD3D9.

Имена в колонке «Замена» — по заголовкам Plugin-SDK (`CRadar.h`, `CHud.h`, `CPostEffects.h`, …).
