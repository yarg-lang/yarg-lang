package block_device

import (
	"log"
	"runtime"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block_device_storage"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"
)

type BlockReader interface {
	ReadBlock(address, size uint32) (buffer []byte)
}

type BlockWriter interface {
	EraseBlockSize() uint32
	EraseBlock(address uint32)
	WriteBlock(address uint32, buffer []byte)
}

type BlockMemoryDeviceWriter interface {
	BlockWriter
	IsBlockStart(targetAddr uint32) bool
}

type BlockMemoryDeviceReader interface {
	BlockReader
	BlocksInUse() uint32
	PagePerBlock() uint32
	BlockCount() uint32
	PagePresent(block, page uint32) bool
	TargetAddress(block, page uint32) uint32
}

type BlockFSReadWriter interface {
	BlockReader
	BlockWriter
	DebugPrint()
}

const PICO_DEVICE_BLOCK_COUNT = pico_flash_device.PICO_FLASH_SIZE_BYTES / pico_flash_device.PICO_ERASE_PAGE_SIZE

type BlockDevice struct {
	storage     *block_device_storage.BlockDeviceStorage
	baseAddress uint32
}

func NewBlockDevice() BlockDevice {

	s := block_device_storage.BlockDeviceStorage{}
	device := BlockDevice{storage: &s, baseAddress: pico_flash_device.PICO_FLASH_BASE_ADDR}
	return device
}

func (bd BlockDevice) Close() error {
	return nil
}

func (bd BlockDevice) BlocksInUse() uint32 {
	count := uint32(0)

	for b := uint32(0); b < bd.BlockCount(); b++ {
		pagePresent := false
		for p := uint32(0); p < bd.PagePerBlock(); p++ {
			pagePresent = pagePresent || bd.PagePresent(b, p)
		}
		if pagePresent {
			count++
		}
	}

	return count
}

func (bd BlockDevice) DebugPrint() {
	for b := range bd.BlockCount() {
		for p := range bd.PagePerBlock() {
			if bd.PagePresent(b, p) {
				log.Printf("Page [%v, %v]: 0x%08x\n", b, p, bd.TargetAddress(b, p))
			}
		}
	}
}

func DebugPrintDevice(bd BlockMemoryDeviceReader) {
	for b := range bd.BlockCount() {
		for p := range bd.PagePerBlock() {
			if bd.PagePresent(b, p) {
				log.Printf("Page [%v, %v]: 0x%08x\n", b, p, bd.TargetAddress(b, p))
			}
		}
	}
}

func (bd BlockDevice) PagePresent(block, page uint32) bool {
	return bd.storage.PagePresent(block, page)
}

func (bd BlockDevice) storageOffset(block, page uint32) uint32 {
	return block*bd.EraseBlockSize() + page*pico_flash_device.PICO_PROG_PAGE_SIZE
}

func (bd BlockDevice) TargetAddress(block, page uint32) uint32 {
	return bd.baseAddress + bd.storageOffset(block, page)
}

func (bd BlockDevice) IsBlockStart(targetAddr uint32) bool {
	return ((targetAddr - bd.baseAddress) % bd.EraseBlockSize()) == 0
}

func (bd BlockDevice) blockStorageAdddress(address uint32) (block, page, offset uint32) {
	page_offset := (address - bd.baseAddress) % bd.EraseBlockSize()
	page = page_offset / pico_flash_device.PICO_PROG_PAGE_SIZE
	offset = page_offset % pico_flash_device.PICO_PROG_PAGE_SIZE
	block = (address - bd.baseAddress) / bd.EraseBlockSize()

	return block, page, offset
}

func (bd BlockDevice) BlockCount() uint32 {
	return bd.storage.BlockCount()
}

func (bd BlockDevice) PagePerBlock() uint32 {
	return bd.storage.PagePerBlock()
}

func (bd BlockDevice) EraseBlockSize() uint32 {
	return bd.storage.EraseBlockSize()
}

func (bd BlockDevice) EraseBlock(address uint32) {

	block, _, _ := bd.blockStorageAdddress(address)

	bd.storage.EraseBlock(block)
}

func (bd BlockDevice) ReadBlock(address, size uint32) (buffer []byte) {

	block, page, offset := bd.blockStorageAdddress(address)

	data := bd.storage.Read(block, page, offset, size)
	return data
}

func (bd BlockDevice) WriteBlock(address uint32, buffer []byte) {

	block, page, _ := bd.blockStorageAdddress(address)

	bd.storage.WritePage(block, page, buffer)
}

func (bd BlockDevice) PinStorage(pins *runtime.Pinner) {
	pins.Pin(bd.storage)
}
