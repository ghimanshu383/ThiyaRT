//
// Created by ghima on 05-05-2026.
//
#include "compute/ComputeRT.h"

#include "spdlog/fmt/bundled/chrono.h"

namespace te {
    ComputeRT::ComputeRT(GpuContext *ctx, SwapchainContext *swapCtx,
                         const char *shaderPath) : m_ctx(ctx), m_swapCtx(swapCtx), m_shader_path(shaderPath) {
        m_tree.reserve(MAX_NODES);
        Sphere sphereOne{};
        sphereOne.geometry = glm::vec4{0, 0, -1, .5};
        sphereOne.material = glm::vec4{0.1, 0.2, 0.5, 1};
        create_bounding_box_for_sphere(sphereOne);
        m_scene_spheres[0] = sphereOne;
        m_objectCountSpheres++;

        Sphere sphereGround{};
        sphereGround.geometry = glm::vec4(0, -100.5, -1, 100);
        sphereGround.material = glm::vec4(0.8, 0.8, 0.0, 1);
        create_bounding_box_for_sphere(sphereGround);
        m_scene_spheres[1] = sphereGround;
        m_objectCountSpheres++;

        Sphere sphereMetalLeft{};
        sphereMetalLeft.geometry = glm::vec4{-1.0, 0.0, -1.0, .5};
        sphereMetalLeft.material = glm::vec4{.9, .9, .9, 3};
        sphereMetalLeft.properties = glm::vec4{0, (1.f / 1.5f), 0, 0};
        create_bounding_box_for_sphere(sphereMetalLeft);
        m_scene_spheres[2] = sphereMetalLeft;
        m_objectCountSpheres++;
        Sphere bubble{};
        bubble.geometry = glm::vec4{-1, 0.f, -1.0, .4f};
        bubble.material = glm::vec4{1, 1, 1, 3};
        bubble.properties = glm::vec4{0, (1.5 / 1), 0, 0};
        create_bounding_box_for_sphere(bubble);
        m_scene_spheres[3] = bubble;
        m_objectCountSpheres++;
        Sphere sphereMetalRight{};
        sphereMetalRight.geometry = glm::vec4{1.0, 0.0, -1.0, .5};
        sphereMetalRight.material = glm::vec4{0.9, 0.9, 0.9, 2};
        sphereMetalRight.properties = glm::vec4{.1, 0, 0, 0};
        create_bounding_box_for_sphere(sphereMetalRight);
        m_scene_spheres[4] = sphereMetalRight;
        m_objectCountSpheres++;

        Quad redQuad {};
        redQuad.corner = glm::vec4{0, 1, 0, 0};
        redQuad.uDir = glm::vec4{0, 0, -1, 0};
        redQuad.vDir = glm::vec4{ 1 , 0, 0, 0};
        redQuad.material = glm::vec4{1, .2, .2, 1};
        setup_quad_bounding_box_and_parameters(redQuad);
        m_scene_quads[0] = redQuad;
        m_objectCountQuads++;

        List<Primitive> primitiveList{};
        for (int i = 0; i < m_objectCountSpheres; i++) {
            primitiveList.push_back({0, i, m_scene_spheres[i].box});
        }
        for (int i = 0 ; i < m_objectCountQuads; i++) {
            primitiveList.push_back({1, i, m_scene_quads[i].box});
        }
        create_bvh_tree_nodes(m_tree, primitiveList, 0, primitiveList.size() - 1);
        for (int i = 0; i < m_tree.size(); i++) {
            printf("node %d: objectIndex=%d left=%d right=%d\n",
                   i, m_tree[i].objectIndex, m_tree[i].leftIndex, m_tree[i].rightIndex);
        }
        setup_scene();
        m_descriptorSets.resize(m_swapCtx->imageCount);
        create_image_and_sampler();
        setup_descriptors();
        setup_pipeline();
    }

    void ComputeRT::create_image_and_sampler() {
        create_image(m_ctx, m_storageImage, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                     m_storageImageMemory,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_FORMAT_R32G32B32A32_SFLOAT, m_swapCtx->extends.width,
                     m_swapCtx->extends.height, "Compute Storage Image");
        create_image_view(m_ctx->logicalDevice, m_storageImage, m_storageImageView, VK_FORMAT_R32G32B32A32_SFLOAT);
        transition_image(m_ctx, m_storageImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_ACCESS_SHADER_READ_BIT);
    }

    void ComputeRT::setup_pipeline() {
        VkShaderModule module = create_shader_module(m_ctx->logicalDevice, m_shader_path);

        VkPipelineShaderStageCreateInfo computeStage{};
        computeStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStage.module = module;
        computeStage.pName = "main";
        computeStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;

        VkPushConstantRange pushRange{};
        pushRange.offset = 0;
        pushRange.size = sizeof(FRAME_PUSH_STRUCT);
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkPipelineLayoutCreateInfo layout_create_info{};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 1;
        layout_create_info.pSetLayouts = &m_descriptorSetLayout;
        layout_create_info.pushConstantRangeCount = 1;
        layout_create_info.pPushConstantRanges = &pushRange;

        VK_CHECK(vkCreatePipelineLayout(m_ctx->logicalDevice, &layout_create_info, nullptr, &m_pipelineLayout),
                 "Failed to create the pipeline layout for the compute pipeline");
        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.stage = computeStage;
        computePipelineCreateInfo.layout = m_pipelineLayout;
        computePipelineCreateInfo.basePipelineIndex = 0;

        VK_CHECK(
            vkCreateComputePipelines(m_ctx->logicalDevice, nullptr, 1, &computePipelineCreateInfo, nullptr,
                &m_pipeline
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


        VkDescriptorSetLayoutBinding bindingSceneSpheres{};
        bindingSceneSpheres.binding = 1;
        bindingSceneSpheres.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingSceneSpheres.descriptorCount = 1;
        bindingSceneSpheres.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding bindingTree{};
        bindingTree.binding = 2;
        bindingTree.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingTree.descriptorCount = 1;
        bindingTree.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding bindingSceneQuad{};
        bindingSceneQuad.binding = 3;
        bindingSceneQuad.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindingSceneQuad.descriptorCount = 1;
        bindingSceneQuad.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        List<VkDescriptorSetLayoutBinding> bindings{
            bindingOutImage, bindingSceneSpheres, bindingTree, bindingSceneQuad
        };

        VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = bindings.size();
        layoutCreateInfo.pBindings = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->logicalDevice, &layoutCreateInfo, nullptr, &m_descriptorSetLayout),
                 "Failed to create the descriptor set layout for the compute");

        VkDescriptorPoolSize poolSizeStorage{};
        poolSizeStorage.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizeStorage.descriptorCount = m_swapCtx->imageCount;

        VkDescriptorPoolSize poolUniform{};
        poolUniform.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolUniform.descriptorCount = 3 * m_swapCtx->imageCount;

        List<VkDescriptorPoolSize> poolSizes{poolSizeStorage, poolUniform};

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

            VkDescriptorBufferInfo sceneBuffer{};
            sceneBuffer.offset = 0;
            sceneBuffer.range = sizeof(Sphere) * MAX_OBJECTS_SPHERES;
            sceneBuffer.buffer = m_sceneBuffer;
            VkWriteDescriptorSet writeScene{};
            writeScene.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeScene.descriptorCount = 1;
            writeScene.dstBinding = 1;
            writeScene.dstArrayElement = 0;
            writeScene.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeScene.dstSet = m_descriptorSets[i];
            writeScene.pBufferInfo = &sceneBuffer;

            VkDescriptorBufferInfo treeBufferInfo{};
            treeBufferInfo.offset = 0;
            treeBufferInfo.range = sizeof(BVHNode) * MAX_NODES;
            treeBufferInfo.buffer = m_treeBuffer;
            VkWriteDescriptorSet writeTree{};
            writeTree.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeTree.descriptorCount = 1;
            writeTree.dstBinding = 2;
            writeTree.dstArrayElement = 0;
            writeTree.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeTree.dstSet = m_descriptorSets[i];
            writeTree.pBufferInfo = &treeBufferInfo;

            VkDescriptorBufferInfo quadsBufferInfo{};
            quadsBufferInfo.offset = 0;
            quadsBufferInfo.range = sizeof(Quad) * MAX_OBJECTS_QUADS;
            quadsBufferInfo.buffer = m_sceneQuadsBuffer;
            VkWriteDescriptorSet writeQuads{};
            writeQuads.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeQuads.descriptorCount = 1;
            writeQuads.dstBinding = 3;
            writeQuads.dstArrayElement = 0;
            writeQuads.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writeQuads.dstSet = m_descriptorSets[i];
            writeQuads.pBufferInfo = &quadsBufferInfo;

            List<VkWriteDescriptorSet> writes{writeImageInfo, writeScene, writeTree,  writeQuads};

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
        for (int i = 0; i < SAMPLES; i++) {
            FRAME_PUSH_STRUCT frameInfo{};
            frameInfo.sampleCount = i + 1;
            frameInfo.objectCountSphere = m_objectCountSpheres;
            frameInfo.treeCount = m_tree.size();
            frameInfo.objectCountQuads = m_objectCountQuads;

            vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(FRAME_PUSH_STRUCT), &frameInfo);
            vkCmdDispatch(commandBuffer, (m_swapCtx->extends.width + 15) / 16, (m_swapCtx->extends.height + 15) / 16,
                          1);
            record_image_transition(commandBuffer, m_storageImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
        }

        record_image_transition(commandBuffer, m_storageImage, VK_IMAGE_LAYOUT_GENERAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    }

    void ComputeRT::clean_up() {
        vkDestroyPipelineLayout(m_ctx->logicalDevice, m_pipelineLayout, nullptr);
        vkDestroyPipeline(m_ctx->logicalDevice, m_pipeline, nullptr);
        vkDestroyDescriptorSetLayout(m_ctx->logicalDevice, m_descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(m_ctx->logicalDevice, m_descriptorPool, nullptr);
        vkDestroyImageView(m_ctx->logicalDevice, m_storageImageView, nullptr);
        vkDestroyImage(m_ctx->logicalDevice, m_storageImage, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_storageImageMemory, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, m_sceneStagingBuffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_sceneStagingBufferMemory, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, m_sceneBuffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_sceneBufferMemory, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, m_treeStagingBuffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_treeStagingBufferMemory, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, m_treeBuffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_treeBufferMemory, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, m_sceneQuadsStagingBuffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_sceneQuadsStagingBufferMemory, nullptr);
        vkDestroyBuffer(m_ctx->logicalDevice, m_sceneQuadsBuffer, nullptr);
        vkFreeMemory(m_ctx->logicalDevice, m_sceneQuadsBufferMemory, nullptr);
    }

    void ComputeRT::setup_scene() {
        VkDeviceSize sceneSize = sizeof(Sphere) * MAX_OBJECTS_SPHERES;
        VkDeviceSize sceneQuadsSize = sizeof(Quad) * MAX_OBJECTS_QUADS;
        VkDeviceSize treeSize = sizeof(BVHNode) * (MAX_NODES);

        create_buffer(m_ctx, m_sceneStagingBuffer, m_sceneStagingBufferMemory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      sceneSize);
        create_buffer(m_ctx, m_sceneBuffer, m_sceneBufferMemory,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sceneSize);

        create_buffer(m_ctx, m_sceneQuadsStagingBuffer, m_sceneQuadsStagingBufferMemory,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      sceneQuadsSize);
        create_buffer(m_ctx, m_sceneQuadsBuffer, m_sceneQuadsBufferMemory,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sceneQuadsSize);
        create_buffer(m_ctx, m_treeStagingBuffer, m_treeStagingBufferMemory, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      treeSize);
        create_buffer(m_ctx, m_treeBuffer, m_treeBufferMemory,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, treeSize);
        void *data;
        vkMapMemory(m_ctx->logicalDevice, m_sceneStagingBufferMemory, 0, sceneSize, 0, &data);
        memcpy(data, m_scene_spheres, sceneSize);
        vkUnmapMemory(m_ctx->logicalDevice, m_sceneStagingBufferMemory);
        copy_buffer_to_buffer(m_ctx, m_sceneStagingBuffer, m_sceneBuffer, sceneSize);
        vkMapMemory(m_ctx->logicalDevice, m_sceneQuadsStagingBufferMemory, 0, sceneQuadsSize, 0, &data);
        memcpy(data, m_scene_quads, sceneQuadsSize);
        vkUnmapMemory(m_ctx->logicalDevice, m_sceneQuadsStagingBufferMemory);
        copy_buffer_to_buffer(m_ctx, m_sceneQuadsStagingBuffer, m_sceneQuadsBuffer, sceneQuadsSize);

        vkMapMemory(m_ctx->logicalDevice, m_treeStagingBufferMemory, 0, treeSize, 0, &data);
        memcpy(data, m_tree.data(), treeSize);
        vkUnmapMemory(m_ctx->logicalDevice, m_treeStagingBufferMemory);
        copy_buffer_to_buffer(m_ctx, m_treeStagingBuffer, m_treeBuffer, treeSize);
    }
}
