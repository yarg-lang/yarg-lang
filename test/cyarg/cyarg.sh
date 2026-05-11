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

OUTPUT_PATH=`mktemp -d`simple.yb

$INTERPRETER --compile test/cyarg/simple.ya $OUTPUT_PATH || CYARG_ERROR=$?
$INTERPRETER --lib yarg/specimen $OUTPUT_PATH || CYARG_ERROR=$?
if [ -f "$OUTPUT_PATH" ]; then
    rm -rf "$OUTPUT_PATH"
else
    echo "Expected output path to be a file, got $OUTPUT_PATH"
    CYARG_ERROR=1
fi

$INTERPRETER --disassemble test/cyarg/simple.ya || CYARG_ERROR=$?
exit $CYARG_ERROR