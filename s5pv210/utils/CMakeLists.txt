# 宿主机 USB 烧录工具
add_executable(
    usbtool
    usbtool.c
)

target_link_libraries(
    usbtool
    PRIVATE
    usb-1.0
)

set_target_properties(usbtool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# 宿主机 SD卡 签名工具
add_executable(
    sigtool
    sigtool.c
)
set_target_properties(sigtool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
