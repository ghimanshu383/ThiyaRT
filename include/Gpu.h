//
// Created by ghima on 03-05-2026.
//

#ifndef THIYART_GPU_H
#define THIYART_GPU_H

#include "Util.h"
#include <memory>
#include <GLFW/glfw3.h>

namespace te {
    class Gpu {
    private:
        GLFWwindow *m_window;
        std::shared_ptr<GpuContext> m_gpuContext;
        std::shared_ptr<SwapchainContext> m_swapContext;

        void init();

        void create_instance();

        void get_window_extensions(List<const char *> &extensions);

        VkDebugUtilsMessengerCreateInfoEXT create_debug_util_messenger();

        void pick_physical_device_and_create_logical_device();

        void create_logical_device(VkPhysicalDevice &device);

        bool is_device_valid(VkPhysicalDevice &device);

        VkSurfaceFormatKHR chooseSurfaceFormat();

        VkPresentModeKHR choosePresentMode();

        void create_swapchain();

        void create_command_pool();

    public:
        explicit Gpu(GLFWwindow *window);

        const std::shared_ptr<GpuContext> get_gpu_ctx() const { return m_gpuContext; }

        const std::shared_ptr<SwapchainContext> get_swap_ctx() const { return m_swapContext; }
        void cleanup();
    };
}
#endif //THIYART_GPU_H
