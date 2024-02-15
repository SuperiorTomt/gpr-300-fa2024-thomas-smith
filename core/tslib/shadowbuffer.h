#pragma once

#include "../ew/external/glad.h"

namespace tslib {
	struct Shadowbuffer {
		unsigned int fbo;
		unsigned int shadowMap;
		unsigned int colorBuffer;
		unsigned int depthBuffer;
		unsigned int resolution;
	};

	Shadowbuffer createShadowbuffer(unsigned int resolution) {
		Shadowbuffer sb = Shadowbuffer();

		glCreateFramebuffers(1, &sb.fbo);
		glGenTextures(1, &sb.shadowMap);
		glBindTexture(GL_TEXTURE_2D, sb.shadowMap);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, resolution, resolution);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

		glBindFramebuffer(GL_FRAMEBUFFER, sb.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sb.shadowMap, 0);

		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		sb.resolution = resolution;

		return sb;
	}
}