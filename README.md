# The King of Fighters Neowave - XBE Recomp

[![AI Assisted](https://img.shields.io/badge/Assisted_by-Google_Gemini-4285F4?style=for-the-badge&logo=google&logoColor=white)](https://deepmind.google/technologies/gemini/)
[![Platform](https://img.shields.io/badge/Platform-Windows_x64-0078D6?style=for-the-badge&logo=windows)](https://github.com/bromurok/TheKingofFightersNeowaveRecomp)
[![Backend](https://img.shields.io/badge/Backend-Direct3D_12-green?style=for-the-badge&logo=directx)](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)

An unofficial native PC port of the original Xbox version of **The King of Fighters Neowave** (*SNK Playmore*), featuring a native Direct3D 12 renderer, custom PC Settings menu, resolution scaling, full controller and keyboard support, and locked 60 FPS.

This port was created using the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) toolchain.

> [!TIP]
> **Why Native PC Recompilation?**
> By recompiling the game natively for Windows x64 using Direct3D 12, it completely eliminates emulation overhead—delivering a rock-solid 60 FPS, ultra-low input latency, and pristine high-resolution rendering without requiring complex emulators.

> [!NOTE]
> This project does not contain any copyrighted game assets or proprietary code. You must provide the files or disc image from your own legally acquired copy of the game to install or build.

---

## In-Game PC Settings Menu

Press **`T + Y`** on your keyboard or **`START + BACK`** (or `X + Y`) on your controller at any time (in main menu, Graphic Setting, in-game match, or during **PAUSE**) to open the integrated PC Settings overlay:

* **Navigate:** D-Pad / Thumbstick or Arrow Keys / `W` `S`
* **Change values:** `< >` or `A` `D`
* **Apply / Confirm:** **`START`** on controller or **`ENTER`** on keyboard
* **Return / Close:** **`BACK`** on controller or **`ESC`** / **`T + Y`** on keyboard

### Available Options:
1. **MODO DE PANTALLA / DISPLAY MODE:** Fullscreen, Borderless Windowed (Recommended), Windowed.
2. **PROPORCION / ASPECT RATIO:** Toggle between `16:9` widescreen and original `4:3`.
3. **RESOLUCION / RESOLUTION:** Full list of monitor display resolutions (including native monitor resolution). Features an automatic 3-second reversion trial if unconfirmed.
4. **CALIDAD / QUALITY:** Internal 3D render scaling multiplier (`x1`, `x2`, `x4`, `x8`).
5. **MANDO / CONTROLLER ASSIGNMENT:**
   * `< JUGADOR 1 >`: Controller controls Player 1 simultaneously with keyboard for single-player arcade.
   * `< JUGADOR 2 >`: Controller is assigned to Player 2 (for local versus against Player 1 on keyboard).
6. **ANTI-ALIASING:** Toggle smoothing on 3D stage geometry.
7. **BORDES NEGROS / BLACK BARS:** Original aspect ratio vs Fill screen.
8. **IDIOMA / LANGUAGE:** Spanish or English.

---

## Installation

1. Download the latest release from the [Releases](https://github.com/bromurok/TheKingofFightersNeowaveRecomp/releases) page.
2. Extract the archive into a folder of your choice.
3. Place your game files in the `assets` folder. You can use:
   - The game's disc image (`.iso`).
   - Or the extracted game directory containing `default.xbe`.
4. Run `KOFNeowave.exe`.

---

## Building from Source

### Prerequisites
* **Windows 10 / 11 (64-bit)**
* **CMake 3.25+**
* **Clang/LLVM** or **MSVC toolchain**
* **Ninja** build system

### Build Steps
1. Clone the repository:
   ```bash
   git clone https://github.com/bromurok/TheKingofFightersNeowaveRecomp.git
   cd TheKingofFightersNeowaveRecomp
   ```
2. Place your legally obtained `default.xbe` in the `assets/` folder.
3. Run the automated build script:
   ```cmd
   build.bat
   ```
4. Or configure and build with CMake & Ninja:
   ```bash
   cmake --preset release
   ninja -C out/build/release
   ```

---

## Credits & Acknowledgements

* **SNK Playmore** for *The King of Fighters Neowave*.
* **[TGP482](https://github.com/TGP482)** for the [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp) framework.
* **[Google Gemini](https://deepmind.google/technologies/gemini/)** for assistance in porting, bug fixes, D3D12 fog/texture decoding corrections, and PC settings implementation.
