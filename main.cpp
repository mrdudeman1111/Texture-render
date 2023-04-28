#include <cstdlib>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <cstring>
#include <bitset>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

std::vector<const char*> Layers = {"VK_LAYER_KHRONOS_validation"};
std::vector<const char*> InstExt = {"VK_KHR_external_memory_capabilities", "VK_KHR_surface"};
std::vector<const char*> DevExt = {"VK_KHR_external_memory", "VK_KHR_external_memory_fd", "VK_KHR_swapchain"};

struct Image
{
  VkImage Image;
  VkImageView ImageView;
  VkDeviceMemory Memory;
  VkAttachmentDescription AttachmentDescription;
  VkAttachmentReference AttachmentReference;

  VkFormat ImageFormat;
  VkImageLayout ImageLayout;
};

struct Vulkan
{
  public:
  VkInstance Instance;
  VkPhysicalDevice PhysicalDevice;
  VkDevice Device;
  VkRenderPass Renderpass;

  GLFWwindow* Window;

  VkCommandPool CommandPool;
  std::vector<VkCommandBuffer> RenderBuffers;

  VkPipelineLayout PipeLayout;

  VkSurfaceKHR RenderSurface;
  VkSwapchainKHR Swapchain;

  VkQueue GraphicsQueue;

  std::vector<Image> SwapImages;
  std::vector<Image> DepthStencils;
  std::vector<VkFramebuffer> FrameBuffers;

  VkExtent3D Extent{1280, 720, 1};
};

Vulkan* Context;

int GetMemIndex(uint32_t MemFlags)
{
  VkPhysicalDeviceMemoryProperties MemProps;
  vkGetPhysicalDeviceMemoryProperties(Context->PhysicalDevice, &MemProps);

  for(int i = 0; i < MemProps.memoryTypeCount; i++)
  {
    // std::cout << std::bitset<8>{MemProps.memoryTypes[i].propertyFlags} << '\n';
    if(MemProps.memoryTypes[i].propertyFlags & MemFlags)
    {
      return i;
    }
  }

  throw std::runtime_error("Failed to find valid memory type");
}

Image CreateImage(VkFormat Format, VkExtent3D Extent, VkImageUsageFlags Usage)
{
  Image Ret;

  VkImageCreateInfo ImageCI{};
  ImageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ImageCI.extent = Extent;
  ImageCI.arrayLayers = 1;
  ImageCI.format = Format;
  ImageCI.imageType = VK_IMAGE_TYPE_2D;
  ImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ImageCI.mipLevels = 1;
  ImageCI.samples = VK_SAMPLE_COUNT_1_BIT;
  ImageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ImageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
  ImageCI.usage = Usage;

  if(vkCreateImage(Context->Device, &ImageCI, nullptr, &Ret.Image) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create Image");
  }

  VkMemoryRequirements MemReq;
  vkGetImageMemoryRequirements(Context->Device, Ret.Image, &MemReq);

  VkMemoryAllocateInfo AllocInfo{};
  AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  AllocInfo.allocationSize = MemReq.size;
  AllocInfo.memoryTypeIndex = GetMemIndex(MemReq.memoryTypeBits);

  if(vkAllocateMemory(Context->Device, &AllocInfo, nullptr, &Ret.Memory) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to Allocate memory");
  }

  // No offset because every Image will have dedicated allocations
  vkBindImageMemory(Context->Device, Ret.Image, Ret.Memory, 0);

  return Ret;
}

std::vector<char> ReadFile(const char* FilePath)
{
  std::ifstream File(FilePath, std::ios::ate | std::ios::binary);

  if(File.is_open())
  {
    size_t FileSize = File.tellg();

    std::vector<char> Characters(FileSize);
    File.seekg(0);
    File.read(Characters.data(), FileSize);

    File.close();

    return Characters;
  }

  throw std::runtime_error("Failed to read a file");
}

void InitVulkan()
{
  glfwInit();

  uint32_t glfwCount = 0;

  const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwCount);

  for(uint32_t i = 0; i < glfwCount; i++)
  {
    InstExt.push_back(glfwExt[i]);
  }






  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  Context->Window = glfwCreateWindow(Context->Extent.width, Context->Extent.height, "Texture render", NULL, NULL);

  VkApplicationInfo AppInfo{};
  AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  AppInfo.apiVersion = VK_API_VERSION_1_2;
  AppInfo.pEngineName = "Texture Renderer";
  AppInfo.engineVersion = 1;
  AppInfo.pApplicationName = "TexRender";
  AppInfo.applicationVersion = 1;

  VkInstanceCreateInfo Info{};
  Info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  Info.pApplicationInfo = &AppInfo;
  Info.enabledLayerCount = Layers.size();
  Info.ppEnabledLayerNames = Layers.data();
  Info.enabledExtensionCount = InstExt.size();
  Info.ppEnabledExtensionNames = InstExt.data();

  if(vkCreateInstance(&Info, nullptr, &Context->Instance) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create instance");
  }

  uint32_t PDevCount;
  vkEnumeratePhysicalDevices(Context->Instance, &PDevCount, nullptr);
  std::vector<VkPhysicalDevice> PDevices(PDevCount);
  vkEnumeratePhysicalDevices(Context->Instance, &PDevCount, PDevices.data());

  for(uint32_t i = 0; i < PDevCount; i++)
  {
    VkPhysicalDeviceProperties DevProps;
    vkGetPhysicalDeviceProperties(PDevices[i], &DevProps);
    if(DevProps.deviceType & VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
      Context->PhysicalDevice = PDevices[i];
      break;
    }
  }

  // Device
    uint32_t QueueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties2(Context->PhysicalDevice, &QueueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> QueueProps(QueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(Context->PhysicalDevice, &QueueFamilyCount, QueueProps.data());

    uint32_t GraphicsFamily;

    for(uint32_t i = 0; i < QueueFamilyCount; i++)
    {
      if(QueueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
        GraphicsFamily = i;
        break;
      }
    }

    VkDeviceQueueCreateInfo QueueCI{};
    QueueCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QueueCI.queueCount = 1;
    QueueCI.queueFamilyIndex = GraphicsFamily;

    VkDeviceCreateInfo DevCI{};
    DevCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DevCI.queueCreateInfoCount = 1;
    DevCI.pQueueCreateInfos = &QueueCI;
    DevCI.enabledExtensionCount= DevExt.size();
    DevCI.ppEnabledExtensionNames = DevExt.data();

    if(vkCreateDevice(Context->PhysicalDevice, &DevCI, nullptr, &Context->Device) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create device");
    }
  // Device

  vkGetDeviceQueue(Context->Device, GraphicsFamily, 0, &Context->GraphicsQueue);

  VkResult SurfaceError = glfwCreateWindowSurface(Context->Instance, Context->Window, nullptr, &Context->RenderSurface);
  std::cout << SurfaceError << '\n';

  VkSurfaceCapabilitiesKHR SurfaceCap;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Context->PhysicalDevice, Context->RenderSurface, &SurfaceCap);

  uint32_t SurfaceFrmCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(Context->PhysicalDevice, Context->RenderSurface, &SurfaceFrmCount, nullptr);
  std::vector<VkSurfaceFormatKHR> SurfaceFormats(SurfaceFrmCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(Context->PhysicalDevice, Context->RenderSurface, &SurfaceFrmCount, SurfaceFormats.data());
  
  // Swapchain
    VkSwapchainCreateInfoKHR SwapCI{};
    SwapCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    SwapCI.clipped = VK_TRUE;
    SwapCI.surface = Context->RenderSurface;

    SwapCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    SwapCI.imageExtent = VkExtent2D{Context->Extent.width, Context->Extent.height};
    if(SurfaceFrmCount > 0)
    {
      SwapCI.imageFormat = SurfaceFormats[0].format;
      SwapCI.imageColorSpace = SurfaceFormats[0].colorSpace;
    }
    else
    {
      throw std::runtime_error("no supported surface formats available");
    }
    SwapCI.imageArrayLayers = 1; SwapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SwapCI.minImageCount = SurfaceCap.minImageCount;

    SwapCI.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    SwapCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    SwapCI.preTransform = SurfaceCap.currentTransform;

    if(vkCreateSwapchainKHR(Context->Device, &SwapCI, nullptr, &Context->Swapchain) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create swapchain");
    }
  // Swapchain

  // Framebuffer
    uint32_t FbCount;
    vkGetSwapchainImagesKHR(Context->Device, Context->Swapchain, &FbCount, nullptr);
    std::vector<VkImage> VkSwapImages(FbCount);
    vkGetSwapchainImagesKHR(Context->Device, Context->Swapchain, &FbCount, VkSwapImages.data());
    Context->SwapImages.resize(FbCount);

    Context->DepthStencils.resize(FbCount);

    for(uint32_t i = 0; i < FbCount; i++)
    {
      Context->DepthStencils[i] = CreateImage(VK_FORMAT_D16_UNORM, Context->Extent, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
      VkImageViewCreateInfo DepthView{};
      DepthView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      DepthView.format = VK_FORMAT_D16_UNORM;
      DepthView.image = Context->DepthStencils[i].Image;
      DepthView.viewType = VK_IMAGE_VIEW_TYPE_2D;

      DepthView.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      DepthView.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      DepthView.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      DepthView.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

      DepthView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      DepthView.subresourceRange.baseMipLevel = 0;
      DepthView.subresourceRange.levelCount = 1;
      DepthView.subresourceRange.baseArrayLayer = 0;
      DepthView.subresourceRange.layerCount = 1;

      if(vkCreateImageView(Context->Device, &DepthView, nullptr, &Context->DepthStencils[i].ImageView) != VK_SUCCESS)
      {
        throw std::runtime_error("failed to create depth image view");
      }

      Context->DepthStencils[i].AttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      Context->DepthStencils[i].AttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      Context->DepthStencils[i].AttachmentDescription.format = VK_FORMAT_D16_UNORM;
      Context->DepthStencils[i].AttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
      Context->DepthStencils[i].AttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      Context->DepthStencils[i].AttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      Context->DepthStencils[i].AttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      Context->DepthStencils[i].AttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

      Context->DepthStencils[i].AttachmentDescription.flags = 0;

      Context->DepthStencils[i].AttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      Context->DepthStencils[i].AttachmentReference.attachment = 1;


      Context->SwapImages[i].Image = VkSwapImages[i];
      Context->SwapImages[i].ImageFormat = SurfaceFormats[0].format;
      Context->SwapImages[i].ImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      Context->SwapImages[i].AttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      Context->SwapImages[i].AttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      Context->SwapImages[i].AttachmentDescription.format = Context->SwapImages[i].ImageFormat;
      Context->SwapImages[i].AttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
      Context->SwapImages[i].AttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      Context->SwapImages[i].AttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      Context->SwapImages[i].AttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      Context->SwapImages[i].AttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

      Context->SwapImages[i].AttachmentDescription.flags = 0;

      Context->SwapImages[i].AttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      Context->SwapImages[i].AttachmentReference.attachment = 0;

      VkImageViewCreateInfo ImageView{};
      ImageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      ImageView.format = Context->SwapImages[i].ImageFormat;
      ImageView.image = Context->SwapImages[i].Image;
      ImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;

      ImageView.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      ImageView.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      ImageView.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      ImageView.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

      ImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      ImageView.subresourceRange.baseMipLevel = 0;
      ImageView.subresourceRange.levelCount = 1;
      ImageView.subresourceRange.baseArrayLayer = 0;
      ImageView.subresourceRange.layerCount = 1;

      if(vkCreateImageView(Context->Device, &ImageView, nullptr, &Context->SwapImages[i].ImageView) != VK_SUCCESS)
      {
        throw std::runtime_error("Failed to create swap image view");
      }
    }
  //Framebuffer

  // Command Pool
    VkCommandPoolCreateInfo CommandPoolCI{};
    CommandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    CommandPoolCI.queueFamilyIndex = GraphicsFamily;

    if(vkCreateCommandPool(Context->Device, &CommandPoolCI, nullptr, &Context->CommandPool) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create a command pool");
    }

    VkCommandBufferAllocateInfo CmdAllocInfo{};
    CmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    CmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CmdAllocInfo.commandPool = Context->CommandPool;
    CmdAllocInfo.commandBufferCount = Context->SwapImages.size();

    Context->RenderBuffers.resize(Context->SwapImages.size());

    if(vkAllocateCommandBuffers(Context->Device, &CmdAllocInfo, Context->RenderBuffers.data()) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create command buffers");
    }
  // Command Pool

  std::cout << "Finished Initiating vulkan\n";
}

void InitRendering(Image* Texture)
{
  VkSubpassDescription PrimarySubpass{};
  // we will only be passing one frame buffer per frame. So we didn't need to give every framebuffer image a description and reference.

  PrimarySubpass.colorAttachmentCount = 1;
  PrimarySubpass.pColorAttachments = &Context->SwapImages[0].AttachmentReference;
  PrimarySubpass.pDepthStencilAttachment = &Context->DepthStencils[0].AttachmentReference;
  PrimarySubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

  VkRenderPassCreateInfo RenderpassInfo{} ;
  RenderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

  std::vector<VkAttachmentDescription> Attachments = { Context->SwapImages[0].AttachmentDescription, Context->DepthStencils[0].AttachmentDescription };

  // 1. Swapchain Image Attachment 2. Swapchain Depth stencil
  // These attachments tell the renderpass what buffers are in the VkFramebuffers we will be using, in this case a color buffer for present, and a depth stencil for occlusion
  RenderpassInfo.attachmentCount = Attachments.size();
  RenderpassInfo.pAttachments = Attachments.data();
  RenderpassInfo.subpassCount = 1;
  RenderpassInfo.pSubpasses = &PrimarySubpass;

  if(vkCreateRenderPass(Context->Device, &RenderpassInfo, nullptr, &Context->Renderpass) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create renderpass");
  }

  Context->FrameBuffers.resize(Context->SwapImages.size());

  for(uint32_t i = 0; i < Context->SwapImages.size(); i++)
  {
    VkFramebufferCreateInfo FBInfo{};
    FBInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    FBInfo.renderPass = Context->Renderpass;

    FBInfo.attachmentCount = 2;

    VkImageView FrameBufferAttachments[] = { Context->SwapImages[i].ImageView, Context->DepthStencils[i].ImageView };

    FBInfo.pAttachments = FrameBufferAttachments;
    FBInfo.width = 1280;
    FBInfo.height = 720;
    FBInfo.layers = 1;

    if(vkCreateFramebuffer(Context->Device, &FBInfo, nullptr, &Context->FrameBuffers[i]) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create a framebuffer");
    }
  }
}

VkPipeline InitPipeline(Image* Texture, VkDescriptorSetLayout TextureLayout)
{
  VkShaderModule Vert;
  VkShaderModule Frag;

  VkPipeline Pipeline;

  // Shaders
    std::vector<char> VertCode, FragCode;
    VertCode = ReadFile("/home/ethanw/Repos/TextureRender/Shaders/vert.spv");
    FragCode = ReadFile("/home/ethanw/Repos/TextureRender/Shaders/frag.spv");

    VkShaderModuleCreateInfo VertInfo{};
    VertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    VertInfo.pCode = reinterpret_cast<const uint32_t*>(VertCode.data());
    VertInfo.codeSize = VertCode.size();

    if(vkCreateShaderModule(Context->Device, &VertInfo, nullptr, &Vert) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create Vertex shader");
    }

    VkShaderModuleCreateInfo FragInfo{};
    FragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    FragInfo.codeSize = FragCode.size();
    FragInfo.pCode = reinterpret_cast<const uint32_t*>(FragCode.data());

    if(vkCreateShaderModule(Context->Device, &FragInfo, nullptr, &Frag) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create fragment Shader");
    }

    VkPipelineShaderStageCreateInfo VertStage{};
    VertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    VertStage.pName = "main";
    VertStage.module = Vert;
    VertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineShaderStageCreateInfo FragStage{};
    FragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    FragStage.pName = "main";
    FragStage.module = Frag;
    FragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkPipelineShaderStageCreateInfo> ShaderStages = { VertStage, FragStage };
  // Shaders

  VkPipelineLayoutCreateInfo PipeLayoutInfo{};
  PipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  PipeLayoutInfo.setLayoutCount = 1;
  PipeLayoutInfo.pSetLayouts = &TextureLayout;
  PipeLayoutInfo.pushConstantRangeCount = 0;

  if(vkCreatePipelineLayout(Context->Device, &PipeLayoutInfo, nullptr, &Context->PipeLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create pipeline layout");
  }



  // Viewport
    VkViewport ViewPort{};
    ViewPort.width = Context->Extent.width;
    ViewPort.height = Context->Extent.height;
    ViewPort.x = 0;
    ViewPort.y = 0;
    ViewPort.minDepth = 0.f;
    ViewPort.maxDepth = 1.f;

    VkRect2D RenderArea;
    RenderArea.extent.width = Context->Extent.width;
    RenderArea.extent.height = Context->Extent.height;
    RenderArea.offset.x = 0;
    RenderArea.offset.y = 0;

    VkPipelineViewportStateCreateInfo ViewPortInfo{};
    ViewPortInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    ViewPortInfo.scissorCount = 1;
    ViewPortInfo.pScissors = &RenderArea;
    ViewPortInfo.viewportCount = 1;
    ViewPortInfo.pViewports = &ViewPort;
  // ViewPort

  // Color 
    VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
    ColorBlendAttachment.blendEnable = VK_FALSE;
    ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo ColorBlendInfo{};
    ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ColorBlendInfo.logicOpEnable = VK_FALSE;
    ColorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
    ColorBlendInfo.attachmentCount = 1;
    ColorBlendInfo.pAttachments = &ColorBlendAttachment;
  // Color

  // Rasterizer
    VkPipelineRasterizationStateCreateInfo Rasterizer{};
    Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    Rasterizer.depthClampEnable = VK_FALSE;
    Rasterizer.rasterizerDiscardEnable = VK_FALSE;
    Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    Rasterizer.lineWidth = 1.f;
    Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    Rasterizer.depthBiasEnable = VK_FALSE;
  // Rasterizer

  // Depth Stencil
    VkPipelineDepthStencilStateCreateInfo DepthStencilState{};
    DepthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    DepthStencilState.depthTestEnable = VK_FALSE;
    DepthStencilState.depthWriteEnable = VK_FALSE;
    DepthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    DepthStencilState.depthBoundsTestEnable = VK_FALSE;
    DepthStencilState.minDepthBounds = 0.f;
    DepthStencilState.maxDepthBounds = 1.f;
    DepthStencilState.stencilTestEnable = VK_FALSE;
  // Depth stencil

  // Input state
    VkPipelineVertexInputStateCreateInfo VertInput{};
    VertInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VertInput.vertexAttributeDescriptionCount = 0;
    VertInput.vertexBindingDescriptionCount = 0;
  // Input state

  // Input assembly
    VkPipelineInputAssemblyStateCreateInfo InputState{};
    InputState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    InputState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    InputState.primitiveRestartEnable = VK_FALSE;
  // Input assembly

  // MSAA
    VkPipelineMultisampleStateCreateInfo MultisampleState{};
    MultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    MultisampleState.sampleShadingEnable = VK_FALSE;
    MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  // MSAA

  VkGraphicsPipelineCreateInfo GraphicsPipe{};
  GraphicsPipe.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  GraphicsPipe.pVertexInputState = &VertInput;
  GraphicsPipe.layout = Context->PipeLayout;
  GraphicsPipe.stageCount = ShaderStages.size();
  GraphicsPipe.pStages = ShaderStages.data();
  GraphicsPipe.subpass = 0;
  GraphicsPipe.renderPass = Context->Renderpass;
  GraphicsPipe.pViewportState = &ViewPortInfo;
  GraphicsPipe.pColorBlendState = &ColorBlendInfo;
  GraphicsPipe.pInputAssemblyState = &InputState;
  GraphicsPipe.pRasterizationState = &Rasterizer;
  GraphicsPipe.pMultisampleState = &MultisampleState;
  GraphicsPipe.pDepthStencilState = &DepthStencilState;

  if(vkCreateGraphicsPipelines(Context->Device, nullptr, 1, &GraphicsPipe, nullptr, &Pipeline) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create graphics pipeline\n");
  }

  return Pipeline;
}

int main()
{
  Context = new Vulkan();

  InitVulkan();

  // Image
    int Width, Height, Channels;
    stbi_uc* Pixels = stbi_load("/home/ethanw/Repos/TextureRender/Texture.jpg", &Width, &Height, &Channels, STBI_rgb_alpha);

    Image Texture;

    VkImageCreateInfo ImageCI{};
    ImageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    ImageCI.extent = VkExtent3D{(uint32_t)Width, (uint32_t)Height, 1};
    ImageCI.format = VK_FORMAT_R8G8B8A8_SRGB;
    ImageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageCI.imageType = VK_IMAGE_TYPE_2D;
    ImageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageCI.mipLevels = 1;
    ImageCI.arrayLayers = 1;
    ImageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    Texture.ImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
    Texture.ImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    if(vkCreateImage(Context->Device, &ImageCI, nullptr, &Texture.Image) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create load texture");
    }

    VkMemoryRequirements MemReqs;
    vkGetImageMemoryRequirements(Context->Device, Texture.Image, &MemReqs);

    VkMemoryAllocateInfo AllocInfo{};
    AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocInfo.allocationSize = MemReqs.size;

    AllocInfo.memoryTypeIndex = GetMemIndex(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    if(vkAllocateMemory(Context->Device, &AllocInfo, nullptr, &Texture.Memory) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate memory");
    }

    vkBindImageMemory(Context->Device, Texture.Image, Texture.Memory, 0);

    void* Memory;

    vkMapMemory(Context->Device, Texture.Memory, 0, MemReqs.size, 0, &Memory);
      memcpy(Memory, Pixels, Height*Width*4);
    vkUnmapMemory(Context->Device, Texture.Memory);

    Texture.AttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    Texture.AttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    Texture.AttachmentDescription.format = ImageCI.format;
    Texture.AttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    Texture.AttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Texture.AttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Texture.AttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Texture.AttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    Texture.AttachmentDescription.flags = 0;

    Texture.AttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    Texture.AttachmentReference.attachment = 2;

    VkImageViewCreateInfo TextureViewCI{};
    TextureViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    TextureViewCI.image = Texture.Image;
    TextureViewCI.format = Texture.ImageFormat;
    TextureViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;

    TextureViewCI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    TextureViewCI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    TextureViewCI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    TextureViewCI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    TextureViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    TextureViewCI.subresourceRange.layerCount = 1;
    TextureViewCI.subresourceRange.baseMipLevel = 0;
    TextureViewCI.subresourceRange.levelCount = 1;
    TextureViewCI.subresourceRange.baseArrayLayer = 0;

    if(vkCreateImageView(Context->Device, &TextureViewCI, nullptr, &Texture.ImageView) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create image view");
    }
  // Image

  VkSampler TextureSampler;

  VkSamplerCreateInfo SamplerCI{};
  SamplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  SamplerCI.minFilter = VK_FILTER_LINEAR;
  SamplerCI.magFilter = VK_FILTER_LINEAR;

  SamplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  SamplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  SamplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

  SamplerCI.anisotropyEnable = VK_FALSE;

  SamplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  SamplerCI.unnormalizedCoordinates = VK_FALSE;

  SamplerCI.compareEnable = VK_FALSE;
  SamplerCI.compareOp = VK_COMPARE_OP_ALWAYS;

  SamplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  SamplerCI.mipLodBias = 0.0f;
  SamplerCI.minLod = 0.f;
  SamplerCI.maxLod = 0.f;

  if(vkCreateSampler(Context->Device, &SamplerCI, nullptr, &TextureSampler) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create sampler");
  }

  // Descriptor
    VkDescriptorPool FragShaderPool;
    VkDescriptorSet TextureSet;
    VkDescriptorSetLayout TextureSetLayout;

    VkDescriptorPoolSize ImageSize{};
    ImageSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ImageSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo PoolInfo{};
    PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    PoolInfo.maxSets = 1;
    PoolInfo.poolSizeCount = 1;
    PoolInfo.pPoolSizes = &ImageSize;

    if(vkCreateDescriptorPool(Context->Device, &PoolInfo, nullptr, &FragShaderPool) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed descriptor Pool");
    }

    VkDescriptorSetLayoutBinding SetBinding;
    SetBinding.binding = 0;
    SetBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    SetBinding.descriptorCount = 1;
    SetBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    SetBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo TextureLayoutCI{};
    TextureLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    TextureLayoutCI.pBindings = &SetBinding;
    TextureLayoutCI.bindingCount = 1;

    if(vkCreateDescriptorSetLayout(Context->Device, &TextureLayoutCI, nullptr, &TextureSetLayout) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to create descriptor");
    }

    VkDescriptorSetAllocateInfo DescriptorAllocInfo{};
    DescriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    DescriptorAllocInfo.descriptorPool = FragShaderPool;
    DescriptorAllocInfo.descriptorSetCount = 1;
    DescriptorAllocInfo.pSetLayouts = &TextureSetLayout;

    if(vkAllocateDescriptorSets(Context->Device, &DescriptorAllocInfo, &TextureSet) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to allocate descriptor");
    }

    VkDescriptorImageInfo DescImgInf{};
    DescImgInf.sampler = TextureSampler;
    DescImgInf.imageView = Texture.ImageView;
    DescImgInf.imageLayout = Texture.ImageLayout;

    VkWriteDescriptorSet TextureWrite{};
    TextureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

    TextureWrite.descriptorCount = 1;
    TextureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    TextureWrite.dstSet = TextureSet;

    TextureWrite.dstBinding = 0;
    TextureWrite.dstArrayElement = 0;
    TextureWrite.pImageInfo = &DescImgInf;

    vkUpdateDescriptorSets(Context->Device, 1, &TextureWrite, 0, nullptr);
  // Descriptor

  InitRendering(&Texture);

  VkPipeline OurPipe = InitPipeline(&Texture, TextureSetLayout);

  // Commands

    for(int i = 0; i < Context->RenderBuffers.size(); i++)
    {
      VkClearDepthStencilValue DepthValue{};
      DepthValue.depth = 0.f;

      VkClearColorValue ColorValue;
      ColorValue.int32[0] = 0; ColorValue.int32[1] = 0; ColorValue.int32[2] = 0; ColorValue.int32[3] = 0;
      ColorValue.uint32[0] = 0; ColorValue.uint32[1] = 0; ColorValue.uint32[2] = 0; ColorValue.uint32[3] = 0;
      ColorValue.float32[0] = 0.f; ColorValue.float32[1] = 0.f; ColorValue.float32[2] = 0.f; ColorValue.float32[3] = 0.f;

      VkClearValue ClearValue;
      ClearValue.color = ColorValue;
      ClearValue.depthStencil = DepthValue;

      for(int i = 0; i < Context->RenderBuffers.size(); i ++)
      {
        VkCommandBufferBeginInfo BeginInf{};
        BeginInf.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        VkRect2D RenderArea{(int32_t)Context->Extent.width, (int32_t)Context->Extent.height};

        VkRenderPassBeginInfo RenderBegin{};
        RenderBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderBegin.renderPass = Context->Renderpass;
        RenderBegin.renderArea = RenderArea;
        RenderBegin.clearValueCount = 1;
        RenderBegin.pClearValues = &ClearValue;
        RenderBegin.framebuffer = Context->FrameBuffers[i];

        vkBeginCommandBuffer(Context->RenderBuffers[i], &BeginInf);
          vkCmdBeginRenderPass(Context->RenderBuffers[i], &RenderBegin, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindDescriptorSets(Context->RenderBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Context->PipeLayout, 0, 1, &TextureSet, 0, 0);
            vkCmdBindPipeline(Context->RenderBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, OurPipe);
            vkCmdDraw(Context->RenderBuffers[i], 4, 0, 0, 0);

          vkCmdEndRenderPass(Context->RenderBuffers[i]);
        vkEndCommandBuffer(Context->RenderBuffers[i]);
      }
    }

  // Commands

  // Rendering
    while(!glfwWindowShouldClose(Context->Window))
    {
      glfwPollEvents();
    }
  // Rendering

  std::cout << "Run Success\n";
  return 0;
}
