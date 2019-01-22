#ifndef HASH_H_
#define HASH_H_

#include "autoconf.h"
#include "types.h"
#include "shr.h"

# if CONFIG_LOADER_FW_HASH_CHECK
bool check_fw_hash(const t_firmware_state *fw, uint32_t partition_base_addr, uint32_t partition_size);

# endif

#endif