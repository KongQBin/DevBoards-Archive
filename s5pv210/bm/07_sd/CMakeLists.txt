file(GLOB SRCS CONFIGURE_DEPENDS
    ${BM_SRCS}
    *.c
    ../01_led/led.c
    ../02_uart/uart.c
    ../02_uart/printk.c
    ../03_buzzer/buzzer.c
    ../04_clock/clock.c
    ../05_timer/timer.c
    ../06_ddr/ddr.c
    ../06_ddr/ddr_test.c
)

# 获取编译器底层的 libgcc 数学辅助库路径 (解决不支持硬件除法的问题)
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-libgcc-file-name
    OUTPUT_VARIABLE LIBGCC_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 方案 A: USB Boot 目标
add_executable(sd_usb.elf ${SRCS})
target_link_options(sd_usb.elf PRIVATE
    -T ${USB_LINK}
    -nostdlib
)
# 明确链接 libgcc
target_link_libraries(sd_usb.elf PRIVATE ${LIBGCC_PATH})

add_custom_command(TARGET sd_usb.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary sd_usb.elf ${CMAKE_CURRENT_BINARY_DIR}/sd_usb.bin
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/sd_usb.bin
    COMMENT "Generating raw binary: sd_usb.bin..."
)

add_custom_target(sd_usb
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Entry point (USB):"
    COMMAND sh -c "${CMAKE_READELF} -h sd_usb.elf | grep 'Entry point'"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Running USB flash tool..."
    COMMAND sudo $<TARGET_FILE:usbtool> ${CMAKE_CURRENT_BINARY_DIR}/sd_usb.bin
    DEPENDS sd_usb.elf usbtool
    USES_TERMINAL
    VERBATIM
)

# 方案 B: SD Boot 目标
add_executable(sd_sd.elf ${SRCS})
target_link_options(sd_sd.elf PRIVATE
    -T ${SD_LINK}
    -nostdlib
)
# 明确链接 libgcc
target_link_libraries(sd_sd.elf PRIVATE ${LIBGCC_PATH})

add_custom_command(TARGET sd_sd.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary sd_sd.elf ${CMAKE_CURRENT_BINARY_DIR}/sd_sd.bin
    COMMAND $<TARGET_FILE:sigtool> ${CMAKE_CURRENT_BINARY_DIR}/sd_sd.bin ${CMAKE_CURRENT_BINARY_DIR}/210.bin
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/sd_sd.bin ${CMAKE_CURRENT_BINARY_DIR}/210.bin
    COMMENT "Generating signed SD image: 210.bin..."
)

add_dependencies(sd_sd.elf sigtool)

add_custom_target(sd_sd
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Entry point (SD Boot):"
    COMMAND sh -c "${CMAKE_READELF} -h sd_sd.elf | grep 'Entry point'"

    # 确认设备文件存在
    COMMAND sh -c "if [ ! -b '${SD_DEV}' ]; then echo ''; echo '*** [ERROR] Target block device ${SD_DEV} does not exist! Please plug in the SD card. ***'; echo ''; exit 1; fi"

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Burning 210.bin to ${SD_DEV} (seek=1) ..."
    COMMAND sudo dd if=${CMAKE_CURRENT_BINARY_DIR}/210.bin of=${SD_DEV} seek=1 bs=512
    COMMAND sudo sync
    COMMAND ${CMAKE_COMMAND} -E echo "Burn completed successfully!"
    DEPENDS sd_sd.elf sigtool
    USES_TERMINAL
    VERBATIM
)

add_custom_target(sd_dump
    COMMAND ${CMAKE_COMMAND} -E echo "--- USB ELF Dump ---"
    COMMAND ${CMAKE_OBJDUMP} -d sd_usb.elf
    COMMAND ${CMAKE_COMMAND} -E echo "--- SD ELF Dump ---"
    COMMAND ${CMAKE_OBJDUMP} -d sd_sd.elf
    DEPENDS sd_usb.elf sd_sd.elf
)
