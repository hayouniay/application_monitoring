BUILD_DIR := build

CLI_TARGET := app_top_monitoring
QT_TARGET := app_top_monitoring_qt

.PHONY: all build cli qt debug release install clean test

all: build

build:
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

cli:
	cmake -S . -B $(BUILD_DIR) -DBUILD_QT_APP=OFF
	cmake --build $(BUILD_DIR) --target $(CLI_TARGET)

test:
	cmake -S . -B $(BUILD_DIR) -DBUILD_QT_APP=OFF -DBUILD_TESTS=ON
	cmake --build $(BUILD_DIR)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

qt:
	cmake -S . -B $(BUILD_DIR) -DBUILD_QT_APP=ON
	cmake --build $(BUILD_DIR) --target $(QT_TARGET)

release:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)

debug:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)

install:
	cmake --install $(BUILD_DIR)

clean:
	cmake --build $(BUILD_DIR) --target clean
