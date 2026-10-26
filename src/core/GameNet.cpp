// Modo en red: dos PC juegan la misma partida de a 2 a la vez (Mario el
// anfitrion, Luigi el otro), por la red local o por internet con un codigo
// de sala. La sala y el protocolo son los de Battle City (src/net/lan.*,
// src/net/nat.*): cada PC manda sus botones y simula el mismo juego con los
// botones de las dos ("lockstep"), asi que todo lo que cambia la simulacion
// tiene que ser igual en las dos (el azar sale de sim::seedRandom con la
// semilla del anfitrion, los extras son los del anfitrion, sin pausa).
#include "Game.h"

#include "Text.h"
#include "core/SimRandom.h"
#include "core/Version.h"
#include "net/lan.h"
#include "net/updater.h"
#include "render/DebugFont.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>

namespace {
constexpr float kNetDt = 1.0f / 60.0f;   // el mismo paso fijo que Game.cpp
const SDL_Color kNetWhite{255, 255, 255, 255};
const SDL_Color kNetYellow{252, 216, 88, 255};
const SDL_Color kNetGray{120, 120, 140, 255};
const SDL_Color kNetDim{180, 180, 200, 255};
const SDL_Color kNetRed{248, 56, 0, 255};
const SDL_Color kNetGreen{0, 200, 0, 255};

// Los extras que cambian la simulacion, en el orden de los bits de las
// opciones de la sala.
const char* const kNetExtras[] = {"luigi_lost_levels", "level_hammer_flower", "level_boomerang_flower",
                                  "infinite_lives", "infinite_time"};
constexpr int kNetExtraCount = 5;
// Opciones de la sala (un int que la sala ya lleva a todos): bits 0-4 los
// extras, 5 choque entre jugadores, 6 PARTIDA +, 7-11 el nivel (0 = 1-1),
// 12-30 la semilla del azar.
constexpr int kOptCollision = 1 << 5;
constexpr int kOptSecondQuest = 1 << 6;
constexpr int kOptLevelShift = 7;
constexpr int kOptSeedShift = 12;

// La fuente del juego no tiene tildes: los avisos de la sala vienen con ellas.
std::string plainUpper(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == 0xC3 && i + 1 < text.size()) {
            const unsigned char d = static_cast<unsigned char>(text[++i]);
            const char* map = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
            const char m = d >= 0x80 && d <= 0xBF ? map[d - 0x80] : '?';
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(m))));
            continue;
        }
        if (c == 0xC2) {   // ¿ ¡
            ++i;
            continue;
        }
        out.push_back(static_cast<char>(std::toupper(c)));
    }
    return out;
}

uint32_t fnv(uint32_t h, const void* data, size_t n) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

template <typename T>
uint32_t fnvValue(uint32_t h, const T& value) {
    return fnv(h, &value, sizeof(value));
}
}  // namespace

// ============================================================ extras

bool Game::extraActive(const char* id) const {
    if (netGame_) {
        for (int i = 0; i < kNetExtraCount; ++i) {
            if (std::strcmp(kNetExtras[i], id) == 0) return (netOptions_ & (1 << i)) != 0;
        }
        return false;
    }
    return settings_.extraOwned(id) && settings_.extraEnabled(id);
}

int Game::packNetOptions() const {
    int options = 0;
    for (int i = 0; i < kNetExtraCount; ++i) {
        if (settings_.extraOwned(kNetExtras[i]) && settings_.extraEnabled(kNetExtras[i])) options |= 1 << i;
    }
    if (settings_.playerCollision) options |= kOptCollision;
    if (netSecondQuest_) options |= kOptSecondQuest;
    options |= (netStartIndex_ & 31) << kOptLevelShift;
    return options;
}

// ============================================================ pantallas

void Game::openNet() {
    netOpen_ = true;
    netSelection_ = 0;
    netMessage_.clear();
    if (settings_.playerName.empty()) {
        // Primero el nombre (es el que ve el otro en la sala).
        netAfterName_ = NetScreen::Menu;
        netScreen_ = NetScreen::Name;
        netText_.clear();
        SDL_StartTextInput();
    } else {
        netScreen_ = NetScreen::Menu;
    }
}

void Game::closeNet() {
    if (lan_ && !netGame_) lan_.reset();
    netOpen_ = false;
    netScreen_ = NetScreen::Menu;
    SDL_StopTextInput();
}

void Game::netShowMessage(const std::string& message) {
    netOpen_ = true;
    netScreen_ = NetScreen::Message;
    netMessage_ = plainUpper(message);
    SDL_StopTextInput();
}

bool Game::handleNetTyping(const SDL_Event& event) {
    if (netOpen_ && netScreen_ == NetScreen::Room && lan_ && lan_->IsHost() && event.type == SDL_KEYDOWN &&
        event.key.keysym.sym == SDLK_c) {
        // C: copia el codigo de la sala (para mandarselo al otro).
        const bool online = lan_->InternetState() == net::Lan::Internet::Ready;
        const std::string code = online ? lan_->RoomCode() : lan_->LanCode();
        if (!code.empty()) {
            SDL_SetClipboardText(code.c_str());
            netNoteTimer_ = 2.0f;
            audio_.play(Sfx::Coin);
        }
        return true;
    }
    if (!netOpen_ || (netScreen_ != NetScreen::Name && netScreen_ != NetScreen::Code)) return false;
    const size_t maxLen = netScreen_ == NetScreen::Name ? 12 : 14;
    const auto addText = [&](const char* text) {
        for (const char* p = text; *p; ++p) {
            const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
            const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                            (c == ' ' && netScreen_ == NetScreen::Name);
            if (ok && netText_.size() < maxLen) netText_.push_back(c);
        }
    };
    if (event.type == SDL_TEXTINPUT) {
        addText(event.text.text);
        return true;
    }
    if (event.type != SDL_KEYDOWN) return false;
    const SDL_Keycode key = event.key.keysym.sym;
    if (key == SDLK_BACKSPACE) {
        if (!netText_.empty()) netText_.pop_back();
        return true;
    }
    if (key == SDLK_v && (event.key.keysym.mod & KMOD_CTRL)) {
        if (char* clip = SDL_GetClipboardText()) {
            addText(clip);
            SDL_free(clip);
        }
        return true;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_ESCAPE) return false;   // los maneja handleNetInput
    return true;   // el resto de las teclas (letras = direcciones del menu) solo escriben
}

void Game::netHostRoom() {
    lan_ = std::make_unique<net::Lan>();
    lan_->SetCapacity(2);
    lan_->SetNoLateJoin(true);
    if (!lan_->Host(netPlayerName())) {
        const std::string error = lan_->Error();
        lan_.reset();
        netShowMessage(error.empty() ? tr(Txt::NetCantHost) : error);
        return;
    }
    netStartIndex_ = 0;
    netSecondQuest_ = false;
    lan_->SetOptions(packNetOptions());
    netScreen_ = NetScreen::Room;
    netSelection_ = 0;
}

void Game::handleNetInput(MenuInput input) {
    switch (netScreen_) {
        case NetScreen::Menu: {
            // CREAR PARTIDA, BUSCAR EN LA RED LOCAL, UNIRSE CON CODIGO, NOMBRE, VOLVER
            constexpr int kCount = 5;
            if (input == MenuInput::Up || input == MenuInput::Down) {
                netSelection_ = (netSelection_ + (input == MenuInput::Up ? kCount - 1 : 1)) % kCount;
                audio_.play(Sfx::TimeTally);
                return;
            }
            if (input == MenuInput::Back) {
                closeNet();
                audio_.play(Sfx::Bump);
                return;
            }
            if (input != MenuInput::Confirm) return;
            audio_.play(Sfx::Coin);
            switch (netSelection_) {
                case 0: netHostRoom(); break;
                case 1:
                    lan_ = std::make_unique<net::Lan>();
                    if (!lan_->Search()) {
                        const std::string error = lan_->Error();
                        lan_.reset();
                        netShowMessage(error);
                        break;
                    }
                    netScreen_ = NetScreen::Search;
                    netSelection_ = 0;
                    break;
                case 2:
                    netScreen_ = NetScreen::Code;
                    netText_.clear();
                    SDL_StartTextInput();
                    break;
                case 3:
                    netAfterName_ = NetScreen::Menu;
                    netScreen_ = NetScreen::Name;
                    netText_ = settings_.playerName;
                    SDL_StartTextInput();
                    break;
                default: closeNet(); break;
            }
            return;
        }
        case NetScreen::Name:
            if (input == MenuInput::Back) {
                SDL_StopTextInput();
                if (settings_.playerName.empty()) {
                    closeNet();
                } else {
                    netScreen_ = NetScreen::Menu;
                }
                audio_.play(Sfx::Bump);
            } else if (input == MenuInput::Confirm) {
                const std::string name = net::Lan::CleanName(netText_);
                if (name.empty()) return;
                settings_.playerName = name;
                settings_.save();
                SDL_StopTextInput();
                netScreen_ = netAfterName_;
                netSelection_ = 0;
                audio_.play(Sfx::Coin);
            }
            return;
        case NetScreen::Code:
            if (input == MenuInput::Back) {
                SDL_StopTextInput();
                netScreen_ = NetScreen::Menu;
                audio_.play(Sfx::Bump);
            } else if (input == MenuInput::Confirm) {
                lan_ = std::make_unique<net::Lan>();
                if (!lan_->JoinCode(netText_, netPlayerName())) {
                    lan_.reset();
                    netShowMessage(tr(Txt::NetBadCode));
                    return;
                }
                SDL_StopTextInput();
                netScreen_ = NetScreen::Joining;
                audio_.play(Sfx::Coin);
            }
            return;
        case NetScreen::Search: {
            const int count = lan_ ? static_cast<int>(lan_->Games().size()) : 0;
            if (input == MenuInput::Back) {
                lan_.reset();
                netScreen_ = NetScreen::Menu;
                netSelection_ = 1;
                audio_.play(Sfx::Bump);
            } else if ((input == MenuInput::Up || input == MenuInput::Down) && count > 0) {
                netSelection_ = (netSelection_ + (input == MenuInput::Up ? count - 1 : 1)) % count;
                audio_.play(Sfx::TimeTally);
            } else if (input == MenuInput::Confirm && count > 0) {
                const net::LanGame game = lan_->Games()[static_cast<size_t>(std::min(netSelection_, count - 1))];
                if (!lan_->Join(game, netPlayerName())) {
                    const std::string error = lan_->Error();
                    lan_.reset();
                    netShowMessage(error);
                    return;
                }
                netScreen_ = NetScreen::Joining;
                audio_.play(Sfx::Coin);
            }
            return;
        }
        case NetScreen::Joining:
            if (input == MenuInput::Back) {
                lan_.reset();
                netScreen_ = NetScreen::Menu;
                audio_.play(Sfx::Bump);
            }
            return;
        case NetScreen::Room: {
            if (!lan_) return;
            if (!lan_->IsHost()) {
                if (input == MenuInput::Back) {
                    lan_.reset();
                    netScreen_ = NetScreen::Menu;
                    audio_.play(Sfx::Bump);
                }
                return;
            }
            // Anfitrion: NIVEL, PARTIDA +, INTERNET, EMPEZAR, VOLVER.
            constexpr int kCount = 5;
            const bool plusAllowed = settings_.extraUnlocked("new_game_plus");
            if (input == MenuInput::Up || input == MenuInput::Down) {
                const int step = input == MenuInput::Up ? kCount - 1 : 1;
                netSelection_ = (netSelection_ + step) % kCount;
                if (netSelection_ == 1 && !plusAllowed) netSelection_ = (netSelection_ + step) % kCount;
                audio_.play(Sfx::TimeTally);
                return;
            }
            if (input == MenuInput::Back) {
                lan_.reset();
                netScreen_ = NetScreen::Menu;
                netSelection_ = 0;
                audio_.play(Sfx::Bump);
                return;
            }
            if (netSelection_ == 0 && (input == MenuInput::Left || input == MenuInput::Right)) {
                // Los niveles que ya paso el anfitrion (como ELEGIR MUNDO), y el 1-1.
                const int step = input == MenuInput::Left ? -1 : 1;
                int index = netStartIndex_;
                for (int tries = 0; tries < 32; ++tries) {
                    index = ((index + step) % 32 + 32) % 32;
                    if (index == 0 || levelCleared(index / 4 + 1, index % 4 + 1)) break;
                }
                netStartIndex_ = index;
                lan_->SetOptions(packNetOptions());
                audio_.play(Sfx::TimeTally);
                return;
            }
            if (input != MenuInput::Confirm && !(netSelection_ == 1 && (input == MenuInput::Left || input == MenuInput::Right))) {
                return;
            }
            switch (netSelection_) {
                case 1:
                    netSecondQuest_ = !netSecondQuest_;
                    lan_->SetOptions(packNetOptions());
                    audio_.play(Sfx::TimeTally);
                    break;
                case 2:
                    if (lan_->InternetState() == net::Lan::Internet::Off ||
                        lan_->InternetState() == net::Lan::Internet::Failed) {
                        lan_->OpenToInternet();
                    } else {
                        lan_->CloseInternet();
                    }
                    audio_.play(Sfx::Coin);
                    break;
                case 3:
                    if (lan_->NumPlayers() < 2) {
                        audio_.play(Sfx::Bump);
                        break;
                    }
                    lan_->StartGame(packNetOptions() | (static_cast<int>(std::time(nullptr) & 0x7FFFF) << kOptSeedShift));
                    audio_.play(Sfx::Coin);
                    startNetGame();
                    break;
                default:
                    lan_.reset();
                    netScreen_ = NetScreen::Menu;
                    netSelection_ = 0;
                    audio_.play(Sfx::Bump);
                    break;
            }
            return;
        }
        case NetScreen::Message:
            if (input == MenuInput::Confirm || input == MenuInput::Back) {
                netScreen_ = NetScreen::Menu;
                netSelection_ = 0;
                netMessage_.clear();
                audio_.play(Sfx::Bump);
            }
            return;
    }
}

std::string Game::netPlayerName() const {
    if (!netAutoHost_.empty()) return netAutoHost_;
    if (!netAutoJoin_.empty()) return netAutoJoin_;
    return settings_.playerName;
}

void Game::updateNetAutoTest() {
    // --net-host NOMBRE: abre la sala, deja el codigo en --net-code-out y
    // empieza apenas entra el otro. --net-join NOMBRE: espera el codigo en
    // --net-code-in y se une. Los dos corren --net-frames cuadros con
    // botones automaticos y anotan el hash en --net-out (deben coincidir).
    if (netGame_) return;
    if (!netAutoHost_.empty()) {
        if (!lan_) {
            if (appState_ != AppState::Title) return;
            netOpen_ = true;
            netHostRoom();
            if (lan_ && !netCodeOut_.empty()) {
                std::ofstream out(netCodeOut_);
                out << lan_->LanCode() << "\n";
            }
        } else if (lan_->IsHost() && lan_->NumPlayers() >= 2 && !lan_->Started()) {
            lan_->StartGame(packNetOptions() | (12345 << kOptSeedShift));
            startNetGame();
        }
    } else if (!netAutoJoin_.empty() && !lan_ && appState_ == AppState::Title) {
        std::ifstream in(netCodeIn_);
        std::string code;
        if (!(in >> code) || code.empty()) return;
        netOpen_ = true;
        lan_ = std::make_unique<net::Lan>();
        if (lan_->JoinCode(code, netPlayerName())) {
            netScreen_ = NetScreen::Joining;
        } else {
            lan_.reset();
        }
    }
}

void Game::updateNetLobby() {
    // Una vez por paso de 1/60 s mientras estan abiertas las pantallas de red.
    if (!netAutoHost_.empty() || !netAutoJoin_.empty()) updateNetAutoTest();
    if (!lan_ || netGame_) return;
    lan_->Update();
    if (netNoteTimer_ > 0.0f) netNoteTimer_ -= kNetDt;
    const net::Lan::State state = lan_->state();
    if (netScreen_ == NetScreen::Joining) {
        if (state == net::Lan::State::Joined) {
            netScreen_ = NetScreen::Room;
            netSelection_ = 0;
        } else if (state == net::Lan::State::Failed || state == net::Lan::State::Full ||
                   state == net::Lan::State::Closed) {
            const std::string error = lan_->Error();
            lan_.reset();
            netShowMessage(error.empty() ? tr(Txt::NetLost) : error);
        }
        return;
    }
    if (netScreen_ == NetScreen::Room && !lan_->IsHost()) {
        if (lan_->Started()) {
            startNetGame();
        } else if (lan_->Lost() || state == net::Lan::State::Closed || state == net::Lan::State::Failed) {
            lan_.reset();
            netShowMessage(tr(Txt::NetLost));
        }
    }
}

void Game::renderNet() {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 240);
    SDL_RenderFillRect(renderer_, nullptr);
    drawCentered(16, tr(Txt::NetMenu), 2, kNetWhite);
    const int left = (gLogicalWidth - 256) / 2 + 24;
    const auto option = [&](int index, int y, const std::string& text, SDL_Color color) {
        if (index == netSelection_) titleCursorSprite_.draw(renderer_, 0, 0, left - 14, y, false);
        DebugFont::drawText(renderer_, left, y, text, 2, color);
    };
    switch (netScreen_) {
        case NetScreen::Menu: {
            const std::string name = std::string(tr(Txt::NetName)) + ": " + settings_.playerName;
            const std::string items[] = {tr(Txt::NetCreate), tr(Txt::NetSearch), tr(Txt::NetJoinCode), name, tr(Txt::Back)};
            for (int i = 0; i < 5; ++i) option(i, 56 + i * 18, items[i], i == netSelection_ ? kNetYellow : kNetWhite);
            break;
        }
        case NetScreen::Name:
        case NetScreen::Code: {
            drawCentered(64, tr(netScreen_ == NetScreen::Name ? Txt::NetTypeName : Txt::NetTypeCode), 2, kNetWhite);
            const bool blink = (SDL_GetTicks() / 400) % 2 == 0;
            drawCentered(96, netText_ + (blink ? "_" : " "), 2, kNetYellow);
            if (netScreen_ == NetScreen::Code) drawCentered(140, tr(Txt::NetPasteHint), 2, kNetDim);
            drawCentered(160, tr(Txt::NetTypeHint), 2, kNetDim);
            break;
        }
        case NetScreen::Search: {
            drawCentered(40, tr(Txt::NetSearching), 2, kNetDim);
            const auto& games = lan_ ? lan_->Games() : std::vector<net::LanGame>{};
            if (games.empty()) drawCentered(80, tr(Txt::NetNoGames), 2, kNetGray);
            for (size_t i = 0; i < games.size() && i < 8; ++i) {
                const net::LanGame& g = games[i];
                char line[64];
                std::snprintf(line, sizeof(line), "%s  %d/%d", plainUpper(g.name).c_str(), g.players, g.maxPlayers);
                option(static_cast<int>(i), 64 + static_cast<int>(i) * 16, line,
                       !g.compatible ? kNetGray : static_cast<int>(i) == netSelection_ ? kNetYellow : kNetWhite);
            }
            drawCentered(200, tr(Txt::NetTypeHint), 2, kNetDim);
            break;
        }
        case NetScreen::Joining:
            drawCentered(96, tr(Txt::NetConnecting), 2, kNetWhite);
            break;
        case NetScreen::Room: {
            if (!lan_) break;
            drawCentered(36, tr(Txt::NetRoom), 2, kNetYellow);
            // Los dos lugares: Mario (el anfitrion) y Luigi.
            const auto& players = lan_->Players();
            for (int i = 0; i < 2; ++i) {
                const int y = 56 + i * 16;
                DebugFont::drawText(renderer_, left, y, i == 0 ? "MARIO" : "LUIGI", 2, i == 0 ? kNetRed : kNetGreen);
                const std::string who = static_cast<size_t>(i) < players.size() ? plainUpper(players[static_cast<size_t>(i)])
                                                                                : tr(Txt::NetWaitingPlayer);
                DebugFont::drawText(renderer_, left + 56, y, who, 2,
                                    static_cast<size_t>(i) < players.size() ? kNetWhite : kNetGray);
            }
            // Lo que eligio el anfitrion (el otro lo ve igual).
            const int options = lan_->IsHost() ? packNetOptions() : lan_->FriendlyFire();
            const int index = (options >> kOptLevelShift) & 31;
            char level[40];
            std::snprintf(level, sizeof(level), "%s < %d-%d >%s", tr(Txt::NetLevel), index / 4 + 1, index % 4 + 1,
                          (options & kOptSecondQuest) ? " +" : "");
            if (lan_->IsHost()) {
                const bool plusAllowed = settings_.extraUnlocked("new_game_plus");
                std::string internet = std::string(tr(Txt::NetInternet)) + ": ";
                switch (lan_->InternetState()) {
                    case net::Lan::Internet::Off: internet += tr(Txt::NetClosed); break;
                    case net::Lan::Internet::Working: internet += tr(Txt::NetOpening); break;
                    case net::Lan::Internet::Ready: internet += tr(Txt::NetOpen); break;
                    case net::Lan::Internet::Failed: internet += tr(Txt::NetFailed); break;
                }
                const std::string plus = std::string(tr(Txt::NewGamePlus)) + ": " + tr(netSecondQuest_ ? Txt::Yes : Txt::No);
                option(0, 100, level, netSelection_ == 0 ? kNetYellow : kNetWhite);
                option(1, 116, plus, !plusAllowed ? kNetGray : netSelection_ == 1 ? kNetYellow : kNetWhite);
                option(2, 132, internet, netSelection_ == 2 ? kNetYellow : kNetWhite);
                option(3, 148, tr(Txt::NetStart), lan_->NumPlayers() < 2 ? kNetGray : netSelection_ == 3 ? kNetYellow : kNetWhite);
                option(4, 164, tr(Txt::Back), netSelection_ == 4 ? kNetYellow : kNetWhite);
                // El codigo: el de internet si esta abierta, si no el de la red local.
                const bool online = lan_->InternetState() == net::Lan::Internet::Ready;
                const std::string code = online ? lan_->RoomCode() : lan_->LanCode();
                if (!code.empty()) {
                    const std::string label = std::string(tr(online ? Txt::NetCode : Txt::NetLanCode)) + ": " + code;
                    drawCenteredFit(190, label, kNetWhite);
                    drawCentered(214, tr(netNoteTimer_ > 0.0f ? Txt::NetCopied : Txt::NetCopyHint), 2, kNetDim);
                }
                if (lan_->InternetState() == net::Lan::Internet::Failed && !lan_->InternetNote().empty()) {
                    drawCenteredFit(214, plainUpper(lan_->InternetNote()), kNetGray);
                }
            } else {
                DebugFont::drawText(renderer_, left, 100, level, 2, kNetWhite);
                drawCenteredFit(150, tr(Txt::NetWaitHost), kNetDim);
            }
            break;
        }
        case NetScreen::Message:
            drawCenteredFit(96, netMessage_.empty() ? std::string(tr(Txt::NetLost)) : netMessage_, kNetWhite);
            break;
    }
}

// ============================================================ partida

void Game::startNetGame() {
    if (!lan_) return;
    const int options = lan_->FriendlyFire();
    netOptions_ = options;
    netGame_ = true;
    netFrame_ = 0;
    netStall_ = 0;
    netMenuOpen_ = false;
    netOpen_ = false;
    SDL_StopTextInput();
    // Las dos PC arrancan del mismo estado: la misma semilla, los mismos
    // extras (los del anfitrion), personajes chicos y sin restos de antes.
    sim::seedRandom(static_cast<uint32_t>(options >> kOptSeedShift) * 2654435761u + 1u);
    const int index = (options >> kOptLevelShift) & 31;
    secondQuestNext_ = (options & kOptSecondQuest) != 0;
    startWorld_ = index / 4 + 1;
    startLevel_ = index % 4 + 1;
    worldSelectFlow_ = index != 0;   // empezar a mitad no cuenta para los logros "desde el 1-1"
    currentSlot_ = -1;               // no se guarda en ningun espacio
    for (Mario* character : {&mario_, &luigi_}) {
        character->setSize(MarioSize::Small);
        character->setInvincible(false);
    }
    startCampaign(true);
    worldSelectFlow_ = false;
    characterCollisionEnabled_ = (options & kOptCollision) != 0;
    input1_.setScripted(false);
    input2_.setScripted(false);
    input1_.clearPresses();
    input2_.clearPresses();
}

void Game::endNetGame(const std::string& message) {
    const bool wasGame = netGame_;
    netGame_ = false;
    netMenuOpen_ = false;
    if (lan_) lan_.reset();
    sim::seedRandom(static_cast<uint32_t>(std::time(nullptr)));
    characterCollisionEnabled_ = settings_.playerCollision;
    if (wasGame && appState_ != AppState::Title && appState_ != AppState::Summary && appState_ != AppState::HighScores) {
        openTitle();
    }
    if (!message.empty()) netShowMessage(message);
}

uint32_t Game::netStateHash() const {
    // Lo esencial de la simulacion: si difiere entre las dos PC, se
    // desincronizaron (la sala lo compara cada 60 cuadros).
    uint32_t h = 2166136261u;
    for (const Mario* character : {&mario_, &luigi_}) {
        h = fnvValue(h, character->x());
        h = fnvValue(h, character->y());
        h = fnvValue(h, character->velocityX());
        h = fnvValue(h, character->velocityY());
        h = fnvValue(h, static_cast<int>(character->size()));
    }
    h = fnvValue(h, score_);
    h = fnvValue(h, coinCount_);
    h = fnvValue(h, livesCount_);
    h = fnvValue(h, luigiLives_);
    h = fnvValue(h, campaignWorld_);
    h = fnvValue(h, campaignLevel_);
    h = fnvValue(h, static_cast<int>(appState_));
    h = fnvValue(h, sim::randomState());
    for (const Goomba& goomba : goombas_) {
        if (!goomba.isAlive()) continue;
        h = fnvValue(h, goomba.x());
    }
    for (const Koopa& koopa : levelKoopas_) {
        if (!koopa.isAlive()) continue;
        h = fnvValue(h, koopa.x());
    }
    return h;
}

void Game::stepNet() {
    // Un cuadro de la partida en red (cada 1/60 s): botones locales -> sala
    // -> botones de los dos -> simulacion. Si esta PC quedo atrasada simula
    // varios de una para alcanzar.
    if (!lan_) {
        endNetGame(tr(Txt::NetLost));
        return;
    }
    lan_->Update();
    if (lan_->Desynced()) {
        endNetGame(tr(Txt::NetDesync));
        return;
    }
    if (lan_->Lost()) {
        endNetGame(tr(Txt::NetLost));
        return;
    }
    const bool behind = lan_->FramesAvailable() - netFrame_ > lan_->InputDelay() + 4;
    const int budget = behind ? 8 : 1;
    for (int i = 0; i < budget && netGame_; ++i) {
        if (!stepNetFrame()) return;
    }
}

bool Game::stepNetFrame() {
    uint8_t mine = 0;
    if (!netMenuOpen_) mine = input1_.sampleButtons();
    if (netAutoFrames_ > 0) {
        // Prueba automatica: corre a la derecha y salta cada tanto (distinto
        // en cada PC, asi se nota si algun boton no llega).
        mine = static_cast<uint8_t>(Input::kNetRight | Input::kNetRun);
        const int phase = netFrame_ % (lan_->IsHost() ? 50 : 70);
        if (phase < 20) mine |= Input::kNetJump;
    }
    lan_->SubmitLocal(netFrame_, mine);
    if (!lan_->FrameReady(netFrame_)) {
        ++netStall_;
        return false;
    }
    netStall_ = 0;
    uint8_t in[8];
    lan_->FrameInputs(netFrame_, in);
    // Lugar 0 = el anfitrion (Mario), lugar 1 = el otro (Luigi).
    int hostSlot = 0;
    for (size_t i = 0; i < lan_->Owners().size(); ++i) {
        if (lan_->Owners()[i] == 0) {
            hostSlot = static_cast<int>(i);
            break;
        }
    }
    const int guestSlot = hostSlot == 0 ? 1 : 0;
    if (net::Lan::IsMark(in[guestSlot]) || net::Lan::IsMark(in[hostSlot])) {
        endNetGame(tr(Txt::NetOtherLeft));
        return false;
    }
    input1_.beginNetFrame(in[hostSlot]);
    input2_.beginNetFrame(in[guestSlot]);
    update(kNetDt);
    ++netFrame_;
    if (netFrame_ % 60 == 0 && lan_) lan_->ReportHash(netFrame_, netStateHash());
    if (!netHashOut_.empty() && netFrame_ % 60 == 0) {
        std::ofstream out(netHashOut_, std::ios::app);
        out << netFrame_ << " " << netStateHash() << "\n";
    }
    if (netAutoFrames_ > 0 && netFrame_ >= netAutoFrames_) {
        running_ = false;
        return false;
    }
    // Termino la partida (GAME OVER, el final): cada PC sigue por su lado.
    if (netGame_ && appState_ != AppState::Playing && appState_ != AppState::LevelIntro &&
        appState_ != AppState::GameOver && appState_ != AppState::DemoEnd) {
        endNetGame("");
        return false;
    }
    return netGame_;
}

void Game::handleNetGameEvent(const SDL_Event& event) {
    // Durante la partida en red: sin pausa (el otro sigue jugando). ESC /
    // Start abre CONTINUAR / ABANDONAR PARTIDA con el juego andando atras.
    const MenuInput input = menuInputFromEvent(event);
    const bool openKey = (event.type == SDL_KEYDOWN && !event.key.repeat &&
                          event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) ||
                         (event.type == SDL_CONTROLLERBUTTONDOWN && event.cbutton.button == SDL_CONTROLLER_BUTTON_START);
    if (!netMenuOpen_) {
        if (openKey) {
            netMenuOpen_ = true;
            netMenuChoice_ = 0;
            audio_.play(Sfx::Pause);
        }
        return;
    }
    if (openKey || input == MenuInput::Back) {
        netMenuOpen_ = false;
        return;
    }
    if (input == MenuInput::Up || input == MenuInput::Down) {
        netMenuChoice_ = 1 - netMenuChoice_;
        audio_.play(Sfx::TimeTally);
    } else if (input == MenuInput::Confirm) {
        if (netMenuChoice_ == 0) {
            netMenuOpen_ = false;
        } else {
            endNetGame("");
        }
    }
}

void Game::renderNetGameOverlay() {
    if (!netGame_) return;
    if (netStall_ > 30) {
        // Hace medio segundo que no llegan los botones del otro.
        drawCenteredFit(120, tr(Txt::NetWaitingFrames), kNetYellow);
    }
    if (!netMenuOpen_) return;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    const int boxW = 200;
    const SDL_Rect box{(gLogicalWidth - boxW) / 2, 84, boxW, 64};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 220);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &box);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    const int x = box.x + 24;
    DebugFont::drawText(renderer_, x, box.y + 14, tr(Txt::Resume), 2, netMenuChoice_ == 0 ? kNetYellow : kNetWhite);
    DebugFont::drawText(renderer_, x, box.y + 36, tr(Txt::NetLeave), 2, netMenuChoice_ == 1 ? kNetYellow : kNetWhite);
    titleCursorSprite_.draw(renderer_, 0, 0, x - 14, box.y + (netMenuChoice_ == 0 ? 14 : 36), false);
}

// ============================================================ actualizaciones

void Game::checkForUpdates(bool manual) {
    if (noUpdate_ && !manual) return;
    if (!updater_) updater_ = std::make_unique<net::Updater>();
    if (manual) updatePromptShown_ = false;
    updater_->StartCheck();
}

void Game::updateUpdater() {
    // Una vez por cuadro: si hay una version nueva y se esta en el titulo
    // (sin otro cartel abierto), se ofrece una vez.
    if (!updater_) return;
    updater_->Poll();
    using S = net::Updater::State;
    const S state = updater_->state();
    if (state == S::Available && !updatePromptShown_ && appState_ == AppState::Title && !netOpen_ && !choosingSlot_ &&
        !extraContentOpen_ && !jukeboxOpen_ && !confirmingQuit_ && !choosingLevel_ && !choosingPlayers_ &&
        !choosingCharacter_ && !choosingTwoMode_) {
        updatePromptOpen_ = true;
        updatePromptShown_ = true;
        updateChoice_ = 0;
    }
    if (updatePromptOpen_ && state == S::Downloaded) {
        // Listo: el instalador cierra el juego, lo actualiza y lo vuelve a abrir.
        if (updater_->Install()) running_ = false;
    }
}

void Game::handleUpdateInput(MenuInput input) {
    using S = net::Updater::State;
    const S state = updater_ ? updater_->state() : S::Idle;
    if (state == S::Downloading) {
        if (input == MenuInput::Back) {
            updater_->Cancel();
            updatePromptOpen_ = false;
            audio_.play(Sfx::Bump);
        }
        return;
    }
    if (state == S::Failed || state == S::Downloaded) {
        if (input == MenuInput::Confirm || input == MenuInput::Back) updatePromptOpen_ = false;
        return;
    }
    if (input == MenuInput::Up || input == MenuInput::Down || input == MenuInput::Left || input == MenuInput::Right) {
        updateChoice_ = 1 - updateChoice_;
        audio_.play(Sfx::TimeTally);
    } else if (input == MenuInput::Back || (input == MenuInput::Confirm && updateChoice_ == 1)) {
        updatePromptOpen_ = false;
        audio_.play(Sfx::Bump);
    } else if (input == MenuInput::Confirm) {
        updater_->StartDownload();
        audio_.play(Sfx::Coin);
    }
}

void Game::renderUpdateOverlay() {
    if (!updatePromptOpen_ || !updater_) return;
    using S = net::Updater::State;
    const S state = updater_->state();
    const int boxW = 280;
    const SDL_Rect box{(gLogicalWidth - boxW) / 2, 92, boxW, 80};
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 245);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer_, &box);
    const std::string version = updater_->release().version;
    if (state == S::Downloading || state == S::Downloaded) {
        drawCentered(104, std::string(tr(Txt::UpdDownloading)) + " " + version, 2, kNetWhite);
        const float progress = updater_->progress();
        const SDL_Rect bar{box.x + 20, 126, boxW - 40, 10};
        SDL_SetRenderDrawColor(renderer_, 90, 90, 110, 255);
        SDL_RenderDrawRect(renderer_, &bar);
        if (progress > 0.0f) {
            const SDL_Rect fill{bar.x + 1, bar.y + 1,
                                static_cast<int>(static_cast<float>(bar.w - 2) * std::min(1.0f, progress)), bar.h - 2};
            SDL_SetRenderDrawColor(renderer_, 252, 216, 88, 255);
            SDL_RenderFillRect(renderer_, &fill);
        }
        drawCentered(148, tr(state == S::Downloaded ? Txt::UpdInstalling : Txt::NetTypeHint), 2, kNetDim);
        return;
    }
    if (state == S::Failed) {
        drawCentered(104, tr(Txt::UpdFailed), 2, kNetWhite);
        drawCenteredFit(128, plainUpper(updater_->error()), kNetDim);
        return;
    }
    drawCentered(102, std::string(tr(Txt::UpdFound)) + ": " + version, 2, kNetYellow);
    const int x = box.x + 64;
    DebugFont::drawText(renderer_, x, 126, tr(Txt::UpdNow), 2, updateChoice_ == 0 ? kNetYellow : kNetWhite);
    DebugFont::drawText(renderer_, x, 146, tr(Txt::UpdLater), 2, updateChoice_ == 1 ? kNetYellow : kNetWhite);
    titleCursorSprite_.draw(renderer_, 0, 0, x - 14, updateChoice_ == 0 ? 126 : 146, false);
}

std::string Game::updateSettingValue() const {
    // La fila de CONFIGURACION: lo que paso con la ultima busqueda, o si
    // busca sola al abrir (AUTO) o no (MANUAL), con la version de este juego.
    using S = net::Updater::State;
    const S state = updater_ ? updater_->state() : S::Idle;
    const std::string version = std::string(" V") + SMC_VERSION;
    switch (state) {
        case S::Checking: return tr(Txt::UpdChecking);
        case S::Available: return "V" + updater_->release().version + " !";
        case S::UpToDate: return std::string(tr(Txt::UpdUpToDate)) + version;
        default: break;
    }
    return std::string(tr(settings_.autoUpdate ? Txt::UpdAuto : Txt::UpdManual)) + version;
}
