//
// Created by ghima on 03-05-2026.
//
#include <array>
#include "GraphicsPipeline.h"

#include "compute/ComputeRT.h"

namespace te {
    GraphicsPipeline::GraphicsPipeline(const std::shared_ptr<GpuContext> &gpuContext,
                                       const std::shared_ptr<SwapchainContext> &swapContext) : m_ctx{gpuContext},
        m_swap_ctx{swapContext} {
        m_desSets.resize(m_swap_ctx->imageCount);
        m_computeRT = std::make_shared<ComputeRT>(m_ctx.get(), m_swap_ctx.get(),
                                                  R"(D:\rayTracing\ThiyaRT\shaders\baseRT.comp.spv)");
        vkDeviceWaitIdle(m_ctx->logicalDevice);
        create_sampler(m_ctx.get(), m_sampler);
        create_render_pass();
        create_frame_buffers();
        setup_descriptors();
        create_pipeline();
        create_command_buffer();
        setup_display_quads();
    }

    void GraphicsPipeline::create_render_pass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_swap_ctx->surfaceFormat.format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        VkAttachmentReference colorRef{};
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorRef.attachment = 0;

        VkSubpassDescription subpassOne{};
        subpassOne.colorAttachmentCount = 1;
        subpassOne.pColorAttachments = &colorRef;
        subpassOne.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkSubpassDependency dependencyOne{};
        dependencyOne.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencyOne.dstSubpass = 0;
        dependencyOne.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencyOne.srcAccessMask = 0;
        dependencyOne.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencyOne.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencyOne.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &colorAttachment;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpassOne;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependencyOne;

        VK_CHECK(vkCreateRenderPass(m_ctx->logicalDevice, &createInfo, nullptr, &m_render_pass),
                 "Failed to create the graphics render pass");
    }

    void GraphicsPipeline::create_frame_buffers() {
        m_frame_buffers.resize(m_swap_ctx->imageCount);
        for (int i = 0; i < m_swap_ctx->imageCount; i++) {
            VkFramebufferCreateInfo framebufferCreateInfo{};
            framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferCreateInfo.layers = 1;
            framebufferCreateInfo.attachmentCount = 1;
            framebufferCreateInfo.pAttachments = &m_swap_ctx->imageViews[i];
            framebufferCreateInfo.renderPass = m_render_pass;
            framebufferCreateInfo.width = m_swap_ctx->extends.width;
            framebufferCreateInfo.height = m_swap_ctx->extends.height;
            VK_CHECK(vkCreateFramebuffer(m_ctx->logicalDevice, &framebufferCreateInfo, nullptr, &m_frame_buffers[i]),
                     "Failed to create the frame buffers");
        }
    }

    void GraphicsPipeline::create_command_buffer() {
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = m_ctx->graphicsCommandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(m_ctx->logicalDevice, &allocateInfo, &m_commandBuffer);
        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(m_ctx->logicalDevice, &semaphoreCreateInfo, nullptr, &m_image_semaphore),
                 "Failed to create the image available semaphore");
        VK_CHECK(vkCreateSemaphore(m_ctx->logicalDevice, &semaphoreCreateInfo, nullptr, &m_image_render_finish),
                 "Failed to create the render finish semaphore");
        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(m_ctx->logicalDevice, &fenceCreateInfo, nullptr, &m_render_fence),
                 "Failed to create the render fence");
    }

    void GraphicsPipeline::create_pipeline() {
        std::string vertexShaderFile = R"(D:\rayTracing\ThiyaRT\shaders\default.vert.spv)";
        std::string fragShaderFile = R"(D:\rayTracing\ThiyaRT\shaders\default.frag.spv)";

        VkShaderModule vertexShaderModule = create_shader_module(m_ctx->logicalDevice, vertexShaderFile.c_str());
        VkShaderModule fragmentShaderModule = create_shader_module(m_ctx->logicalDevice, fragShaderFile.c_str());

        VkPipelineShaderStageCreateInfo vertexShaderStage{};
        vertexShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexShaderStage.module = vertexShaderModule;
        vertexShaderStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragmentShaderStage{};
        fragmentShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentShaderStage.module = fragmentShaderModule;
        fragmentShaderStage.pName = "main";

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{vertexShaderStage, fragmentShaderStage};

        std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.dynamicStateCount = dynamicStates.size();
        dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

        mViewport.x = 0;
        mViewport.y = 0;
        mViewport.width = static_cast<std::float_t>(m_swap_ctx->extends.width);
        mViewport.height = static_cast<std::float_t>(m_swap_ctx->extends.height);
        mViewport.minDepth = 0;
        mViewport.maxDepth = 1;

        mScissors.offset = {0, 0};
        mScissors.extent = m_swap_ctx->extends;

        VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
        viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCreateInfo.viewportCount = 1;
        viewportStateCreateInfo.pViewports = &mViewport;
        viewportStateCreateInfo.scissorCount = 1;
        viewportStateCreateInfo.pScissors = &mScissors;

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.binding = 0;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription positionAttribute{};
        positionAttribute.binding = 0;
        positionAttribute.location = 0;
        positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
        positionAttribute.offset = offsetof(Vertex, pos);

        VkVertexInputAttributeDescription colorAttribute{};
        colorAttribute.binding = 0;
        colorAttribute.location = 1;
        colorAttribute.format = VK_FORMAT_R32G32_SFLOAT;
        colorAttribute.offset = offsetof(Vertex, uv);

        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{
            positionAttribute, colorAttribute
        };
        VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
        vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputStateCreateInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
        vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
        inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
        rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
        rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
        rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;;
        rasterizationStateCreateInfo.lineWidth = 1.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
        depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
        depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
        depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
        pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
        pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blendStates[1]{};

        blendStates[0].blendEnable = VK_FALSE;
        blendStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
        colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
        colorBlendStateCreateInfo.attachmentCount = 1;
        colorBlendStateCreateInfo.pAttachments = blendStates;

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.setLayoutCount = 1;
        layoutCreateInfo.pSetLayouts = &m_desLayout;

        VK_CHECK(vkCreatePipelineLayout(m_ctx->logicalDevice, &layoutCreateInfo, nullptr, &m_layout),
                 "Failed to create the graphics pipeline layout");

        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.renderPass = m_render_pass;
        pipelineCreateInfo.subpass = 0;
        pipelineCreateInfo.layout = m_layout;
        pipelineCreateInfo.stageCount = shaderStages.size();
        pipelineCreateInfo.pStages = shaderStages.data();
        pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
        pipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
        pipelineCreateInfo.pDynamicState = nullptr;
        pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
        pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
        pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;

        VK_CHECK(vkCreateGraphicsPipelines(m_ctx->logicalDevice, nullptr, 1, &pipelineCreateInfo, nullptr, &m_pipeline),
                 "Failed to create the graphics pipeline");
        vkDestroyShaderModule(m_ctx->logicalDevice, vertexShaderModule, nullptr);
        vkDestroyShaderModule(m_ctx->logicalDevice, fragmentShaderModule, nullptr);
    }

    void GraphicsPipeline::setup_descriptors() {
        VkDescriptorSetLayoutBinding bindingOutImage{};
        bindingOutImage.binding = 0;
        bindingOutImage.descriptorCount = 1;
        bindingOutImage.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindingOutImage.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        List<VkDescriptorSetLayoutBinding> bindings{bindingOutImage};

        VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = bindings.size();
        layoutCreateInfo.pBindings = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->logicalDevice, &layoutCreateInfo, nullptr, &m_desLayout),
                 "Failed to create the descriptor set layout for the graphics");

        VkDescriptorPoolSize poolSizeStorage{};
        poolSizeStorage.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizeStorage.descriptorCount = m_swap_ctx->imageCount;

        List<VkDescriptorPoolSize> poolSizes{poolSizeStorage};

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.maxSets = m_swap_ctx->imageCount;
        descriptorPoolCreateInfo.poolSizeCount = poolSizes.size();
        descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
        VK_CHECK(vkCreateDescriptorPool(m_ctx->logicalDevice, &descriptorPoolCreateInfo, nullptr, &m_desPool),
                 "Failed to create the descriptor Pool for graphics");
        List<VkDescriptorSetLayout> layouts(m_swap_ctx->imageCount, m_desLayout);
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.descriptorPool = m_desPool;
        allocateInfo.descriptorSetCount = m_swap_ctx->imageCount;
        allocateInfo.pSetLayouts = layouts.data();
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        vkAllocateDescriptorSets(m_ctx->logicalDevice, &allocateInfo, m_desSets.data());
        LOG_INFO("The Descriptor sets allocated and configured successfully");
        for (int i = 0; i < m_swap_ctx->imageCount; i++) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = m_computeRT->get_storage_image_view();
            imageInfo.sampler = m_sampler;

            VkWriteDescriptorSet writeImageInfo{};
            writeImageInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeImageInfo.dstBinding = 0;
            writeImageInfo.dstSet = m_desSets[i];
            writeImageInfo.dstArrayElement = 0;
            writeImageInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeImageInfo.descriptorCount = 1;
            writeImageInfo.pImageInfo = &imageInfo;

            List<VkWriteDescriptorSet> writes{writeImageInfo};

            vkUpdateDescriptorSets(m_ctx->logicalDevice, writes.size(), writes.data(), 0, nullptr);
            LOG_INFO("Descriptor set written correctly");
        }
    }

    void GraphicsPipeline::begin_frame(VkCommandBuffer &commandBuffer) {
        vkWaitForFences(m_ctx->logicalDevice, 1, &m_render_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(m_ctx->logicalDevice, 1, &m_render_fence);
        vkAcquireNextImageKHR(m_ctx->logicalDevice, m_swap_ctx->swapchain, UINT64_MAX, m_image_semaphore, nullptr,
                              &m_image_count);

        vkResetCommandBuffer(commandBuffer, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        m_computeRT->compute(commandBuffer, m_image_count);

        VkClearValue clearValue{};
        clearValue.color = {{.2, .2, .2, .2}};
        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.framebuffer = m_frame_buffers[m_image_count];
        renderPassBeginInfo.renderPass = m_render_pass;
        renderPassBeginInfo.renderArea = {0, 0, m_swap_ctx->extends};
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;
        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    }

    void GraphicsPipeline::draw(VkCommandBuffer &commandBuffer) {
        VkDeviceSize offset{};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertBuffer, &offset);
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_layout, 0, 1,
                                &m_desSets[m_image_count], 0, nullptr);
        vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
    }

    void GraphicsPipeline::end_frame(VkCommandBuffer &commandBuffer) {
        vkCmdEndRenderPass(commandBuffer);
        vkEndCommandBuffer(commandBuffer);

        List<VkPipelineStageFlags> wait{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_image_semaphore;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &m_image_render_finish;
        submitInfo.pWaitDstStageMask = wait.data();

        vkQueueSubmit(m_ctx->queueFamilies.graphicsQueue, 1, &submitInfo, m_render_fence);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_image_render_finish;
        presentInfo.swapchainCount = 1;
        presentInfo.pImageIndices = &m_image_count;
        presentInfo.pSwapchains = &m_swap_ctx->swapchain;

        vkQueuePresentKHR(m_ctx->queueFamilies.presentationQueue, &presentInfo);
    }

    void GraphicsPipeline::render() {
        begin_frame(m_commandBuffer);
        draw(m_commandBuffer);
        end_frame(m_commandBuffer);
    }

    void GraphicsPipeline::setup_display_quads() {
        std::vector<Vertex> vert{
            {{-1, -1, 0}, {0, 0}},
            {{-1, 1, 0}, {0, 1}},
            {{1, 1, 0}, {1, 1}},
            {{1, -1, 0}, {1, 0}}
        };
        std::vector<std::uint32_t> indices{
            0, 1, 2,
            0, 2, 3
        };
        create_buffer(m_ctx.get(), m_vertBufferStaging, m_vertBufferMemoryStaging,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      vert.size() * sizeof(Vertex));
        create_buffer(m_ctx.get(), m_vertBuffer, m_vertBufferMemory,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vert.size() * sizeof(Vertex));
        create_buffer(m_ctx.get(), m_indexBufferStaging, m_indexBufferMemoryStaging,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                      indices.size() * sizeof(uint32_t));
        create_buffer(m_ctx.get(), m_indexBuffer, m_indexBufferMemory,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indices.size() * sizeof(uint32_t));

        void *data;
        vkMapMemory(m_ctx->logicalDevice, m_vertBufferMemoryStaging, 0,
                    vert.size() * sizeof(Vertex), 0, &data);
        memcpy(data, vert.data(), vert.size() * sizeof(Vertex));
        vkUnmapMemory(m_ctx->logicalDevice, m_vertBufferMemoryStaging);
        copy_buffer_to_buffer(m_ctx.get(), m_vertBufferStaging, m_vertBuffer,
                              vert.size() * sizeof(Vertex));

        vkMapMemory(m_ctx->logicalDevice, m_indexBufferMemoryStaging, 0,
                    indices.size() * sizeof(uint32_t), 0, &data);
        memcpy(data, indices.data(), indices.size() * sizeof(uint32_t));
        vkUnmapMemory(m_ctx->logicalDevice, m_indexBufferMemoryStaging);
        copy_buffer_to_buffer(m_ctx.get(), m_indexBufferStaging, m_indexBuffer,
                              indices.size() * sizeof(uint32_t));

        vkDestroyBuffer(m_ctx->logicalDevice, m_vertBufferStaging, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_vertBufferMemoryStaging, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, m_indexBufferStaging, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_indexBufferMemoryStaging, nullptr);

        LOG_INFO("Display Quad Created Successfully");
    }

    void GraphicsPipeline::clean_up() const {
        vkDeviceWaitIdle(m_ctx->logicalDevice);
        m_computeRT->clean_up();

        vkDestroySemaphore(m_ctx->logicalDevice, m_image_semaphore, nullptr);
        vkDestroySemaphore(m_ctx->logicalDevice, m_image_render_finish, nullptr);
        vkDestroyFence(m_ctx->logicalDevice, m_render_fence, nullptr);

        vkDestroyBuffer(m_ctx->logicalDevice, m_vertBuffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_vertBufferMemory, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, m_indexBuffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_indexBufferMemory, nullptr);

        for (int i = 0; i < m_swap_ctx->imageCount; i++) {
            vkDestroyFramebuffer(m_ctx->logicalDevice, m_frame_buffers[i], nullptr);
        }
        vkDestroyPipeline(m_ctx->logicalDevice, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_ctx->logicalDevice, m_layout, nullptr);
        vkDestroyRenderPass(m_ctx->logicalDevice, m_render_pass, nullptr);

        vkDestroyDescriptorSetLayout(m_ctx->logicalDevice, m_desLayout, nullptr);
        vkDestroyDescriptorPool(m_ctx->logicalDevice, m_desPool, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_sampler, nullptr);
    }
}
