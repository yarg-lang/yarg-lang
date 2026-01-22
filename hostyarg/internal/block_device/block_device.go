package block_device

import (
	"log"
	"runtime"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"
)

const PICO_DEVICE_BLOCK_COUNT = pico_flash_device.PICO_FLASH_SIZE_BYTES / pico_flash_device.PICO_ERASE_PAGE_SIZE
const PICO_FLASH_PAGE_PER_BLOCK = pico_flash_device.PICO_ERASE_PAGE_SIZE / pico_flash_device.PICO_PROG_PAGE_SIZE

type BlockDeviceStorage struct {
	data        [pico_flash_device.PICO_FLASH_SIZE_BYTES]byte
	pagePresent [PICO_DEVICE_BLOCK_COUNT][PICO_FLASH_PAGE_PER_BLOCK]bool
}

func (s *BlockDeviceStorage) BlockPageLocation(block, page uint32) uint32 {
	return block*s.EraseBlockSize() + page*s.ProgPageSize()
}

func (s *BlockDeviceStorage) BlockPageOffsetLocation(block, page, offset uint32) uint32 {
	return s.BlockPageLocation(block, page) + offset
}

func (s *BlockDeviceStorage) ProgPageSize() uint32 {
	return pico_flash_device.PICO_PROG_PAGE_SIZE
}

func (s *BlockDeviceStorage) EraseBlockSize() uint32 {
	return pico_flash_device.PICO_ERASE_PAGE_SIZE
}

func (s *BlockDeviceStorage) EraseBlock(block uint32) {

	for p := range PICO_FLASH_PAGE_PER_BLOCK {
		s.pagePresent[block][p] = false
	}
}

func (s *BlockDeviceStorage) Read(block, page, offset uint32, size uint32) []byte {

	storage_offset := s.BlockPageOffsetLocation(block, page, offset)

	if s.pagePresent[block][page] {
		log.Printf("Read   available page[%v][%v] off %v (size: %v)\n", block, page, offset, size)

	} else {
		log.Printf("Read unavailable page[%v][%v] off %v (size: %v)\n", block, page, offset, size)
	}
	return s.data[storage_offset : storage_offset+size]
}

func (s *BlockDeviceStorage) WritePage(block, page uint32, buffer []byte) {

	s.pagePresent[block][page] = true
	location := s.BlockPageLocation(block, page)

	copy(s.data[location:int(location)+len(buffer)], buffer)
}

type BlockDevice struct {
	storage     *BlockDeviceStorage
	baseAddress uint32
}

func NewBlockDevice() BlockDevice {

	s := BlockDeviceStorage{}
	device := BlockDevice{storage: &s, baseAddress: pico_flash_device.PICO_FLASH_BASE_ADDR}
	return device
}

func (bd BlockDevice) Close() error {
	return nil
}

func (bd BlockDevice) CountPages() uint32 {
	count := uint32(0)

	for b := range bd.BlockCount() {
		for p := range bd.PagePerBlock() {
			if bd.PagePresent(b, p) {
				count++
			}
		}
	}

	return count
}

func (bd BlockDevice) CountBlocks() uint32 {
	count := uint32(0)

	for b := range bd.BlockCount() {
		pagePresent := false
		for p := range bd.PagePerBlock() {
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

func (bd BlockDevice) PagePresent(block, page uint32) bool {
	return bd.storage.pagePresent[block][page]
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

func (bd BlockDevice) BlockStorageAdddress(address uint32) (block, page, offset uint32) {
	page_offset := (address - bd.baseAddress) % bd.EraseBlockSize()
	page = page_offset / pico_flash_device.PICO_PROG_PAGE_SIZE
	offset = page_offset % pico_flash_device.PICO_PROG_PAGE_SIZE
	block = (address - bd.baseAddress) / bd.EraseBlockSize()

	return block, page, offset
}

func (bd BlockDevice) BlockCount() uint32 {
	return PICO_DEVICE_BLOCK_COUNT
}

func (bd BlockDevice) PagePerBlock() uint32 {
	return PICO_FLASH_PAGE_PER_BLOCK
}

func (bd BlockDevice) EraseBlockSize() uint32 {
	return bd.storage.EraseBlockSize()
}

func (bd BlockDevice) EraseBlock(address uint32) {

	block, _, _ := bd.BlockStorageAdddress(address)

	bd.storage.EraseBlock(block)
}

func (bd BlockDevice) ReadBlock(address, size uint32) (buffer []byte) {

	block, page, offset := bd.BlockStorageAdddress(address)

	data := bd.storage.Read(block, page, offset, size)
	return data
}

func (bd BlockDevice) WriteBlock(address uint32, buffer []byte) {

	block, page, _ := bd.BlockStorageAdddress(address)

	bd.storage.WritePage(block, page, buffer)
}

func (bd BlockDevice) PinStorage(pins *runtime.Pinner) {
	pins.Pin(bd.storage)
}
