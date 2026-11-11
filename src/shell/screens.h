// Neutro X - telas proprias do aplicativo (fora dos dashboards).
#pragma once
#include <functional>
#include <memory>
#include <string>
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

}  // namespace nx
