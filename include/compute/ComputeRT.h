//
// Created by ghima on 05-05-2026.
//

#ifndef COMPUTERT_H
#define COMPUTERT_H

#include "Util.h"

namespace te {
    class ComputeRT {
        bool isFirstPass = true;
        GpuContext *m_ctx;
        const char *m_shader_path;
        SwapchainContext *m_swapCtx;
        VkPipeline m_pipeline;
        VkPipelineLayout m_pipelineLayout;
        VkDescriptorSetLayout m_descriptorSetLayout;
        List<VkDescriptorSet> m_descriptorSets;
        VkDescriptorPool m_descriptorPool;
        VkImage m_storageImage = VK_NULL_HANDLE;
        VkImageView m_storageImageView = VK_NULL_HANDLE;
        VkDeviceMemory m_storageImageMemory = VK_NULL_HANDLE;
        VkSampler m_sampler;

        int m_objectCountSpheres = 0;
        int m_objectCountQuads = 0;
        int m_treeSize = 0;
        Sphere m_scene_spheres[MAX_OBJECTS_SPHERES];
        Quad m_scene_quads[MAX_OBJECTS_QUADS];
        List<BVHNode> m_tree {};
        VkBuffer m_sceneStagingBuffer{};
        VkDeviceMemory m_sceneStagingBufferMemory{};
        VkBuffer m_sceneBuffer{};
        VkDeviceMemory m_sceneBufferMemory{};
        VkBuffer m_sceneQuadsStagingBuffer;
        VkDeviceMemory m_sceneQuadsStagingBufferMemory;
        VkBuffer m_sceneQuadsBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_sceneQuadsBufferMemory = VK_NULL_HANDLE;
        VkBuffer m_treeStagingBuffer{};
        VkDeviceMemory m_treeStagingBufferMemory{};
        VkBuffer m_treeBuffer{};
        VkDeviceMemory m_treeBufferMemory{};

        void create_image_and_sampler();

        void setup_pipeline();

        void setup_descriptors();

        void setup_scene();

    public:
        ComputeRT(GpuContext *ctx, SwapchainContext *swapCtx, const char *shaderPath);

        const VkImageView &get_storage_image_view() const { return m_storageImageView; }

        void compute(VkCommandBuffer &commandBuffer, std::uint32_t currImage);

        void clean_up();
    };
}
#endif //COMPUTERT_H
