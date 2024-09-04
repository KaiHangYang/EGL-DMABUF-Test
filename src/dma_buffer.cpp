#include <iostream>
#include <mutex>

#include "dma_buffer.hpp"
#include "logging.hpp"

#if defined(__ANDROID__)
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#endif


extern "C" {
// DMA Buffer function type.
__attribute__((weak)) extern void *CreateDmabufHeapBufferAllocator();
typedef void *(*rem_dmabuf_create_fn)();

__attribute__((weak)) extern int
DmabufHeapAlloc(void *buffer_allocator, const char *heap_name, size_t len,
                unsigned int heap_flags, size_t legacy_align);
typedef int (*rem_dmabuf_alloc_fn)(void *, const char *, size_t, unsigned int,
                                   size_t);
__attribute__((weak)) extern void
FreeDmabufHeapBufferAllocator(void *buffer_allocator);
typedef void (*rem_dmabuf_deinit_fn)(void *);

const char *dmabuflib_name = "libdmabufheap.so";
}

// Currently only implement DMABuffer for android
class DMABufferLib {
public:
  using Ptr = std::shared_ptr<DMABufferLib>;
  static Ptr GetInstance();
  ~DMABufferLib();

  bool Alloc(size_t size, size_t alignment, const std::string &heap_name,
             DMAAllocationRecord &record);
  void Free(DMAAllocationRecord &record);

private:
  DMABufferLib() = default;
  bool Setup();
  void Release();

  static std::shared_ptr<DMABufferLib> _instance;
  static std::mutex _mutex;

  void *_lib = nullptr;
  // DMA Buffer function.
  void *_dmabuf_allocator = nullptr;
  rem_dmabuf_create_fn _dmabuf_create_fn = nullptr;
  rem_dmabuf_alloc_fn _dmabuf_alloc_fn = nullptr;
  rem_dmabuf_deinit_fn _dmabuf_deinit_fn = nullptr;
};

// The release order is LIFO (not FIFO)
std::mutex DMABufferLib::_mutex;
DMABufferLib::Ptr DMABufferLib::_instance = nullptr;

DMABufferLib::Ptr DMABufferLib::GetInstance() {
#if defined(__ANDROID__)
  if (_instance == nullptr) {
    // Init the instance.
    DMABufferLib::Ptr cur_instance = DMABufferLib::Ptr(new DMABufferLib);
    if (cur_instance->Setup()) {
      _instance = cur_instance;
    }
  }
  return _instance;
#else
  return nullptr;
#endif
}

bool DMABufferLib::Setup() {
#if defined(__ANDROID__)
  if (_dmabuf_allocator != nullptr) {
    // Already inited.
    return true;
  }
  MY_LOGD("Setup allocator.");
  std::lock_guard<std::mutex> lock(_mutex);

  if (_lib == nullptr) {
    // if lib is closed.
    _lib = dlopen(dmabuflib_name, RTLD_LAZY);
  }

  if (_lib) {
    MY_LOGD("Setup dmabuf.");
    _dmabuf_create_fn =
        (rem_dmabuf_create_fn)dlsym(_lib, "CreateDmabufHeapBufferAllocator");
    _dmabuf_deinit_fn =
        (rem_dmabuf_deinit_fn)dlsym(_lib, "FreeDmabufHeapBufferAllocator");
    _dmabuf_alloc_fn = (rem_dmabuf_alloc_fn)dlsym(_lib, "DmabufHeapAlloc");

    if (!_dmabuf_create_fn || !_dmabuf_deinit_fn || !_dmabuf_alloc_fn) {
      MY_LOGE("Failed to get function pointers from libdmabufheap.so");
      return false;
    }
    _dmabuf_allocator = _dmabuf_create_fn();
    if (!_dmabuf_allocator) {
      MY_LOGE("Failed to init dmabuf.");
      return false;
    }
    MY_LOGD("Successfully setup dmabuf.");
    return true;
  } else {
    MY_LOGE("Failed to open libdmabufheap.so.");
    return false;
  }
  return true;
#else
  return false;
#endif
}

void DMABufferLib::Release() {
#if defined(__ANDROID__)
  std::lock_guard<std::mutex> lock(_mutex);
  if (_dmabuf_allocator) {
    MY_LOGD("Deinit dmabuf.");
    _dmabuf_deinit_fn(_dmabuf_allocator);
    _dmabuf_allocator = nullptr;

    _dmabuf_create_fn = nullptr;
    _dmabuf_deinit_fn = nullptr;
    _dmabuf_alloc_fn = nullptr;
    if (_lib) {
      // close the lib
      dlclose(_lib);
      _lib = nullptr;
    }
  }
#else
  return;
#endif
}

DMABufferLib::~DMABufferLib() { Release(); }

bool DMABufferLib::Alloc(size_t size, size_t alignment,
                         const std::string &heap_name,
                         DMAAllocationRecord &record) {
#if defined(__ANDROID__)
  // Hexagon can only access a small number of mappings of these
  // sizes. We reduce the number of mappings required by aligning
  // large allocations to these sizes.
  // const size_t alignments[] = {0x1000, 0x4000, 0x10000, 0x40000, 0x100000};
  // size_t alignment = alignments[0];

  // Align the size up to the minimum aligment.
  size = (size + alignment - 1) & ~(alignment - 1);

  int buf_fd = 0;

  // const char *dmabuf_heap = "system"; // "qcom,system"
  buf_fd = _dmabuf_alloc_fn(_dmabuf_allocator, heap_name.c_str(), size, 0, 0);
  if (buf_fd < 0) {
    MY_LOGE("DmabufHeapAlloc(\"%s\", %i, %i, %i) failed", heap_name.c_str(), size,
                 0, 0);
    return false;
  }

  record.buf_fd = buf_fd;
  record.size = size;
  MY_LOGD("DMABufferLib::Alloc: alloc succeeded");
  return true;
#else
  return false;
#endif
}

void DMABufferLib::Free(DMAAllocationRecord &record) {
#if defined(__ANDROID__)
  // [ATTENTION]: This api must be called after unregister and unmaped.

  // free the ION or DMA-BUF allocation
  if (record.buf_fd >= 0) {
    close(record.buf_fd);
    // then reset it.
    record.buf_fd = -1;
    record.size = 0;
  } else {
    MY_LOGW("DMABufferLib::Free: buf_fd is invalid, %i", record.buf_fd);
  }
  MY_LOGD("DMABufferLib::Free: free buffer.");
#else
  return;
#endif
}

DMABuffer::Ptr DMABuffer::Create(size_t size, size_t alignment,
                                 const std::string &heap_name) {
#if defined(__ANDROID__)
  DMABufferLib::Ptr dmabuf_lib = DMABufferLib::GetInstance();
  if (!dmabuf_lib) {
    MY_LOGE("DMABuffer::Create: Get dmabuf lib failed.");
    return nullptr;
  }

  Ptr new_dma_buffer = DMABuffer::Ptr(new DMABuffer);
  new_dma_buffer->_size = size;
  new_dma_buffer->_is_owning = true;
  if (dmabuf_lib->Alloc(size, alignment, heap_name, new_dma_buffer->_record)) {
    return new_dma_buffer;
  }
  return nullptr;
#else
  return nullptr;
#endif
}

DMABuffer::~DMABuffer() {
#if defined(__ANDROID__)
  // First unmap and unregister.
  UnMapCPU(); // Finally release the CPU memory.

  // Then free the lib.
  DMABufferLib::Ptr dmabuf_lib = DMABufferLib::GetInstance();
  if (!dmabuf_lib) {
    MY_LOGE("DMABuffer::~DMABuffer: Get dmabuf lib failed.");
    return;
  }

  if (_is_owning) {
    // own the dma buffer, so free it.
    dmabuf_lib->Free(_record);
  }

#else
  return;
#endif
}

bool DMABuffer::SyncBegin(SyncType sync_type) {
#if defined(__ANDROID__)
  dma_buf_sync sync;
  sync.flags = DMA_BUF_SYNC_START;
  if (SyncType::READ_ONLY == sync_type) {
    // Read only.
    sync.flags |= DMA_BUF_SYNC_READ;
  } else if (SyncType::WRITE_ONLY == sync_type) {
    // Write only.
    sync.flags |= DMA_BUF_SYNC_WRITE;
  } else {
    // Read write.
    sync.flags |= DMA_BUF_SYNC_RW;
  }
  if (ioctl(_record.buf_fd, DMA_BUF_IOCTL_SYNC, &sync)) {
    MY_LOGE("Error preparing the cache for buffer process. The "
                 "DMA_BUF_IOCTL_SYNC operation returned %s",
                 strerror(errno));
    return false;
  } else {
    MY_LOGD("SyncBegin success.");
  }
  return true;
#else
  return false;
#endif
}

bool DMABuffer::SyncEnd(SyncType sync_type) {
#if defined(__ANDROID__)
  dma_buf_sync sync;
  sync.flags = DMA_BUF_SYNC_END;
  if (SyncType::READ_ONLY == sync_type) {
    // Read only.
    sync.flags |= DMA_BUF_SYNC_READ;
  } else if (SyncType::WRITE_ONLY == sync_type) {
    // Write only.
    sync.flags |= DMA_BUF_SYNC_WRITE;
  } else {
    // Read write.
    sync.flags |= DMA_BUF_SYNC_RW;
  }
  if (ioctl(_record.buf_fd, DMA_BUF_IOCTL_SYNC, &sync)) {
    MY_LOGE("Error preparing the cache for buffer process. The "
                 "DMA_BUF_IOCTL_SYNC operation returned %s",
                 strerror(errno));
    return false;
  } else {
    MY_LOGD("SyncEnd success.");
  }
  return true;
#else
  return false;
#endif
}

void *DMABuffer::GetCPUPtr() {
#if defined(__ANDROID__)
  if (_cpu_ptr == nullptr) {
    // Map the file buffer to a cpu pointer.
    void *buf = mmap(nullptr, _record.size, PROT_READ | PROT_WRITE, MAP_SHARED,
                     _record.buf_fd, 0);
    if (buf == MAP_FAILED) {
      MY_LOGE("DMABuffer::GetCPUPtr: mmap(nullptr, %ld, PROT_READ | "
                   "PROT_WRITE, MAP_SHARED, %i, 0) failed",
                   (long long)_record.size, _record.buf_fd);
      return nullptr;
    }

    MY_LOGD("DMABuffer::GetCPUPtr: mmaped cpu ptr");
    _cpu_ptr = buf;
  }
  return _cpu_ptr;
#else
  return nullptr;
#endif
}

void DMABuffer::UnMapCPU() {
#if defined(__ANDROID__)
  if (_cpu_ptr) {
    // Unmap the cpu pointer.
    MY_LOGD("DMABuffer::UnMapCPU: unmaped cpu pointer.");
    munmap(_cpu_ptr, _record.size);
    _cpu_ptr = nullptr;
  }
#else
  return;
#endif
}
