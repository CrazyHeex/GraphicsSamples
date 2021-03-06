//----------------------------------------------------------------------------------
// File:        NvVkUtil/NvVkContext.h
// SDK Version: v3.00 
// Email:       gameworks@nvidia.com
// Site:        http://developer.nvidia.com/
//
// Copyright (c) 2014-2015, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------------
#ifndef NV_VKCONTEXT_H
#define NV_VKCONTEXT_H

/// \file
/// VK context utilities and wrappers.

#include <NvSimpleTypes.h>
#include "NvPlatformVK.h"
#include <NV/NvGfxConfiguration.h>
#include <NvVkUtil/NvVkUtil.h>
#include <vector>

void checkVkResult(const char* file, int32_t line, VkResult result);
#ifndef CHECK_VK_RESULT
#define CHECK_VK_RESULT() checkVkResult(__FILE__, __LINE__, result)
#endif

/// VK config representation.
struct NvVKConfiguration : public NvGfxConfiguration {
	NvVKConfiguration(const NvGfxConfiguration& gfx) :
		NvGfxConfiguration(gfx) {}

	/// Inline all-elements constructor.
	/// \param[in] r the red color depth in bits
	/// \param[in] g the green color depth in bits
	/// \param[in] b the blue color depth in bits
	/// \param[in] a the alpha color depth in bits
	/// \param[in] d the depth buffer depth in bits
	/// \param[in] s the stencil buffer depth in bits
	/// \param[in] msaa the MSAA buffer sample count
	NvVKConfiguration(uint32_t r = 8, uint32_t g = 8,
		uint32_t b = 8, uint32_t a = 8,
		uint32_t d = 24, uint32_t s = 0, uint32_t msaa = 0) :
		NvGfxConfiguration(r, g, b, a, d, s, msaa) {}
};

class NvImage;
class NvVkContext;
class NvGPUTimerVK;

/// A wrapper for a VkBuffer and its memory
struct NvVkBuffer {
	VkDeviceMemory mem; ///< Buffer memory object
	VkBuffer buffer; ///< Buffer object
	/// Value-of operator.  Returns the VkBuffer
	/// \return the VkBuffer
	VkBuffer& operator ()() { return buffer; }
	NvVkBuffer() : mem(0), buffer(0) {}
};

/// A wrapper for a VkImage and its memory
struct NvVkImage {
	VkDeviceMemory mem; ///< Image memory object
	VkImage image; ///< Image object
	/// Value-of operator.  Returns the VkImage
	/// \return the VkImage
	VkImage& operator ()() { return image; }
	NvVkImage() : mem(0), image(0) {}
};

/// A "texture": a wrapper for a VkImage and its VkImageView
struct NvVkTexture {
	/// Value-of operator.  Returns the VkImage
	/// \return the VkImage
	NvVkImage& operator ()() { return image; }
	NvVkImage image; ///< Image object
	VkImageView view; ///< Image view object
};

/// An allocatable staging buffer for transfers
class NvVkStagingBuffer {
public:
	VkBuffer getBuffer(){
		return mBuffer();
	}
	bool needSync(size_t sz) {
		return (mAllocated && mUsed + sz > mAllocated);
	}

	size_t append(size_t sz, const void* data);

	void reset(){
		mUsed = 0;
	}

	void init(NvVkContext* vk){
		mVk = vk;
		mAllocated = 0;
		mUsed = 0;
	}

	void deinit();

	NvVkStagingBuffer() : mVk(NULL), mAllocated(0), mUsed(0), mBuffer() {}
	~NvVkStagingBuffer() {
		deinit();
	}

private:
	NvVkContext* mVk;
	NvVkBuffer   mBuffer;
	char*        mMapping;
	size_t       mUsed;
	size_t       mAllocated;
};

/// A wrapper for all forms of render target
class NvVkRenderTarget {
public:
	~NvVkRenderTarget();

	/// Retrieve the current frame's NvVkImage
	virtual NvVkImage& target() = 0;

	/// Retrieve the current frame's VkImage
	virtual VkImage& image() = 0;

	/// Retrieve the current frame's depth/stencil image
	virtual VkImage& depthStencil() = 0;

	/// Retrieve the VkImageView of the current frame's color target
	virtual VkImageView& targetView() = 0;

	/// Retrieve the framebuffer object for the current frame
	virtual VkFramebuffer& frameBuffer() = 0;

	/// Retrieve the VK app context associated with the render target
	NvVkContext& vk() { return _vk; }

	/// Retrieve the color format of the target
	VkFormat& targetFormat() { return _targetFormat; }

	/// Retrieve the despth/stencil format of the target
	VkFormat& depthStencilFormat() { return _depthStencilFormat; }

	/// Retrieve the a compatible clearing render pass for this target
	VkRenderPass& clearRenderPass() { return _clearRenderPass; }

	/// Retrieve the a compatible non-clearing render pass for this target
	VkRenderPass& copyRenderPass() { return _copyRenderPass; }

	/// Retrieve the target width
	int32_t width() { return _width; }

	/// Retrieve the target height
	int32_t height() { return _height; }

	/// \privatesection

	bool initialize(VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	virtual bool resize(int32_t& w, int32_t& h) = 0;

	virtual bool beginFrame() { return true; }
	virtual bool endFrame() { return true; }

	virtual VkSemaphore* getWaitSemaphore() = 0;
	virtual VkSemaphore* getSignalSemaphore() = 0;

protected:
	NvVkRenderTarget(NvVkContext& vk) :
		_vk(vk),
		_targetFormat(VK_FORMAT_UNDEFINED),
		_depthStencilFormat(VK_FORMAT_UNDEFINED),
		_clearRenderPass(VK_NULL_HANDLE),
		_copyRenderPass(VK_NULL_HANDLE),
		_width(0), _height(0)
	{}

	NvVkContext& _vk;
	VkFormat _targetFormat;
	VkFormat _depthStencilFormat;
	VkRenderPass _clearRenderPass;
	VkRenderPass _copyRenderPass;
	int32_t _width;
	int32_t _height;
};

/// A wrapper for most major Vulkan objects used by an application
class NvVkContext {
public:
	NvVkContext() :
		_instance(NULL),
		_physicalDevice(NULL),
		_device(NULL),
		_queue(NULL),
		_queueFamilyIndex(0),
		_queueIndex(0)
	{ }

	/// VkInstance access
	VkInstance instance() { return _instance; }

	/// Device access
	VkDevice device() { return _device; }

	/// Queue access
	VkQueue queue() { return _queue; }

	/// Queue family access
	uint32_t queueFamilyIndex() { return _queueFamilyIndex; }

	/// Queue index access
	uint32_t queueIndex() { return _queueIndex; }


	/// VkPhysicalDevice access
	VkPhysicalDevice& physicalDevice() { return _physicalDevice; }

	/// VkPhysicalDeviceLimits access
	VkPhysicalDeviceLimits& physicalDeviceLimits() { return _physicalDeviceLimits; }

	/// VkPhysicalDeviceProperties access
	VkPhysicalDeviceProperties& physicalDeviceProperties() { return _physicalDeviceProperties; }

	/// VkPhysicalDeviceMemoryProperties access
	VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties() { return _physicalDeviceMemoryProperties; }

	/// Get the on-screen (or main) render target
	virtual NvVkRenderTarget* mainRenderTarget() = 0;

	VkResult allocMemAndBindImage(NvVkImage& image, VkFlags memProps = 0);
	VkResult createImage(VkImageCreateInfo& info, NvVkImage& image, VkFlags memProps = 0);

	VkResult allocMemAndBindBuffer(NvVkBuffer& buffer, VkFlags memProps = 0);
	VkResult createAndFillBuffer(size_t size, VkFlags usage, VkFlags memProps, NvVkBuffer& buffer, const void* data = NULL, NvVkStagingBuffer* staging = NULL);

	/// Creates a texture from an asset-based DDS file
	/// \param[in] filename the asset-path of the DDS file to load
	/// \param[out] tex the loaded texture
	/// \return true on success and false on failure
	bool uploadTextureFromDDSFile(const char* filename, NvVkTexture& tex);

	/// Creates a texture from preloaded data in DDS file format
	/// \param[in] ddsData pointer to the DDS file data to load
	/// \param[out] tex the loaded texture
	/// \return true on success and false on failure
	bool uploadTextureFromDDSData(const char* ddsData, int32_t length, NvVkTexture& tex);

	/// Creates a texture from an in-memory NvImage
	/// \param[in] image the image to load
	/// \param[out] tex the loaded texture
	/// \return true on success and false on failure
	bool uploadTexture(const NvImage* image, NvVkTexture& tex);

	/// Deprecated: Only supported on platforms/drivers that can accept GLSL directly
	uint32_t createShadersFromSourceFile(const std::string& sourceText, VkPipelineShaderStageCreateInfo* shaders, uint32_t maxShaders);

	/// Load SPIR-V shaders from binary file data (generated from the glsl2spirv tool)
	/// \param[in] data the binary data with multiple shaders
	/// \param[in] leng the size of the data in bytes
	/// \param[in/out] shaders pointer to an array of shader stage create info structs into which the shaders will be written.  
	/// Array must be at least as large as the number of shader stages in the file
	/// \param[in] maxShaders the number of shader structs in the array
	/// \return the number of shader stages loaded, or zero on failure.
	uint32_t createShadersFromBinaryFile(uint32_t* data, uint32_t leng, VkPipelineShaderStageCreateInfo* shaders, uint32_t maxShaders);

	VkResult transitionImageLayout(VkImage& image, VkImageAspectFlags aspect,
		VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlagBits inSrcAccessmask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
		VkAccessFlagBits inDstAccessmask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

	enum CmdBufferIndex {
		CMD_ONSCREEN = 0,
		CMD_UI_FRAMEWORK,
		CMD_COUNT
	};
	// Retrieves a reference to a managed, one-per-frame primary command buffer
	// Do not submit directly to a queue - use submitMainCommandBuffer
	VkCommandBuffer getMainCommandBuffer(CmdBufferIndex index = CMD_ONSCREEN);

	// Submits this frame's main command buffer and does any end-of-frame sync
	bool submitMainCommandBuffer(CmdBufferIndex index = CMD_ONSCREEN);

	VkCommandBuffer   createCmdBuffer(VkCommandPool pool, bool primary);

	void purgeTempCmdBuffers() {
		vkDeviceWaitIdle(_device);
		for (int f = 0; f < MAX_BUFFERED_FRAMES; f++){
			FrameInfo& frame = mFrames[f];
			if (!frame.mSubmittedTempCmdBuffers.empty()){
				vkFreeCommandBuffers(_device, mTempCmdPool, 
					(uint32_t)frame.mSubmittedTempCmdBuffers.size(), &frame.mSubmittedTempCmdBuffers[0]);
				frame.mSubmittedTempCmdBuffers.clear();
			}
			frame.mOpen = false;
		}
		mCurrFrameIndex = 0;
	}

	VkCommandBuffer beginTempCmdBuffer()
	{
		VkCommandBuffer cmd = createCmdBuffer(mTempCmdPool, true);

		// Record the commands.
		VkCommandBufferInheritanceInfo inheritInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };

		VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = &inheritInfo;

		if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
			return 0;

		return cmd;
	}

	VkResult doneWithTempCmdBufferSubmit(VkCommandBuffer& cmd, VkFence* fence = NULL) {
		VkResult result;
		result = vkEndCommandBuffer(cmd);
		if (result != VK_SUCCESS) {
			return result;
		}

		VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;

		result = vkQueueSubmit(queue(), 1, &submitInfo, fence ? *fence : VK_NULL_HANDLE);
		if (result != VK_SUCCESS)
			return result;

		doneWithTempCmdBuffer(cmd);
		return VK_SUCCESS;
	}

	void doneWithTempCmdBuffer(VkCommandBuffer& cmd) {
		int current = mCurrFrameIndex % MAX_BUFFERED_FRAMES;
		mFrames[current].mSubmittedTempCmdBuffers.push_back(cmd);
	}

	void destroyPastFrameCmdBuffers()
	{
		int current = mCurrFrameIndex % MAX_BUFFERED_FRAMES;
		int past = (mCurrFrameIndex + 1) % MAX_BUFFERED_FRAMES;

		if (mCurrFrameIndex < MAX_BUFFERED_FRAMES){
			return;
		}

		// not exactly efficient, should do per pool processing
		std::vector<VkCommandBuffer>& pastCmds = mFrames[past].mSubmittedTempCmdBuffers;
		if (pastCmds.size())
			vkFreeCommandBuffers(_device, mTempCmdPool, (uint32_t)pastCmds.size(),	&pastCmds[0]);
		pastCmds.clear();
	}

	NvVkStagingBuffer& stagingBuffer() {
		return mStaging;
	}

	NvGPUTimerVK& getFrameTimer() { return *m_frameTimer; }

	/// \privatesection
	bool nextFrame();

	VkFormat pickOptimalFormat(uint32_t count, const VkFormat *formats, VkFlags properties);

	VkShaderModule createShader(const std::string& shaderSource, VkShaderStageFlagBits inStage);

protected:
	enum {
		MAX_BUFFERED_FRAMES = 2
	};

	VkResult fillBuffer(NvVkStagingBuffer* staging, NvVkBuffer& buffer, size_t offset, size_t size, const void* data);
	VkShaderModule createShader(const char* shaderSource, VkShaderStageFlagBits inStage);

	virtual bool reshape(int32_t& w, int32_t& h);

	virtual VkSemaphore* getWaitSync() = 0;
	virtual VkSemaphore* getSignalSync() = 0;

	VkInstance    _instance;

	VkPhysicalDevice            _physicalDevice;
	VkPhysicalDeviceProperties  _physicalDeviceProperties;
	VkPhysicalDeviceMemoryProperties _physicalDeviceMemoryProperties;
	VkPhysicalDeviceLimits      _physicalDeviceLimits;
    VkPhysicalDeviceFeatures    _physicalDeviceFeatures;
    VkPhysicalDeviceFeatures    _physicalDeviceFeaturesEnabled;
	VkDevice      _device;

	VkQueue       _queue;
	VkQueueFamilyProperties _queueProperties;

    uint32_t        _queueFamilyIndex;
    uint32_t        _queueIndex;
	VkCommandPool mTempCmdPool;

	struct FrameInfo {
		VkFence                      mFence;
		std::vector<VkCommandBuffer> mSubmittedTempCmdBuffers;
		bool mOpen;
		VkCommandBuffer mCmdBuffers;
	};

	FrameInfo mFrames[MAX_BUFFERED_FRAMES];
	uint32_t mCurrFrameIndex;

	NvVkStagingBuffer mStaging;

	NvGPUTimerVK* m_frameTimer;

#if VK_EXT_debug_report 
    PFN_vkCreateDebugReportCallbackEXT ext_vkCreateDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT ext_vkDestroyDebugReportCallbackEXT;
    VkDebugReportCallbackEXT msg_callback;
#endif
};


#endif
