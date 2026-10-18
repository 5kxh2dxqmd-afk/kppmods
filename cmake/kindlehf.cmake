set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_CXX_COMPILER "$ENV{KPPMODS_TOOLCHAIN_ROOT}/bin/arm-kindlehf-linux-gnueabihf-g++")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=neon -mfloat-abi=hard")
