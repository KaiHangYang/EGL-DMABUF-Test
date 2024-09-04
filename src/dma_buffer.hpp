#ifndef __DMA_BUFFER_H__
#define __DMA_BUFFER_H__

#include <memory>
#include <string>

struct DMAAllocationRecord {
  int buf_fd = -1;
  size_t size = 0;
};

class DMABuffer {
public:
  using Ptr = std::shared_ptr<DMABuffer>;
  static Ptr Create(size_t size, size_t alignment = 0x1000,
                    const std::string &heap_name = "system");
  ~DMABuffer();

  /*
   * For data consistency, you need to use the buffer as follow, eg:
   * ...
   * if (!SyncBegin(SyncType::READ_ONLY)) {
   *    // Failed to sync, do somethings.
   *    return;
   * }
   * // Read the data by cpu pointer.
   * ...
   * if(!SyncEnd(SyncType::READ_ONLY)) {
   *    // Failed to sync, do somethings.
   *    return;
   * }
   * ...
   */
  enum class SyncType { READ_ONLY = 0, WRITE_ONLY = 1, READ_WRITE = 2 };
  bool SyncBegin(SyncType sync_type);
  bool SyncEnd(SyncType sync_type);
  // Map to cpu pointer.
  void *GetCPUPtr();

  // The create size.
  size_t GetSize() const { return _size; }
  // The aligned buffer size.
  size_t GetRecordSize() const { return _record.size; }

  int GetRecordFD() const { return _record.buf_fd; }

private:
  DMABuffer() = default;

  void UnMapCPU();

  bool _is_owning = false;
  DMAAllocationRecord _record;
  void *_cpu_ptr = nullptr;
  size_t _size = 0;
};

#endif // __DMA_BUFFER_H__