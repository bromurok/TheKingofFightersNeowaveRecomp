#include <cstdint>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "cpu/x86_ops.h"
#include "core/log.h"

// D3DX Image Loader Guards:
// D3DX itera a través de los formatos de imagen llamando al decodificador de cada uno en orden.
// libjpeg (sub_003510ED) y libpng (sub_00351A21) usan _setjmp3 / longjmp para el manejo de errores.
// Cuando se abre un archivo que no es JPEG ni PNG (como cursor.tga u otras texturas TGA/DDS),
// libjpeg invoca longjmp para saltar de regreso en la pila, lo cual genera un salto indirecto fuera de función
// (a 0x00351133) que falla en código recompilado estáticamente.
//
// Al validar el número mágico del archivo antes de pasarlo al decodificador de JPEG y PNG,
// evitamos que archivos no-JPEG/no-PNG disparen longjmp, permitiendo que D3DX pase limpiamente
// al cargador correcto (TGA, DDS, BMP).

// Guardia para cargador JPEG (sub_003510ED)
X86_EXTERN_FUNC(__imp__sub_003510ED);
X86_FUNC(sub_003510ED) {
    uint32_t pData = X86_MEM32(ctx.esp + 4);
    uint32_t len   = X86_MEM32(ctx.esp + 8);
    // JPEG mágico: FF D8
    if (!pData || len < 4 || X86_MEM8(pData) != 0xFF || X86_MEM8(pData + 1) != 0xD8) {
        ctx.eax = 0x80004005; // E_FAIL
        ctx.esp += 12;        // ret 8 (retorna limpiamente con E_FAIL al despachador de D3DX)
        return;
    }
    __imp__sub_003510ED(ctx, base);
}

// Guardia para cargador PNG (sub_00351A21)
X86_EXTERN_FUNC(__imp__sub_00351A21);
X86_FUNC(sub_00351A21) {
    uint32_t pData = X86_MEM32(ctx.esp + 4);
    uint32_t len   = X86_MEM32(ctx.esp + 8);
    // PNG mágico: \x89PNG (0x474E5089)
    if (!pData || len < 8 || X86_MEM32(pData) != 0x474E5089) {
        ctx.eax = 0x80004005; // E_FAIL
        ctx.esp += 12;        // ret 8 (retorna limpiamente con E_FAIL al despachador de D3DX)
        return;
    }
    __imp__sub_00351A21(ctx, base);
}

// Pad / Input Manager Guards:
// En Neowave, el bucle de actualización de mandos (puertos 0-3) pasa el puntero del gestor
// de mandos en [ebp+0x22824]. Durante el arranque o cuando no hay un gestor activo, ese puntero
// es NULL (0x00000000). Las funciones de esta tabla suman desplazamientos a la base nula
// (0xD0, 0x1E0, 0x1A8, etc.) provocando excepciones de acceso C0000005.

// sub_0032C308: Actualizador de estado de mando interno
X86_EXTERN_FUNC(__imp__sub_0032C308);
X86_FUNC(sub_0032C308) {
    if (!ctx.ecx || ctx.ecx < 0x10000) {
        ctx.eax = 0;
        ctx.esp += 12; // ret 8 (pops return address 4 bytes + 8 bytes args)
        return;
    }
    __imp__sub_0032C308(ctx, base);
}

// sub_00322FE6: Enrutador de actualización de mandos
X86_EXTERN_FUNC(__imp__sub_00322FE6);
X86_FUNC(sub_00322FE6) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0;
        ctx.esp += 16; // ret 12 (pops return address 4 bytes + 12 bytes args)
        return;
    }
    __imp__sub_00322FE6(ctx, base);
}

// sub_003222D4: Consulta de gestor de mandos
X86_EXTERN_FUNC(__imp__sub_003222D4);
X86_FUNC(sub_003222D4) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0x80004005; // E_FAIL
        ctx.esp += 16;        // ret 12
        return;
    }
    __imp__sub_003222D4(ctx, base);
}

// sub_00322FF0: Verificación de estado de gestor
X86_EXTERN_FUNC(__imp__sub_00322FF0);
X86_FUNC(sub_00322FF0) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0;
        ctx.esp += 8; // ret 4
        return;
    }
    __imp__sub_00322FF0(ctx, base);
}

// sub_00323884: Bombeo de eventos / sondeo de gestor de entrada (lee [esi+0x1E0])
X86_EXTERN_FUNC(__imp__sub_00323884);
X86_FUNC(sub_00323884) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    uint32_t pOut     = X86_MEM32(ctx.esp + 8);
    if (!pManager || pManager < 0x10000) {
        if (pOut && pOut >= 0x10000) {
            X86_MEM32(pOut) = 0;
        }
        ctx.eax = 0;
        ctx.esp += 12; // ret 8
        return;
    }
    __imp__sub_00323884(ctx, base);
}

// sub_00323889: Consulta de dispositivo (lee [esi+0x220])
X86_EXTERN_FUNC(__imp__sub_00323889);
X86_FUNC(sub_00323889) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0x80070057; // E_INVALIDARG
        ctx.esp += 12;        // ret 8
        return;
    }
    __imp__sub_00323889(ctx, base);
}

// sub_003238B4: Estado del gestor de entrada
X86_EXTERN_FUNC(__imp__sub_003238B4);
X86_FUNC(sub_003238B4) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0;
        ctx.esp += 8; // ret 4
        return;
    }
    __imp__sub_003238B4(ctx, base);
}

// sub_00323364: Helper de estado interno (lee [esi+0x1A8])
X86_EXTERN_FUNC(__imp__sub_00323364);
X86_FUNC(sub_00323364) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0;
        ctx.esp += 8; // ret 4
        return;
    }
    __imp__sub_00323364(ctx, base);
}

// sub_003238CB: Despachador de acción de mandos
X86_EXTERN_FUNC(__imp__sub_003238CB);
X86_FUNC(sub_003238CB) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0;
        ctx.esp += 16; // ret 12
        return;
    }
    __imp__sub_003238CB(ctx, base);
}

// sub_003238E9: Obtención de propiedades de gestor (lee [eax+0x04])
X86_EXTERN_FUNC(__imp__sub_003238E9);
X86_FUNC(sub_003238E9) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    uint32_t pOut     = X86_MEM32(ctx.esp + 8);
    if (!pManager || pManager < 0x10000) {
        if (pOut && pOut >= 0x10000) {
            X86_MEM32(pOut) = 0;
        }
        ctx.eax = 0;
        ctx.esp += 12; // ret 8
        return;
    }
    __imp__sub_003238E9(ctx, base);
}

// Network / Live Session Manager Guards:
// Cuando se navega por menús o se confirman opciones en modo offline/standalone,
// el puntero del gestor de red [esi+0x22824] permanece en NULL.
// Las funciones de esta tabla (sub_0032469F, sub_00322FE1, etc.) asumen que el gestor existe
// y desreferencian [esi + eax*4 + 0x194], [esi + 0x198], etc. provocando C0000005.

// sub_0032469F: Enrutador de eventos de sesión / red (salta a 0x324468)
X86_EXTERN_FUNC(__imp__sub_0032469F);
X86_FUNC(sub_0032469F) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0x80070490; // ERROR_NOT_FOUND
        ctx.esp += 16;        // ret 12
        return;
    }
    __imp__sub_0032469F(ctx, base);
}

// sub_00322FE1: Consulta de estado de sesión de red (salta a 0x322195)
X86_EXTERN_FUNC(__imp__sub_00322FE1);
X86_FUNC(sub_00322FE1) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 1;
        ctx.esp += 12; // ret 8
        return;
    }
    __imp__sub_00322FE1(ctx, base);
}

// sub_00323004: Estado de sesión de red (salta a 0x322299)
X86_EXTERN_FUNC(__imp__sub_00323004);
X86_FUNC(sub_00323004) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0x80004005; // E_FAIL
        ctx.esp += 8;         // ret 4
        return;
    }
    __imp__sub_00323004(ctx, base);
}

// sub_00322FFF: Sincronización de sesión de red (salta a 0x32236C)
X86_EXTERN_FUNC(__imp__sub_00322FFF);
X86_FUNC(sub_00322FFF) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0x80004005; // E_FAIL
        ctx.esp += 12;        // ret 8
        return;
    }
    __imp__sub_00322FFF(ctx, base);
}

// sub_00323893: Consulta de configuración de sesión de red (salta a 0x3233D6)
X86_EXTERN_FUNC(__imp__sub_00323893);
X86_FUNC(sub_00323893) {
    uint32_t pManager = X86_MEM32(ctx.esp + 4);
    if (!pManager || pManager < 0x10000) {
        ctx.eax = 0;
        ctx.esp += 32; // ret 28
        return;
    }
    __imp__sub_00323893(ctx, base);
}



// ========================================================================
// Intercepción y expansión del menú nativo "Game option" (9 opciones)
// ========================================================================

#include "common/config_menu.h"

X86_EXTERN_FUNC(__imp__sub_0008FBD0);
X86_EXTERN_FUNC(__imp__sub_0008FC30);
X86_EXTERN_FUNC(__imp__sub_0008FDB0);
X86_EXTERN_FUNC(__imp__sub_0008F120);
X86_EXTERN_FUNC(__imp__sub_00091F40);
X86_EXTERN_FUNC(__imp__sub_00092020);
X86_EXTERN_FUNC(__imp__sub_00092260);
X86_EXTERN_FUNC(sub_000878F0);
X86_EXTERN_FUNC(sub_0008EC40);
X86_EXTERN_FUNC(sub_0008B120);
X86_EXTERN_FUNC(sub_0008B2A0);
X86_EXTERN_FUNC(sub_0008B320);

static uint8_t* s_game_base = nullptr;
static int s_opt_row = 0; // 0..8
static bool s_inited_sound_cb = false;

static void NativePlaySfx(uint16_t id) {
    if (!s_game_base) return;
    X86Context sfx{};
    sfx.esp = 0x03FF0000;
    sfx.esp -= 4;
    *(uint32_t*)(s_game_base + sfx.esp) = id;
    sfx.esp -= 4;
    *(uint32_t*)(s_game_base + sfx.esp) = 0x8F000u;
    sub_0008B2A0(sfx, s_game_base);
}

// Bucle de Main Menu (sub_0008F120)
X86_FUNC(sub_0008F120) {
    if (cfgmenu::IsInGameOption()) {
        cfgmenu::SetInGameOption(false);
    }
    __imp__sub_0008F120(ctx, base);
}

// Salida de Game option (sub_0008FDB0)
X86_FUNC(sub_0008FDB0) {
    cfgmenu::SetInGameOption(false);
    __imp__sub_0008FDB0(ctx, base);
}

// Inicialización de Game option (sub_0008FBD0)
X86_FUNC(sub_0008FBD0) {
    s_opt_row = 0;
    cfgmenu::SetCursorRow(0);
    cfgmenu::SetInGameOption(true);
    __imp__sub_0008FBD0(ctx, base);
}

// Bucle de Game option (sub_0008FC30)
X86_FUNC(sub_0008FC30) {
    s_game_base = base;
    if (!s_inited_sound_cb) {
        s_inited_sound_cb = true;
        cfgmenu::RegisterSoundCallback(NativePlaySfx);
    }

    // Si la pantalla de ajustes de PC está abierta, congelamos el tick del menú de fondo
    if (cfgmenu::IsNativePCSettingsOpen()) {
        ctx.esp += 4;
        return;
    }

    // Estamos activamente dentro de Game option
    cfgmenu::SetInGameOption(true);

    uint32_t pManager = X86_MEM32(0x00ADDC00);
    if (!pManager || pManager < 0x10000) {
        __imp__sub_0008FC30(ctx, base);
        return;
    }

    // Leemos los botones del mando llamando a sub_000878F0(2, 2, 0)
    X86Context btn_ctx = ctx;
    btn_ctx.esp -= 4; X86_MEM32(btn_ctx.esp) = 0x00;
    btn_ctx.esp -= 4; X86_MEM32(btn_ctx.esp) = 0x02;
    btn_ctx.esp -= 4; X86_MEM32(btn_ctx.esp) = 0x02;
    btn_ctx.esp -= 4; X86_MEM32(btn_ctx.esp) = 0x8FC51u;
    sub_000878F0(btn_ctx, base);
    uint16_t buttons = (uint16_t)btn_ctx.eax;

    // Actualizamos el cursor con sub_0008EC40 pasando max_items = 8 (menú 100% nativo)
    X86Context nav_ctx = ctx;
    nav_ctx.esp -= 4; X86_MEM32(nav_ctx.esp) = buttons;
    nav_ctx.esp -= 4; X86_MEM32(nav_ctx.esp) = (uint32_t)s_opt_row;
    nav_ctx.esp -= 4; X86_MEM32(nav_ctx.esp) = 0x08; // 8 OPCIONES NATIVAS
    nav_ctx.esp -= 4; X86_MEM32(nav_ctx.esp) = 0x8FC68u;
    sub_0008EC40(nav_ctx, base);

    s_opt_row = (int)(uint8_t)nav_ctx.eax;
    if (s_opt_row < 0 || s_opt_row > 7) s_opt_row = 0;
    cfgmenu::SetCursorRow(s_opt_row);

    // Mapeo nativo en la estructura del juego
    X86_MEM16(pManager + 0xD4) = (uint16_t)s_opt_row;

    // Botón A (0x4000)
    if (buttons & 0x4000) {
        if (s_opt_row == 7) {
            // Fila 7: EXIT nativo (volver al Main Menu)
            X86Context sfx = ctx;
            sfx.esp -= 4; X86_MEM32(sfx.esp) = 0x5Fu;
            sfx.esp -= 4; X86_MEM32(sfx.esp) = 0x8F000u;
            sub_0008B120(sfx, base);

            X86Context trans = ctx;
            trans.esp -= 4; X86_MEM32(trans.esp) = 0x8FDB0u; // Retorno a Main Menu
            trans.esp -= 4; X86_MEM32(trans.esp) = 0x8E6C0u;
            trans.esp -= 4; X86_MEM32(trans.esp) = 0x8FDA3u;
            sub_0008B320(trans, base);

            cfgmenu::SetInGameOption(false);
            ctx.esp += 4;
            return;
        } else {
            // Filas 0..6 (incluyendo 3 = GRAPHIC SETTING): Abre el submenú 100% nativo original del juego
            X86_MEM16(pManager + 0xD4) = (uint16_t)s_opt_row;
            __imp__sub_0008FC30(ctx, base);
            cfgmenu::SetInGameOption(false);
            return;
        }
    }

    // Botón B (0x2000): Salir de Game option al Main Menu
    if (buttons & 0x2000) {
        X86Context sfx = ctx;
        sfx.esp -= 4; X86_MEM32(sfx.esp) = 0x5Fu;
        sfx.esp -= 4; X86_MEM32(sfx.esp) = 0x8F000u;
        sub_0008B120(sfx, base);

        X86Context trans = ctx;
        trans.esp -= 4; X86_MEM32(trans.esp) = 0x8FDB0u;
        trans.esp -= 4; X86_MEM32(trans.esp) = 0x8E6C0u;
        trans.esp -= 4; X86_MEM32(trans.esp) = 0x8FDA3u;
        sub_0008B320(trans, base);

        cfgmenu::SetInGameOption(false);
        ctx.esp += 4;
        return;
    }

    ctx.esp += 4;
}

// ========================================================================
// Pantalla nativa "Graphic Setting" (sub_00091F40, sub_00092020, sub_00092260)
// ========================================================================

// Inicialización de Graphic Setting
X86_FUNC(sub_00091F40) {
    cfgmenu::SetInGraphicSetting(true);
    cfgmenu::CloseNativePCSettings();
    __imp__sub_00091F40(ctx, base);
}

// Bucle / Tick de Graphic Setting
X86_FUNC(sub_00092020) {
    s_game_base = base;
    cfgmenu::SetInGraphicSetting(true);

    // Si el menú de PC está abierto, consumimos el tick para no mover el menú de fondo
    if (cfgmenu::IsNativePCSettingsOpen()) {
        ctx.esp += 4;
        return;
    }

    // Si no está abierto, el menú nativo Graphic Setting del juego funciona 100% normal
    __imp__sub_00092020(ctx, base);
}

// Salida de Graphic Setting hacia Game option
X86_FUNC(sub_00092260) {
    cfgmenu::SetInGraphicSetting(false);
    cfgmenu::CloseNativePCSettings();
    __imp__sub_00092260(ctx, base);
}
