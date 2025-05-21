.PHONY: build-debug build-release


# Builds debug version of vtcpd binary (suitable for internal testing).
build-debug:
	mkdir -p build-debug
	cd build-debug && cmake -B ./ -S .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_GCC_SANITIZERS=ON && cmake --build . -j 8


# Builds release version of vtcpd binary.
build-release:
	mkdir -p build-release
	cd build-release && rm -f CMakeCache.txt && cmake -B ./ -S .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j 8
