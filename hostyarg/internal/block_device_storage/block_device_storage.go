package block_device_storage

import (
	"log"

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

func (s *BlockDeviceStorage) PagePresent(block, page uint32) bool {
	return s.pagePresent[block][page]
}

func (s *BlockDeviceStorage) PagePerBlock() uint32 {
	return PICO_FLASH_PAGE_PER_BLOCK
}

func (*BlockDeviceStorage) BlockCount() uint32 {
	return PICO_DEVICE_BLOCK_COUNT
}
