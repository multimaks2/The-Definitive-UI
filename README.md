# Radar Trilogy SA

Custom radar for GTA San Andreas with extended features.

<img width="2560" height="1440" alt="Screenshot (250)" src="https://github.com/user-attachments/assets/7093eae1-857e-4ed0-a5d2-26e8416d7a5e" />

## Features

- Full replacement of the standard radar
- Display of radio station name
- Gang zone support
- Extended markers — based on the [More Icon](https://gist.github.com/JuniorDjjr/38ba16704a2de63fd7ea31711d6e9e0f) code example
- Flexible configuration via a config file

## Installation

1. Copy `radar-trilogy-sa.asi` to your GTA SA `scripts` folder
2. Make sure [ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) is installed
3. Launch the game

## Requirements

- GTA San Andreas (version 1.0 US)
- [Plugin SDK](https://github.com/DK22Pac/plugin-sdk) (for building)
- Visual Studio 2026 

## Building

1. Clone the repository into `plugin-sdk\tools\myplugin-gen\generated\`
2. Set the environment variable `PLUGIN_SDK_DIR` to your Plugin SDK path
3. Open `radar-trilogy-sa.sln` in Visual Studio 2026
4. Build the project in Release mode

## Configuration

Config file: `radar-trilogy-sa.ini` (created automatically)

### Options

- `Shape` — radar shape (0 = square, 1 = circle)
- `ShowGangZones` — show gang zones (0 = off, 1 = on)
- `ModeMoreIcon` — icon mode
- `CircleSize` — radar size

## License / Лицензия

**[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/)** — see [LICENSE](LICENSE).

**English:** You may share and adapt this work for **non-commercial** purposes only, with **attribution**, and derivatives must stay under the **same license** (source remains open). Selling or other commercial use is not allowed without a separate permission from the copyright holder.

**Русский:** Разрешено распространять и изменять работу **только в некоммерческих целях**, с указанием **авторства**; производные работы должны оставаться под **той же лицензией** (исходный код остаётся открытым). **Продажа и иное коммерческое использование запрещены** без отдельного разрешения правообладателя.

## Acknowledgements

- [DK22Pac](https://github.com/DK22Pac) for Plugin SDK
- GTA modding community for documentation
- [JuniorDjjr](https://gist.github.com/JuniorDjjr) for the More Icon example
