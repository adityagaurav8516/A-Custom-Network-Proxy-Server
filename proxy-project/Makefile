CXX = g++
CXXFLAGS = -std=c++11 -pthread -Iinclude
LDFLAGS = -pthread

all: proxy_server

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
CONFIG_DIR = config
TESTS_DIR = tests

TARGET = proxy_server

SOURCES = $(SRC_DIR)/proxy_server.cpp

HEADERS = $(INCLUDE_DIR)/config.hpp \
          $(INCLUDE_DIR)/filter.hpp \
          $(INCLUDE_DIR)/logger.hpp \
          $(INCLUDE_DIR)/http_parser.hpp

OBJECTS = $(BUILD_DIR)/proxy_server.o

all: directories $(TARGET)

directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(CONFIG_DIR)
	@mkdir -p $(TESTS_DIR)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "✓ Build successful: $(TARGET)"

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)
	rm -f proxy.log
	@echo "✓ Cleaned build artifacts"

# Deep clean (including logs and temporary files)
distclean: clean
	rm -f *.log
	rm -f *~
	rm -f $(SRC_DIR)/*~
	rm -f $(INCLUDE_DIR)/*~
	@echo "✓ Deep clean complete"

# Run the proxy server
run: $(TARGET)
	./$(TARGET) $(CONFIG_DIR)/proxy.conf

# Run tests
test: $(TARGET)
	@echo "Running test suite..."
	@cd $(TESTS_DIR) && chmod +x run_tests.sh && ./run_tests.sh

# Install configuration files (if not present)
install-config:
	@if [ ! -f $(CONFIG_DIR)/proxy.conf ]; then \
		echo "Creating default proxy.conf..."; \
		cp $(CONFIG_DIR)/proxy.conf.example $(CONFIG_DIR)/proxy.conf 2>/dev/null || \
		echo "# Create proxy.conf manually in config/ directory"; \
	fi
	@if [ ! -f $(CONFIG_DIR)/filters.txt ]; then \
		echo "Creating default filters.txt..."; \
		cp $(CONFIG_DIR)/filters.txt.example $(CONFIG_DIR)/filters.txt 2>/dev/null || \
		echo "# Create filters.txt manually in config/ directory"; \
	fi

# Help target
help:
	@echo "HTTP Forward Proxy - Makefile Targets"
	@echo "======================================"
	@echo "make              - Build the proxy server"
	@echo "make clean        - Remove build artifacts"
	@echo "make distclean    - Deep clean (includes logs)"
	@echo "make run          - Build and run the server"
	@echo "make test         - Run test suite"
	@echo "make install-config - Create default config files"
	@echo "make help         - Show this help message"

# Debug target (build with debug symbols)
debug: CXXFLAGS += -g -DDEBUG
debug: clean $(TARGET)
	@echo "✓ Debug build complete"

# Check dependencies
check:
	@echo "Checking dependencies..."
	@which $(CXX) > /dev/null || echo "ERROR: g++ not found"
	@which curl > /dev/null || echo "WARNING: curl not found (needed for tests)"
	@which nc > /dev/null || echo "WARNING: nc (netcat) not found (needed for tests)"
	@echo "Dependency check complete"

.PHONY: all clean distclean run test install-config help debug check directories
