package xiplibrary

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"math"
	"os"
	"sort"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/tokeniser"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/xip/command"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/xip/library"
)

// note ordering and size are designed with alignment on ARM in mind.
type LibraryImageHeader struct {
	Magic          [4]byte
	Endianness     uint16
	Version        uint16
	Length         uint32
	DirectoryNode  uint16
	NodeZeroOffset uint8
}

type LibraryNodeEntry struct {
	Length    uint32
	Alignment uint32
}

type LibraryWriter interface {
	io.WriteSeeker
	io.WriterAt
}

type LibraryDirEntry struct {
	FileNode uint16
	NameNode uint16
}

var endianness = binary.LittleEndian

// when serialised in little endian, this will have the first 5 bytes spelling 'yargX' (with 0xa for a)
// 4 bytes from packageMagic, and the following byte from the low byte of endiannessMarker.

var packageMagic = [4]byte{'y', 0x0a, 'r', 'g'}

const endiannessMarker uint16 = 0xff58

const packageVersion uint16 = 0x2602

const nodeZeroAlignment = 4

func readLibraryImageHeader(data []byte) (header LibraryImageHeader, err error) {
	buf := bytes.NewReader(data)
	header = LibraryImageHeader{}
	err = binary.Read(buf, endianness, &header)
	if err != nil {
		return header, err
	}
	if header.Magic != packageMagic {
		return header, fmt.Errorf("invalid magic: %v", header.Magic)
	}
	if header.Endianness != endiannessMarker {
		return header, fmt.Errorf("invalid endianness marker: %v", header.Endianness)
	}
	if header.Version != packageVersion {
		return header, fmt.Errorf("invalid version: %v", header.Version)
	}
	if header.NodeZeroOffset == 0 {
		return header, fmt.Errorf("invalid node zero offset: %v", header.NodeZeroOffset)
	}

	return header, nil
}

func binaryWriteAt(w io.WriterAt, order binary.ByteOrder, data any, offset uint32) (err error) {
	buf := new(bytes.Buffer)
	err = binary.Write(buf, order, data)
	if err != nil {
		return err
	}
	_, err = w.WriteAt(buf.Bytes(), int64(offset))
	return err
}

func writeLibraryImageHeader(w io.WriterAt, length uint32, directoryNode uint16) (err error) {

	headerSize := uint32(binary.Size(LibraryImageHeader{}))
	nodeZeroOffset := nodePadding(headerSize, nodeZeroAlignment) + headerSize
	if nodeZeroOffset > math.MaxUint8 {
		return fmt.Errorf("node zero offset too large: %v", nodeZeroOffset)
	}

	header := LibraryImageHeader{
		Magic:          packageMagic,
		Endianness:     endiannessMarker,
		Version:        packageVersion,
		Length:         length,
		NodeZeroOffset: uint8(nodeZeroOffset),
		DirectoryNode:  directoryNode,
	}

	err = binaryWriteAt(w, endianness, header, 0)
	if err != nil {
		return err
	}
	return nil
}

func nodePadding(startLen uint32, alignment uint) uint32 {
	padding := uint32(alignment) - (startLen % uint32(alignment))
	if padding == uint32(alignment) {
		return 0
	}
	return padding
}

func writeLibraryPadding(w io.WriterAt, startLen uint32, alignment uint) (err error) {
	padding := nodePadding(startLen, alignment)
	if padding != 0 {
		_, err = w.WriteAt(make([]byte, padding), int64(startLen))
		return err
	}
	return nil
}

func writeStringNode(w LibraryWriter, s string, alignment uint) (err error) {
	c_string := append([]byte(s), 0)

	return writeLibraryNode(w, c_string, alignment)
}

func writeLibraryNode(w LibraryWriter, node []byte, alignment uint) (err error) {
	startLen64, err := w.Seek(0, io.SeekEnd)
	if err != nil {
		return err
	}
	startLen := uint32(startLen64)
	err = writeLibraryPadding(w, startLen, alignment)
	if err != nil {
		return err
	}
	offset64, err := w.Seek(0, io.SeekEnd)
	if err != nil {
		return err
	}
	//	paddedStartLen64, err := w.Seek(0, io.SeekEnd)
	//	if err != nil {
	//		return err
	//	}
	//	paddedStartLen := uint32(paddedStartLen64)
	//	dataLength := uint32(len(node))
	_, err = w.Write(node)
	endPosition, err := w.Seek(0, io.SeekEnd)
	if err != nil {
		return err
	}

	fmt.Printf("Node at offset %v, length: %v, stored size %v\n", offset64, len(node), endPosition-startLen64)

	//	err = writeLibraryImageHeader(w, paddedStartLen+dataLength, 3)
	return err
}

func writeLibraryIndex(w LibraryWriter, lengths []LibraryNodeEntry) (err error) {

	indexNode := new(bytes.Buffer)
	var indexOffset uint32 = 16
	indexLength := uint32(len(lengths)) * 4 * 2
	indexOffset += nodePadding(indexOffset, nodeZeroAlignment)

	binary.Write(indexNode, endianness, indexOffset)
	binary.Write(indexNode, endianness, uint32(indexLength))

	var offset uint32 = indexOffset + uint32(indexLength)
	for _, length := range lengths[1:] {
		offset += nodePadding(offset, uint(length.Alignment))
		err = binary.Write(indexNode, endianness, offset)
		if err != nil {
			return err
		}
		log.Printf("Offset: %d, Length: %d (alignment: %d, test %d)\n", offset, length.Length, length.Alignment, offset%uint32(length.Alignment))
		offset += length.Length
		err = binary.Write(indexNode, endianness, length.Length)
		if err != nil {
			return err
		}
	}
	return writeLibraryNode(w, indexNode.Bytes(), nodeZeroAlignment)
}

func writeDirectory(w LibraryWriter, directoryEntries []LibraryDirEntry) (err error) {
	dirNode := new(bytes.Buffer)
	for _, entry := range directoryEntries {
		err = binary.Write(dirNode, endianness, entry.FileNode)
		if err != nil {
			return err
		}
		err = binary.Write(dirNode, endianness, entry.NameNode)
		if err != nil {
			return err
		}
	}
	return writeLibraryNode(w, dirNode.Bytes(), 2)
}

func nodeDataBuffer(data []byte, nodeOffset uint32, nodeLength uint32) ([]byte, error) {
	if int(nodeOffset)+int(nodeLength) > len(data) {
		return nil, fmt.Errorf("node offset and length exceed contents length")
	}
	return data[nodeOffset : nodeOffset+nodeLength], nil
}

func nodeData(data []byte, node uint16) (nodeBytes []byte, err error) {
	header, err := readLibraryImageHeader(data)
	if err != nil {
		return nil, err
	}
	if header.Version != packageVersion {
		return nil, fmt.Errorf("unsupported library image version %d", header.Version)
	}

	if node == 0 {
		indexOffset := endianness.Uint32(data[header.NodeZeroOffset : header.NodeZeroOffset+4])
		indexLength := endianness.Uint32(data[header.NodeZeroOffset+4 : header.NodeZeroOffset+8])

		if indexOffset != uint32(header.NodeZeroOffset) {
			return nil, fmt.Errorf("index offset %d does not match header node zero offset %d", indexOffset, header.NodeZeroOffset)
		}

		return nodeDataBuffer(data, indexOffset, indexLength)
	} else {
		index, err := nodeData(data, 0)
		if err != nil {
			return nil, err
		}
		nodeCount, err := nodeCount(data)
		if err != nil {
			return nil, err
		}
		if int(node) >= nodeCount {
			return nil, fmt.Errorf("node %d out of range", node)
		}
		nodeOffset := endianness.Uint32(index[node*8 : node*8+4])
		nodeLength := endianness.Uint32(index[node*8+4 : node*8+8])
		return nodeDataBuffer(data, nodeOffset, nodeLength)
	}
}

func nodeCount(data []byte) (int, error) {
	index, err := nodeData(data, 0)
	if err != nil {
		return 0, err
	}
	return len(index) / 8, nil
}

func directoryNode(data []byte) (uint16, error) {
	header, err := readLibraryImageHeader(data)
	if err != nil {
		return 0, err
	}
	if header.DirectoryNode == 0 {
		log.Print("no directory node present")
	}
	nodes, err := nodeCount(data)
	if err != nil {
		return 0, err
	}
	if int(header.DirectoryNode) >= nodes {
		return 0, fmt.Errorf("directory node %d out of range", header.DirectoryNode)
	}
	return header.DirectoryNode, nil
}

func directories(data []byte) (dirs []LibraryDirEntry, err error) {
	numNodes, err := nodeCount(data)
	if err != nil {
		return nil, err
	}

	dirNode, err := directoryNode(data)
	if err != nil {
		return nil, err
	}
	if dirNode == 0 {
		return nil, nil
	}

	dirData, err := nodeData(data, dirNode)
	if err != nil {
		return nil, err
	}
	numEntries := len(dirData) / 4
	directoryEntries := make([]LibraryDirEntry, numEntries)
	for i := range directoryEntries {
		fileNode := endianness.Uint16(dirData[i*4 : i*4+2])
		nameNode := endianness.Uint16(dirData[i*4+2 : i*4+4])

		if fileNode >= uint16(numNodes) {
			return nil, fmt.Errorf("file node %d out of range", fileNode)
		}
		if nameNode >= uint16(numNodes) {
			return nil, fmt.Errorf("name node %d out of range", nameNode)
		}

		directoryEntries[i].FileNode = fileNode
		directoryEntries[i].NameNode = nameNode

	}
	return directoryEntries, nil
}

func CmdLs(fsFilename string, dirEntry string, long bool) (e error) {
	data, e := os.ReadFile(fsFilename)
	if e != nil {
		return e
	}

	_, e = readLibraryImageHeader(data)
	if e != nil {
		return e
	}

	directoryEntries, err := directories(data)
	if err != nil {
		return err
	}

	log.Printf("Directory Entries: %d\n", len(directoryEntries))

	for i, entry := range directoryEntries {
		fileNode := entry.FileNode
		nameNode := entry.NameNode

		nameData, err := nodeData(data, nameNode)
		if err != nil {
			return err
		}
		name := string(nameData[:len(nameData)-1])
		fileData, err := nodeData(data, fileNode)
		if err != nil {
			return err
		}
		log.Printf("Entry %d: File Node=%d, Name Node=%d, Name=%s (%d bytes)\n", i, fileNode, nameNode, name, len(fileData))

		if long {
			fmt.Printf("\t\t%d\t%s\n", len(fileData), name)
		} else {
			fmt.Printf("%s\n", name)
		}
	}

	return nil
}

type FsInfo struct {
	Version          uint16
	Size             uint32
	NodeCount        uint16
	UsefulLength     uint32
	DirectoryEntries uint16
	IndexedEntries   uint16
}

func readFsInfo(data []byte) (FsInfo, error) {
	header, e := readLibraryImageHeader(data)
	if e != nil {
		return FsInfo{}, e
	}

	var info FsInfo
	info.Version = header.Version
	info.Size = header.Length
	numNodes, e := nodeCount(data)
	if e != nil {
		return FsInfo{}, e
	}
	info.NodeCount = uint16(numNodes)

	nodeZero, e := nodeData(data, 0)
	if e != nil {
		return FsInfo{}, e
	}

	var usefulLength uint32 = 0
	for i := range numNodes {
		nodeOffset := endianness.Uint32(nodeZero[i*8 : i*8+4])
		nodeLength := endianness.Uint32(nodeZero[i*8+4 : i*8+8])

		if int(nodeOffset)+int(nodeLength) > len(data) {
			return FsInfo{}, fmt.Errorf("node offset and length exceed contents length")
		}

		usefulLength += nodeLength
	}

	info.UsefulLength = uint32(usefulLength)

	if header.DirectoryNode != 0 {
		nodeDir, e := nodeData(data, header.DirectoryNode)
		if e != nil {
			return FsInfo{}, e
		}
		info.DirectoryEntries = uint16(len(nodeDir) / 4)
	}

	var directoryNodes uint16
	if header.DirectoryNode != 0 {
		directoryNodes = 1
		directoryNodes += info.DirectoryEntries * 2
	}

	info.IndexedEntries = info.NodeCount - 1 - directoryNodes

	return info, nil
}

func CmdFsInfo(fsFilename string) (e error) {
	data, e := os.ReadFile(fsFilename)
	if e != nil {
		return e
	}

	info, e := readFsInfo(data)
	if e != nil {
		return e
	}

	fmt.Printf("Library Version: %d\n", info.Version)
	fmt.Printf("Library Size: %d bytes\n", info.Size)
	fmt.Printf("Node Count: %d\n", info.NodeCount)
	fmt.Printf("Node Data: %d bytes\n", info.UsefulLength)
	fmt.Printf("Library Overhead: %d bytes\n", int(info.Size)-int(info.UsefulLength))
	fmt.Printf("Directory Entries: %d\n", info.DirectoryEntries)
	fmt.Printf("Indexed Entries: %d\n", info.IndexedEntries)

	for i := uint16(0); i < info.IndexedEntries; i++ {
		node := i + 1
		data, e := nodeData(data, node)
		if e != nil {
			return e
		}
		fmt.Printf("Indexed Node %d Size: %d bytes\n", node, len(data))
	}

	return nil
}

func appendLibraryNode(lengths []LibraryNodeEntry, length uint32, alignment uint32) []LibraryNodeEntry {
	cursor := LibraryNodeEntry{}
	cursor.Length = length
	cursor.Alignment = alignment
	lengths = append(lengths, cursor)
	return lengths
}

type LibrarySkeleton struct {
	Lengths          []LibraryNodeEntry
	DirectoryEntries []LibraryDirEntry
	LibraryLength    int
	DirectoryNode    uint16
}

func (s *LibrarySkeleton) addNode(length uint32, alignment uint32) {
	s.Lengths = appendLibraryNode(s.Lengths, length, alignment)
}

func buildNodes(lib *library.XIPLibrary) (output LibrarySkeleton) {

	nodeCursor := uint16(0)
	nodeCount := lib.NodeCount()
	fmt.Printf("Node count: %d\n", nodeCount)

	output.addNode(4*2*uint32(nodeCount), 4)
	nodeCursor++

	for _, indexedFile := range lib.IndexedFiles {
		c := indexedFile.(*command.IndexFileCommand)
		output.addNode(uint32(c.Length), uint32(c.Alignment))
		nodeCursor++
	}
	if lib.NamedFileCount() > 0 {
		output.addNode(uint32(len(lib.NamedFiles))*uint32(binary.Size(LibraryDirEntry{})), 2)
		nodeCursor++
		for _, namedFile := range lib.NamedFiles {
			c := namedFile.(*command.FileCommand)
			output.addNode(uint32(c.Length), uint32(c.Alignment))
			output.addNode(uint32(len(c.TargetPath)+1), 1)
			output.DirectoryEntries = append(output.DirectoryEntries, LibraryDirEntry{
				FileNode: nodeCursor,
				NameNode: nodeCursor + 1,
			})
			nodeCursor += 2
		}
	}

	output.LibraryLength = 16
	for _, length := range output.Lengths {
		output.LibraryLength += int(nodePadding(uint32(output.LibraryLength), uint(length.Alignment)))
		output.LibraryLength += int(length.Length)
	}

	output.DirectoryNode = 0
	if lib.NamedFileCount() > 0 {
		output.DirectoryNode = uint16(len(lib.IndexedFiles)) + 1
	}

	return output
}

func writeLibrary(lib *library.XIPLibrary, TargetPath string) error {
	fmt.Printf("Writing library to %s\n", TargetPath)
	libraryimage, err := os.Create(TargetPath)
	if err != nil {
		return err
	}
	defer libraryimage.Close()

	skeleton := buildNodes(lib)

	err = writeLibraryImageHeader(libraryimage, uint32(skeleton.LibraryLength), skeleton.DirectoryNode)
	if err != nil {
		return err
	}
	err = writeLibraryPadding(libraryimage, uint32(binary.Size(LibraryImageHeader{})), 4)
	if err != nil {
		return err
	}
	err = writeLibraryIndex(libraryimage, skeleton.Lengths)
	if err != nil {
		return err
	}
	for _, indexedFile := range lib.IndexedFiles {
		c := indexedFile.(*command.IndexFileCommand)
		data, err := os.ReadFile(c.SourcePath)
		if err != nil {
			return err
		}
		err = writeLibraryNode(libraryimage, data, uint(c.Alignment))
		if err != nil {
			return err
		}
	}
	if lib.NamedFileCount() > 0 {

		err = writeDirectory(libraryimage, skeleton.DirectoryEntries)
		if err != nil {
			return err
		}
		for _, file := range lib.NamedFiles {
			c := file.(*command.FileCommand)
			data, err := os.ReadFile(c.SourcePath)
			if err != nil {
				return err
			}

			err = writeLibraryNode(libraryimage, data, uint(c.Alignment))
			if err != nil {
				return err
			}
			err = writeStringNode(libraryimage, c.TargetPath, 1)
			if err != nil {
				return err
			}
		}
	}

	return nil
}

func buildXIPLibrary(script string, commands []library.Command) (*library.XIPLibrary, error) {
	lib := &library.XIPLibrary{CommandPath: script}

	for _, command := range commands {
		command.Execute(lib)
		fmt.Printf("%s\n", command)
	}

	sort.Slice(lib.IndexedFiles, func(i, j int) bool {
		iCommand := lib.IndexedFiles[i].(*command.IndexFileCommand)
		jCommand := lib.IndexedFiles[j].(*command.IndexFileCommand)
		return iCommand.Index < jCommand.Index
	})

	sort.Slice(lib.NamedFiles, func(i, j int) bool {
		iCommand := lib.NamedFiles[i].(*command.FileCommand)
		jCommand := lib.NamedFiles[j].(*command.FileCommand)
		return iCommand.TargetPath < jCommand.TargetPath
	})
	return lib, nil
}

func parseLibraryCommands(libContents string) ([]library.Command, error) {
	lines, e := tokeniser.TokeniseFile(libContents)
	if e != nil {
		return nil, e
	}

	for _, token := range lines {
		fmt.Println(token)
	}

	commands, e := command.Parse(lines)
	if e != nil {
		return nil, e
	}
	return commands, nil
}

func CmdBuildWithContents(libContents string, outputFile string) (e error) {
	stat, e := os.Stat(libContents)
	if e != nil {
		return e
	}
	if stat.IsDir() {
		return fmt.Errorf("libContents should be a file, not a directory")
	}

	commands, e := parseLibraryCommands(libContents)
	if e != nil {
		return e
	}

	lib, err := buildXIPLibrary(libContents, commands)
	if err != nil {
		return err
	}
	e = writeLibrary(lib, outputFile)
	if e != nil {
		return e
	}
	return nil
}
