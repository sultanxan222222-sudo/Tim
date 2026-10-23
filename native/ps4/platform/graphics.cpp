#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <new>

#include <string>

#include "graphics.h"
#include "log.h"

// Single translation-unit definition for the shared logger stream.
std::stringstream debugLogStream;

Scene2D::Scene2D(int w, int h, int pixelDepth)
{
	this->width = w;
	this->height = h;
	this->depth = pixelDepth;
	this->video = -1;
	this->directMemOff = 0;
	this->directMemAllocationSize = 0;
	this->videoMemSP = 0;
	this->videoMem = NULL;
	this->frameBuffers = NULL;
	this->flipQueue = 0;
	this->flipQueueReady = false;
	this->buffersRegistered = false;
	this->frameBufferCount = 0;
	this->activeFrameBufferIdx = 0;
	memset(&this->attr, 0, sizeof(this->attr));
	this->frameBufferSize = 0;
}

Scene2D::~Scene2D()
{
	/* Unregister GPU-visible memory before unmapping and freeing its backing. */
	if (this->buffersRegistered && this->video >= 0) {
		sceVideoOutUnregisterBuffers(this->video, 0);
		this->buffersRegistered = false;
	}
	deallocateVideoMem();
	if (this->flipQueueReady) {
		sceKernelDeleteEqueue(this->flipQueue);
		this->flipQueueReady = false;
		this->flipQueue = 0;
	}
	if (this->video >= 0) {
		sceVideoOutClose(this->video);
		this->video = -1;
	}
}

bool Scene2D::Init(size_t memSize, int numFrameBuffers)
{
	int rc;
	size_t w, h, d, pixels, bytesPerFrame;

	/* Renderer stores 32-bit pixels and swaps at least two buffers. */
	if (this->width <= 0 || this->height <= 0 || this->depth != 4 ||
	    numFrameBuffers < 2)
		return false;
	w = (size_t)this->width;
	h = (size_t)this->height;
	d = (size_t)this->depth;
	if (w > SIZE_MAX / h) return false;
	pixels = w * h;
	if (pixels > SIZE_MAX / d) return false;
	bytesPerFrame = pixels * d;
	if (bytesPerFrame > (size_t)INT_MAX ||
	    bytesPerFrame > memSize / (size_t)numFrameBuffers)
		return false;
	this->frameBufferSize = (int)bytesPerFrame;

	this->video = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0);
	this->videoMem = NULL;
	if (this->video < 0) {
		DEBUGLOG << "Failed to open a video out handle: " << std::string(strerror(errno));
		return false;
	}

#ifdef GRAPHICS_USES_FONT
	rc = sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL);
	if (rc < 0) {
		DEBUGLOG << "Failed to load freetype: " << std::string(strerror(errno));
		return false;
	}
	rc = FT_Init_FreeType(&this->ftLib);
	if (rc != 0) {
		DEBUGLOG << "Failed to initialize freetype: " << std::string(strerror(errno));
		return false;
	}
#endif

	if (!initFlipQueue()) {
		DEBUGLOG << "Failed to initialize flip queue: " << std::string(strerror(errno));
		return false;
	}
	if (!allocateVideoMem(memSize, 0x200000)) {
		DEBUGLOG << "Failed to allocate video memory: " << std::string(strerror(errno));
		return false;
	}
	if (!allocateFrameBuffers(numFrameBuffers)) {
		DEBUGLOG << "Failed to allocate frame buffers: " << std::string(strerror(errno));
		return false;
	}

	sceVideoOutSetFlipRate(this->video, 0);
	return true;
}
bool Scene2D::initFlipQueue()
{
	int rc = sceKernelCreateEqueue(&flipQueue, "TIM flip queue");
	if (rc < 0) return false;
	this->flipQueueReady = true;
	rc = sceVideoOutAddFlipEvent(flipQueue, this->video, 0);
	if (rc < 0) {
		sceKernelDeleteEqueue(flipQueue);
		this->flipQueueReady = false;
		this->flipQueue = 0;
		return false;
	}
	return true;
}

bool Scene2D::allocateFrameBuffers(int num)
{
	if (num < 2 || this->videoMem == NULL || this->frameBufferSize <= 0 ||
	    (size_t)this->frameBufferSize > this->directMemAllocationSize / (size_t)num)
		return false;

	this->frameBuffers = new (std::nothrow) char*[num]();
	if (this->frameBuffers == NULL) return false;
	for (int i = 0; i < num; ++i) {
		this->frameBuffers[i] = this->allocateDisplayMem((size_t)frameBufferSize);
		if (this->frameBuffers[i] == NULL) return false;
	}

	sceVideoOutSetBufferAttribute(&this->attr, 0x80000000, 1, 0,
		this->width, this->height, this->width);
	if (sceVideoOutRegisterBuffers(this->video, 0,
			(void * const *)this->frameBuffers, num, &this->attr) != 0)
		return false;

	this->buffersRegistered = true;
	this->frameBufferCount = num;
	return true;
}

char *Scene2D::allocateDisplayMem(size_t size)
{
	if (this->videoMem == NULL || this->directMemAllocationSize == 0)
		return NULL;
	const uintptr_t base = (uintptr_t)this->videoMem;
	const uintptr_t end = base + this->directMemAllocationSize;
	if (this->videoMemSP < base || this->videoMemSP > end ||
	    size > (size_t)(end - this->videoMemSP))
		return NULL;
	char *allocatedPtr = (char *)this->videoMemSP;
	this->videoMemSP += size;
	return allocatedPtr;
}

bool Scene2D::allocateVideoMem(size_t size, int alignment)
{
	int rc;
	
	if (alignment <= 0 || size == 0 ||
	    size > SIZE_MAX - ((size_t)alignment - 1))
		return false;
	this->videoMem = NULL;
	// Align the allocation size
	this->directMemAllocationSize =
		(size + (size_t)alignment - 1) / (size_t)alignment * (size_t)alignment;
	
	// Allocate memory for display buffer
	rc = sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), this->directMemAllocationSize, alignment, 3, &this->directMemOff);
	
	if(rc < 0)
	{
		this->directMemAllocationSize = 0;
		return false;
	}
	
	// Map the direct memory
	rc = sceKernelMapDirectMemory(&this->videoMem, this->directMemAllocationSize, 0x33, 0, this->directMemOff, alignment);
	
	if(rc < 0)
	{
		sceKernelReleaseDirectMemory(this->directMemOff, this->directMemAllocationSize);
		
		this->directMemOff = 0;
		this->directMemAllocationSize = 0;
		
		return false;
	}
	
	// Set the stack pointer to the beginning of the buffer
	this->videoMemSP = (uintptr_t)this->videoMem;
	return true;
}

void Scene2D::deallocateVideoMem()
{
	/* Unmap the CPU virtual mapping before releasing its physical allocation. */
	if (this->videoMem != NULL && this->directMemAllocationSize > 0)
		sceKernelMunmap(this->videoMem, this->directMemAllocationSize);
	if (this->directMemAllocationSize > 0)
		sceKernelReleaseDirectMemory(this->directMemOff, this->directMemAllocationSize);
	this->videoMem = NULL;
	this->videoMemSP = 0;
	this->directMemOff = 0;
	this->directMemAllocationSize = 0;
	delete[] this->frameBuffers;
	this->frameBuffers = NULL;
	this->frameBufferCount = 0;
}
void Scene2D::SetActiveFrameBuffer(int index)
{
	if (index >= 0 && index < this->frameBufferCount)
		this->activeFrameBufferIdx = index;
}

void Scene2D::SubmitFlip(int frameID)
{
	sceVideoOutSubmitFlip(this->video, this->activeFrameBufferIdx, ORBIS_VIDEO_OUT_FLIP_VSYNC, frameID);
}

void Scene2D::FrameWait(int frameID)
{
	OrbisKernelEvent evt;
	int count;
	
	// If the video handle is not initialized, bail out. This is mostly a failsafe, this should never happen.
	if (this->video < 0 || !this->flipQueueReady)
		return;
		
	for(;;)
	{
		OrbisVideoOutFlipStatus flipStatus;
		
		// Get the flip status and check the arg for the given frame ID
		sceVideoOutGetFlipStatus(video, &flipStatus);
		
		if(flipStatus.flipArg == frameID)
			break;
			
		// Wait on next flip event
		if(sceKernelWaitEqueue(this->flipQueue, &evt, 1, &count, 0) != 0)
			break;
	}
}

void Scene2D::FrameBufferSwap()
{
	if (this->frameBufferCount < 2) return;
	this->activeFrameBufferIdx =
		(this->activeFrameBufferIdx + 1) % this->frameBufferCount;
}

void Scene2D::FrameBufferClear()
{
	// Clear the screen with a white frame buffer
	Color blank = { 255, 255, 255 };
	FrameBufferFill(blank);
}

#ifdef GRAPHICS_USES_FONT
bool Scene2D::InitFont(FT_Face *face, const char *fontPath, int fontSize)
{
	int rc;
	
	rc = FT_New_Face(this->ftLib, fontPath, 0, face);
	
	if(rc < 0)
		return false;

	rc = FT_Set_Pixel_Sizes(*face, 0, fontSize);
	
	if(rc < 0)
		return false;
	
	return true;
}
#endif

void Scene2D::FrameBufferFill(Color color)
{
	DrawRectangle(0, 0, this->width, this->height, color);
}

void Scene2D::DrawPixel(int x, int y, Color color)
{
	if (x < 0 || y < 0 || x >= this->width || y >= this->height ||
	    this->frameBuffers == NULL || this->activeFrameBufferIdx < 0 ||
	    this->activeFrameBufferIdx >= this->frameBufferCount ||
	    this->frameBuffers[this->activeFrameBufferIdx] == NULL)
		return;

	const size_t pixel = (size_t)y * (size_t)this->width + (size_t)x;
	const uint32_t encodedColor = 0x80000000U |
		((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | color.b;
	((uint32_t *)this->frameBuffers[this->activeFrameBufferIdx])[pixel] = encodedColor;
}
void Scene2D::DrawRectangle(int x, int y, int w, int h, Color color)
{
	if (w <= 0 || h <= 0 || this->width <= 0 || this->height <= 0 ||
	    this->frameBuffers == NULL || this->activeFrameBufferIdx < 0 ||
	    this->activeFrameBufferIdx >= this->frameBufferCount ||
	    this->frameBuffers[this->activeFrameBufferIdx] == NULL)
		return;

	/* Widen before coordinate addition, then clip to avoid signed overflow. */
	int64_t left = x, top = y;
	int64_t right = left + (int64_t)w, bottom = top + (int64_t)h;
	if (right <= 0 || bottom <= 0 || left >= this->width || top >= this->height)
		return;
	if (left < 0) left = 0;
	if (top < 0) top = 0;
	if (right > this->width) right = this->width;
	if (bottom > this->height) bottom = this->height;
	if (right <= left || bottom <= top) return;

	const int clippedLeft = (int)left;
	const int clippedTop = (int)top;
	const int clippedWidth = (int)(right - left);
	const int clippedHeight = (int)(bottom - top);
	const uint32_t encodedColor = 0x80000000U |
		((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | color.b;
	uint32_t *pixels = (uint32_t *)this->frameBuffers[this->activeFrameBufferIdx];
	for (int row = 0; row < clippedHeight; ++row) {
		uint32_t *dst = pixels + (size_t)(clippedTop + row) * (size_t)this->width
			+ (size_t)clippedLeft;
		for (int col = 0; col < clippedWidth; ++col)
			dst[col] = encodedColor;
	}
}
#ifdef GRAPHICS_USES_FONT
void Scene2D::DrawText(char *txt, FT_Face face, int startX, int startY, Color bgColor, Color fgColor)
{
	int rc;
	int xOffset = 0;
	int yOffset = 0;
	
	// Get the glyph slot for bitmap and font metrics
	FT_GlyphSlot slot = face->glyph;
	
	// Iterate each character of the text to write to the screen
	for(int n = 0; n < strlen(txt); n++)
	{
		FT_UInt glyph_index;

        // Get the glyph for the ASCII code
        glyph_index = FT_Get_Char_Index(face, txt[n]);

        // Load and render in 8-bit color
        rc = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);

        if (rc)
            continue;

        rc = FT_Render_Glyph(slot, ft_render_mode_normal);

        if (rc)
            continue;

        // If we get a newline, increment the y offset, reset the x offset, and skip to the next character
        if (txt[n] == '\n')
        {
            xOffset = 0;
            yOffset += slot->bitmap.width * 2;

            continue;
        }

        // Parse and write the bitmap to the frame buffer
        for (int yPos = 0; yPos < slot->bitmap.rows; yPos++)
        {
            for (int xPos = 0; xPos < slot->bitmap.width; xPos++)
            {
                // Decode the 8-bit bitmap
                char pixel = slot->bitmap.buffer[(yPos * slot->bitmap.width) + xPos];

                // Get new pixel coordinates to account for the character position and baseline, as well as newlines
                int x = startX + xPos + xOffset + slot->bitmap_left;
                int y = startY + yPos + yOffset - slot->bitmap_top;

                // Linearly interpolate between the foreground and background for smoother rendering
				uint8_t r = (pixel * fgColor.r) / 255;
				uint8_t g = (pixel * fgColor.g) / 255;
				uint8_t b = (pixel * fgColor.b) / 255;

                // Create new color struct with lerp'd values
                Color finalColor = { r, g, b };

                // We need to do bounds checking before commiting the pixel write due to our transformations, or we
                // could write out-of-bounds of the frame buffer
                if (x < 0 || y < 0 || x >= this->width || y >= this->height)
                    continue;

                // If the pixel in the bitmap isn't blank, we'll draw it
                if(pixel != 0x00)
                    this->DrawPixel(x, y, finalColor);
            }
        }

        // Increment x offset for the next character
        xOffset += slot->advance.x >> 6;
    }
}
#endif

