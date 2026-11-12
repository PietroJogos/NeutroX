// Neutro X - destinos de saida (Aurora / dashboard oficial) e caminhos no
// estilo do console/DashLaunch.
//
// Tudo aqui e SOMENTE LEITURA: o launch.ini do DashLaunch e lido para descobrir
// onde o Aurora esta, nunca alterado.
#pragma once
#include <string>
#include <vector>
#include "platform/platform.h"

namespace nx {

enum class ExitTarget : int { Default = 0, Aurora, Dashboard };

struct LaunchTarget {
    bool found = false;
    std::string path;    // caminho para iniciar (formato da plataforma, ex.: "hdd:/Aurora/Aurora.xex")
    std::string source;  // de onde veio: "configuracao", "launch.ini (Default)", "local padrao"
    std::string reason;  // quando nao encontrado: o motivo
};

// "Hdd:\Aurora\Aurora.xex", "Usb0:\Apps\x.xex", "Hdd1:\..." -> caminho numa das raizes.
// Retorna vazio se a unidade nao estiver entre as raizes.
std::string consolePathToLocal(const std::string& consolePath, const std::vector<StorageRoot>& roots);
// Caminho local -> forma do console ("hdd:/Games/X" -> "Hdd1:\Games\X"); sem raiz conhecida, devolve o original.
std::string localPathToConsole(const std::string& localPath, const std::vector<StorageRoot>& roots);

// Valor de uma chave do launch.ini (secao qualquer), sem comentarios; vazio se nao houver.
std::string iniValue(const std::string& text, const std::string& key);

// Procura o Aurora: 1) caminho configurado; 2) "Default" do launch.ini do DashLaunch
// (se apontar para o Aurora); 3) locais comuns em cada unidade.
LaunchTarget findAurora(const std::vector<StorageRoot>& roots, const std::string& configured);

}  // namespace nx
