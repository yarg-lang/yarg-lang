package pico_memory

import (
	"testing"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"
)

func TestSetup(t *testing.T) {
	test := BlockDeviceStorage{}

	test.blockPageLocation(0, 0)
	test.EraseBlock(0)
}

func TestSizing(t *testing.T) {
	test := BlockDeviceStorage{}

	if test.EraseBlockSize() != pico_flash_device.ErasePageSize {
		t.Errorf("Expected EraseBlockSize to be %v, got %v", pico_flash_device.ErasePageSize, test.EraseBlockSize())
	}
	if test.ProgPageSize() != pico_flash_device.ProgPageSize {
		t.Errorf("Expected progPageSize to be %v, got %v", pico_flash_device.ProgPageSize, test.ProgPageSize())
	}
	if test.PagePerBlock() != pico_flash_device.ErasePageSize/pico_flash_device.ProgPageSize {
		t.Errorf("Expected PagePerBlock to be %v, got %v", pico_flash_device.ErasePageSize/pico_flash_device.ProgPageSize, test.PagePerBlock())
	}
	if test.BlockCount() != pico_flash_device.SizeBytes/pico_flash_device.ErasePageSize {
		t.Errorf("Expected BlockCount to be %v, got %v", pico_flash_device.SizeBytes/pico_flash_device.ErasePageSize, test.BlockCount())
	}
}

func TestWriteRead(t *testing.T) {
	test := BlockDeviceStorage{}

	block := 0
	page := 0
	offset := 0
	size := 4

	buffer := []byte{1, 2, 3, 4}
	test.WritePage(buffer, block, page)

	readBack := [4]byte{}
	test.ReadPage(readBack[:], block, page, offset)

	for i := range buffer {
		if readBack[i] != buffer[i] {
			t.Errorf("Expected read data to match written data at index %v, got %v", i, readBack[i])
		}
	}

	testbuffer := make([]byte, size)
	test.ReadAt(testbuffer, 0)
	for i := range buffer {
		if testbuffer[i] != buffer[i] {
			t.Errorf("Expected read data to match written data at index %v, got %v", i, testbuffer[i])
		}
	}
}
