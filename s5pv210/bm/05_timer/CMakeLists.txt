file(GLOB SRCS CONFIGURE_DEPENDS
    ${BM_SRCS}
    *.c
    ../01_led/led.c
    ../02_uart/uart.c
    ../03_buzzer/buzzer.c
    ../04_clock/clock.c
)

# 获取编译器底层的 libgcc 数学辅助库路径 (解决不支持硬件除法的问题)
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-libgcc-file-name
    OUTPUT_VARIABLE LIBGCC_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 方案 A: USB Boot 目标
add_executable(timer_usb.elf ${SRCS})
target_link_options(timer_usb.elf PRIVATE
    -T ${USB_LINK}
    -nostdlib
)
# 明确链接 libgcc
target_link_libraries(timer_usb.elf PRIVATE ${LIBGCC_PATH})

add_custom_command(TARGET timer_usb.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary timer_usb.elf ${CMAKE_CURRENT_BINARY_DIR}/timer_usb.bin
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/timer_usb.bin
    COMMENT "Generating raw binary: timer_usb.bin..."
)

add_custom_target(timer_usb
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Entry point (USB):"
    COMMAND sh -c "${CMAKE_READELF} -h timer_usb.elf | grep 'Entry point'"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Running USB flash tool..."
    COMMAND sudo $<TARGET_FILE:usbtool> ${CMAKE_CURRENT_BINARY_DIR}/timer_usb.bin
    DEPENDS timer_usb.elf usbtool
    USES_TERMINAL
    VERBATIM
)

# 方案 B: SD Boot 目标
add_executable(timer_sd.elf ${SRCS})
target_link_options(timer_sd.elf PRIVATE
    -T ${SD_LINK}
    -nostdlib
)
# 明确链接 libgcc
target_link_libraries(timer_sd.elf PRIVATE ${LIBGCC_PATH})

add_custom_command(TARGET timer_sd.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary timer_sd.elf ${CMAKE_CURRENT_BINARY_DIR}/timer_sd.bin
    COMMAND $<TARGET_FILE:sigtool> ${CMAKE_CURRENT_BINARY_DIR}/timer_sd.bin ${CMAKE_CURRENT_BINARY_DIR}/210.bin
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/timer_sd.bin ${CMAKE_CURRENT_BINARY_DIR}/210.bin
    COMMENT "Generating signed SD image: 210.bin..."
)

add_dependencies(timer_sd.elf sigtool)

add_custom_target(timer_sd
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Entry point (SD Boot):"
    COMMAND sh -c "${CMAKE_READELF} -h timer_sd.elf | grep 'Entry point'"

    # 确认设备文件存在
    COMMAND sh -c "if [ ! -b '${SD_DEV}' ]; then echo ''; echo '*** [ERROR] Target block device ${SD_DEV} does not exist! Please plug in the SD card. ***'; echo ''; exit 1; fi"

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Burning 210.bin to ${SD_DEV} (seek=1) ..."
    COMMAND sudo dd if=${CMAKE_CURRENT_BINARY_DIR}/210.bin of=${SD_DEV} seek=1 bs=512
    COMMAND sudo sync
    COMMAND ${CMAKE_COMMAND} -E echo "Burn completed successfully!"
    DEPENDS timer_sd.elf sigtool
    USES_TERMINAL
    VERBATIM
)

add_custom_target(timer_dump
    COMMAND ${CMAKE_COMMAND} -E echo "--- USB ELF Dump ---"
    COMMAND ${CMAKE_OBJDUMP} -d timer_usb.elf
    COMMAND ${CMAKE_COMMAND} -E echo "--- SD ELF Dump ---"
    COMMAND ${CMAKE_OBJDUMP} -d timer_sd.elf
    DEPENDS timer_usb.elf timer_sd.elf
)
