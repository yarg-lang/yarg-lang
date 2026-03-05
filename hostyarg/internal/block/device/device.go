package device

import (
	"log"

	block_io "github.com/yarg-lang/yarg-lang/hostyarg/internal/block/io"
)

type BlockDevice struct {
	storage     block_io.BlockIOReaderWriter
	baseAddress block_io.DeviceAddress
}

func NewBlockDevice(base_addr block_io.DeviceAddress, storage block_io.BlockIOReaderWriter) BlockDevice {

	return BlockDevice{storage: storage, baseAddress: base_addr}
}

func (bd BlockDevice) Close() error {
	return nil
}

func (bd BlockDevice) BlocksInUse() (count int) {

	for b := range bd.BlockCount() {
		pagePresent := false
		for p := range bd.PagePerBlock() {
			pagePresent = pagePresent || bd.storage.PagePresent(b, p)
		}
		if pagePresent {
			count++
		}
	}
	return
}

func DebugPrintDevice(bd block_io.BlockDeviceReaderInfo) {
	for b := range bd.BlockCount() {
		for p := range bd.PagePerBlock() {
			if bd.PagePresent(b, p) {
				log.Printf("Page [%v, %v]: 0x%08x\n", b, p, bd.TargetAddress(b, p))
			}
		}
	}
}

func (bd BlockDevice) TargetAddress(block, page int) block_io.DeviceAddress {
	storageOffset := block*bd.EraseBlockSize() + page*bd.ProgPageSize()

	return bd.baseAddress + block_io.DeviceAddress(storageOffset)
}

func (bd BlockDevice) IsBlockStart(targetAddr block_io.DeviceAddress) bool {
	return ((targetAddr - bd.baseAddress) % block_io.DeviceAddress(bd.EraseBlockSize())) == 0
}

func (bd BlockDevice) blockStorageAdddress(address block_io.DeviceAddress) (block, page, offset int) {
	page_offset := int(address-bd.baseAddress) % bd.EraseBlockSize()
	page = page_offset / bd.ProgPageSize()
	offset = page_offset % bd.ProgPageSize()
	block = int(address-bd.baseAddress) / bd.EraseBlockSize()

	return block, page, offset
}

func (bd BlockDevice) PagePresent(block, page int) bool {
	return bd.storage.PagePresent(block, page)
}

func (bd BlockDevice) BlockCount() int {
	return bd.storage.BlockCount()
}

func (bd BlockDevice) PagePerBlock() int {
	return bd.storage.PagePerBlock()
}

func (bd BlockDevice) EraseBlockSize() int {
	return bd.storage.EraseBlockSize()
}

func (bd BlockDevice) ProgPageSize() int {
	return bd.storage.ProgPageSize()
}

func (bd BlockDevice) EraseBlock(address block_io.DeviceAddress) {

	block, _, _ := bd.blockStorageAdddress(address)

	bd.storage.EraseBlock(int(block))
}

func (bd BlockDevice) ReadPage(p []byte, address block_io.DeviceAddress) (n int, err error) {

	block, page, offset := bd.blockStorageAdddress(address)

	n, err = bd.storage.ReadPage(p, block, page, offset)
	return n, err
}

func (bd BlockDevice) WritePage(p []byte, address block_io.DeviceAddress) (n int, err error) {

	block, page, _ := bd.blockStorageAdddress(address)

	n, err = bd.storage.WritePage(p, block, page)
	return n, err
}
