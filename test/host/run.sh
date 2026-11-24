#!/bin/sh
# Build and run the host tests of lib/gauge (needs g++ and Python with Pillow).
set -e
cd "$(dirname "$0")/../.."
mkdir -p build
python3 tools/make_fixtures.py build/fixtures
g++ -std=c++17 -O2 -Wall -Wextra -Ilib/gauge lib/gauge/gauge.cpp test/host/test_gauge.cpp -o build/test_gauge
g++ -std=c++17 -O2 -Wall -Wextra -Ilib/gauge lib/gauge/gauge.cpp test/host/gauge_cli.cpp -o build/gauge_cli
./build/test_gauge build/fixtures/cases.txt
python3 test/host/check_openapi.py
