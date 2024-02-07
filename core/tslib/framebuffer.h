#pragma once

#include "../ew/external/glad.h"

namespace tslib {
	struct Framebuffer {
		unsigned int fbo;
		unsigned int colorBuffer;
		unsigned int depthBuffer;
		unsigned int width;
		unsigned int height;
	};

	Framebuffer createFramebuffer(unsigned int width, unsigned int height, int colorFormat) {
		Framebuffer fb = Framebuffer();

		glCreateFramebuffers(1, &fb.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);

		glGenTextures(1, &fb.colorBuffer);
		glBindTexture(GL_TEXTURE_2D, fb.colorBuffer);
		glTexStorage2D(GL_TEXTURE_2D, 1, colorFormat, width, height);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fb.colorBuffer, 0);

		glGenTextures(1, &fb.depthBuffer);
		glBindTexture(GL_TEXTURE_2D, fb.depthBuffer);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, width, height);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fb.depthBuffer, 0);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fb.colorBuffer, 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, fb.depthBuffer, 0);

		fb.width = width;
		fb.height = height;

		return fb;
	}
}