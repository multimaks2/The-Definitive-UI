# 🎮 The-Definitive-UI

<p align="center">
  <img src="https://img.shields.io/badge/GTA%20SA-1.0%20US-brightgreen?style=for-the-badge&logo=rockstargames&logoColor=white" alt="GTA SA 1.0 US"/>
  <img src="https://img.shields.io/badge/platform-Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white" alt="Windows"/>
  <img src="https://img.shields.io/badge/type-ASI%20plugin-orange?style=for-the-badge" alt="ASI plugin"/>
  <img src="https://img.shields.io/badge/DirectX-9-5C2D91?style=for-the-badge&logo=directx&logoColor=white" alt="DirectX 9"/>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-MSVC%20v145-00599C?style=flat-square&logo=cplusplus&logoColor=white" alt="C++"/>
  <img src="https://img.shields.io/badge/Visual%20Studio-2022%2B-5C2D91?style=flat-square&logo=visualstudio&logoColor=white" alt="VS2022"/>
  <img src="https://img.shields.io/badge/plugin--sdk-required-yellow?style=flat-square" alt="plugin-sdk"/>
  <img src="https://img.shields.io/badge/UI%20style-Definitive%20Edition-blueviolet?style=flat-square" alt="DE UI"/>
  <img src="https://img.shields.io/badge/languages-6-informational?style=flat-square" alt="6 languages"/>
  <img src="https://img.shields.io/badge/license-GPL--3.0-blue?style=flat-square" alt="GPL-3.0"/>
  <a href="https://github.com/multimaks2/Radar-in-style-GTA-SA-The-Definitive-Edition/releases">
    <img src="https://img.shields.io/github/v/release/multimaks2/Radar-in-style-GTA-SA-The-Definitive-Edition?include_prereleases&style=flat-square&label=release" alt="Release"/>
  </a>
  <a href="https://github.com/multimaks2/Radar-in-style-GTA-SA-The-Definitive-Edition/stargazers">
    <img src="https://img.shields.io/github/stars/multimaks2/Radar-in-style-GTA-SA-The-Definitive-Edition?style=flat-square" alt="Stars"/>
  </a>
</p>

ASI-плагин для **GTA San Andreas 1.0 US**, который заменяет стоковый интерфейс на UI в стиле *GTA: The Definitive Edition*: радар, главное меню, пауза с картой, подсказки и связанные HUD-элементы.

| | |
|---|---|
| 📦 Сборка | `The-Definitive-UI.SA.asi` |
| ⚙️ Конфиг | `The-Definitive-UI.SA.ini` *(создаётся при первом запуске)* |
| 📂 Ресурсы | папка `The-Definitive-UI.SA\` рядом с ASI — внутрь положить содержимое `assets/The-Definitive-UI.SA/` (`map.txd`, `MainMenu.txd`, `blip.txd`, png и т.д.) |

---

## ✨ Возможности

- 🗺️ Радар DE-стиля (круг/квадрат, размер, отступы, цвета рамки и фона)
- 🖥️ Главное меню и пауза с панелями Game / Settings / карта с метками
- 🧭 GPS-маршрут на радаре
- 💬 Подсказки help и название радиостанции своим шрифтом
- 🌐 Языки UI: American, French, German, Italian, Spanish, Russian
- 🪟 Режим окна: exclusive / windowed / borderless
- 🎨 Разрядность цвета (16 / 32 bit) в настройках
- 🌫️ Переключатели эффектов: heat haze, speed blur
- 🧩 Переключение в ini: свои тайлы `map.txd` или сток `radar00`–`radar143`; иконки Definitive или сток `hud.txd`; зоны банд

> Отдельные подсистемы можно отключить через ini (`RadarRender`, `MenuRender` и др.).

---

## 📋 Требования

| Компонент | Версия / условие |
|-----------|------------------|
| 🕹️ Игра | GTA SA **1.0 US** (хотфикс) |
| 🔌 Загрузчик | ASI Loader или ModLoader |
| 🛠️ Сборка | Visual Studio 2022+ (toolset **v145**), Windows 10 SDK |
| 📚 SDK | [plugin-sdk](https://github.com/DK22Pac/plugin-sdk) — переменная окружения `PLUGIN_SDK_DIR` |
| 🖼️ DirectX | DX9 SDK (идёт вместе с plugin-sdk: `shared\dxsdk`) |

Сначала соберите `plugin_sa` (Release / Debug), чтобы в `$(PLUGIN_SDK_DIR)\output\lib` лежали `Plugin.lib` / `Plugin_d.lib`.

---

## 🔨 Сборка

1. Откройте `The-Definitive-UI.sln`
2. Конфигурация: **Release GTA-SA** или **Debug GTA-SA**, платформа **Win32**
3. Build → `bin\GTA-SA\Release\` или `bin\GTA-SA\Debug\`

---

## 📥 Установка

Рядом с `gta_sa.exe` (или в папке мода ModLoader):

1. Скопировать `The-Definitive-UI.SA.asi` (и при желании `.pdb`).
2. Создать папку **`The-Definitive-UI.SA`** рядом с ASI.
3. Скопировать в неё **всё содержимое** `assets/The-Definitive-UI.SA/` из репозитория (`map.txd`, `MainMenu.txd`, `blip.txd`, png…).

Итоговая раскладка:

```
gta_sa.exe
The-Definitive-UI.SA.asi
The-Definitive-UI.SA.pdb          ← по желанию
The-Definitive-UI.SA\
  map.txd
  MainMenu.txd
  blip.txd
  …остальные файлы из assets/The-Definitive-UI.SA/
```

Без этой папки мод не укомплектован: радар/меню не найдут текстуры (`The-Definitive-UI.SA\map.txd` и т.п.).

> 💡 PDB рядом с ASI нужен, если присылают `.dmp`: без него стек будет безымянным.

---

## ⚙️ Конфигурация

`The-Definitive-UI.SA.ini` — рядом с ASI (создаётся при первом запуске). Основные ключи:

| Ключ | Назначение |
|------|------------|
| `RadarRender` / `MenuRender` | вкл. кастомный радар / меню |
| `Language` | язык UI |
| `ZoomKey` | зум камеры радара (по умолчанию `G`) |
| `CustomRadarTxd` | **тайлы карты:** `1` — свои из `map.txd` (папка `The-Definitive-UI.SA`), `0` — оригинал игры `radar00`–`radar143` |
| `DeIcons` | **иконки радара/HUD:** `1` — пак Definitive, `0` — оригинал `hud.txd` (блипы 56/57 всегда сток) |
| `GPS` / `RadioText` / `UpdatedHelp` | GPS, текст радио, обновлённые help |
| `HeatHaze` / `SpeedBlur` | пост-эффекты |
| `WindowMode` / `WindowWidth` / `WindowHeight` / `ColorDepth` | окно и цвет |
| `Shape`, `CircleSize`, `SquareSize*`, `Offset*`, `*Color` | геометрия и цвета радара |

Комментарии в ini дублируются на всех поддерживаемых языках.

---

## 📁 Структура репозитория

```
The-Definitive-UI.sln / .vcxproj
source/
  Main.cpp              вход ASI, жизненный цикл
  Radar/                радар, тайлы, блипы, GPS, шейдеры тумана
  MainMenu/             главное меню
  pMainMenu/            пауза + карта
  Help/                 подсказки (GXT / словарь фраз)
  Config/               ini
  HookManager/          редиректы frontend / HUD
  Game/                 window mode, settings, game state
  LanguageManager/      словари UI и зон
  Draw/, InputManager/, TxdManager/, Shader/, Utils/
assets/
  The-Definitive-UI.SA/ ресурсы мода (map.txd, MainMenu.txd, blip.txd, png…)
```

---

## 🙏 Благодарности

Часть логики портирована или адаптирована из **[Multi Theft Auto: San Andreas](https://github.com/multitheftauto/mtasa-blue)** (`mtasa-blue`):

| Здесь | Источник в MTA |
|-------|----------------|
| `source/Game/WindowMode.*` | `CVideoModeManager` — windowed / exclusive / borderless |
| `source/Draw/Draw.*` | подход `CGraphics` к 2D-отрисовке (текстуры, текст, состояния) |
| `source/InputManager/InputManager.*` | подход `CKeyBinds` к клавиатуре и текстовому вводу |
| патчи heat haze / speed blur в `Config` | тот же принцип отключения через RET в начале функции |

Остальной код (радар, меню, help, GPS и т.д.) — собственная реализация проекта.

---

## 📜 Лицензия

Проект распространяется под **[GNU General Public License v3.0](LICENSE)** (`GPL-3.0`).

Часть кода производна от MTA (`mtasa-blue`, GPLv3), поэтому весь репозиторий и бинарные сборки при распространении идут под GPLv3: исходники должны оставаться открытыми на тех же условиях.

---

## ⚠️ Замечания

- Целевая сборка игры: **SA 1.0 US** (`PLUGIN_SGV_10US`). Другие экзешники не поддерживаются.
- В Release PDB пишется отдельно; в бинарник вшивается только имя файла PDB (`/PDBALTPATH`), без абсолютного пути машины сборки.
- Документ `PLUGIN-SDK-ADDRESS-AUDIT.md` — внутренний аудит адресов plugin-sdk, к установке мода не относится.

<p align="center">
  <sub>Made for GTA San Andreas · Windows · DirectX 9</sub>
</p>
