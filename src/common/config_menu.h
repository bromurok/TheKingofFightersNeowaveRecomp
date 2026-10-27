#pragma once
#include <cstdint>

// Menú de configuración in-game (config_menu.cpp)
namespace cfgmenu {

void Draw(double fps);
void Toggle();
void Close();
bool Open();
// Sin input del juego: menú abierto, o la ventana sin foco (el teclado global no debe colarse)
bool Blocked();

// Integración nativa con Game Option y Graphic Setting
bool IsInGameOption();
void SetInGameOption(bool in_opt);

bool IsInGraphicSetting();
void SetInGraphicSetting(bool in_gfx);

int  GetCursorRow();
void SetCursorRow(int row);

bool IsNativePCSettingsOpen();
void OpenNativePCSettings();
void CloseNativePCSettings();
void ToggleNativePCSettings();

// Notificación de sonido
void RegisterSoundCallback(void (*play_sfx)(uint16_t id));
void PlayMenuSound(uint16_t id);

}  // namespace cfgmenu
