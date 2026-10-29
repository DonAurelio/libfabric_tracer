#!/bin/bash
# run_tracer.sh — build and run the custom fi_send tracer, printing
# which libfabric is in use at every level before starting the test.

REPO=$(cd "$(dirname "$0")" && pwd)

echo "======================================================"
echo "  libfabric in use"
echo "======================================================"

echo ""
echo "--- module-level libfabric ---"
fi_info --version 2>&1

echo ""
echo "--- libfabric linked by pingpong_module (ldd) ---"
ldd "$REPO/examples/pingpong_module" 2>&1 | grep libfabric

echo ""
echo "--- libfabric loaded by tracer DSO (ldd) ---"
ldd "$REPO/my_tracer/libmytracer_fi.so" 2>&1 | grep libfabric

echo ""
echo "======================================================"
echo "  Running pingpong_module with FI_HOOK=mytracer"
echo "======================================================"
echo ""

FI_PROVIDER_PATH="$REPO/my_tracer" \
FI_PROVIDER=tcp \
FI_HOOK=mytracer \
"$REPO/examples/pingpong_module"
