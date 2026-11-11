// Neutro X - base e registro dos dashboards.
#pragma once
#include <memory>
#include <string>
#include "core/screen.h"

namespace nx {

enum class DashboardId : int { Blades = 0, Nxe, Kinect, MetroV1, MetroV2, Count };
constexpr int kDashboardCount = int(DashboardId::Count);

class Dashboard;

struct DashboardInfo {
    DashboardId id;
    const char* key;          // usado no arquivo de configuracao
    const char* name;         // nome exibido
    const char* version;      // versao historica de referencia
    const char* year;
    const char* tagline;      // frase curta
    const char* description;  // descricao (pt-BR)
    Color accent;
    int phase;                // fase do plano em que a recriacao completa e entregue
    const char* const* sections;
    int sectionCount;
    // Desenha uma representacao da interface em coordenadas logicas 1280x720.
    // Usada para miniaturas, pre-visualizacoes e fundo provisorio.
    void (*drawPreview)(IRenderer& r, float time);
    std::unique_ptr<Dashboard> (*create)();
};

const DashboardInfo& dashboardInfo(DashboardId id);

// Tema sonoro de cada interface (a ordem de SoundTheme segue a de DashboardId).
inline SoundTheme soundThemeFor(DashboardId id) { return SoundTheme(int(id) + 1); }
const DashboardInfo* findDashboard(const std::string& key);

class Dashboard : public Screen {
public:
    explicit Dashboard(DashboardId id) : id_(id) {}
    DashboardId id() const { return id_; }
    const DashboardInfo& info() const { return dashboardInfo(id_); }
    const char* name() const override { return info().name; }
    bool wantsGlobalMenu() const override { return true; }

    // Carrega recursos pesados. Retorna false (com mensagem) em caso de erro.
    virtual bool load(Context& ctx, std::string& error) = 0;
    // Libera TODOS os recursos alocados em load().
    virtual void unload(Context& ctx) = 0;
    // Estado interno resumido (para testes automatizados e log).
    virtual std::string debugState() const { return ""; }

private:
    DashboardId id_;
};

// Previews (um por dashboard; ver dashboards/<nome>/).
void drawBladesPreview(IRenderer& r, float time);
void drawNxePreview(IRenderer& r, float time);
void drawKinectPreview(IRenderer& r, float time);
void drawMetroV1Preview(IRenderer& r, float time);
void drawMetroV2Preview(IRenderer& r, float time);

// Fabricas.
std::unique_ptr<Dashboard> createPlaceholderDashboard(DashboardId id);
std::unique_ptr<Dashboard> createBladesDashboard();
std::unique_ptr<Dashboard> createNxeDashboard();

}  // namespace nx
