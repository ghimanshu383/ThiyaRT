//
// Created by ghima on 03-05-2026.
//

#ifndef THIYART_UTIL_H
#define THIYART_UTIL_H


#define GLFW_INCLUDE_VULKAN

#include "GLFW/glfw3.h"
#include "vulkan/vulkan.h"
#include <vector>
#include "spdlog/spdlog.h"
#include <fstream>
#include "glm.hpp"

#define LOG_INFO(M, ...) spdlog::info(M, ##__VA_ARGS__)
#define LOG_ERROR(M, ...) spdlog::error(M, ##__VA_ARGS__)
#define LOG_WARN(M, ...) spdlog::warn(M, ##__VA_ARGS__)

#define VK_CHECK(val, M, ...) if(val != VK_SUCCESS){ \
    LOG_ERROR(M, ##__VA_ARGS__);                              \
    std::exit(EXIT_FAILURE);}
template<typename T>
using List = std::vector<T>;

constexpr std::uint32_t WIN_WIDTH = 800;
constexpr uint32_t WIN_HEIGHT = 600;
constexpr std::uint32_t SAMPLES = 100;
struct FRAME_PUSH_STRUCT {
    std::uint32_t sampleCount;
};
struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
};

struct QueueFamilies {
    int32_t graphicsQueueIndex = -1;
    int32_t presentationQueueIndex = -1;
    int32_t computeQueueIndex = -1;

    bool is_valid() {
        return graphicsQueueIndex != -1 && presentationQueueIndex != -1 && computeQueueIndex != -1;
    }

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue presentationQueue = VK_NULL_HANDLE;
};

struct GpuContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    QueueFamilies queueFamilies{};
    VkCommandPool graphicsCommandPool;
};

struct SwapchainContext {
    VkExtent2D extends;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    uint32_t imageCount = 0;
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surfaceFormat{};
    VkPresentModeKHR presentMode{};
    List<VkImage> images{};
    List<VkImageView> imageViews{};
};

inline int32_t
find_memory_index(VkPhysicalDevice physicalDevice, uint32_t memoryTypeIndex, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    for (int i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((memoryTypeIndex & (1 << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)) {
            return i;
        }
    }
    return -1;
}

inline void create_image(GpuContext *ctx, VkImage &image, VkImageUsageFlags flags,
                         VkDeviceMemory &imageMemory, VkMemoryPropertyFlags memoryPropertyFlags,
                         VkFormat format, uint32_t width,
                         uint32_t height, const char *name) {
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.arrayLayers = 1;
    createInfo.mipLevels = 1;
    createInfo.format = format;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.usage = flags;
    createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

    VK_CHECK(vkCreateImage(ctx->logicalDevice, &createInfo, nullptr, &image), "Failed to create the image {}", name);
    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(ctx->logicalDevice, image, &memoryRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = find_memory_index(ctx->physicalDevice, memoryRequirements.memoryTypeBits,
                                                  memoryPropertyFlags);
    VK_CHECK(vkAllocateMemory(ctx->logicalDevice, &allocInfo, nullptr, &imageMemory),
             "Failed to allocate the memory for {}", name);
    vkBindImageMemory(ctx->logicalDevice, image, imageMemory, 0);
}

inline void
create_image_view(VkDevice device, VkImage &image, VkImageView &imageView, VkFormat format) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;

    VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView),
             "Failed to create the image view");
}

inline void create_sampler(GpuContext *ctx, VkSampler &sampler) {
    VkSamplerCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.magFilter = VK_FILTER_LINEAR;
    createInfo.minFilter = VK_FILTER_LINEAR;
    createInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(ctx->logicalDevice, &createInfo, nullptr, &sampler), "Failed to create the sampler");
}

inline void create_buffer(GpuContext *context, VkBuffer &buffer, VkDeviceMemory &memory,
                          VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags memoryFlags,
                          VkDeviceSize size) {
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = bufferUsage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(context->logicalDevice, &bufferCreateInfo, nullptr, &buffer),
             "Failed to create the buffer");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(context->logicalDevice, buffer, &requirements);
    int32_t memoryIndex = find_memory_index(context->physicalDevice, requirements.memoryTypeBits, memoryFlags);
    if (memoryIndex == -1) {
        LOG_INFO("failed to get any specified memory for the asset");
        std::exit(EXIT_FAILURE);
    }
    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.memoryTypeIndex = memoryIndex;
    allocateInfo.allocationSize = requirements.size;

    VK_CHECK(vkAllocateMemory(context->logicalDevice, &allocateInfo, nullptr, &memory),
             "Failed to allocate the memory for the asset");
    VkDebugUtilsObjectNameInfoEXT name{};
    name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name.objectType = VK_OBJECT_TYPE_DEVICE_MEMORY;
    name.objectHandle = (uint64_t) memory;
    name.pObjectName = "BufferMemory";

    PFN_vkSetDebugUtilsObjectNameEXT setName = (PFN_vkSetDebugUtilsObjectNameEXT) vkGetDeviceProcAddr(
        context->logicalDevice, "vkSetDebugUtilsObjectNameEXT");
    setName(context->logicalDevice, &name);
    vkBindBufferMemory(context->logicalDevice, buffer, memory, 0);
}

inline VkCommandBuffer start_command_buffer(GpuContext *context) {
    VkCommandBuffer commandBuffer{};
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    allocateInfo.commandPool = context->graphicsCommandPool;
    vkAllocateCommandBuffers(context->logicalDevice, &allocateInfo, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

inline void submit_command_buffer(GpuContext *ctx, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    VkFence fence{};
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(ctx->logicalDevice, &fenceCreateInfo, nullptr, &fence);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(ctx->queueFamilies.graphicsQueue, 1, &submitInfo, fence);
    vkWaitForFences(ctx->logicalDevice, 1, &fence, VK_TRUE, UINT32_MAX);
    vkResetFences(ctx->logicalDevice, 1, &fence);
    vkDestroyFence(ctx->logicalDevice, fence, nullptr);
}

inline void submit_to_queue(GpuContext *context, VkCommandBuffer &commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFence submitFence{};
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(context->logicalDevice, &fenceCreateInfo, nullptr, &submitFence);
    vkQueueSubmit(context->queueFamilies.graphicsQueue, 1, &submitInfo, submitFence);
    vkWaitForFences(context->logicalDevice, 1, &submitFence, VK_TRUE, UINT64_MAX);
    vkResetFences(context->logicalDevice, 1, &submitFence);
    vkDestroyFence(context->logicalDevice, submitFence, nullptr);
}

inline void copy_buffer_to_buffer(GpuContext *context, VkBuffer &srcBuffer, VkBuffer &dstBuffer,
                                  VkDeviceSize size) {
    VkCommandBuffer commandBuffer = start_command_buffer(context);

    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &region);
    submit_to_queue(context, commandBuffer);
}

inline void read_file_binary(const char *fileName, List<std::uint8_t> &buffer) {
    std::ifstream inputStream{fileName, std::ios::binary | std::ios::ate};
    if (!inputStream) {
        LOG_ERROR("Failed to Read the Shader Module File {}", fileName);
        std::exit(EXIT_FAILURE);
    }
    uint32_t fileSize = inputStream.tellg();
    buffer.resize(fileSize);
    inputStream.seekg(0);
    inputStream.read(reinterpret_cast<char *>(buffer.data()), fileSize);
    inputStream.close();
}

inline VkShaderModule create_shader_module(VkDevice logicalDevice, const char *filePath) {
    VkShaderModule module{};
    List<uint8_t> moduleCode{};
    VkShaderModuleCreateInfo moduleCreateInfo{};
    read_file_binary(filePath, moduleCode);

    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = moduleCode.size();
    moduleCreateInfo.pCode = reinterpret_cast<std::uint32_t *>(moduleCode.data());

    VK_CHECK(vkCreateShaderModule(logicalDevice, &moduleCreateInfo, nullptr, &module),
             "Failed to create the Shader Module");
    return module;
}
inline void record_image_transition(VkCommandBuffer &commandBuffer, VkImage &image, VkImageLayout prevLayout,
                               VkImageLayout newLayout,
                               VkPipelineStageFlags srcFlag, VkAccessFlags srcAccess, VkPipelineStageFlags dstFlag,
                               VkAccessFlags dstAccess) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = prevLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(commandBuffer,
                         srcFlag, dstFlag, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
}
inline void transition_image(GpuContext* ctx, VkImage &image, VkImageLayout prevLayout,
                               VkImageLayout newLayout,
                               VkPipelineStageFlags srcFlag, VkAccessFlags srcAccess, VkPipelineStageFlags dstFlag,
                               VkAccessFlags dstAccess) {
    VkCommandBuffer buffer = start_command_buffer(ctx);
    record_image_transition(buffer, image, prevLayout, newLayout, srcFlag, srcAccess, dstFlag, dstAccess);
    submit_to_queue(ctx, buffer);

}
#endif //THIYART_UTIL_H
