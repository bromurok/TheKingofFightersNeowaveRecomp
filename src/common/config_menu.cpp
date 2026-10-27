#include "config_menu.h"
#include "app/app.h"
#include "core/input.h"
#include <imgui.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace cfgmenu {
namespace {

std::atomic<bool> s_open = false;             // Ventana flotante de depuración (F1)
std::atomic<bool> s_pc_settings_open = false; // Pantalla nativa clonada "PC SETTINGS"
std::atomic<int>  s_cursor_row = 0;          // Fila del cursor en Game option (0..8)
std::atomic<int>  s_game_option_watchdog = 0; // Watchdog de presencia en Game option
std::atomic<int>  s_gfx_setting_watchdog = 0;  // Watchdog de presencia en Graphic Setting
float s_transition_alpha = 0.0f;              // Opacidad de la transición suave (0.0f a 1.0f)
auto s_prev_frame_time = std::chrono::steady_clock::now();

static void (*s_play_sfx_cb)(uint16_t id) = nullptr;

struct Mode {
    uint32_t w, h;
    bool declared;
};
std::vector<Mode> s_modes;

app::Mode s_desk{0, 0};
bool s_desk_ok = false;

void RefreshDesk() {
    s_desk = app::Desktop();
    s_desk_ok = true;
}

void BuildModes() {
    if (!s_desk_ok) RefreshDesk();
    if (!s_modes.empty()) return;
    auto add = [](uint32_t w, uint32_t h, bool declared) {
        if (w < 640 || h < 480) return;
        for (Mode& m : s_modes) {
            if (m.w == w && m.h == h) {
                m.declared |= declared;
                return;
            }
        }
        s_modes.push_back({w, h, declared});
    };
    if (s_desk.width >= 640 && s_desk.height >= 480) {
        add(s_desk.width, s_desk.height, true);
    }
#ifdef _WIN32
    HWND hwnd = (HWND)app::Window();
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    for (DWORD i = 0;; i++) {
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsW(mi.szDevice, i, &dm)) break;
        uint32_t w = dm.dmPelsWidth, h = dm.dmPelsHeight;
        if (w < 640 || h < 480 || w > 7680 || h > 4320) continue;
        add(w, h, true);
    }
#endif
    for (Mode m : {Mode{1440, 900, false}, Mode{1680, 1050, false}, Mode{1920, 1200, false},
                   Mode{640, 480, false}, Mode{800, 600, false}, Mode{1024, 768, false},
                   Mode{1280, 720, false}, Mode{1280, 800, false}, Mode{1280, 960, false},
                   Mode{1280, 1024, false}, Mode{1366, 768, false}, Mode{1600, 900, false},
                   Mode{1600, 1200, false}, Mode{1920, 1080, false}, Mode{2560, 1440, false},
                   Mode{3840, 2160, false}}) {
        add(m.w, m.h, false);
    }
    std::sort(s_modes.begin(), s_modes.end(), [](const Mode& a, const Mode& b) {
        return a.h != b.h ? a.h > b.h : a.w > b.w;
    });
}

const char* AspectTag(uint32_t w, uint32_t h) {
    (void)w; (void)h;
    return "";
}

// Variables de estado de navegación para PC Settings nativo
int s_pc_sel_option = 0; // 0..6
auto s_last_nav_time = std::chrono::steady_clock::time_point{};
auto s_save_toast_time = std::chrono::steady_clock::time_point{};
bool s_show_saved_toast = false;

// Estado de borrador (draft): nada se aplica ni guarda sin presionar ENTER primero
struct DraftSettings {
    app::DisplayMode display_mode = app::DisplayMode::Borderless;
    uint32_t width = 0;
    uint32_t height = 0;
    std::string aspect_ratio = "16:9";
    uint32_t render_scale = 1;
    int controller_player = 1;
    bool anti_aliasing = true;
    bool crop_black_bars = true;
    std::string menu_language = "es";
};
static DraftSettings s_draft;

// Modo de prueba de resolución (3 segundos para revertir automáticamente si no se confirma)
static bool s_trial_active = false;
static std::chrono::steady_clock::time_point s_trial_deadline{};
static uint32_t s_trial_prev_w = 0;
static uint32_t s_trial_prev_h = 0;
static app::DisplayMode s_trial_prev_mode = app::DisplayMode::Borderless;

// Modal de lista de resoluciones
static bool s_res_list_open = false;
static int s_res_list_sel = 0;
static int s_res_list_scroll = 0;

static void SyncDraftFromApp() {
    RefreshDesk();
    BuildModes();
    auto& st = app::Get();
    s_draft.display_mode = st.display_mode;
    if (st.width && st.height) {
        s_draft.width = st.width;
        s_draft.height = st.height;
    } else {
        s_draft.width = s_desk.width;
        s_draft.height = s_desk.height;
    }
    if (st.aspect_ratio == "4:3") {
        s_draft.aspect_ratio = "4:3";
    } else if (st.aspect_ratio == "16:9") {
        s_draft.aspect_ratio = "16:9";
    } else {
        s_draft.aspect_ratio = (s_draft.width * 2 >= s_draft.height * 3) ? "16:9" : "4:3";
    }
    s_draft.render_scale = st.render_scale;
    if (s_draft.render_scale != 1 && s_draft.render_scale != 2 &&
        s_draft.render_scale != 4 && s_draft.render_scale != 8) {
        s_draft.render_scale = (s_draft.render_scale >= 6) ? 8 : (s_draft.render_scale >= 3) ? 4 : (s_draft.render_scale >= 2) ? 2 : 1;
    }
    s_draft.controller_player = (st.controller_player == 2) ? 2 : 1;
    s_draft.anti_aliasing = st.anti_aliasing;
    s_draft.crop_black_bars = st.crop_black_bars;
    s_draft.menu_language = st.menu_language;
}

static void RevertTrial() {
    if (!s_trial_active) return;
    s_trial_active = false;
    auto& st = app::Get();
    st.width = s_trial_prev_w;
    st.height = s_trial_prev_h;
    st.display_mode = s_trial_prev_mode;
    s_draft.width = s_trial_prev_w;
    s_draft.height = s_trial_prev_h;
    s_draft.display_mode = s_trial_prev_mode;
    app::ApplyDisplay();
}

static void ConfirmTrial() {
    if (!s_trial_active) return;
    s_trial_active = false;
    app::Save();
    s_show_saved_toast = true;
    s_save_toast_time = std::chrono::steady_clock::now();
}

static void StartResolutionTrial(uint32_t new_w, uint32_t new_h) {
    auto& st = app::Get();
    s_trial_prev_w = st.width;
    s_trial_prev_h = st.height;
    s_trial_prev_mode = st.display_mode;

    st.width = new_w;
    st.height = new_h;
    s_draft.width = new_w;
    s_draft.height = new_h;
    app::ApplyDisplay();

    s_trial_active = true;
    s_trial_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
}

// ----------------------------------------------------------------------------
// Render del 9º elemento "PC SETTINGS" (estilo OpenShao)
// ----------------------------------------------------------------------------
void RenderExtraMenuOption() {
    // No se dibuja nada para no obstruir el menú original
}

// ----------------------------------------------------------------------------
// Render de la pantalla PC SETTINGS
// ----------------------------------------------------------------------------
void RenderNativePCSettings() {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();

    float vw = io.DisplaySize.x;
    float vh = io.DisplaySize.y;
    if (vw <= 0 || vh <= 0) return;

    float s = std::min(vw / 640.0f, vh / 480.0f);
    float ox = (vw - 640.0f * s) * 0.5f;
    float oy = (vh - 480.0f * s) * 0.5f;

    auto now = std::chrono::steady_clock::now();

    // Verificación de cuenta regresiva de reversión en 3 segundos
    int trial_rem_sec = 0;
    if (s_trial_active) {
        if (now >= s_trial_deadline) {
            RevertTrial();
        } else {
            auto rem_ms = std::chrono::duration_cast<std::chrono::milliseconds>(s_trial_deadline - now).count();
            trial_rem_sec = (int)((rem_ms + 999) / 1000);
            if (trial_rem_sec < 1) trial_rem_sec = 1;
        }
    }

    // Atenuar el fondo del juego
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(vw, vh), IM_COL32(0, 0, 0, 215));

    // Panel metálico verde principal estilo Neowave
    ImVec2 box_min(ox + 35.0f * s, oy + 25.0f * s);
    ImVec2 box_max(ox + 605.0f * s, oy + 455.0f * s);

    // Fondo verde carbón texturizado
    dl->AddRectFilled(box_min, box_max, IM_COL32(8, 22, 12, 255), 10.0f * s);
    // Borde brillante verde neón
    dl->AddRect(box_min, box_max, IM_COL32(40, 220, 85, 230), 10.0f * s, 0, 2.0f * s);
    dl->AddRect(ImVec2(box_min.x + 4.0f * s, box_min.y + 4.0f * s),
                ImVec2(box_max.x - 4.0f * s, box_max.y - 4.0f * s),
                IM_COL32(20, 90, 40, 140), 8.0f * s, 0, 1.0f * s);

    ImFont* font = ImGui::GetFont();

    // Marquesina / Placa superior con título "PC SETTINGS"
    ImVec2 hdr_min(ox + 160.0f * s, oy + 36.0f * s);
    ImVec2 hdr_max(ox + 480.0f * s, oy + 76.0f * s);
    dl->AddRectFilled(hdr_min, hdr_max, IM_COL32(16, 44, 22, 255), 6.0f * s);
    dl->AddRect(hdr_min, hdr_max, IM_COL32(80, 225, 110, 240), 6.0f * s, 0, 1.5f * s);

    const char* kTitle = "PC SETTINGS";
    float title_fsize = 22.0f * s;
    ImVec2 t_sz = font->CalcTextSizeA(title_fsize, FLT_MAX, 0.0f, kTitle);
    float t_x = (hdr_min.x + hdr_max.x) * 0.5f - t_sz.x * 0.5f;
    float t_y = (hdr_min.y + hdr_max.y) * 0.5f - t_sz.y * 0.5f;
    dl->AddText(font, title_fsize, ImVec2(t_x + 2.0f * s, t_y + 2.0f * s), IM_COL32(0, 0, 0, 230), kTitle);
    dl->AddText(font, title_fsize, ImVec2(t_x, t_y), IM_COL32(255, 255, 255, 255), kTitle);

    auto& st = app::Get();
    bool is_es = (s_draft.menu_language != "en");

    // Etiquetas y valores de las 8 opciones (leídas de s_draft)
    struct OptionDesc {
        const char* label;
        char val[64];
        bool modified;
    } opts[8];

    // 0: Modo de pantalla
    static const char* kModeES[3] = {"PANTALLA COMPLETA", "SIN BORDES (RECOMENDADO)", "VENTANA"};
    static const char* kModeEN[3] = {"FULLSCREEN", "BORDERLESS (RECOMMENDED)", "WINDOWED"};
    int draft_mode = int(s_draft.display_mode);
    if (draft_mode < 0 || draft_mode > 2) draft_mode = 1;
    opts[0].label = is_es ? "MODO DE PANTALLA" : "DISPLAY MODE";
    snprintf(opts[0].val, sizeof(opts[0].val), "< %s >", is_es ? kModeES[draft_mode] : kModeEN[draft_mode]);
    opts[0].modified = (s_draft.display_mode != st.display_mode);

    // 1: Proporción de aspecto (16:9 / 4:3)
    opts[1].label = is_es ? "PROPORCION" : "ASPECT RATIO";
    snprintf(opts[1].val, sizeof(opts[1].val), "< %s >", s_draft.aspect_ratio.c_str());
    opts[1].modified = (s_draft.aspect_ratio != st.aspect_ratio);

    // 2: Resolución (solo números limpios, con indicación de NATIVA si coincide con el monitor)
    opts[2].label = is_es ? "RESOLUCION" : "RESOLUTION";
    bool is_native = (s_draft.width == s_desk.width && s_draft.height == s_desk.height);
    if (s_draft.width && s_draft.height) {
        if (is_native) {
            snprintf(opts[2].val, sizeof(opts[2].val), "< %ux%u (NATIVA) >", s_draft.width, s_draft.height);
        } else {
            snprintf(opts[2].val, sizeof(opts[2].val), "< %ux%u >", s_draft.width, s_draft.height);
        }
    } else {
        snprintf(opts[2].val, sizeof(opts[2].val), "< %ux%u (NATIVA) >", s_desk.width, s_desk.height);
    }
    opts[2].modified = (s_draft.width != st.width || s_draft.height != st.height);

    // 3: Calidad (Render Scale) - SOLO x1, x2, x4, x8 sin HD ni nada más
    opts[3].label = is_es ? "CALIDAD" : "QUALITY";
    snprintf(opts[3].val, sizeof(opts[3].val), "< x%u >", s_draft.render_scale);
    opts[3].modified = (s_draft.render_scale != st.render_scale);

    // 4: Asignación del mando (Jugador 1 o Jugador 2)
    opts[4].label = is_es ? "MANDO" : "CONTROLLER";
    snprintf(opts[4].val, sizeof(opts[4].val), "< %s >",
             (s_draft.controller_player == 2) ? (is_es ? "JUGADOR 2" : "PLAYER 2")
                                              : (is_es ? "JUGADOR 1" : "PLAYER 1"));
    opts[4].modified = (s_draft.controller_player != st.controller_player);

    // 5: Suavizado de bordes (Anti-Aliasing)
    opts[5].label = is_es ? "ANTI-ALIASING" : "ANTI-ALIASING";
    snprintf(opts[5].val, sizeof(opts[5].val), "< %s >", s_draft.anti_aliasing ? (is_es ? "ACTIVADO" : "ENABLED")
                                                                              : (is_es ? "DESACTIVADO" : "DISABLED"));
    opts[5].modified = (s_draft.anti_aliasing != st.anti_aliasing);

    // 6: Recortar bordes negros
    opts[6].label = is_es ? "BORDES NEGROS" : "BLACK BARS";
    snprintf(opts[6].val, sizeof(opts[6].val), "< %s >", s_draft.crop_black_bars ? (is_es ? "LLENAR PANTALLA" : "FILL SCREEN")
                                                                                : (is_es ? "ORIGINAL" : "ORIGINAL"));
    opts[6].modified = (s_draft.crop_black_bars != st.crop_black_bars);

    // 7: Idioma
    opts[7].label = is_es ? "IDIOMA" : "LANGUAGE";
    snprintf(opts[7].val, sizeof(opts[7].val), "< %s >", (s_draft.menu_language == "es") ? "ESPANOL" : "ENGLISH");
    opts[7].modified = (s_draft.menu_language != st.menu_language);

    float opt_start_y = oy + 98.0f * s;
    float opt_step_y  = 38.0f * s;
    float row_fsize   = 16.0f * s;

    for (int i = 0; i < 8; ++i) {
        float row_y = opt_start_y + i * opt_step_y;

        if (i == s_pc_sel_option) {
            // Cápsula de selección azul activa
            ImVec2 cap_min(ox + 55.0f * s, row_y - 16.0f * s);
            ImVec2 cap_max(ox + 585.0f * s, row_y + 16.0f * s);

            dl->AddRectFilled(cap_min, cap_max, IM_COL32(26, 62, 105, 235), 16.0f * s);
            dl->AddLine(ImVec2(cap_min.x + 20.0f * s, cap_min.y + 2.5f * s),
                        ImVec2(cap_max.x - 20.0f * s, cap_min.y + 2.5f * s),
                        IM_COL32(145, 210, 255, 180), 1.5f * s);
            dl->AddRect(cap_min, cap_max, IM_COL32(150, 225, 255, 255), 16.0f * s, 0, 2.0f * s);

            // Texto de opción: Blanco puro con sombra
            dl->AddText(font, row_fsize, ImVec2(ox + 75.0f * s + 1.5f * s, row_y - 9.0f * s + 1.5f * s),
                        IM_COL32(0, 0, 0, 230), opts[i].label);
            dl->AddText(font, row_fsize, ImVec2(ox + 75.0f * s, row_y - 9.0f * s),
                        IM_COL32(255, 255, 255, 255), opts[i].label);

            // Valor: Amarillo dorado si está pendiente de ENTER, dorado claro si no
            ImU32 val_col = opts[i].modified ? IM_COL32(255, 180, 50, 255) : IM_COL32(245, 230, 60, 255);
            ImVec2 v_sz = font->CalcTextSizeA(row_fsize, FLT_MAX, 0.0f, opts[i].val);
            float v_x = ox + 565.0f * s - v_sz.x;
            dl->AddText(font, row_fsize, ImVec2(v_x + 1.5f * s, row_y - 9.0f * s + 1.5f * s),
                        IM_COL32(0, 0, 0, 240), opts[i].val);
            dl->AddText(font, row_fsize, ImVec2(v_x, row_y - 9.0f * s), val_col, opts[i].val);
        } else {
            // Opción inactiva: Plata para etiqueta
            dl->AddText(font, row_fsize, ImVec2(ox + 75.0f * s + 1.5f * s, row_y - 9.0f * s + 1.5f * s),
                        IM_COL32(0, 0, 0, 230), opts[i].label);
            dl->AddText(font, row_fsize, ImVec2(ox + 75.0f * s, row_y - 9.0f * s),
                        IM_COL32(195, 215, 200, 255), opts[i].label);

            // Valor: Naranja si está modificado pero inactivo, verde suave si normal
            ImU32 val_col = opts[i].modified ? IM_COL32(255, 175, 70, 230) : IM_COL32(75, 215, 90, 255);
            ImVec2 v_sz = font->CalcTextSizeA(row_fsize, FLT_MAX, 0.0f, opts[i].val);
            float v_x = ox + 565.0f * s - v_sz.x;
            dl->AddText(font, row_fsize, ImVec2(v_x + 1.5f * s, row_y - 9.0f * s + 1.5f * s),
                        IM_COL32(0, 0, 0, 240), opts[i].val);
            dl->AddText(font, row_fsize, ImVec2(v_x, row_y - 9.0f * s), val_col, opts[i].val);
        }
    }

    // Modal de lista de resoluciones (si está abierto)
    if (s_res_list_open) {
        BuildModes();
        int n_modes = (int)s_modes.size();

        // Ventana emergente centrada para la lista
        ImVec2 pop_min(ox + 105.0f * s, oy + 70.0f * s);
        ImVec2 pop_max(ox + 535.0f * s, oy + 420.0f * s);

        dl->AddRectFilled(ImVec2(0, 0), ImVec2(vw, vh), IM_COL32(0, 0, 0, 160));
        dl->AddRectFilled(pop_min, pop_max, IM_COL32(10, 26, 14, 250), 8.0f * s);
        dl->AddRect(pop_min, pop_max, IM_COL32(60, 240, 110, 255), 8.0f * s, 0, 2.0f * s);

        // Cabecera del modal
        ImVec2 phdr_min(pop_min.x + 8.0f * s, pop_min.y + 8.0f * s);
        ImVec2 phdr_max(pop_max.x - 8.0f * s, pop_min.y + 44.0f * s);
        dl->AddRectFilled(phdr_min, phdr_max, IM_COL32(18, 50, 26, 255), 4.0f * s);

        const char* res_title = is_es ? "LISTA DE RESOLUCIONES DISPONIBLES" : "AVAILABLE RESOLUTIONS LIST";
        ImVec2 rtz = font->CalcTextSizeA(15.0f * s, FLT_MAX, 0.0f, res_title);
        float rx = (phdr_min.x + phdr_max.x) * 0.5f - rtz.x * 0.5f;
        float ry = (phdr_min.y + phdr_max.y) * 0.5f - rtz.y * 0.5f;
        dl->AddText(font, 15.0f * s, ImVec2(rx, ry), IM_COL32(255, 255, 255, 255), res_title);

        // Lista de elementos (muestra hasta 7 resoluciones a la vez)
        const int kVisible = 7;
        if (s_res_list_sel < s_res_list_scroll) s_res_list_scroll = s_res_list_sel;
        if (s_res_list_sel >= s_res_list_scroll + kVisible) s_res_list_scroll = s_res_list_sel - kVisible + 1;

        float item_y_base = pop_min.y + 54.0f * s;
        float item_h = 36.0f * s;

        for (int vi = 0; vi < kVisible && (s_res_list_scroll + vi) < n_modes; ++vi) {
            int mi = s_res_list_scroll + vi;
            float iy = item_y_base + vi * item_h;
            bool is_sel_item = (mi == s_res_list_sel);
            bool is_native_item = (s_modes[mi].w == s_desk.width && s_modes[mi].h == s_desk.height);
            bool is_active_res = (s_modes[mi].w == st.width && s_modes[mi].h == st.height) ||
                                 (st.width == 0 && is_native_item);

            char item_buf[80];
            if (is_native_item && is_active_res) {
                snprintf(item_buf, sizeof(item_buf), "%u x %u  [%s]", s_modes[mi].w, s_modes[mi].h,
                         is_es ? "NATIVA ACTUAL" : "NATIVE CURRENT");
            } else if (is_native_item) {
                snprintf(item_buf, sizeof(item_buf), "%u x %u  [%s]", s_modes[mi].w, s_modes[mi].h,
                         is_es ? "NATIVA" : "NATIVE");
            } else if (is_active_res) {
                snprintf(item_buf, sizeof(item_buf), "%u x %u  [%s]", s_modes[mi].w, s_modes[mi].h,
                         is_es ? "ACTUAL" : "CURRENT");
            } else {
                snprintf(item_buf, sizeof(item_buf), "%u x %u", s_modes[mi].w, s_modes[mi].h);
            }

            ImVec2 row_min(pop_min.x + 14.0f * s, iy);
            ImVec2 row_max(pop_max.x - 14.0f * s, iy + item_h - 4.0f * s);

            if (is_sel_item) {
                dl->AddRectFilled(row_min, row_max, IM_COL32(26, 75, 130, 240), 6.0f * s);
                dl->AddRect(row_min, row_max, IM_COL32(130, 215, 255, 255), 6.0f * s, 0, 1.5f * s);
                dl->AddText(font, 15.0f * s, ImVec2(row_min.x + 16.0f * s, iy + 6.0f * s),
                            IM_COL32(255, 255, 255, 255), item_buf);
            } else {
                ImU32 text_col = is_native_item ? IM_COL32(90, 240, 120, 255) :
                                 is_active_res ? IM_COL32(245, 230, 60, 255) :
                                 IM_COL32(190, 215, 195, 240);
                dl->AddText(font, 14.0f * s, ImVec2(row_min.x + 16.0f * s, iy + 6.0f * s),
                            text_col, item_buf);
            }
        }

        // Pie de la ventana de resoluciones
        ImVec2 pfoot_min(pop_min.x + 8.0f * s, pop_max.y - 36.0f * s);
        ImVec2 pfoot_max(pop_max.x - 8.0f * s, pop_max.y - 8.0f * s);
        dl->AddRectFilled(pfoot_min, pfoot_max, IM_COL32(14, 36, 18, 240), 4.0f * s);
        const char* pop_help = is_es ? "[PAD / FLECHAS] ELEGIR    [ENTER / START] PROBAR (3 SEG)    [ESC / BACK] CANCELAR"
                                     : "[DPAD / ARROWS] CHOOSE    [ENTER / START] TEST (3 SEC)    [ESC / BACK] CANCEL";
        ImVec2 phsz = font->CalcTextSizeA(12.0f * s, FLT_MAX, 0.0f, pop_help);
        dl->AddText(font, 12.0f * s,
                    ImVec2((pfoot_min.x + pfoot_max.x) * 0.5f - phsz.x * 0.5f,
                           (pfoot_min.y + pfoot_max.y) * 0.5f - phsz.y * 0.5f),
                    IM_COL32(210, 240, 215, 255), pop_help);
    }

    // Banner de advertencia de prueba de 3 segundos
    if (s_trial_active) {
        ImVec2 trial_min(ox + 50.0f * s, oy + 355.0f * s);
        ImVec2 trial_max(ox + 590.0f * s, oy + 405.0f * s);

        dl->AddRectFilled(trial_min, trial_max, IM_COL32(38, 18, 6, 245), 6.0f * s);
        dl->AddRect(trial_min, trial_max, IM_COL32(255, 170, 30, 255), 6.0f * s, 0, 2.0f * s);

        char trial_msg[128];
        snprintf(trial_msg, sizeof(trial_msg),
                 is_es ? "¿MANTENER ESTA RESOLUCION? REVIERTE EN %d SEG..."
                       : "KEEP THIS RESOLUTION? REVERTING IN %d SEC...", trial_rem_sec);
        ImVec2 tm_sz = font->CalcTextSizeA(14.0f * s, FLT_MAX, 0.0f, trial_msg);
        dl->AddText(font, 14.0f * s,
                    ImVec2((trial_min.x + trial_max.x) * 0.5f - tm_sz.x * 0.5f, trial_min.y + 6.0f * s),
                    IM_COL32(255, 220, 80, 255), trial_msg);

        const char* trial_actions = is_es ? "[ENTER / START] MANTENER    [ESC / BACK / T+Y] REVERTIR AHORA"
                                          : "[ENTER / START] KEEP    [ESC / BACK / T+Y] REVERT NOW";
        ImVec2 ta_sz = font->CalcTextSizeA(13.0f * s, FLT_MAX, 0.0f, trial_actions);
        dl->AddText(font, 13.0f * s,
                    ImVec2((trial_min.x + trial_max.x) * 0.5f - ta_sz.x * 0.5f, trial_min.y + 26.0f * s),
                    IM_COL32(255, 255, 255, 255), trial_actions);
    }

    // Barra de instrucciones inferior
    ImVec2 b_min(ox + 45.0f * s, oy + 412.0f * s);
    ImVec2 b_max(ox + 595.0f * s, oy + 444.0f * s);
    dl->AddRectFilled(b_min, b_max, IM_COL32(12, 32, 16, 230), 5.0f * s);
    dl->AddRect(b_min, b_max, IM_COL32(40, 120, 50, 160), 5.0f * s, 0, 1.0f * s);

    char help_str[128];
    if (s_show_saved_toast && now - s_save_toast_time < std::chrono::seconds(2)) {
        snprintf(help_str, sizeof(help_str), "%s",
                 is_es ? "[ AJUSTES APLICADOS Y GUARDADOS ]" : "[ SETTINGS APPLIED AND SAVED ]");
        ImVec2 h_sz = font->CalcTextSizeA(14.0f * s, FLT_MAX, 0.0f, help_str);
        float h_x = (b_min.x + b_max.x) * 0.5f - h_sz.x * 0.5f;
        float h_y = (b_min.y + b_max.y) * 0.5f - h_sz.y * 0.5f;
        dl->AddText(font, 14.0f * s, ImVec2(h_x, h_y), IM_COL32(60, 255, 100, 255), help_str);
    } else if (s_trial_active) {
        snprintf(help_str, sizeof(help_str), "%s",
                 is_es ? "[ENTER / START] CONFIRMAR RESOLUCION    [ESC / BACK / T+Y] CANCELAR"
                       : "[ENTER / START] CONFIRM RESOLUTION    [ESC / BACK / T+Y] CANCEL");
        ImVec2 h_sz = font->CalcTextSizeA(13.0f * s, FLT_MAX, 0.0f, help_str);
        float h_x = (b_min.x + b_max.x) * 0.5f - h_sz.x * 0.5f;
        float h_y = (b_min.y + b_max.y) * 0.5f - h_sz.y * 0.5f;
        dl->AddText(font, 13.0f * s, ImVec2(h_x, h_y), IM_COL32(255, 210, 60, 255), help_str);
    } else {
        snprintf(help_str, sizeof(help_str), "%s",
                 is_es ? "[PAD / FLECHAS] NAVEGAR   [< >] ELEGIR   [ENTER / START] APLICAR / LISTA   [T+Y / BACK] SALIR"
                       : "[DPAD / ARROWS] NAVIGATE   [< >] CHOOSE   [ENTER / START] APPLY / LIST   [T+Y / BACK] EXIT");
        ImVec2 h_sz = font->CalcTextSizeA(12.0f * s, FLT_MAX, 0.0f, help_str);
        float h_x = (b_min.x + b_max.x) * 0.5f - h_sz.x * 0.5f;
        float h_y = (b_min.y + b_max.y) * 0.5f - h_sz.y * 0.5f;
        dl->AddText(font, 12.0f * s, ImVec2(h_x, h_y), IM_COL32(210, 230, 215, 230), help_str);
    }

    // ------------------------------------------------------------------------
    // Procesamiento de entrada con gamepad y teclado para PC Settings
    // ------------------------------------------------------------------------
    bool can_nav = (now - s_last_nav_time >= std::chrono::milliseconds(170));

    // Usar RawPoll para leer el mando físico real de SDL sin que OnPad lo anule al estar el menú abierto
    input::Pad pad = input::RawPoll(0);
    for (int p = 1; p < 4; ++p) {
        input::Pad ep = input::RawPoll(p);
        pad.buttons |= ep.buttons;
        for (int i = 0; i < 8; ++i) {
            if (ep.analog[i] > pad.analog[i]) pad.analog[i] = ep.analog[i];
        }
        if (std::abs(ep.lx) > std::abs(pad.lx)) pad.lx = ep.lx;
        if (std::abs(ep.ly) > std::abs(pad.ly)) pad.ly = ep.ly;
    }

    auto is_vk = [](int vk) -> bool {
#ifdef _WIN32
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
#else
        return false;
#endif
    };

    bool up_pressed    = (pad.buttons & 0x0001) || (pad.ly > 12000)  || is_vk(VK_UP)   || is_vk('W');
    bool down_pressed  = (pad.buttons & 0x0002) || (pad.ly < -12000) || is_vk(VK_DOWN) || is_vk('S');
    bool left_pressed  = (pad.buttons & 0x0004) || (pad.lx < -12000) || is_vk(VK_LEFT) || is_vk('A');
    bool right_pressed = (pad.buttons & 0x0008) || (pad.lx > 12000)  || is_vk(VK_RIGHT)|| is_vk('D');

    bool ty_combo      = (is_vk('T') && is_vk('Y')) || ((pad.buttons & 0x0030) == 0x0030) || (pad.analog[2] > 64 && pad.analog[3] > 64);
    bool start_pressed = !ty_combo && ((pad.buttons & 0x0010) != 0);
    bool back_pressed  = !ty_combo && ((pad.buttons & 0x0020) != 0);

    bool a_pressed     = (pad.analog[0] > 64) || start_pressed || is_vk(VK_RETURN)  || is_vk(VK_SPACE) || is_vk('J') || is_vk('Z');
    bool b_pressed     = (pad.analog[1] > 64) || back_pressed  || is_vk(VK_ESCAPE)  || is_vk(VK_BACK)  || is_vk('K') || ty_combo;

    if (can_nav) {
        // 1. Si está activo el modo de prueba de 3 segundos
        if (s_trial_active) {
            if (a_pressed) {
                // Confirmar resolución seleccionada
                ConfirmTrial();
                s_last_nav_time = now + std::chrono::milliseconds(220);
                PlayMenuSound(0x5F);
                return;
            } else if (b_pressed) {
                // Cancelar y revertir de inmediato
                RevertTrial();
                s_last_nav_time = now + std::chrono::milliseconds(220);
                PlayMenuSound(0x5F);
                return;
            }
        }

        // 2. Si está abierto el modal de lista de resoluciones
        if (s_res_list_open) {
            BuildModes();
            int n_modes = (int)s_modes.size();
            if (up_pressed) {
                s_res_list_sel = (s_res_list_sel <= 0) ? (n_modes - 1) : (s_res_list_sel - 1);
                s_last_nav_time = now;
                PlayMenuSound(0x5E);
            } else if (down_pressed) {
                s_res_list_sel = (s_res_list_sel >= n_modes - 1) ? 0 : (s_res_list_sel + 1);
                s_last_nav_time = now;
                PlayMenuSound(0x5E);
            } else if (a_pressed) {
                // Probar la resolución seleccionada con cuenta regresiva de 3 segundos
                s_res_list_open = false;
                StartResolutionTrial(s_modes[s_res_list_sel].w, s_modes[s_res_list_sel].h);
                s_last_nav_time = now + std::chrono::milliseconds(250);
                PlayMenuSound(0x5F);
            } else if (b_pressed) {
                // Cerrar lista sin cambios
                s_res_list_open = false;
                s_last_nav_time = now + std::chrono::milliseconds(220);
                PlayMenuSound(0x5F);
            }
            return;
        }

        // 3. Menú principal de PC Settings
        if (up_pressed) {
            s_pc_sel_option = (s_pc_sel_option == 0) ? 7 : (s_pc_sel_option - 1);
            s_last_nav_time = now;
            PlayMenuSound(0x5E);
        } else if (down_pressed) {
            s_pc_sel_option = (s_pc_sel_option == 7) ? 0 : (s_pc_sel_option + 1);
            s_last_nav_time = now;
            PlayMenuSound(0x5E);
        } else if (left_pressed || right_pressed) {
            int dir = right_pressed ? 1 : -1;
            s_last_nav_time = now;
            PlayMenuSound(0x5E);

            // Modifica SOLO el borrador (s_draft). NADA se aplica ni guarda sin ENTER.
            switch (s_pc_sel_option) {
                case 0: { // Modo de pantalla
                    int m = int(s_draft.display_mode) + dir;
                    if (m < 0) m = 2;
                    if (m > 2) m = 0;
                    s_draft.display_mode = app::DisplayMode(m);
                    break;
                }
                case 1: { // Proporción de aspecto (16:9 / 4:3) - No altera las resoluciones
                    s_draft.aspect_ratio = (s_draft.aspect_ratio == "16:9") ? "4:3" : "16:9";
                    break;
                }
                case 2: { // Resolución (ciclar todas las resoluciones disponibles)
                    BuildModes();
                    int cur_idx = -1;
                    for (int i = 0; i < (int)s_modes.size(); ++i) {
                        if (s_modes[i].w == s_draft.width && s_modes[i].h == s_draft.height) {
                            cur_idx = i;
                            break;
                        }
                    }
                    int n = (int)s_modes.size();
                    int next_idx = cur_idx + dir;
                    if (next_idx < 0) next_idx = n - 1;
                    if (next_idx >= n) next_idx = 0;
                    if (n > 0) {
                        s_draft.width = s_modes[next_idx].w;
                        s_draft.height = s_modes[next_idx].h;
                    }
                    break;
                }
                case 3: { // Calidad (Render Scale: exactamente 1, 2, 4, 8)
                    static const uint32_t kScales[4] = {1, 2, 4, 8};
                    int cur_si = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (s_draft.render_scale == kScales[i]) cur_si = i;
                    }
                    int next_si = cur_si + dir;
                    if (next_si < 0) next_si = 3;
                    if (next_si > 3) next_si = 0;
                    s_draft.render_scale = kScales[next_si];
                    break;
                }
                case 4: { // Mando (Jugador 1 o Jugador 2)
                    s_draft.controller_player = (s_draft.controller_player == 1) ? 2 : 1;
                    break;
                }
                case 5: { // Anti-Aliasing
                    s_draft.anti_aliasing = !s_draft.anti_aliasing;
                    break;
                }
                case 6: { // Recortar bordes negros
                    s_draft.crop_black_bars = !s_draft.crop_black_bars;
                    break;
                }
                case 7: { // Idioma
                    s_draft.menu_language = (s_draft.menu_language == "es") ? "en" : "es";
                    break;
                }
            }
        } else if (a_pressed) {
            // ENTER: Aplica y guarda el ajuste seleccionado
            s_last_nav_time = now + std::chrono::milliseconds(220);
            PlayMenuSound(0x5F);

            if (s_pc_sel_option == 0) { // Modo de pantalla
                st.display_mode = s_draft.display_mode;
                app::ApplyDisplay();
                app::Save();
                s_show_saved_toast = true;
                s_save_toast_time = now;
            } else if (s_pc_sel_option == 1) { // Proporción
                st.aspect_ratio = s_draft.aspect_ratio;
                app::ApplyDisplay();
                app::Save();
                s_show_saved_toast = true;
                s_save_toast_time = now;
            } else if (s_pc_sel_option == 2) { // Resolución
                // Si la resolución cambió en borrador, iniciar prueba de 3 segundos.
                // Si es la misma, abrir la lista de resoluciones para elegir directamente.
                if (s_draft.width != st.width || s_draft.height != st.height) {
                    StartResolutionTrial(s_draft.width, s_draft.height);
                } else {
                    BuildModes();
                    s_res_list_open = true;
                    s_res_list_sel = 0;
                    for (int i = 0; i < (int)s_modes.size(); ++i) {
                        if (s_modes[i].w == st.width && s_modes[i].h == st.height) {
                            s_res_list_sel = i;
                            break;
                        }
                    }
                }
            } else if (s_pc_sel_option == 3) { // Calidad
                st.render_scale = s_draft.render_scale;
                app::Save();
                s_show_saved_toast = true;
                s_save_toast_time = now;
            } else if (s_pc_sel_option == 4) { // Mando
                st.controller_player = s_draft.controller_player;
                app::Save();
                s_show_saved_toast = true;
                s_save_toast_time = now;
            } else if (s_pc_sel_option == 5) { // Anti-Aliasing
                st.anti_aliasing = s_draft.anti_aliasing;
                app::Save();
                s_show_saved_toast = true;
                s_save_toast_time = now;
            } else if (s_pc_sel_option == 6) { // Bordes negros
                st.crop_black_bars = s_draft.crop_black_bars;
                app::Save();
                s_show_saved_toast = true;
                s_save_toast_time = now;
            } else if (s_pc_sel_option == 7) { // Idioma
                st.menu_language = s_draft.menu_language;
                app::Save();
                s_show_saved_toast = true;
                s_save_toast_time = now;
            }
        } else if (b_pressed) {
            // T + Y o B / ESC: Cancelar y volver al Graphic Setting original del juego
            CloseNativePCSettings();
            s_last_nav_time = now + std::chrono::milliseconds(250);
            PlayMenuSound(0x5F);
        }
    }
}

}  // namespace

void Draw(double fps) {
    (void)fps;
    BuildModes();
    if (!s_desk_ok) RefreshDesk();

    // Watchdog de presencia en Game option y Graphic Setting
    int wd = s_game_option_watchdog.load(std::memory_order_relaxed);
    if (wd > 0) {
        s_game_option_watchdog.store(wd - 1, std::memory_order_relaxed);
    }
    int gwd = s_gfx_setting_watchdog.load(std::memory_order_relaxed);
    if (gwd > 0) {
        s_gfx_setting_watchdog.store(gwd - 1, std::memory_order_relaxed);
    }

    // 1. Si la pantalla nativa de PC SETTINGS está abierta, dibujarla
    if (s_pc_settings_open.load(std::memory_order_relaxed)) {
        RenderNativePCSettings();
        return;
    }

    // 2. Menú de desarrollador F1 (opcional)
    if (s_open.load(std::memory_order_relaxed)) {
        auto& st = app::Get();
        bool is_es = (st.menu_language != "en");
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Always);
        if (ImGui::Begin("KOF Neowave - Dev Menu (F1)", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
            ImGui::TextColored(ImVec4(0.98f, 0.72f, 0.22f, 1.0f), "THE KING OF FIGHTERS NEOWAVE");
            ImGui::Separator();
            ImGui::Text(is_es ? "Los ajustes principales se controlan desde 'PC SETTINGS' en Game Option con el mando."
                              : "Main settings are controlled from 'PC SETTINGS' in Game Option with the controller.");
            if (ImGui::Button(is_es ? "Cerrar" : "Close", ImVec2(100, 0))) {
                s_open.store(false, std::memory_order_relaxed);
            }
        }
        ImGui::End();
    }
}

void Toggle() {
    s_open.store(!s_open.load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (s_open.load(std::memory_order_relaxed)) RefreshDesk();
}

void Close() { s_open.store(false, std::memory_order_relaxed); }
bool Open() { return s_open.load(std::memory_order_relaxed); }

bool Blocked() {
    if (s_open.load(std::memory_order_relaxed)) return true;
    if (s_pc_settings_open.load(std::memory_order_relaxed)) return true;
#ifdef _WIN32
    HWND me = (HWND)app::Window();
    if (!me) return false;
    HWND fg = GetForegroundWindow();
    return fg != me && (!fg || GetAncestor(fg, GA_ROOT) != me);
#else
    return false;
#endif
}

bool IsInGameOption() {
    return s_game_option_watchdog.load(std::memory_order_relaxed) > 0;
}

void SetInGameOption(bool in_opt) {
    if (in_opt) {
        s_game_option_watchdog.store(3, std::memory_order_relaxed);
    } else {
        s_game_option_watchdog.store(0, std::memory_order_relaxed);
    }
}

bool IsInGraphicSetting() {
    return s_gfx_setting_watchdog.load(std::memory_order_relaxed) > 0;
}

void SetInGraphicSetting(bool in_gfx) {
    if (in_gfx) {
        s_gfx_setting_watchdog.store(3, std::memory_order_relaxed);
    } else {
        s_gfx_setting_watchdog.store(0, std::memory_order_relaxed);
    }
}

void ToggleNativePCSettings() {
    bool cur = s_pc_settings_open.load(std::memory_order_relaxed);
    if (cur) {
        CloseNativePCSettings();
    } else {
        OpenNativePCSettings();
    }
}

int GetCursorRow() {
    return s_cursor_row.load(std::memory_order_relaxed);
}

void SetCursorRow(int row) {
    s_cursor_row.store(row, std::memory_order_relaxed);
}

bool IsNativePCSettingsOpen() {
    return s_pc_settings_open.load(std::memory_order_relaxed);
}

void OpenNativePCSettings() {
    SyncDraftFromApp();
    s_trial_active = false;
    s_res_list_open = false;
    s_pc_settings_open.store(true, std::memory_order_relaxed);
    s_pc_sel_option = 0;
    s_last_nav_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
}

void CloseNativePCSettings() {
    if (s_trial_active) {
        RevertTrial();
    }
    s_res_list_open = false;
    s_pc_settings_open.store(false, std::memory_order_relaxed);
}

void RegisterSoundCallback(void (*play_sfx)(uint16_t id)) {
    s_play_sfx_cb = play_sfx;
}

void PlayMenuSound(uint16_t id) {
    if (s_play_sfx_cb) {
        s_play_sfx_cb(id);
    }
}

}  // namespace cfgmenu
