//
// Created by ghima on 03-05-2026.
//

#ifndef THIYART_APPWINDOW_H
#define THIYART_APPWINDOW_H
#include "Gpu.h"
#include "GraphicsPipeline.h"
namespace te {
    class AppWindow {
        GLFWwindow *m_window;
        void init();

        std::shared_ptr<Gpu> m_gpu;
        std::shared_ptr<GraphicsPipeline> m_graphics_pipeline {};
    public:
        AppWindow();
        void render();
    };
}
#endif //THIYART_APPWINDOW_H
