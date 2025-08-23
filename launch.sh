rm -rf build
cmake -S . -B build -G Ninja
cmake --build build -j

# Orchestrateur (si src/main.cpp existe)
./build/bbpctl --help
