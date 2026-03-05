package pico_memory

import (
	"fmt"
	"log"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"
)

type BlockDeviceStorage struct {
	data        [pico_flash_device.SizeBytes]byte
	pagePresent [pico_flash_device.SizeBytes / pico_flash_device.ErasePageSize][pico_flash_device.ErasePageSize / pico_flash_device.ProgPageSize]bool
}

func (s *BlockDeviceStorage) blockPageLocation(block, page int) (int64, error) {
	return s.blockPageOffsetLocation(block, page, 0)
}

func (s *BlockDeviceStorage) blockPageOffsetLocation(block, page, offset int) (addr int64, err error) {

	err = s.validRange(block, page, offset)
	if err != nil {
		return
	}

	return int64(block)*int64(s.EraseBlockSize()) + int64(page)*int64(s.ProgPageSize()) + int64(offset), nil
}

func (s *BlockDeviceStorage) ProgPageSize() int {
	return pico_flash_device.ProgPageSize
}

func (s *BlockDeviceStorage) EraseBlockSize() int {
	return pico_flash_device.ErasePageSize
}

func (s *BlockDeviceStorage) EraseBlock(block int) (err error) {

	err = s.validRange(block, 0, 0)
	if err != nil {
		return
	}

	for p := range s.pagePresent[block] {
		s.pagePresent[block][p] = false
	}
	return nil
}

func (s *BlockDeviceStorage) ReadAt(p []byte, offset int64) (n int, err error) {

	block := int(offset / int64(s.EraseBlockSize()))
	page := int(offset % int64(s.EraseBlockSize()) / int64(s.ProgPageSize()))
	page_offset := int(offset % int64(s.ProgPageSize()))

	return s.ReadPage(p, block, page, page_offset)
}

func (s *BlockDeviceStorage) ReadPage(p []byte, block, page, offset int) (n int, err error) {

	storage_offset, err := s.blockPageOffsetLocation(block, page, offset)
	if err != nil {
		return 0, err
	}

	if s.pagePresent[block][page] {
		log.Printf("Read   available page[%v][%v] off %v (size: %v)\n", block, page, offset, len(p))

	} else {
		log.Printf("Read unavailable page[%v][%v] off %v (size: %v)\n", block, page, offset, len(p))
	}

	copy(p, s.data[storage_offset:int(storage_offset)+len(p)])

	return len(p), nil
}

func (s *BlockDeviceStorage) validRange(block, page, size int) error {

	if block < 0 || block >= s.BlockCount() {
		return fmt.Errorf("Invalid block number")
	}
	if page < 0 || page >= s.PagePerBlock() {
		return fmt.Errorf("Invalid page number")
	}
	if size < 0 || size > s.ProgPageSize() {
		return fmt.Errorf("Invalid size")
	}
	return nil
}

func (s *BlockDeviceStorage) WritePage(p []byte, block, page int) (n int, err error) {

	location, err := s.blockPageLocation(block, page)
	if err != nil {
		return
	}
	s.pagePresent[block][page] = true
	copy(s.data[location:int(location)+len(p)], p)

	return len(p), nil
}

func (s *BlockDeviceStorage) PagePresent(block, page int) bool {
	return s.pagePresent[block][page]
}

func (s *BlockDeviceStorage) PagePerBlock() int {
	return s.EraseBlockSize() / s.ProgPageSize()
}

func (s *BlockDeviceStorage) BlockCount() int {
	return len(s.pagePresent)
}
