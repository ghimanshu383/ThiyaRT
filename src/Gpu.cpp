//
// Created by ghima on 03-05-2026.
//
#include "Gpu.h"
#include "GLFW/glfw3.h"
#include <set>
#include <array>

namespace te {

    static VKAPI_ATTR VkBool32

    VKAPI_CALL ValidationCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                  VkDebugUtilsMessageTypeFlagsEXT type,
                                  const VkDebugUtilsMessengerCallbackDataEXT *callback,
                                  void *userData) {
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            LOG_ERROR("VALIDATION_ERROR : {}", callback->pMessage);
        } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            LOG_WARN("VALIDATION_WARNING : {}", callback->pMessage);
        } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
            LOG_INFO("VALIDATION_INFO : {}", callback->pMessage);
        }

        return VK_FALSE;
    }

    Gpu::Gpu(GLFWwindow *window) : m_window{window} {
        m_gpuContext = std::make_shared<GpuContext>();
        m_swapContext = std::make_shared<SwapchainContext>();
        init();
    }

    void Gpu::init() {
        create_instance();
        pick_physical_device_and_create_logical_device();
        create_swapchain();
        create_command_pool();
    }

    void Gpu::cleanup() {
        for (int i = 0; i < m_swapContext->imageCount; i++) {
            vkDestroyImageView(m_gpuContext->logicalDevice, m_swapContext->imageViews[i], nullptr);
        }
        vkDestroySwapchainKHR(m_gpuContext->logicalDevice, m_swapContext->swapchain, nullptr);
        vkDestroyCommandPool(m_gpuContext->logicalDevice, m_gpuContext->graphicsCommandPool, nullptr);
        vkDestroyDevice(m_gpuContext->logicalDevice, nullptr);
        vkDestroySurfaceKHR(m_gpuContext->instance, m_swapContext->surface, nullptr);
        vkDestroyInstance(m_gpuContext->instance, nullptr);
    }

    void Gpu::create_instance() {
        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.apiVersion = VK_API_VERSION_1_1;
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pApplicationName = "Thiya3DRT";
        applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "Thiya3DRTEngine";

        List<const char *> extensions = {"VK_EXT_debug_utils"};
        get_window_extensions(extensions);
        List<const char *> layers = {"VK_LAYER_KHRONOS_validation"};

        VkInstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &applicationInfo;
        instanceCreateInfo.enabledExtensionCount = extensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
        instanceCreateInfo.enabledLayerCount = layers.size();
        instanceCreateInfo.ppEnabledLayerNames = layers.data();
        VkDebugUtilsMessengerCreateInfoEXT messenger = create_debug_util_messenger();
        instanceCreateInfo.pNext = &messenger;
        VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_gpuContext->instance),
                 "Failed to create the instance");
        VK_CHECK(glfwCreateWindowSurface(m_gpuContext->instance, m_window, nullptr, &m_swapContext->surface),
                 "Failed to create the window Surface");
    }

    VkDebugUtilsMessengerCreateInfoEXT Gpu::create_debug_util_messenger() {
        VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfoExt{};
        messengerCreateInfoExt.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerCreateInfoExt.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        messengerCreateInfoExt.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        messengerCreateInfoExt.pfnUserCallback = ValidationCallback;
        return messengerCreateInfoExt;
    }

    void Gpu::get_window_extensions(List<const char *> &extensions) {
        std::uint32_t extensionCount{};
        const char **ext = glfwGetRequiredInstanceExtensions(&extensionCount);
        for (int i = 0; i < extensionCount; i++) {
            extensions.push_back(ext[i]);
        }
    }

    void Gpu::pick_physical_device_and_create_logical_device() {
        std::uint32_t count;
        List<VkPhysicalDevice> devices{};
        vkEnumeratePhysicalDevices(m_gpuContext->instance, &count, nullptr);
        devices.resize(count);
        vkEnumeratePhysicalDevices(m_gpuContext->instance, &count, devices.data());
        List<VkPhysicalDevice>::iterator iter = devices.begin();
        while (iter != devices.end()) {
            if (is_device_valid(*iter)) {
                m_gpuContext->physicalDevice = *iter;
                break;
            }
            iter++;
        }
        if (m_gpuContext->physicalDevice == VK_NULL_HANDLE) {
            LOG_ERROR("Failed to find any valid physical device");
            std::exit(EXIT_FAILURE);
        }
        VkPhysicalDeviceProperties deviceProperties{};
        vkGetPhysicalDeviceProperties(m_gpuContext->physicalDevice, &deviceProperties);
        LOG_INFO("The Device Name {}, Graphics Queue Index {}, Compute Queue {}, Presentation Queue Index {}",
                 deviceProperties.deviceName, m_gpuContext->queueFamilies.graphicsQueueIndex,
                 m_gpuContext->queueFamilies.computeQueueIndex, m_gpuContext->queueFamilies.presentationQueueIndex);
        create_logical_device(m_gpuContext->physicalDevice);
    }

    bool Gpu::is_device_valid(VkPhysicalDevice &device) {
        std::uint32_t count = 0;
        List<VkQueueFamilyProperties> properties{};
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        properties.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());
        auto graphicsIter = std::find_if(properties.begin(), properties.end(),
                                         [](VkQueueFamilyProperties property) -> bool {
                                             return (property.queueFlags &
                                                     (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT));
                                         });
        if (graphicsIter == properties.end()) return false;
        m_gpuContext->queueFamilies.graphicsQueueIndex = (int) (graphicsIter - properties.begin());
        auto computeIter = std::find_if(properties.begin(), properties.end(),
                                        [](VkQueueFamilyProperties property) -> bool {
                                            return property.queueFlags & VK_QUEUE_COMPUTE_BIT;
                                        });
        if (computeIter == properties.end()) return false;
        m_gpuContext->queueFamilies.computeQueueIndex = (int) (computeIter - properties.begin());
        for (int i = 0; i < count; i++) {
            VkBool32 isPresent = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_swapContext->surface, &isPresent);
            if (isPresent) {
                m_gpuContext->queueFamilies.presentationQueueIndex = i;
                break;
            }
        }
        return m_gpuContext->queueFamilies.is_valid();
    }

    void Gpu::create_logical_device(VkPhysicalDevice &device) {
        std::set<int> indices{m_gpuContext->queueFamilies.graphicsQueueIndex,
                              m_gpuContext->queueFamilies.computeQueueIndex,
                              m_gpuContext->queueFamilies.presentationQueueIndex};
        List<VkDeviceQueueCreateInfo> infos{};
        float priority = 1.f;
        for (int i: indices) {
            VkDeviceQueueCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            createInfo.queueCount = 1;
            createInfo.queueFamilyIndex = static_cast<uint32_t>(i);
            createInfo.pQueuePriorities = &priority;
            infos.push_back(createInfo);
        }
        List<const char *> requiredExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                 VK_EXT_NESTED_COMMAND_BUFFER_EXTENSION_NAME};
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.enabledExtensionCount = requiredExtensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();
        deviceCreateInfo.queueCreateInfoCount = infos.size();
        deviceCreateInfo.pQueueCreateInfos = infos.data();

        VK_CHECK(vkCreateDevice(device, &deviceCreateInfo, nullptr, &m_gpuContext->logicalDevice),
                 "Failed to create the logical Device");
        vkGetDeviceQueue(m_gpuContext->logicalDevice, m_gpuContext->queueFamilies.graphicsQueueIndex, 0,
                         &m_gpuContext->queueFamilies.graphicsQueue);
        vkGetDeviceQueue(m_gpuContext->logicalDevice, m_gpuContext->queueFamilies.computeQueueIndex, 0,
                         &m_gpuContext->queueFamilies.computeQueue);
        vkGetDeviceQueue(m_gpuContext->logicalDevice, m_gpuContext->queueFamilies.presentationQueueIndex, 0,
                         &m_gpuContext->queueFamilies.presentationQueue);
        LOG_INFO("Device Created Successfully");
    }

    VkPresentModeKHR Gpu::choosePresentMode() {
        std::uint32_t count;
        List<VkPresentModeKHR> modes{};
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_gpuContext->physicalDevice, m_swapContext->surface, &count,
                                                  nullptr);
        modes.resize(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_gpuContext->physicalDevice, m_swapContext->surface, &count,
                                                  modes.data());

        auto iter = std::find_if(modes.begin(), modes.end(),
                                 [](VkPresentModeKHR mode) -> bool { return mode == VK_PRESENT_MODE_MAILBOX_KHR; });
        if (iter == modes.end()) return VK_PRESENT_MODE_FIFO_KHR;
        return *iter;
    }

    VkSurfaceFormatKHR Gpu::chooseSurfaceFormat() {
        std::uint32_t count;
        List<VkSurfaceFormatKHR> formats{};
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_gpuContext->physicalDevice, m_swapContext->surface, &count, nullptr);
        formats.resize(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_gpuContext->physicalDevice, m_swapContext->surface, &count,
                                             formats.data());
        if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
            return {VkFormat::VK_FORMAT_R8G8B8A8_UNORM, VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }
        return formats[0];
    }

    void Gpu::create_swapchain() {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_gpuContext->physicalDevice, m_swapContext->surface,
                                                  &m_swapContext->surfaceCapabilities);
        m_swapContext->surfaceFormat = chooseSurfaceFormat();
        m_swapContext->presentMode = choosePresentMode();
        m_swapContext->extends = m_swapContext->surfaceCapabilities.currentExtent;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.minImageCount = m_swapContext->surfaceCapabilities.minImageCount + 1;
        createInfo.surface = m_swapContext->surface;
        createInfo.presentMode = m_swapContext->presentMode;
        createInfo.imageFormat = m_swapContext->surfaceFormat.format;
        createInfo.imageColorSpace = m_swapContext->surfaceFormat.colorSpace;
        createInfo.imageExtent = m_swapContext->extends;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.preTransform = m_swapContext->surfaceCapabilities.currentTransform;
        createInfo.imageArrayLayers = 1;
        createInfo.oldSwapchain = nullptr;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.clipped = VK_TRUE;
        if (m_gpuContext->queueFamilies.graphicsQueueIndex != m_gpuContext->queueFamilies.computeQueueIndex) {
            std::array<std::uint32_t, 2> indices{(std::uint32_t) m_gpuContext->queueFamilies.graphicsQueueIndex,
                                                 (std::uint32_t) m_gpuContext->queueFamilies.computeQueueIndex};
            createInfo.queueFamilyIndexCount = indices.size();
            createInfo.pQueueFamilyIndices = indices.data();
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        VK_CHECK(vkCreateSwapchainKHR(m_gpuContext->logicalDevice, &createInfo, nullptr, &m_swapContext->swapchain),
                 "Failed to create the swapchain ");
        LOG_INFO("Swapchain Created Successfully");
        std::uint32_t imageCount{};
        vkGetSwapchainImagesKHR(m_gpuContext->logicalDevice, m_swapContext->swapchain, &imageCount, nullptr);
        m_swapContext->images.resize(imageCount);
        vkGetSwapchainImagesKHR(m_gpuContext->logicalDevice, m_swapContext->swapchain, &imageCount,
                                m_swapContext->images.data());
        m_swapContext->imageCount = imageCount;
        m_swapContext->imageViews.resize(imageCount);
        for (int i = 0; i < imageCount; i++) {
            create_image_view(m_gpuContext->logicalDevice, m_swapContext->images[i], m_swapContext->imageViews[i],
                              m_swapContext->surfaceFormat.format);
        }
    }

    void Gpu::create_command_pool() {
        VkCommandPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreateInfo.queueFamilyIndex = m_gpuContext->queueFamilies.graphicsQueueIndex;
        poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(m_gpuContext->logicalDevice, &poolCreateInfo, nullptr,
                                     &m_gpuContext->graphicsCommandPool), "Failed to create the command Pool");
        LOG_INFO("command Pool Created Successfully");
    }
}