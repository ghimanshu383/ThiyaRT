//
// Created by ghima on 05-05-2026.
//
#include "compute/ComputeRT.h"

namespace te {
    ComputeRT::ComputeRT(GpuContext *ctx, SwapchainContext *swapCtx,
                         const char *shaderPath) : m_ctx(ctx), m_swapCtx(swapCtx), m_shader_path(shaderPath) {
        m_descriptorSets.resize(m_swapCtx->imageCount);
        create_image_and_sampler();
        setup_descriptors();
        setup_pipeline();
    }

    void ComputeRT::create_image_and_sampler() {
        create_image(m_ctx, m_storageImage, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                     m_storageImageMemory,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_FORMAT_R8G8B8A8_UNORM, m_swapCtx->extends.width,
                     m_swapCtx->extends.height, "Compute Storage Image");
        create_image_view(m_ctx->logicalDevice, m_storageImage, m_storageImageView, VK_FORMAT_R8G8B8A8_UNORM);
        transition_image(m_ctx, m_storageImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_ACCESS_SHADER_READ_BIT);
    }

    void ComputeRT::setup_pipeline() {
        VkShaderModule module = create_shader_module(m_ctx->logicalDevice, m_shader_path);
        VkPipelineLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 1;
        layout_create_info.pSetLayouts = &m_descriptorSetLayout;

        VkPipelineShaderStageCreateInfo computeStage{};
        computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStage.module = module;
        computeStage.pName = "main";
        computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;

        VK_CHECK(vkCreatePipelineLayout(m_ctx->logicalDevice, &layout_create_info, nullptr, &m_pipelineLayout),
                 "Failed to create the pipeline layout for the compute pipeline");
        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.stage = computeStage;
        computePipelineCreateInfo.layout = m_pipelineLayout;
        computePipelineCreateInfo.basePipelineIndex = 0;

        VK_CHECK(
            vkCreateComputePipelines(m_ctx->logicalDevice, nullptr, 1, &computePipelineCreateInfo, nullptr, &m_pipeline
            ),
            "Failed to create the compute pipeline");
        LOG_INFO("The Compute Pipeline Create successfully");
        vkDestroyShaderModule(m_ctx->logicalDevice, module, nullptr);
    }

    void ComputeRT::setup_descriptors() {
        VkDescriptorSetLayoutBinding bindingOutImage{};
        bindingOutImage.binding = 0;
        bindingOutImage.descriptorCount = 1;
        bindingOutImage.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindingOutImage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

        List<VkDescriptorSetLayoutBinding> bindings{bindingOutImage};

        VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = bindings.size();
        layoutCreateInfo.pBindings = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->logicalDevice, &layoutCreateInfo, nullptr, &m_descriptorSetLayout),
                 "Failed to create the descriptor set layout for the compute");

        VkDescriptorPoolSize poolSizeStorage{};
        poolSizeStorage.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizeStorage.descriptorCount = m_swapCtx->imageCount;

        List<VkDescriptorPoolSize> poolSizes{poolSizeStorage};

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.maxSets = m_swapCtx->imageCount;
        descriptorPoolCreateInfo.poolSizeCount = poolSizes.size();
        descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
        VK_CHECK(vkCreateDescriptorPool(m_ctx->logicalDevice, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool),
                 "Failed to create the descriptor Pool for compute");
        List<VkDescriptorSetLayout> layouts(m_swapCtx->imageCount, m_descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.descriptorPool = m_descriptorPool;
        allocateInfo.descriptorSetCount = m_swapCtx->imageCount;
        allocateInfo.pSetLayouts = layouts.data();
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        vkAllocateDescriptorSets(m_ctx->logicalDevice, &allocateInfo, m_descriptorSets.data());
        LOG_INFO("The Descriptor sets allocated and configured successfully");
        for (int i = 0; i < m_swapCtx->imageCount; i++) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageInfo.imageView = m_storageImageView;
            imageInfo.sampler = VK_NULL_HANDLE;

            VkWriteDescriptorSet writeImageInfo{};
            writeImageInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeImageInfo.dstBinding = 0;
            writeImageInfo.dstSet = m_descriptorSets[i];
            writeImageInfo.dstArrayElement = 0;
            writeImageInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writeImageInfo.descriptorCount = 1;
            writeImageInfo.pImageInfo = &imageInfo;

            List<VkWriteDescriptorSet> writes{writeImageInfo};

            vkUpdateDescriptorSets(m_ctx->logicalDevice, writes.size(), writes.data(), 0, nullptr);
            LOG_INFO("Descriptor set written correctly");
        }
    }

    void ComputeRT::compute(VkCommandBuffer &commandBuffer, uint32_t currImage) {
        record_image_transition(commandBuffer, m_storageImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1,
                                &m_descriptorSets[currImage], 0, nullptr);
        vkCmdDispatch(commandBuffer, (m_swapCtx->extends.width + 15) / 16, (m_swapCtx->extends.height + 15) / 16, 1);
        record_image_transition(commandBuffer, m_storageImage, VK_IMAGE_LAYOUT_GENERAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    }
    void ComputeRT::clean_up() const{
        vkDestroyPipeline(m_ctx->logicalDevice, m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_ctx->logicalDevice, m_pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_ctx->logicalDevice, m_descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(m_ctx->logicalDevice, m_descriptorPool, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_storageImageView, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_storageImage, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_storageImageMemory, nullptr);
    }
}
