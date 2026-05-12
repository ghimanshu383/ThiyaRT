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
#include <algorithm>
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
constexpr std::uint32_t SAMPLES = 8;
constexpr int MAX_OBJECTS_SPHERES = 100;
constexpr int MAX_OBJECTS_QUADS = 100;
constexpr int MAX_NODES = 2 * (MAX_OBJECTS_QUADS + MAX_OBJECTS_SPHERES) - 1;

struct FRAME_PUSH_STRUCT {
    std::uint32_t sampleCount;
    std::uint32_t objectCountSphere;
    std::uint32_t objectCountQuads;
    std::uint32_t treeCount;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
};

struct Interval {
    float min;
    float max;
    float pad[2];
};

struct AABB {
    Interval x;
    Interval y;
    Interval z;
};

struct Sphere {
    glm::vec4 geometry;
    glm::vec4 material;
    glm::vec4 properties;
    AABB box;
};

struct Quad {
    glm::vec4 corner;
    glm::vec4 uDir;
    glm::vec4 vDir;
    glm::vec4 normal;
    glm::vec4 material;
    glm::vec4 properties;
    AABB box;
};

struct Primitive {
    int type;
    int index;
    AABB box;
};

struct BVHNode {
    AABB box;
    int internalIndex;
    int objectIndex = -1;
    int objectType;
    int leftIndex;
    int rightIndex;
    int pad[3];
};

inline double random_double() {
    return std::rand() / (RAND_MAX + 1.f);
}

inline double random_number(double min, double max) {
    return min + (max - min) * random_double();
}

inline int random_int(int min, int max) {
    return int(random_number(min, max + 1));
}

inline Interval get_bounding_interval(Interval a, Interval b) {
    Interval inter{};
    inter.min = fmin(a.min, b.min);
    inter.max = fmax(a.max, b.max);
    return inter;
}

inline AABB box_from(AABB &a, AABB &b) {
    AABB box{};
    box.x = get_bounding_interval(a.x, b.x);
    box.y = get_bounding_interval(a.y, b.y);
    box.z = get_bounding_interval(a.z, b.z);
    return box;
}

inline bool box_compare(AABB &a, AABB &b, int axis) {
    switch (axis) {
        case 0:
            return a.x.min < b.x.min;
        case 1:
            return a.y.min < b.y.min;
        case 2:
            return a.z.min < b.z.min;
        default:
            return a.x.min < b.x.min;
    }
}

inline bool box_compare_x(Primitive &a, Primitive &b) {
    return box_compare(a.box, b.box, 0);
}

inline bool box_compare_y(Primitive &a, Primitive &b) {
    return box_compare(a.box, b.box, 1);
}

inline bool box_compare_z(Primitive &a, Primitive &b) {
    return box_compare(a.box, b.box, 2);
}

inline void create_bounding_box_for_sphere(Sphere &sphere) {
    Interval X{};
    Interval Y{};
    Interval Z{};
    X.min = sphere.geometry.x - sphere.geometry.w;
    X.max = sphere.geometry.x + sphere.geometry.w;
    Y.min = sphere.geometry.y - sphere.geometry.w;
    Y.max = sphere.geometry.y + sphere.geometry.w;
    Z.min = sphere.geometry.z - sphere.geometry.w;
    Z.max = sphere.geometry.z + sphere.geometry.w;

    AABB box{X, Y, Z};
    sphere.box = box;
}

inline AABB create_aabb_from_points(glm::vec3 pointOne, glm::vec3 pointTwo) {
    Interval X{};
    Interval Y{};
    Interval Z{};
    X.min = glm::min(pointOne.x, pointTwo.x);
    X.max = glm::max(pointOne.x, pointTwo.x);
    Y.min = glm::min(pointOne.y, pointTwo.y);
    Y.max = glm::max(pointOne.y, pointTwo.y);
    Z.min = glm::min(pointOne.z, pointTwo.z);
    Z.max = glm::max(pointOne.z, pointTwo.z);
    AABB box{X, Y, Z};
    return box;
}

inline void setup_quad_bounding_box_and_parameters(Quad &quad) {
    // creating  the bounding box;
    AABB diagonalOne = create_aabb_from_points(quad.corner, quad.corner + quad.uDir + quad.vDir);
    AABB diagonalTwo = create_aabb_from_points(quad.corner + quad.uDir, quad.corner + quad.vDir);
    quad.box = box_from(diagonalOne, diagonalTwo);

    glm::vec3 uDir = {quad.uDir.x, quad.uDir.y, quad.uDir.z};
    glm::vec3 vDir = {quad.vDir.x, quad.vDir.y, quad.vDir.z};

    auto n = glm::cross(uDir, vDir);
    quad.normal = {glm::normalize(n), 0};
    quad.properties.x = glm::dot(quad.normal, quad.corner);
    const glm::vec3 w = n / glm::dot(n, n);
    quad.properties.y = w.x;
    quad.properties.z = w.y;
    quad.properties.w = w.z;
}

inline BVHNode create_bvh_tree_nodes(List<BVHNode> &treeList, List<Primitive> &objects, int start, int end) {
    BVHNode node{};
    int randomIndex = random_int(0, 2);
    std::function<bool(Primitive &, Primitive &)> comparator =
            randomIndex == 0 ? box_compare_x : randomIndex == 1 ? box_compare_y : box_compare_z;

    int span = end - start;

    treeList.push_back(node);
    size_t currIndex = treeList.size() - 1;
    treeList[currIndex].internalIndex = currIndex;
    if (span == 0) {
        treeList[currIndex].box = objects[start].box;
        treeList[currIndex].objectIndex = objects[start].index;
        treeList[currIndex].objectType = objects[start].type;
        treeList[currIndex].leftIndex = -1;
        treeList[currIndex].rightIndex = -1;
    } else if (span == 1) {
        treeList[currIndex].box = box_from(objects[start].box, objects[start + 1].box);
        BVHNode left = create_bvh_tree_nodes(treeList, objects, start, start);
        BVHNode right = create_bvh_tree_nodes(treeList, objects, start + 1, start + 1);
        treeList[currIndex].leftIndex = left.internalIndex;
        treeList[currIndex].rightIndex = right.internalIndex;
    } else {
        std::sort(std::begin(objects) + start, std::begin(objects) + end, comparator);
        int mid = start + span / 2;
        BVHNode left = create_bvh_tree_nodes(treeList, objects, start, mid);
        BVHNode right = create_bvh_tree_nodes(treeList, objects, mid + 1, end);

        treeList[currIndex].leftIndex = left.internalIndex;
        treeList[currIndex].rightIndex = right.internalIndex;
        treeList[currIndex].box = box_from(left.box, right.box);
    }
    return treeList[currIndex];
}

struct Camera {
    glm::vec4 posAndFov;
    glm::vec4 lookAt;
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

inline void transition_image(GpuContext *ctx, VkImage &image, VkImageLayout prevLayout,
                             VkImageLayout newLayout,
                             VkPipelineStageFlags srcFlag, VkAccessFlags srcAccess, VkPipelineStageFlags dstFlag,
                             VkAccessFlags dstAccess) {
    VkCommandBuffer buffer = start_command_buffer(ctx);
    record_image_transition(buffer, image, prevLayout, newLayout, srcFlag, srcAccess, dstFlag, dstAccess);
    submit_to_queue(ctx, buffer);
}

#endif //THIYART_UTIL_H
