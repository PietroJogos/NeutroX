cmake_minimum_required(VERSION 3.20)
project(NeutroX VERSION 0.8.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Runtime estatico no MSVC: o .exe roda sem instalar o VC++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Nucleo portavel: nao depende de nenhuma API de sistema operacional.
# E esta biblioteca que sera compilada para o Xbox 360 (ver docs/TOOLCHAIN.md).
add_library(neutrox_core STATIC
    src/core/app.cpp
    src/core/log.cpp
    src/core/png_writer.cpp
    src/graphics/font.cpp
    src/graphics/soft_renderer.cpp
    src/graphics/ui.cpp
    src/input/input_mapper.cpp
    src/audio/mixer.cpp
    src/audio/original_sounds.cpp
    src/audio/themes/neutrox.cpp
    src/audio/themes/blades_6717.cpp
    src/audio/themes/nxe_9199.cpp
    src/audio/themes/kinect_13604.cpp
    src/audio/themes/metro_v1_15574.cpp
    src/audio/themes/metro_v2_17559.cpp
    src/settings/settings.cpp
    src/dashboards/registry.cpp
    src/dashboards/common/demo_content.cpp
    src/dashboards/blades/blades_dashboard.cpp
    src/dashboards/nxe/nxe_dashboard.cpp
    src/dashboards/kinect/kinect_dashboard.cpp
    src/dashboards/metro_v1/metro_v1_dashboard.cpp
    src/dashboards/metro_v2/metro_v2_dashboard.cpp
    src/shell/main_menu.cpp
    src/shell/screens.cpp
)
target_include_directories(neutrox_core PUBLIC src)

if(MSVC)
    target_compile_options(neutrox_core PUBLIC /utf-8 /W4 /permissive- /wd4100 /wd4244 /wd4267 /wd4305)
    target_compile_definitions(neutrox_core PUBLIC _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(neutrox_core PUBLIC -Wall -Wextra -Wno-unused-parameter)
endif()

# Xbox 360 (RGH/JTAG) com o SDK homebrew xsdk, configurado com:
#   cmake -S . -B build_xbox -G Ninja -DCMAKE_TOOLCHAIN_FILE=<xsdk>/cmake/xsdk.cmake -DXSDK_TOOLCHAIN=<prefixo>
# Gera build_xbox/NeutroX.xex (ver docs/TOOLCHAIN.md e docs/INSTALACAO_XBOX.md).
if(XSDK)
    add_executable(NeutroX src/platform/xbox360/platform_xbox360.cpp)
    # Inicializadores designados das structs C do xsdk deixam os demais campos em zero de proposito.
    set_source_files_properties(src/platform/xbox360/platform_xbox360.cpp PROPERTIES COMPILE_OPTIONS -Wno-missing-field-initializers)
    target_link_libraries(NeutroX PRIVATE neutrox_core)
    xsdk_add_shaders(NeutroX src/platform/xbox360/blit.vert src/platform/xbox360/blit.frag)
    xsdk_add_xex(NeutroX TITLE_ID 0x4E455558 NAME "Neutro X" VERSION ${PROJECT_VERSION}.0)
    return()  # ferramentas e testes de PC nao sao compilados para o console
endif()

find_package(Threads REQUIRED)
target_link_libraries(neutrox_core PUBLIC Threads::Threads)

# Pre-visualizacao no Windows.
if(WIN32)
    add_executable(NeutroX WIN32 src/platform/win32/main_win32.cpp)
    target_link_libraries(NeutroX PRIVATE neutrox_core winmm gdi32 user32 shell32 ole32)
endif()

# Ferramenta: exporta os temas sonoros RECRIADOS (originais do Neutro X) em WAV.
add_executable(neutrox_sound_export tools/sound_export.cpp)
target_link_libraries(neutrox_sound_export PRIVATE neutrox_core)

# Ferramenta: extrai a imagem de um XEX NAO criptografado (somente leitura; usada por tools/sound_extract.py).
add_executable(neutrox_xex_unpack tools/xex_unpack.cpp)

# Testes automatizados (plataforma headless).
add_executable(neutrox_tests tests/test_main.cpp)
target_link_libraries(neutrox_tests PRIVATE neutrox_core)

enable_testing()
add_test(NAME neutrox_tests COMMAND neutrox_tests ${CMAKE_BINARY_DIR}/test_output)
