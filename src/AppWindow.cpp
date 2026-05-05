//
// Created by ghima on 03-05-2026.
//
#include "AppWindow.h"
#include "Util.h"

namespace te {

    void AppWindow::init() {
        if (!glfwInit()) {
            LOG_ERROR("Failed to initialize the window");
            std::exit(EXIT_FAILURE);
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Thiya RT", nullptr, nullptr);
        if (m_window == nullptr) {
            LOG_ERROR("Failed to create the window ");
            std::exit(EXIT_FAILURE);
        }
        m_gpu = std::make_shared<Gpu>(m_window);
        m_graphics_pipeline = std::make_shared<GraphicsPipeline>(m_gpu->get_gpu_ctx(), m_gpu->get_swap_ctx());
    }

    void AppWindow::render() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();
            m_graphics_pipeline->render();
        }
        m_graphics_pipeline->clean_up();
        m_gpu->cleanup();
    }

    AppWindow::AppWindow() {
        init();
    }
}