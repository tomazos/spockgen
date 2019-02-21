#pragma once

namespace spkx {

constexpr size_t roundup(size_t x, size_t f) {
  size_t r = x % f;
  if (r == 0)
    return x;
  else
    return x - r + f;
}

struct AllocationHeader {
  size_t payload_size;
};
static_assert(sizeof(AllocationHeader) % alignof(AllocationHeader) == 0);

inline AllocationHeader* payload_to_header(void* payload) {
  return (AllocationHeader*)(((char*)payload) - sizeof(AllocationHeader));
}
inline void* header_to_payload(AllocationHeader* header) {
  return ((char*)header) + sizeof(AllocationHeader);
}

inline void* create_allocation(const size_t payload_size,
                               const size_t payload_alignment) {
  const size_t actual_alignment =
      std::max(payload_alignment, alignof(AllocationHeader));
  const size_t actual_size =
      roundup(sizeof(AllocationHeader) + payload_size, actual_alignment);
  AllocationHeader* header =
      (AllocationHeader*)std::aligned_alloc(actual_alignment, actual_size);
  if (header == nullptr) return nullptr;
  header->payload_size = payload_size;
  return header_to_payload(header);
}

struct Allocator {
  void* allocate(size_t payload_size, size_t payload_alignment,
                 spk::system_allocation_scope allocation_scope) {
    void* new_payload = create_allocation(payload_size, payload_alignment);

    LOG(ERROR) << "allocate size " << payload_size << " alignment "
               << payload_alignment << " allocation_scope " << allocation_scope
               << " ptr " << new_payload;
    return new_payload;
  }

  void* reallocate(void* original_payload, size_t payload_size,
                   size_t payload_alignment,
                   spk::system_allocation_scope allocation_scope) {
    void* new_payload = create_allocation(payload_size, payload_alignment);
    if (new_payload != nullptr && original_payload != nullptr) {
      AllocationHeader* original_header = payload_to_header(original_payload);
      std::memcpy(new_payload, original_payload,
                  std::min(original_header->payload_size, payload_size));
      std::free(original_header);
    }
    LOG(ERROR) << "reallocate original " << original_payload << " size "
               << payload_size << " alignment " << payload_alignment
               << " allocation_scope " << allocation_scope << " ptr "
               << new_payload;
    return new_payload;
  }

  void free(void* memory) {
    if (memory == nullptr) return;
    AllocationHeader* header = payload_to_header(memory);
    LOG(ERROR) << "free memory " << memory;
    std::free(header);
  }

  void notify_internal_allocation(
      size_t size, spk::internal_allocation_type allocation_type,
      spk::system_allocation_scope allocation_scope) {
    LOG(ERROR) << "notify_internal_allocation size " << size
               << " allocation_type " << allocation_type << " allocation_scope "
               << allocation_scope;
  }

  void notify_internal_free(size_t size,
                            spk::internal_allocation_type allocation_type,
                            spk::system_allocation_scope allocation_scope) {
    LOG(ERROR) << "notify_internal_free size " << size << " allocation_type "
               << allocation_type << " allocation_scope " << allocation_scope;
  }
};

}  // namespace spkx
