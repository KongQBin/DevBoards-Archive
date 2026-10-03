# 本地工具链 编译
add_subdirectory(utils)

# 交叉工具链 编译
include(${CMAKE_CURRENT_SOURCE_DIR}/arm-toolchain.cmake)
add_subdirectory(led)
