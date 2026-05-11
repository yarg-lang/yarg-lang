#!/bin/bash
INTERPRETER="./bin/cyarg"
CYARG_ERROR=0

$INTERPRETER --help || CYARG_ERROR=$?
$INTERPRETER --lib yarg/specimen test/cyarg/simple.ya || CYARG_ERROR=$?
$INTERPRETER --lib yarg/specimen test/cyarg/hosted.ya -- test || CYARG_ERROR=$?
$INTERPRETER --lib yarg/specimen test/cyarg/hosted.ya || CYARG_ERROR=$?
$INTERPRETER --lib yarg/specimen test/cyarg/compile-error.ya
ERROR=$?
if [ $ERROR -ne 65 ]; then
    echo "Expected compile error, got exit code $ERROR"
    CYARG_ERROR=1
fi
$INTERPRETER --compile test/cyarg/compile-error.ya
ERROR=$?
if [ $ERROR -ne 65 ]; then
    echo "Expected compile error, got exit code $ERROR"
    CYARG_ERROR=1
fi

OUTPUT_DIR=`mktemp -d`
OUTPUT_FILE="$OUTPUT_DIR/simple.yb"

$INTERPRETER --compile test/cyarg/simple.ya "$OUTPUT_FILE" || CYARG_ERROR=$?
$INTERPRETER --lib yarg/specimen "$OUTPUT_FILE" || CYARG_ERROR=$?
if [ -d "$OUTPUT_DIR" ] && [ -f "$OUTPUT_FILE" ]; then
    rm "$OUTPUT_FILE"
    rmdir "$OUTPUT_DIR"
else
    echo "Expected output dir to exist at $OUTPUT_DIR and output file to exist at $OUTPUT_FILE"
    CYARG_ERROR=1
fi

$INTERPRETER --disassemble test/cyarg/simple.ya || CYARG_ERROR=$?
exit $CYARG_ERROR