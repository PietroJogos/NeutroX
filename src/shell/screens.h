// Neutro X - telas proprias do aplicativo (fora dos dashboards).
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "core/screen.h"

namespace nx {

std::unique_ptr<Screen> createBootScreen();
std::unique_ptr<Screen> createMainMenuScreen();
std::unique_ptr<Screen> createSettingsScreen();
std::unique_ptr<Screen> createAboutScreen();
std::unique_ptr<Screen> createGlobalMenuScreen();
std::unique_ptr<Screen> createSoundStatusScreen();
std::unique_ptr<Screen> createErrorScreen(const std::string& title, const std::string& message);
// Dialogo Sim/Nao sobreposto.
std::unique_ptr<Screen> createConfirmDialog(const std::string& title, const std::string& message,
                                            std::function<void(Context&)> onYes);

// Dialogo com varias opcoes em lista. onChoose recebe o indice escolhido; B fecha sem escolher.
struct ChoiceOption {
    std::string label;
    std::string detail;   // linha menor abaixo do rotulo
    bool enabled = true;  // desabilitada: mostra 'detail' como motivo e nao escolhe
};
std::unique_ptr<Screen> createChoiceDialog(const std::string& title, const std::string& message,
                                           std::vector<ChoiceOption> options,
                                           std::function<void(Context&, int)> onChoose);

// Saida controlada: Voltar ao Aurora / Voltar ao Xbox Dashboard / Cancelar.
std::unique_ptr<Screen> createExitDialog(Context& ctx);

// Meus Jogos
std::unique_ptr<Screen> createLibraryScreen();
std::unique_ptr<Screen> createGameDetailScreen(const std::string& gameId);
std::unique_ptr<Screen> createLibrarySettingsScreen();
enum class BrowseMode { PickFolder, PickTitle, PickXex };
std::unique_ptr<Screen> createFolderBrowser(BrowseMode mode, const std::string& title,
                                            std::function<void(Context&, const std::string&)> onChoose);
std::unique_ptr<Screen> createKeyboardScreen(const std::string& title, const std::string& initial,
                                             std::function<void(Context&, const std::string&)> onDone);

}  // namespace nx
