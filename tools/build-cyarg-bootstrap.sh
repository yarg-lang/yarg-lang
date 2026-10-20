#!/bin/sh

HOSTYARG=./bin/yarg
echo "Building cyarg bootstrap files..."

$HOSTYARG compile --interpreter bin/cyarg --source "cyarg/startup.ya" --output "cyarg/bootstrap/startup.yb"
$HOSTYARG compile --interpreter bin/cyarg --source "cyarg/startup-hosted.ya" --output "cyarg/bootstrap/startup-hosted.yb"

$HOSTYARG buildlib --contents cyarg/bootstrap/cyarg.yarglib --output cyarg/bootstrap/cyarg.ylib

xxd -i -n cyarg_ylib cyarg/bootstrap/cyarg.ylib | \
 sed 's/^unsigned char cyarg_ylib\[\] =/alignas(8) const unsigned char cyarg_ylib[]=/' | \
 awk 'BEGIN { print "#include <stdalign.h>" } { print }' > \
 'cyarg/bootstrap/cyarg_ylib.c'

$HOSTYARG buildlib --contents cyarg/bootstrap/cyarg-hosted.yarglib --output cyarg/bootstrap/cyarg-hosted.ylib

xxd -i -n cyarg_hosted_ylib cyarg/bootstrap/cyarg-hosted.ylib | \
 sed 's/^unsigned char cyarg_hosted_ylib\[\] =/alignas(8) const unsigned char cyarg_hosted_ylib[]=/' | \
 awk 'BEGIN { print "#include <stdalign.h>" } { print }' > \
 'cyarg/bootstrap/cyarg_hosted_ylib.c'
