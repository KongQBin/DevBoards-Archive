file(GLOB SRCS CONFIGURE_DEPENDS
    ${BM_SRCS}
    *.c
    ../01_led/led.c
    ../02_uart/uart.c
    ../03_buzzer/buzzer.c
    ../04_clock/clock.c
    ../05_timer/timer.c
)

# 获取编译器底层的 libgcc 数学辅助库路径 (解决不支持硬件除法的问题)
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-libgcc-file-name
    OUTPUT_VARIABLE LIBGCC_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 方案 A: USB Boot 目标
add_executable(ddr_usb.elf ${SRCS})
target_link_options(ddr_usb.elf PRIVATE
    -T ${USB_LINK}
    -nostdlib
)
# 明确链接 libgcc
target_link_libraries(ddr_usb.elf PRIVATE ${LIBGCC_PATH})

add_custom_command(TARGET ddr_usb.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ddr_usb.elf ${CMAKE_CURRENT_BINARY_DIR}/ddr_usb.bin
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/ddr_usb.bin
    COMMENT "Generating raw binary: ddr_usb.bin..."
)

add_custom_target(ddr_usb
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Entry point (USB):"
    COMMAND sh -c "${CMAKE_READELF} -h ddr_usb.elf | grep 'Entry point'"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Running USB flash tool..."
    COMMAND sudo $<TARGET_FILE:usbtool> ${CMAKE_CURRENT_BINARY_DIR}/ddr_usb.bin
    DEPENDS ddr_usb.elf usbtool
    USES_TERMINAL
    VERBATIM
)

# 方案 B: SD Boot 目标
add_executable(ddr_sd.elf ${SRCS})
target_link_options(ddr_sd.elf PRIVATE
    -T ${SD_LINK}
    -nostdlib
)
# 明确链接 libgcc
target_link_libraries(ddr_sd.elf PRIVATE ${LIBGCC_PATH})

add_custom_command(TARGET ddr_sd.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ddr_sd.elf ${CMAKE_CURRENT_BINARY_DIR}/ddr_sd.bin
    COMMAND $<TARGET_FILE:sigtool> ${CMAKE_CURRENT_BINARY_DIR}/ddr_sd.bin ${CMAKE_CURRENT_BINARY_DIR}/210.bin
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/ddr_sd.bin ${CMAKE_CURRENT_BINARY_DIR}/210.bin
    COMMENT "Generating signed SD image: 210.bin..."
)

add_dependencies(ddr_sd.elf sigtool)

add_custom_target(ddr_sd
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Entry point (SD Boot):"
    COMMAND sh -c "${CMAKE_READELF} -h ddr_sd.elf | grep 'Entry point'"

    # 确认设备文件存在
    COMMAND sh -c "if [ ! -b '${SD_DEV}' ]; then echo ''; echo '*** [ERROR] Target block device ${SD_DEV} does not exist! Please plug in the SD card. ***'; echo ''; exit 1; fi"

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Burning 210.bin to ${SD_DEV} (seek=1) ..."
    COMMAND sudo dd if=${CMAKE_CURRENT_BINARY_DIR}/210.bin of=${SD_DEV} seek=1 bs=512
    COMMAND sudo sync
    COMMAND ${CMAKE_COMMAND} -E echo "Burn completed successfully!"
    DEPENDS ddr_sd.elf sigtool
    USES_TERMINAL
    VERBATIM
)

add_custom_target(ddr_dump
    COMMAND ${CMAKE_COMMAND} -E echo "--- USB ELF Dump ---"
    COMMAND ${CMAKE_OBJDUMP} -d ddr_usb.elf
    COMMAND ${CMAKE_COMMAND} -E echo "--- SD ELF Dump ---"
    COMMAND ${CMAKE_OBJDUMP} -d ddr_sd.elf
    DEPENDS ddr_usb.elf ddr_sd.elf
)
