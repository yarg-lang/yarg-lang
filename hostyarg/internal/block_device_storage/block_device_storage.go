package block_device_storage

import (
	"log"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"
)

const deviceErasePageCount = pico_flash_device.Size / pico_flash_device.ErasePageSize
const deviceProgPagePerErasePage = pico_flash_device.ErasePageSize / pico_flash_device.ProgPageSize

type BlockDeviceStorage struct {
	data        [pico_flash_device.Size]byte
	pagePresent [deviceErasePageCount][deviceProgPagePerErasePage]bool
}

func (s *BlockDeviceStorage) blockPageLocation(block, page uint32) uint32 {
	return block*s.EraseBlockSize() + page*s.progPageSize()
}

func (s *BlockDeviceStorage) blockPageOffsetLocation(block, page, offset uint32) uint32 {
	return s.blockPageLocation(block, page) + offset
}

func (s *BlockDeviceStorage) progPageSize() uint32 {
	return pico_flash_device.ProgPageSize
}

func (s *BlockDeviceStorage) EraseBlockSize() uint32 {
	return pico_flash_device.ErasePageSize
}

func (s *BlockDeviceStorage) EraseBlock(block uint32) {

	for p := range deviceProgPagePerErasePage {
		s.pagePresent[block][p] = false
	}
}

func (s *BlockDeviceStorage) Read(block, page, offset uint32, size uint32) []byte {

	storage_offset := s.blockPageOffsetLocation(block, page, offset)

	if s.pagePresent[block][page] {
		log.Printf("Read   available page[%v][%v] off %v (size: %v)\n", block, page, offset, size)

	} else {
		log.Printf("Read unavailable page[%v][%v] off %v (size: %v)\n", block, page, offset, size)
	}
	return s.data[storage_offset : storage_offset+size]
}

func (s *BlockDeviceStorage) WritePage(block, page uint32, buffer []byte) {

	s.pagePresent[block][page] = true
	location := s.blockPageLocation(block, page)

	copy(s.data[location:int(location)+len(buffer)], buffer)
}

func (s *BlockDeviceStorage) PagePresent(block, page uint32) bool {
	return s.pagePresent[block][page]
}

func (s *BlockDeviceStorage) PagePerBlock() uint32 {
	return deviceProgPagePerErasePage
}

func (*BlockDeviceStorage) BlockCount() uint32 {
	return deviceErasePageCount
}
