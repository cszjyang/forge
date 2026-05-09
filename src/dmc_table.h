#ifndef _DMC_TABLE_H_
#define _DMC_TABLE_H_

// sid: server id
// fid: fifo id             (virtual)
// gid: group id            (virtual)
// gnode_id: group node id  (storage-related)
// fnode_id: fifo node id   (storage-related)
// Currently, we don't consider fid (uint32_t) overflow
// Note important difference between fid/gid and fnode_id/gnode_id

/*
 * Rationale for WRITE_EVICT_FLAG:
 * In traditional object-level eviction, evicting an object requires an atomic CAS 
 * to prevent double-free issues. However, in FORGE's group-level eviction, the 
 * entire group is evicted atomically, which inherently avoids double-frees. 
 * * Therefore, the evicting CN can safely use a lightweight, exclusive WRITE operation 
 * to mark objects as evicted, bypassing the costly CAS overhead. To prevent data 
 * corruption from concurrent slot updates, we embed this flag within the `group` 
 * field rather than overwriting the `index` field. Concurrent readers are designed 
 * to detect and correctly handle the resulting inconsistency between the `index` 
 * and `group` fields.
 */

 /*
 * Rationale for PREFILL_MM:
 * When enabled (1), Compute Nodes (CNs) statically partition and pre-allocate 
 * the free memory space on Memory Nodes (MNs) during initialization, bypassing 
 * runtime remote segment allocation (i.e., alloc_segment RPCs) during warmup.
 * * This is an experimental simplification that DOES NOT affect the performance 
 * conclusions or the fairness of the evaluation. According to our methodology, 
 * the cache is rigorously warmed up with the first 20% of the workloads. During 
 * this warmup phase, the memory capacity is completely exhausted, and the system 
 * inevitably transitions into a steady-state eviction mode (server_oom_ = true). 
 * * Therefore, during the actual performance measurement phase after warmup, the 
 * segment allocation is never triggered regardless of whether PREFILL_MM is 0 or 1. 
 * All evaluated systems (including baselines) strictly operate under the exact same 
 * steady-state eviction overheads.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "debug.h"
#include "dmc_utils.h"

#define DEFAULT_MAX_GROUP_SIZE (256)

/*
 * FORGE uses a fixed, over-provisioned group metadata table to simplify the
 * implementation and reproducibility. This auxiliary metadata is accounted
 * outside server_data_len; server_data_len represents cache data capacity only.
 */
#define MAX_GROUP_NUM (1048576 * DEFAULT_MAX_GROUP_SIZE / MAX_GROUP_SIZE)
static_assert(MAX_GROUP_SIZE % 8 == 0, "MAX_GROUP_SIZE must be a multiple of 8");

#define BLOCK_SIZE (256)
#define LEN_TO_SIZE(len) ((uint32_t)(((uint16_t)len) << 8))
#define SIZE_TO_LEN(size) ((uint16_t)(((uint32_t)size) >> 8))
#define INVALID_GID (0)
#define PREFILL_MM (1)
#define HOT_THRESH (MAX_GROUP_SIZE / 2)
#define EN_LOCAL_OBJECT_CACHE (0)
#define EN_RNIC_DM (1)
#define MAX_NUM_THREAD (128)
#define GET_CHECK_MM (0)
#define WRITE_EVICT_FLAG (-1)
#define SAMPLE_WAIT_DIE (0)

#define NUM_FIFO_TYPE (2)  // 1: single FIFO, 2: two FIFOs
enum FifoType {
  SFIFO = 0,
  MFIFO = NUM_FIFO_TYPE - 1,
};

/******************************** Object Index /********************************/
#include <cassert>
#include <cstdint>

typedef struct _AtomicEntry {
  uint64_t data;

 private:
  // Constants for bitfield positions and sizes

  // for index entry
  static constexpr uint8_t IDX_KV_LEN_SHIFT = 0;
  static constexpr uint8_t IDX_FP_SHIFT = 8;
  static constexpr uint8_t IDX_POINTER_SHIFT = 16;
  static constexpr uint8_t IDX_LOCK_SHIFT = 60;
  static constexpr uint8_t IDX_VER_SHIFT = 61;

  static constexpr uint64_t IDX_KV_LEN_MASK = 0xFFULL;            // 8 bits
  static constexpr uint64_t IDX_FP_MASK = 0xFFULL;                // 8 bits
  static constexpr uint64_t IDX_POINTER_MASK = 0xFFFFFFFFFFFULL;  // 44 bits
  static constexpr uint64_t IDX_LOCK_MASK = 1ULL;                 // 1 bit
  static constexpr uint64_t IDX_VER_MASK = 0x7ULL;                // 3 bits

  // for extra entry
  static constexpr uint8_t EXT_GID_SHIFT = 0;
  static constexpr uint8_t EXT_FP_SHIFT = 32;
  static constexpr uint8_t EXT_SN_SHIFT = 48;
  static constexpr uint8_t EXT_VER_SHIFT = 61;

  static constexpr uint64_t EXT_GID_MASK = 0xFFFFFFFFULL;  // 32 bits
  static constexpr uint64_t EXT_FP_MASK = 0xFFFFULL;       // 16 bits
  static constexpr uint64_t EXT_SN_MASK = 0x1FFFULL;       // 13 bits
  static constexpr uint64_t EXT_VER_MASK = 0x7ULL;         // 3 bits

  // for group node
  static constexpr uint8_t GNODE_SIZE_SHIFT = 0;
  static constexpr uint8_t GNODE_POINTER_SHIFT = 16;
  static constexpr uint8_t GNODE_STATE_SHIFT = 60;
  static constexpr uint8_t GNODE_SEGMENT_SHIFT = 62;

  static constexpr uint64_t GNODE_SIZE_MASK = 0xFFFFULL;            // 16 bits
  static constexpr uint64_t GNODE_POINTER_MASK = 0xFFFFFFFFFFFULL;  // 44 bits
  static constexpr uint64_t GNODE_STATE_MASK = 0x3ULL;              // 2 bits
  static constexpr uint64_t GNODE_SEGMENT_MASK = 0x3ULL;            // 2 bits

  // for group allocator
  static constexpr uint8_t GALLOC_LOFF_SHIFT = 0;
  static constexpr uint8_t GALLOC_SN_SHIFT = 16;
  static constexpr uint8_t GALLOC_GID_SHIFT = 32;

  static constexpr uint64_t GALLOC_LOFF_MASK = 0xFFFFULL;     // 16 bits
  static constexpr uint64_t GALLOC_SN_MASK = 0xFFFFULL;       // 16 bits
  static constexpr uint64_t GALLOC_GID_MASK = 0xFFFFFFFFULL;  // 32 bits

  // for fifo node
  static constexpr uint8_t FNODE_GID_SHIFT = 0;
  static constexpr uint8_t FNODE_ACTIVE_NUM_SHIFT = 32;
  static constexpr uint8_t FNODE_FLUSH_NUM_SHIFT = 48;
  static constexpr uint8_t FNODE_SEGMENT_SHIFT = 56;
  static constexpr uint8_t FNODE_EPOCH_SHIFT = 60;

  static constexpr uint64_t FNODE_GID_MASK = 0xFFFFFFFFULL;     // 32 bits
  static constexpr uint64_t FNODE_ACTIVE_NUM_MASK = 0xFFFFULL;  // 16 bits
  static constexpr uint64_t FNODE_FLUSH_NUM_MASK = 0xFFULL;     // 8 bits
  static constexpr uint64_t FNODE_SEGMENT_MASK = 0xFULL;        // 4 bits
  static constexpr uint64_t FNODE_EPOCH_MASK = 0xFULL;          // 4 bits

  // for local cache, similar to extra entry
  static constexpr uint8_t LC_GID_SHIFT = 0;
  static constexpr uint8_t LC_KV_SIZE_SHIFT = 32;
  static constexpr uint8_t LC_SN_SHIFT = 48;
  static constexpr uint8_t LC_VER_SHIFT = 61;

  static constexpr uint64_t LC_GID_MASK = 0xFFFFFFFFULL;  // 32 bits
  static constexpr uint64_t LC_KV_SIZE_MASK = 0xFFFFULL;  // 16 bits
  static constexpr uint64_t LC_SN_MASK = 0x1FFFULL;       // 13 bits
  static constexpr uint64_t LC_VER_MASK = 0x7ULL;         // 3 bits

  // Helper template for extracting and setting bitfields
  template <uint8_t SHIFT, uint64_t MASK>
  inline uint64_t get_field() const { return (data >> SHIFT) & MASK; }

  template <uint8_t SHIFT, uint64_t MASK>
  inline void set_field(uint64_t value) { data = (data & ~(MASK << SHIFT)) | ((value & MASK) << SHIFT); }

 public:
  // for index entry
  inline void init_idx(uint8_t fp, uint8_t kv_len, uint64_t pointer, bool lock, uint8_t version) {
    data = ((uint64_t)fp << IDX_FP_SHIFT) | ((uint64_t)kv_len << IDX_KV_LEN_SHIFT) |
           ((uint64_t)pointer << IDX_POINTER_SHIFT) | ((uint64_t)lock << IDX_LOCK_SHIFT) |
           ((uint64_t)version << IDX_VER_SHIFT);
  }
  inline uint8_t get_idx_fp() const { return static_cast<uint8_t>(get_field<IDX_FP_SHIFT, IDX_FP_MASK>()); }
  inline uint8_t get_idx_kv_len() const { return static_cast<uint8_t>(get_field<IDX_KV_LEN_SHIFT, IDX_KV_LEN_MASK>()); }
  inline uint64_t get_idx_pointer() const { return get_field<IDX_POINTER_SHIFT, IDX_POINTER_MASK>(); }
  inline bool get_idx_lock() const { return get_field<IDX_LOCK_SHIFT, IDX_LOCK_MASK>() != 0; }
  inline uint8_t get_idx_version() const { return static_cast<uint8_t>(get_field<IDX_VER_SHIFT, IDX_VER_MASK>()); }

  static constexpr inline uint8_t get_idx_fp_shift() { return IDX_FP_SHIFT; }

  inline void set_idx_fp(uint8_t fp) { set_field<IDX_FP_SHIFT, IDX_FP_MASK>(fp); }
  inline void set_idx_kv_len(uint8_t kv_len) { set_field<IDX_KV_LEN_SHIFT, IDX_KV_LEN_MASK>(kv_len); }
  inline void set_idx_pointer(uint64_t pointer) { set_field<IDX_POINTER_SHIFT, IDX_POINTER_MASK>(pointer); }
  inline void set_idx_lock(bool lock) { set_field<IDX_LOCK_SHIFT, IDX_LOCK_MASK>(lock); }
  inline void set_idx_version(uint8_t version) { set_field<IDX_VER_SHIFT, IDX_VER_MASK>(version); }

  inline uint64_t mask_idx_lock() const { return data & ~(IDX_LOCK_MASK << IDX_LOCK_SHIFT); }
  inline uint64_t mask_idx_version() const { return data & ~(IDX_VER_MASK << IDX_VER_SHIFT); }
  static constexpr inline uint64_t get_idx_pointer_mask() { return IDX_POINTER_MASK << IDX_POINTER_SHIFT; }
  static constexpr inline uint64_t get_idx_lock_mask() { return IDX_LOCK_MASK << IDX_LOCK_SHIFT; }

  // for extra entry
  inline void init_ext(uint32_t gid, uint16_t fp, uint16_t sn, uint8_t version) {
    data = ((uint64_t)gid << EXT_GID_SHIFT) | ((uint64_t)fp << EXT_FP_SHIFT) |
           ((uint64_t)sn << EXT_SN_SHIFT) | ((uint64_t)version << EXT_VER_SHIFT);
  }
  inline uint32_t get_ext_gid() const { return static_cast<uint32_t>(get_field<EXT_GID_SHIFT, EXT_GID_MASK>()); }
  inline uint16_t get_ext_fp() const { return static_cast<uint16_t>(get_field<EXT_FP_SHIFT, EXT_FP_MASK>()); }
  inline uint16_t get_ext_sn() const { return static_cast<uint16_t>(get_field<EXT_SN_SHIFT, EXT_SN_MASK>()); }
  inline uint8_t get_ext_version() const { return static_cast<uint8_t>(get_field<EXT_VER_SHIFT, EXT_VER_MASK>()); }

  inline void set_ext_gid(uint32_t gid) { set_field<EXT_GID_SHIFT, EXT_GID_MASK>(gid); }
  inline void set_ext_fp(uint16_t ext_fp) { set_field<EXT_FP_SHIFT, EXT_FP_MASK>(ext_fp); }
  inline void set_ext_sn(uint16_t sn) { set_field<EXT_SN_SHIFT, EXT_SN_MASK>(sn); }
  inline void set_ext_version(uint8_t version) { set_field<EXT_VER_SHIFT, EXT_VER_MASK>(version); }

  inline uint64_t mask_ext_sn() const { return data & ~(EXT_SN_MASK << EXT_SN_SHIFT); }

  // for group node
  inline void init_gnode(uint16_t size, uint64_t pointer, uint8_t state, uint8_t segment) {
    data = ((uint64_t)size << GNODE_SIZE_SHIFT) | ((uint64_t)pointer << GNODE_POINTER_SHIFT) |
           ((uint64_t)state << GNODE_STATE_SHIFT) | ((uint64_t)segment << GNODE_SEGMENT_SHIFT);
  }
  inline uint16_t get_gnode_size() const { return static_cast<uint16_t>(get_field<GNODE_SIZE_SHIFT, GNODE_SIZE_MASK>()); }
  inline uint64_t get_gnode_pointer() const { return get_field<GNODE_POINTER_SHIFT, GNODE_POINTER_MASK>(); }
  inline uint8_t get_gnode_state() const { return static_cast<uint8_t>(get_field<GNODE_STATE_SHIFT, GNODE_STATE_MASK>()); }
  inline uint8_t get_gnode_segment() const { return static_cast<uint8_t>(get_field<GNODE_SEGMENT_SHIFT, GNODE_SEGMENT_MASK>()); }

  inline void set_gnode_size(uint16_t size) { set_field<GNODE_SIZE_SHIFT, GNODE_SIZE_MASK>(size); }
  inline void set_gnode_pointer(uint64_t pointer) { set_field<GNODE_POINTER_SHIFT, GNODE_POINTER_MASK>(pointer); }
  inline void set_gnode_state(uint8_t state) { set_field<GNODE_STATE_SHIFT, GNODE_STATE_MASK>(state); }
  inline void set_gnode_segment(uint8_t segment) { set_field<GNODE_SEGMENT_SHIFT, GNODE_SEGMENT_MASK>(segment); }

  // for group allocator
  inline void init_galloc(uint16_t loff, uint16_t sn, uint32_t gid) {
    data = ((uint64_t)loff << GALLOC_LOFF_SHIFT) | ((uint64_t)sn << GALLOC_SN_SHIFT) |
           ((uint64_t)gid << GALLOC_GID_SHIFT);
  }
  inline uint16_t get_galloc_loff() const { return static_cast<uint16_t>(get_field<GALLOC_LOFF_SHIFT, GALLOC_LOFF_MASK>()); }
  inline uint16_t get_galloc_sn() const { return static_cast<uint16_t>(get_field<GALLOC_SN_SHIFT, GALLOC_SN_MASK>()); }
  inline uint32_t get_galloc_gid() const { return static_cast<uint32_t>(get_field<GALLOC_GID_SHIFT, GALLOC_GID_MASK>()); }
  inline bool is_galloc_full() const { return get_galloc_gid() == INVALID_GID || get_galloc_sn() > MAX_GROUP_SIZE || get_galloc_loff() > SIZE_TO_LEN(GROUP_BYTES); }

  inline void set_galloc_loff(uint16_t loff) { set_field<GALLOC_LOFF_SHIFT, GALLOC_LOFF_MASK>(loff); }
  inline void set_galloc_sn(uint16_t sn) { set_field<GALLOC_SN_SHIFT, GALLOC_SN_MASK>(sn); }
  inline void set_galloc_gid(uint32_t gid) { set_field<GALLOC_GID_SHIFT, GALLOC_GID_MASK>(gid); }

  // for fifo node
  inline void init_fnode(uint32_t gid, uint16_t active_num, uint8_t flush_num, uint8_t segment, uint8_t epoch) {
    data = ((uint64_t)gid << FNODE_GID_SHIFT) | ((uint64_t)active_num << FNODE_ACTIVE_NUM_SHIFT) |
           ((uint64_t)flush_num << FNODE_FLUSH_NUM_SHIFT) | ((uint64_t)segment << FNODE_SEGMENT_SHIFT) |
           ((uint64_t)epoch << FNODE_EPOCH_SHIFT);
  }
  inline uint32_t get_fnode_gid() const { return static_cast<uint32_t>(get_field<FNODE_GID_SHIFT, FNODE_GID_MASK>()); }
  inline uint16_t get_fnode_active_num() const { return static_cast<uint16_t>(get_field<FNODE_ACTIVE_NUM_SHIFT, FNODE_ACTIVE_NUM_MASK>()); }
  inline uint8_t get_fnode_flush_num() const { return static_cast<uint8_t>(get_field<FNODE_FLUSH_NUM_SHIFT, FNODE_FLUSH_NUM_MASK>()); }
  inline uint8_t get_fnode_segment() const { return static_cast<uint8_t>(get_field<FNODE_SEGMENT_SHIFT, FNODE_SEGMENT_MASK>()); }
  inline uint8_t get_fnode_epoch() const { return static_cast<uint8_t>(get_field<FNODE_EPOCH_SHIFT, FNODE_EPOCH_MASK>()); }

  inline void set_fnode_gid(uint32_t gid) { set_field<FNODE_GID_SHIFT, FNODE_GID_MASK>(gid); }
  inline void set_fnode_active_num(uint16_t active_num) { set_field<FNODE_ACTIVE_NUM_SHIFT, FNODE_ACTIVE_NUM_MASK>(active_num); }
  inline void set_fnode_flush_num(uint8_t flush_num) { set_field<FNODE_FLUSH_NUM_SHIFT, FNODE_FLUSH_NUM_MASK>(flush_num); }
  inline void set_fnode_segment(uint8_t segment) { set_field<FNODE_SEGMENT_SHIFT, FNODE_SEGMENT_MASK>(segment); }
  inline void set_fnode_epoch(uint8_t epoch) { set_field<FNODE_EPOCH_SHIFT, FNODE_EPOCH_MASK>(epoch); }

  static constexpr inline uint64_t get_fnode_epoch_mask() { return FNODE_EPOCH_MASK << FNODE_EPOCH_SHIFT; }

  // for local cache
  inline void init_lc(uint32_t gid, uint16_t kv_size, uint16_t sn, uint8_t version) {
    data = ((uint64_t)gid << LC_GID_SHIFT) | ((uint64_t)kv_size << LC_KV_SIZE_SHIFT) |
           ((uint64_t)sn << LC_SN_SHIFT) | ((uint64_t)version << LC_VER_SHIFT);
  }
  inline uint32_t get_lc_gid() const { return static_cast<uint32_t>(get_field<LC_GID_SHIFT, LC_GID_MASK>()); }
  inline uint16_t get_lc_kv_size() const { return static_cast<uint16_t>(get_field<LC_KV_SIZE_SHIFT, LC_KV_SIZE_MASK>()); }
  inline uint16_t get_lc_sn() const { return static_cast<uint16_t>(get_field<LC_SN_SHIFT, LC_SN_MASK>()); }
  inline uint8_t get_lc_version() const { return static_cast<uint8_t>(get_field<LC_VER_SHIFT, LC_VER_MASK>()); }

  inline void set_lc_gid(uint32_t gid) { set_field<LC_GID_SHIFT, LC_GID_MASK>(gid); }
  inline void set_lc_kv_size(uint16_t kv_size) { set_field<LC_KV_SIZE_SHIFT, LC_KV_SIZE_MASK>(kv_size); }
  inline void set_lc_sn(uint16_t sn) { set_field<LC_SN_SHIFT, LC_SN_MASK>(sn); }
  inline void set_lc_version(uint8_t version) { set_field<LC_VER_SHIFT, LC_VER_MASK>(version); }

  inline bool ext_match_mask_efp(const _AtomicEntry ext) const {
    uint64_t mask = ~(LC_KV_SIZE_MASK << LC_KV_SIZE_SHIFT);
    return (data & mask) == (ext.data & mask);
  }

  inline bool ext_match_mask_ver(const _AtomicEntry ext) const {
    uint64_t mask = ~(EXT_VER_MASK << EXT_VER_SHIFT);
    return (data & mask) == (ext.data & mask);
  }
} AtomicEntry;

typedef struct _Slot {
  AtomicEntry index;
  AtomicEntry extra;

#ifdef WRITE_EVICT_FLAG
  inline bool is_empty() const { return index.mask_ext_sn() == extra.mask_ext_sn(); }
  inline bool is_consistent() const { return index.get_idx_version() == extra.get_ext_version() && extra.get_ext_sn() != WRITE_EVICT_FLAG; }
#else
  inline bool is_empty() const { return index.get_idx_pointer() == 0; }
  inline bool is_consistent() const { return index.get_idx_version() == extra.get_ext_version(); }
#endif
} Slot;

#define SLOT_IDX_OFF (offsetof(Slot, index))
#define SLOT_EXT_OFF (offsetof(Slot, extra))

typedef Slot Bucket[HASH_BUCKET_ASSOC_NUM];
typedef Bucket Table[HASH_NUM_BUCKETS];

class DMCHash {
 public:
  virtual uint64_t hash_func1(const void* data, uint64_t length) = 0;
  virtual uint64_t hash_func2(const void* data, uint64_t length) = 0;
};

class DefaultHash : public DMCHash {
 public:
  uint64_t hash_func1(const void* data, uint64_t length);
  uint64_t hash_func2(const void* data, uint64_t length);
};

class DumbHash : public DMCHash {
  // a hash function that always return the same value for testing
 public:
  uint64_t hash_func1(const void* data, uint64_t length) { return 233; }
  uint64_t hash_func2(const void* data, uint64_t length) { return 666; }
};

static inline bool is_key_match(const void* key1,
                                uint32_t key_len1,
                                const void* key2,
                                uint32_t key_len2) {
  if (key_len1 != key_len2) {
    return false;
  }
  return memcmp(key1, key2, key_len1) == 0;
}

static inline void HashIndexComputeFp(uint64_t hash,
                                      __OUT uint8_t* fp,
                                      __OUT uint16_t* ext_fp) {
  uint8_t fp8 = 0;
  uint16_t fp16 = 0;
  hash >>= 32;
  fp16 ^= hash;
  hash >>= 16;
  fp16 ^= hash;
  fp8 ^= hash;
  hash >>= 8;
  fp8 ^= hash;

  *fp = fp8;
  *ext_fp = fp16;
}

static inline DMCHash* dmc_new_hash(uint8_t hash_type) {
  switch (hash_type) {
    case HASH_DEFAULT:
      return new DefaultHash;
    case HASH_DUMB:
      return new DumbHash;
    default:
      printd(L_ERROR, "Unsupported hash type (%d)!", hash_type);
      return NULL;
  }
}

/******************************** Two-Way Index Map /********************************/
typedef AtomicEntry TWIM[MAX_GROUP_SIZE];
typedef uint16_t LoffMap[MAX_GROUP_SIZE];

/******************************** Group Metadata /********************************/
typedef uint8_t ByteMap[MAX_GROUP_SIZE];
static_assert(sizeof(ByteMap) == MAX_GROUP_SIZE, "ByteMap size mismatch");

typedef struct _LocalGroupNode {  // for local cache
  uint32_t gid;
  uint8_t age;
  uint8_t segment;
  uint16_t num_active;
  ByteMap freq_map;
} LocalGroupNode;

typedef struct _InitGroupNode {
  uint32_t gid;
  uint32_t fid;
  AtomicEntry atomic;
  // uint8_t padding[48];

 public:
  enum {
    GNODE_EMPTY = 0,
    GNODE_CREATING = 1,
    GNODE_ALIVE = 2,
    GNODE_DEAD = 3,
  };
} InitGroupNode;

typedef struct _FreqGroupNode : public InitGroupNode {
  ByteMap freq_map;
} FreqGroupNode;

typedef struct _SampleGroupNode : public FreqGroupNode {
  AtomicEntry* twim;
  uint16_t* loff_map;
} SampleGroupNode;

typedef struct _GroupNode : public FreqGroupNode {
  TWIM twim;
} GroupNode;

typedef GroupNode GNodeTable[MAX_GROUP_NUM];

#define GNODE_ATOMIC_OFF (offsetof(GroupNode, atomic))
#define GNODE_FREQ_MAP_OFF (offsetof(GroupNode, freq_map))
#define GNODE_TWIM_OFF (offsetof(GroupNode, twim))
static_assert((GNODE_TWIM_OFF - GNODE_FREQ_MAP_OFF) % sizeof(ByteMap) == 0, "ByteMap size mismatch");

static inline uint32_t get_gnode_id(uint32_t gid) { return gid % MAX_GROUP_NUM; }
static inline uint32_t get_next_gid(uint32_t gid) { return gid + MAX_GROUP_NUM; }

/******************************** Object Data Structure /********************************/
typedef struct _ObjHeader {
  AtomicEntry ext_checker;  // reverse pointer to obj index
  uint32_t key_size;
  uint32_t val_size;
} ObjHeader;

#define OBJ_HEADER_EXT_CHECKER_OFF (offsetof(ObjHeader, ext_checker))
#define OBJ_HEADER_KEY_SIZE_OFF (offsetof(ObjHeader, key_size))
#define OBJ_HEADER_VAL_SIZE_OFF (offsetof(ObjHeader, val_size))

static inline void get_obj_value(const void* obj,
                                 __OUT void** value,
                                 __OUT uint32_t* val_size) {
  const ObjHeader* header = (const ObjHeader*)obj;
  *value = (void*)((uint8_t*)obj + sizeof(ObjHeader) + header->key_size);
  *val_size = header->val_size;
}

static inline void set_obj_header(void* obj, AtomicEntry ext_checker, uint32_t key_size, uint32_t val_size) {
  ObjHeader* header = (ObjHeader*)obj;
  header->ext_checker = ext_checker;
  header->key_size = key_size;
  header->val_size = val_size;
}

static inline void parse_obj(void* obj,
                             __OUT void** key,
                             __OUT uint32_t* key_size,
                             __OUT void** value,
                             __OUT uint32_t* val_size) {
  ObjHeader* header = (ObjHeader*)obj;
  *key = (void*)((uint8_t*)obj + sizeof(ObjHeader));
  *key_size = header->key_size;
  *value = (void*)((uint8_t*)obj + sizeof(ObjHeader) + header->key_size);
  *val_size = header->val_size;
}

/******************************** FIFO Buffer /********************************/
// typedef struct _FifoNode {
//   uint32_t gid;
//   uint16_t num_active;  // obj or block num
//   uint8_t segment;
//   uint8_t epoch;  // to check whether the slot is valid
// } AtomicEntry;
#define NUM_FIFO_NODE (MAX_GROUP_NUM)
typedef AtomicEntry FifoBuffer[NUM_FIFO_NODE];

#define FIFO_WINDOW_BYTES (128 * 1024 / (NUM_FIFO_TYPE))           // 128KB FIFO window consisting of OFMs
#define NUM_FWINDOW_NODE (FIFO_WINDOW_BYTES / MAX_GROUP_SIZE)      // the number of groups in the FIFO window
#define RNIC_DM_BYTES ((FIFO_WINDOW_BYTES + 8) * (NUM_FIFO_TYPE))  // FIFO window + 8 bytes for the cursor

static inline uint32_t get_fnode_id(uint32_t fid) { return fid % NUM_FIFO_NODE; }
static inline uint8_t get_fnode_epoch(uint32_t fid) { return (fid / NUM_FIFO_NODE) & AtomicEntry::get_fnode_epoch_mask(); }
static inline uint32_t get_fwnode_id(uint32_t fid) { return fid % NUM_FWINDOW_NODE; }

/******************************** Flags /********************************/
#define MSG_BUF_SIZE (4 * 1024ULL)
#define MSG_BUF_NUM (512)

#define ROUNDUP(x, n) ((x + (n - 1ULL)) & (~(n - 1ULL)))

#define NUM_CORES (72)  // CPU cores

#define HASH_SPACE_BYTES (ROUNDUP(sizeof(Table), 1024))
#define GNODE_SPACE_BYTES (ROUNDUP(sizeof(GNodeTable), 1024))
#define FIFO_SPACE_BYTES (ROUNDUP(sizeof(FifoBuffer), 1024))

#define MAX_KV_LEN (0xFF)

#endif
