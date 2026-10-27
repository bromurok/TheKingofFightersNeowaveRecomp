#include "app/app.h"
#include "config_menu.h"
#include "core/input.h"
#include "core/log.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

#include <atomic>
#include <chrono>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

std::atomic<bool> s_keys[256] = {};
std::atomic<bool> s_active = false;

bool OnMessage(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result) {
    switch (msg) {
        case WM_KEYDOWN:
            if (wp == VK_F1 && !(lp & 0x40000000)) {  // F1 abre/cierra PC Settings (sin repetir)
                cfgmenu::ToggleNativePCSettings();
                for (auto& k : s_keys) k.store(false, std::memory_order_relaxed);
                return true;
            }
            if (cfgmenu::Blocked()) return true;  // con el menu abierto el teclado no pasa al juego
            if (wp < 256) {
                s_keys[wp].store(true, std::memory_order_relaxed);
                if (!s_active.load(std::memory_order_relaxed)) {
                    s_active.store(true, std::memory_order_relaxed);
                    LOGI("game: keyboard controller activated by key %u", (uint32_t)wp);
                }
            }
            break;
        case WM_KEYUP:
            if (wp < 256) s_keys[wp].store(false, std::memory_order_relaxed);
            break;
        case WM_KILLFOCUS:
            for (auto& k : s_keys) k.store(false, std::memory_order_relaxed);
            break;
    }
    return false;
}

void OnPad(int port, input::Pad& pad) {
    auto is_down = [](int vk) -> bool {
        return vk >= 0 && vk < 256 && s_keys[vk].load(std::memory_order_relaxed);
    };

    // Detección de atajo en mando para abrir/cerrar ajustes en pausa / gameplay / menú (en teclado solo F1)
    static bool s_pad_menu_was_down = false;
    if (port == 0) {
        input::Pad p_chk = input::RawPoll(0);
        // Back + Start (0x0010 + 0x0020 = 0x0030) o X + Y (analog[2] y analog[3])
        bool pad_menu_down = (((p_chk.buttons & 0x0030) == 0x0030) || (p_chk.analog[2] > 64 && p_chk.analog[3] > 64));

        if (pad_menu_down && !s_pad_menu_was_down) {
            cfgmenu::PlayMenuSound(0x5F);
            cfgmenu::ToggleNativePCSettings();
        }
        s_pad_menu_was_down = pad_menu_down;
    }

    // Menu abierto o ventana sin foco: ningun input se cuela al juego
    if (cfgmenu::Blocked()) {
        pad.buttons = 0;
        std::memset(pad.analog, 0, sizeof(pad.analog));
        pad.lx = pad.ly = pad.rx = pad.ry = 0;
        return;
    }

    int ctrl_player = app::Get().controller_player;
    if (ctrl_player != 2) ctrl_player = 1; // por defecto mando es Jugador 1

    if (ctrl_player == 1) {
        // MODO MANDO = JUGADOR 1
        if (port == 0) {
            // Puerto 0 (Jugador 1): Recibe Mando 0 de SDL Y Teclado combinados
            pad = input::RawPoll(0);
            pad.connected = true;

            // Combinar con teclas del teclado para Jugador 1
            if (is_down(VK_UP) || is_down('W')) { pad.buttons |= 0x0001; if (pad.ly < 16000) pad.ly = 32767; }
            if (is_down(VK_DOWN) || is_down('S')) { pad.buttons |= 0x0002; if (pad.ly > -16000) pad.ly = -32767; }
            if (is_down(VK_LEFT) || is_down('A')) { pad.buttons |= 0x0004; if (pad.lx > -16000) pad.lx = -32767; }
            if (is_down(VK_RIGHT) || is_down('D')) { pad.buttons |= 0x0008; if (pad.lx < 16000) pad.lx = 32767; }

            if (is_down(VK_RETURN) || is_down(VK_SPACE)) pad.buttons |= 0x0010;
            if (is_down(VK_ESCAPE) || is_down(VK_BACK)) pad.buttons |= 0x0020;

            if (is_down('J') || is_down('Z')) pad.analog[0] = 0xFF; // A
            if (is_down('K') || is_down('X')) pad.analog[1] = 0xFF; // B
            if (is_down('U') || is_down('C')) pad.analog[2] = 0xFF; // X
            if (is_down('I') || is_down('V')) pad.analog[3] = 0xFF; // Y

            if (is_down('E') || is_down('2')) pad.analog[4] = 0xFF; // Black / RB
            if (is_down('Q') || is_down('1')) pad.analog[5] = 0xFF; // White / LB
            if (is_down(VK_SHIFT) || is_down(VK_LSHIFT)) pad.analog[6] = 0xFF; // LT
            if (is_down(VK_RSHIFT) || is_down(VK_CONTROL)) pad.analog[7] = 0xFF; // RT
            return;
        } else if (port == 1) {
            // Puerto 1 (Jugador 2): Si hay un segundo mando conectado en SDL (slot 1)
            pad = input::RawPoll(1);
            pad.connected = true; // Forzar para permitir retar
            return;
        } else if (port >= 2 && port <= 3) {
            pad = input::RawPoll(port);
            return;
        }
    } else {
        // MODO MANDO = JUGADOR 2 (Retar: teclado J1, mando J2)
        if (port >= 1 && port <= 3) {
            pad = input::RawPoll(port - 1);
            pad.connected = true;
            return;
        }
        if (port != 0) return;

        // Puerto 0: solo teclado para Jugador 1
        pad.connected = true;
        pad.buttons = 0;
        std::memset(pad.analog, 0, sizeof(pad.analog));
        pad.lx = pad.ly = pad.rx = pad.ry = 0;

        if (is_down(VK_UP) || is_down('W')) { pad.buttons |= 0x0001; pad.ly = 32767; }
        if (is_down(VK_DOWN) || is_down('S')) { pad.buttons |= 0x0002; pad.ly = -32767; }
        if (is_down(VK_LEFT) || is_down('A')) { pad.buttons |= 0x0004; pad.lx = -32767; }
        if (is_down(VK_RIGHT) || is_down('D')) { pad.buttons |= 0x0008; pad.lx = 32767; }

        if (is_down(VK_RETURN) || is_down(VK_SPACE)) pad.buttons |= 0x0010;
        if (is_down(VK_ESCAPE) || is_down(VK_BACK)) pad.buttons |= 0x0020;

        if (is_down('J') || is_down('Z')) pad.analog[0] = 0xFF; // A
        if (is_down('K') || is_down('X')) pad.analog[1] = 0xFF; // B
        if (is_down('U') || is_down('C')) pad.analog[2] = 0xFF; // X
        if (is_down('I') || is_down('V')) pad.analog[3] = 0xFF; // Y

        if (is_down('E') || is_down('2')) pad.analog[4] = 0xFF; // Black / RB
        if (is_down('Q') || is_down('1')) pad.analog[5] = 0xFF; // White / LB
        if (is_down(VK_SHIFT) || is_down(VK_LSHIFT)) pad.analog[6] = 0xFF; // LT
        if (is_down(VK_RSHIFT) || is_down(VK_CONTROL)) pad.analog[7] = 0xFF; // RT
        return;
    }
}

void OnRumble(int port, uint16_t& left, uint16_t& right) {
    if (port < 0 || port > 3) return;
    static thread_local bool inner = false;
    if (inner) return;
    inner = true;
    int ctrl_player = app::Get().controller_player;
    if (ctrl_player == 1) {
        if (port == 0) {
            input::Rumble(0, left, right);
        } else if (port == 1) {
            input::Rumble(1, left, right);
        }
    } else {
        if (port >= 1 && port <= 3) {
            input::Rumble(port - 1, left, right);
        }
    }
    inner = false;
}

void OnLoaded() {
    // 128 MB de memoria guest
    mem::Commit(0x00010000, 0x08000000);
    // Espacio de registros de hardware MMIO de APU
    mem::Commit(0xFE800000, 0x00800000);
    // Estado del Voice Processor de APU como listo
    kernel::W32(0xFE820010, 0x7FFFFFFC);
    LOGI("game: OnLoaded listo - memoria guest y APU inicializados");
}

}  // namespace

// Información que el runtime solicita del juego
const app::GameInfo app::kGame = {
    .title = L"The King of Fighters Neowave",       // ventana y splash
    .short_title = L"KOF Neowave",                  // cuadros de mensaje
    .name = L"KOFNeowave",                          // clases de ventana, crash dumps
    .log = "kofneowave",                            // prefijo de archivo de log
    .settings = "settings.toml",                    // archivo de configuración junto al exe
    .splash = L"splash",                            // splash.png o splash.bmp opcional
    .missing = L"Coloca los archivos del juego en la carpeta assets: su imagen .iso o default.xbe con sus carpetas.",
    .images = {{"default.xbe", "KOFNeowave"}},      // xbe recompilado y su ejecutable
    .options = {},
    .load = nullptr,
    .save = nullptr,
    .init = nullptr,
    .loaded = OnLoaded,
    .window = nullptr,
    .message = OnMessage,
    .ui = [](double fps) { cfgmenu::Draw(fps); },
    .pad = OnPad,
    .rumble = OnRumble,
};
