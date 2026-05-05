//
// Created by ghima on 03-05-2026.
//

#ifndef THIYART_GRAPHICSPIPELINE_H
#define THIYART_GRAPHICSPIPELINE_H

#include "Util.h"

namespace te {
    class ComputeRT;

    class GraphicsPipeline {
    private:
        std::shared_ptr<GpuContext> m_ctx = nullptr;
        std::shared_ptr<SwapchainContext> m_swap_ctx = nullptr;
        std::shared_ptr<ComputeRT> m_computeRT = nullptr;
        std::uint32_t m_image_count = 0;
        VkRenderPass m_render_pass = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_layout = VK_NULL_HANDLE;
        VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
        VkViewport mViewport;
        VkRect2D mScissors;
        VkFence m_render_fence {};
        VkSemaphore m_image_semaphore{};
        VkSemaphore m_image_render_finish{};
        List<VkFramebuffer > m_frame_buffers {};

        VkBuffer m_vertBufferStaging;
        VkDeviceMemory m_vertBufferMemoryStaging;
        VkBuffer m_vertBuffer;
        VkDeviceMemory m_vertBufferMemory;
        VkBuffer m_indexBufferStaging;
        VkDeviceMemory m_indexBufferMemoryStaging;
        VkBuffer m_indexBuffer;
        VkDeviceMemory m_indexBufferMemory;

        VkDescriptorSetLayout m_desLayout;
        VkDescriptorPool m_desPool;
        List<VkDescriptorSet> m_desSets;
        VkSampler m_sampler;

        void create_render_pass();

        void create_pipeline();
        void setup_descriptors();
        void create_frame_buffers();
        void create_command_buffer();
        void setup_display_quads();
        void begin_frame(VkCommandBuffer &commandBuffer);

        void draw(VkCommandBuffer &commandBuffer);

        void end_frame(VkCommandBuffer &commandBuffer);

    public:
        GraphicsPipeline(const std::shared_ptr<GpuContext>& gpuContext, const std::shared_ptr<SwapchainContext>& swapContext);
        void render();
        void clean_up();

    };
}
#endif //THIYART_GRAPHICSPIPELINE_H
