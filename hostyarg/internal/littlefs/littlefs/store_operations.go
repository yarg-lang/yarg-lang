package littlefs

// store_operations provides a simple set of thunks that translate from the littelfs C callbacks to
// the fs_store object. This keeps the cgo and unsafe code local to littlefs.
// storeOps is a struct that the cgo runtime is happy to marshal as a C pointer, and just contains a StoreOperations interface
// that clients actually implement.

/*
#include "store_operations.h"
*/
import "C"
import (
	"unsafe"
)

// struct for marshalling the StoreOperations interface as a C pointer.
type storeOps struct {
	StoreOperations
}

func (config *Config) installStoreOperations() {

	storeOpsHandle := uintptr(unsafe.Pointer(config.store))
	C.install_store_operations(config.lfs_config, C.uintptr_t(storeOpsHandle))
}

//export store_read_page
func store_read_page(config C.lfs_const_config_ptr, block C.lfs_block_t, off C.lfs_off_t, buffer *C.uint8_t, size C.lfs_size_t) C.int {

	ops := (*storeOps)(config.context)
	var cbuffer []byte = unsafe.Slice((*uint8)(buffer), size)

	ops.ReadPage(cbuffer, uint32(block), uint32(off))

	return C.LFS_ERR_OK
}

//export store_prog_page
func store_prog_page(config C.lfs_const_config_ptr, block C.lfs_block_t, off C.lfs_off_t, buffer *C.uint8_t, size C.lfs_size_t) C.int {

	ops := (*storeOps)(config.context)
	gobuffer := C.GoBytes(unsafe.Pointer(buffer), C.int(size))

	ops.ProgPage(gobuffer, uint32(block), uint32(off))

	return C.LFS_ERR_OK
}

//export store_erase_block
func store_erase_block(config C.lfs_const_config_ptr, block C.lfs_block_t) C.int {

	ops := (*storeOps)(config.context)

	ops.EraseBlock(uint32(block))

	return C.LFS_ERR_OK
}
