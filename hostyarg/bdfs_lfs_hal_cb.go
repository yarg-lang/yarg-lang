package main

/*
#include "lfs.h"
*/
import "C"
import "unsafe"

func go_flash_fs(flash_fs C.uintptr_t) *FlashFS {
	// possible misuse of unsafe.Pointer, needed to obtain a go context from C,
	// seems to work...
	rawptr := uintptr(flash_fs)
	ptr := unsafe.Pointer(rawptr)
	return (*FlashFS)(ptr)
}

//export go_bdfs_read
func go_bdfs_read(flash_fs C.uintptr_t, block C.lfs_block_t, off C.lfs_off_t, buffer *C.uint8_t, size C.lfs_size_t) C.int {

	fs := go_flash_fs(flash_fs)
	device_address := fs.AddressForBlock(uint32(block), uint32(off))

	data := fs.device.ReadBlock(device_address, uint32(size))

	var cdata []byte = unsafe.Slice((*uint8)(buffer), size)

	copy(cdata, data)

	return C.LFS_ERR_OK
}

//export go_bdfs_prog_page
func go_bdfs_prog_page(flash_fs C.uintptr_t, block C.lfs_block_t, off C.lfs_off_t, buffer *C.uint8_t, size C.lfs_size_t) C.int {

	fs := go_flash_fs(flash_fs)
	device_address := fs.AddressForBlock(uint32(block), uint32(off))

	godata := C.GoBytes(unsafe.Pointer(buffer), C.int(size))

	fs.device.WriteBlock(device_address, godata)

	fs.device.DebugPrint()

	return C.LFS_ERR_OK
}

//export go_bdfs_erase_block
func go_bdfs_erase_block(flash_fs C.uintptr_t, block C.lfs_block_t) C.int {

	fs := go_flash_fs(flash_fs)
	device_address := fs.AddressForBlock(uint32(block), 0)

	fs.device.EraseBlock(device_address)

	return C.LFS_ERR_OK
}
