.PHONY: build-debug build-release build-tests clean help
.PHONY: test test-unit test-integration test-postgresql 
.PHONY: test-single test-single-unit test-example

# Default target
all: build-debug

# Builds debug version of vtcpd binary (suitable for internal testing).
build-debug:
	mkdir -p build-debug
	cd build-debug && cmake -B ./ -S .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_GCC_SANITIZERS=ON && cmake --build . -j 8

# Builds release version of vtcpd binary.
build-release:
	mkdir -p build-release
	cd build-release && rm -f CMakeCache.txt && cmake -B ./ -S .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j 8

# Builds debug version with tests enabled (uses 'build' directory for compatibility)
build-tests:
	mkdir -p build-tests
	cd build-tests && cmake -B ./ -S .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCMAKE_PREFIX_PATH=/usr/local/openssl -DCMAKE_CXX_FLAGS="-isystem /usr/local/openssl/include" && cmake --build . -j 8

# Run all tests (unit + integration)
test: build-tests
	@echo "Running all tests..."
	cd build-tests && ctest --output-on-failure --parallel 4

# Run unit tests only
test-unit: build-tests
	@echo "Running unit tests..."
	cd build-tests && ./bin/unit_tests

# Run integration tests only  
test-integration: build-tests
	@echo "Running integration tests..."
	cd build-tests && ./bin/postgresql_integration_tests

# Run PostgreSQL integration tests with specific filter
test-postgresql: build-tests
	@echo "Running PostgreSQL integration tests..."
	cd build-tests && ./bin/postgresql_integration_tests

# Run a single test by name pattern
# Usage: make test-single TEST=OwnKeysHandler
test-single: build-tests
	@if [ -z "$(TEST)" ]; then \
		echo "Error: TEST parameter is required. Usage: make test-single TEST=<test-pattern>"; \
		echo "Examples:"; \
		echo "  make test-single TEST=OwnKeysHandler"; \
		echo "  make test-single TEST=PaymentKeys"; \
		echo "  make test-single TEST='*InvalidKey*'"; \
		exit 1; \
	fi
	@echo "Running test pattern: $(TEST)"
	cd build-tests && ./bin/postgresql_integration_tests --gtest_filter="*$(TEST)*"

# Run unit test by name pattern (requires tests to be built first)
# Usage: make test-single-unit TEST=YourTestName
test-single-unit: build-tests
	@if [ -z "$(TEST)" ]; then \
		echo "Error: TEST parameter is required. Usage: make test-single-unit TEST=<test-pattern>"; \
		echo "Examples:"; \
		echo "  make test-single-unit TEST=SphincsKeys"; \
		echo "  make test-single-unit TEST='*Memory*'"; \
		exit 1; \
	fi
	@echo "Running unit test pattern: $(TEST)"
	cd build-tests && ./bin/unit_tests --gtest_filter="*$(TEST)*"

# Example of running specific tests (for documentation)
test-example:
	@echo "Examples of running specific tests:"
	@echo ""
	@echo "1. Run all OwnKeysHandler tests:"
	@echo "   make test-single TEST=OwnKeysHandler"
	@echo ""
	@echo "2. Run all PaymentKeys tests:"
	@echo "   make test-single TEST=PaymentKeys"
	@echo ""
	@echo "3. Run specific test by full name:"
	@echo "   make test-single TEST='OwnKeysHandlerPostgreSQLIntegrationTest.saveKey_ValidData_SavesSuccessfully'"
	@echo ""
	@echo "4. Run tests matching pattern:"
	@echo "   make test-single TEST='*InvalidKey*'"
	@echo ""
	@echo "5. Run unit test by pattern:"
	@echo "   make test-single-unit TEST=SphincsKeys"
	@echo ""
	@echo "6. Run unit tests directly:"
	@echo "   cd build-tests && ./bin/unit_tests --gtest_filter='*YourPattern*'"
	@echo ""
	@echo "7. Run integration tests directly:"
	@echo "   cd build-tests && ./bin/postgresql_integration_tests --gtest_filter='*YourPattern*'"

# Clean all build directories
clean:
	rm -rf build-tests build-debug build-release

# Show help
help:
	@echo "Available targets:"
	@echo ""
	@echo "  Build targets:"
	@echo "    build-debug       - Build debug version with GCC sanitizers"
	@echo "    build-release     - Build release version"  
	@echo "    build-tests       - Build debug version with tests enabled"
	@echo ""
	@echo "  Test targets:"
	@echo "    test             - Run all tests (unit + integration)"
	@echo "    test-unit        - Run unit tests only"
	@echo "    test-integration - Run integration tests only"  
	@echo "    test-postgresql  - Run PostgreSQL integration tests"
	@echo "    test-single      - Run specific integration test pattern (requires TEST parameter)"
	@echo "    test-single-unit - Run specific unit test pattern (requires TEST parameter)"
	@echo "    test-example     - Show examples of running specific tests"
	@echo ""
	@echo "  Utility targets:"
	@echo "    clean            - Clean all build directories"
	@echo "    help             - Show this help message"
	@echo ""
	@echo "  Examples:"
	@echo "    make test-single TEST=OwnKeysHandler"
	@echo "    make test-single TEST='*InvalidKey*'"
