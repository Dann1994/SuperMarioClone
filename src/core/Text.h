#pragma once

// Textos de menus y carteles en espanol e ingles (seccion 10.2, opcion
// "Idioma"). Sin enies ni tildes: la fuente (DebugFont) no las tiene.
// El HUD (MARIO / WORLD / TIME) queda en ingles en los dos, como el original.
enum class Txt {
    // Menu principal
    OnePlayer, TwoPlayers, Continue, WorldSelect, Options, TestMode, Credits, Quit,
    // Pausa
    Paused, Resume, RestartLevel, QuitToMenu,
    // Configuracion
    Display, Windowed, Fullscreen, WindowSize, Auto, AspectRatio, ShowFps, MasterVolume, MusicVolume,
    SfxVolume, Language, LanguageName, Vibration, Controls, Back, On, Off,
    // Controles
    Player1, Player2, Gamepad, Left, Right, Jump, Run, Down, PressKey, PressButton, ResetDefaults,
    // Carteles y ayudas
    CourseClear, EndOfDemo, ThanksForPlaying, MenuHint, ValueHint, BindHint, ComingSoon,
    Count
};

void setEnglish(bool english);
const char* tr(Txt id);
