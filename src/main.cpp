#if defined(__ANDROID__)
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

// GL related.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <android/hardware_buffer.h>
#include <drm/drm_fourcc.h>

#include "dma_buffer.hpp"
#include "logging.hpp"

const int image_width = 512;
const int image_height = 512;
const int image_channel = 4; // suppose RGBA

EGLDisplay display;
EGLContext context;
EGLSurface surface;
EGLConfig config;

bool InitializeEGL() {
  display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY) {
    MY_LOGE("Failed to get EGL display.");
    return false;
  }

  if (!eglInitialize(display, nullptr, nullptr)) {
    MY_LOGE("Failed to initialize EGL.");
    return false;
  }
  const char *eglVersion = eglQueryString(display, EGL_VERSION);
  const char *eglVendor = eglQueryString(display, EGL_VENDOR);
  const char *eglClientApis = eglQueryString(display, EGL_CLIENT_APIS);
  MY_LOGI("EGL Version: %s", eglVersion);
  MY_LOGI("EGL Vendor: %s", eglVendor);
  MY_LOGI("EGL Client APIs: %s", eglClientApis);
  const char *eglExtensions = eglQueryString(display, EGL_EXTENSIONS);
  MY_LOGI("EGL Extensions: %s", eglExtensions);

  if (std::string(eglExtensions).find("EGL_EXT_image_dma_buf_import") == std::string::npos) {
    MY_LOGI("EGL doesn't support \"EGL_EXT_image_dma_buf_import\" extension.");
  }

  EGLint configAttribs[] = {EGL_RENDERABLE_TYPE,
                            EGL_OPENGL_ES2_BIT,
                            EGL_CONFORMANT,
                            EGL_OPENGL_ES2_BIT,
                            EGL_SURFACE_TYPE,
                            EGL_PBUFFER_BIT,
                            EGL_NONE};

  EGLint numConfigs;
  if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) ||
      numConfigs <= 0) {
    MY_LOGE("Failed to choose EGL config.");
    return false;
  }

  EGLint pbufferAttribs[] = {EGL_WIDTH, 512, EGL_HEIGHT, 512, EGL_NONE};

  surface = eglCreatePbufferSurface(display, config, pbufferAttribs);
  if (surface == EGL_NO_SURFACE) {
    MY_LOGE("Failed to create EGL Pbuffer surface.");
    return false;
  }

  EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
  if (context == EGL_NO_CONTEXT) {
    MY_LOGE("Failed to create EGL context.");
    return false;
  }

  if (!eglMakeCurrent(display, surface, surface, context)) {
    MY_LOGE("Failed to make EGL context current.");
    return false;
  }

  return true;
}

void CleanupEGL() {
  if (display != EGL_NO_DISPLAY) {
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (context != EGL_NO_CONTEXT) {
      eglDestroyContext(display, context);
    }
    if (surface != EGL_NO_SURFACE) {
      eglDestroySurface(display, surface);
    }
    eglTerminate(display);
  }
}

int main() {
  if (!InitializeEGL()) {
    MY_LOGE("Failed to init EGL.");
    return EXIT_FAILURE;
  }

  auto dma_buf = DMABuffer::Create(image_width * image_height * image_channel *
                                       sizeof(uint8_t),
                                   0x1000, "system");

  MY_LOGI("dmabuf: 0x%X, dmabuf_fd: %d", dma_buf.get(), dma_buf->GetRecordFD());

  auto eglCreateImageKHR =
      (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");

  EGLint attribs[] = {EGL_WIDTH,
                      image_width,
                      EGL_HEIGHT,
                      image_height,
                      EGL_LINUX_DRM_FOURCC_EXT,
                      DRM_FORMAT_ARGB8888,
                      EGL_DMA_BUF_PLANE0_FD_EXT,
                      dma_buf->GetRecordFD(), // fd
                      EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                      0,
                      EGL_DMA_BUF_PLANE0_PITCH_EXT,
                      image_width * image_channel,
                      EGL_NONE};

  EGLImageKHR eglImage = eglCreateImageKHR(
      display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attribs);

  MY_LOGI("eglimage: 0x%X", eglImage);
  EGLint egl_error = eglGetError();
  if (egl_error != EGL_SUCCESS) {
    MY_LOGE("eglCreateImageKHR failed: 0x%X", egl_error);
    return EXIT_SUCCESS;
  }

  CleanupEGL();

  return EXIT_SUCCESS;
}

#endif
