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
    // Castillos (Toad) y final del juego (princesa, despues del 8-4)
    ThankYouMario, PrincessElsewhere1, PrincessElsewhere2, QuestOver1, QuestOver2, TheEnd,
    ChooseCharacter, ThankYouLuigi, ChooseLevel, ClimbUp, WelcomeWarpZone, SecondQuest, TwoPlayerMode, Simultaneous, TakeTurns, HudWorld, HudTime,
    PlayerBump, Play, BackOption, Extras, LevelEditor, EditorCreate, EditorSelect, NewGamePlus, ExtraOptions, PushButtonB, ToSelectWorld, ChooseSlot, SlotLabel, SlotEmpty, Overwrite1, Overwrite2, ModeCoop, ModeTurns, DeleteOption, ExtrasLocked, ExtrasOn, ExtrasOff, ExtrasEmpty, ExtrasHowTo, ExtrasInMenu1, ExtrasInMenu2, PauseItems, UnlockToast, UnlockToast2, OnlyNewGamePlus, QuitConfirm1, QuitConfirm2, Yes, No, NewGame, ChooseReplace, Coins, ShopPrice, ShopBuy, ShopMissing, ShopConfirm, ShopForSale, SummaryTitle, SummaryEnemies, SummaryDeaths, SummaryTime, SummaryScore, HighScoresTitle, HighScoresMenu, EnterName, NameHint, ContinueHint, HighScoresEmpty, Jukebox, JukeboxHint, JukeboxMusic, JukeboxSound, Achievements, Shop, MyExtras, AchDone, AchReward, AchUnlocks, ShopLockedBy, ShopBought, MyExtrasEmpty, ToastAchievement, ToastNew, BackHint, MyExtrasHint, ShopHint, AchHowTo, Secrets, ScrollHint,
    Count
};

void setEnglish(bool english);
const char* tr(Txt id);
bool isEnglish();
