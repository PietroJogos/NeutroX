// Neutro X - registro dos cinco dashboards.
#include <cstring>
#include "dashboards/dashboard.h"

namespace nx {

namespace {
std::unique_ptr<Dashboard> mkBlades() { return createBladesDashboard(); }
std::unique_ptr<Dashboard> mkNxe() { return createNxeDashboard(); }
std::unique_ptr<Dashboard> mkKinect() { return createKinectDashboard(); }
std::unique_ptr<Dashboard> mkMetroV1() { return createMetroV1Dashboard(); }
std::unique_ptr<Dashboard> mkMetroV2() { return createPlaceholderDashboard(DashboardId::MetroV2); }

const char* const kBladesSections[] = {"Xbox Live", "Jogos", "Mídia", "Sistema"};
const char* const kNxeSections[] = {"Meu Xbox", "Biblioteca de Jogos", "Mídia", "Configurações"};
const char* const kKinectSections[] = {"Início", "Jogos", "Aplicativos", "Música", "Vídeos", "Configurações"};
const char* const kMetroV1Sections[] = {"início", "social", "tv", "vídeo", "jogos", "música", "aplicativos", "configurações"};
const char* const kMetroV2Sections[] = {"início", "jogos", "filmes e tv", "aplicativos", "configurações"};

#define NX_COUNT(a) int(sizeof(a) / sizeof((a)[0]))

const DashboardInfo kInfos[kDashboardCount] = {
    {DashboardId::Blades, "blades", "Blades", "2.0.6717.0", "2008", "Lâminas verticais clássicas",
     "Lâminas coloridas que deslizam na horizontal, painéis com profundidade e tons de verde e cinza.",
     Color::hex(0x5FAE2A), 4, kBladesSections, NX_COUNT(kBladesSections), drawBladesPreview, mkBlades},
    {DashboardId::Nxe, "nxe", "NXE", "2.0.9199.0", "2010", "New Xbox Experience",
     "Canais empilhados, cartões grandes em profundidade e um avatar genérico no centro.",
     Color::hex(0x8CC63A), 5, kNxeSections, NX_COUNT(kNxeSections), drawNxePreview, mkNxe},
    {DashboardId::Kinect, "kinect", "Kinect", "2.0.13604.0", "2011", "Cartões grandes da era Kinect",
     "Cartões amplos sobre ondas roxas e verdes. Funciona só com o controle, sem sensor.",
     Color::hex(0x7A4FB0), 6, kKinectSections, NX_COUNT(kKinectSections), drawKinectPreview, mkKinect},
    {DashboardId::MetroV1, "metro_v1", "Metro V1", "2.0.15574.0", "2012", "Blocos claros e tipografia leve",
     "Categorias em tipografia leve, grupos de blocos retangulares e fundo claro.",
     Color::hex(0x5DAE2A), 7, kMetroV1Sections, NX_COUNT(kMetroV1Sections), drawMetroV1Preview, mkMetroV1},
    {DashboardId::MetroV2, "metro_v2", "Metro V2", "2.0.17559.0", "2019", "Tema escuro final",
     "Geração final: fundo escuro, destaques verdes e navegação rápida entre blocos.",
     Color::hex(0x1F8A1F), 8, kMetroV2Sections, NX_COUNT(kMetroV2Sections), drawMetroV2Preview, mkMetroV2},
};
}  // namespace

const DashboardInfo& dashboardInfo(DashboardId id) {
    int i = int(id);
    if (i < 0 || i >= kDashboardCount) i = 0;
    return kInfos[i];
}

const DashboardInfo* findDashboard(const std::string& key) {
    for (const DashboardInfo& d : kInfos)
        if (key == d.key) return &d;
    return nullptr;
}

}  // namespace nx
