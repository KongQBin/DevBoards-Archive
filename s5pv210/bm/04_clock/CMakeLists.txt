file(GLOB SRCS CONFIGURE_DEPENDS
    ${BM_SRCS}
    *.c
    ../01_led/led.c
    ../02_uart/uart.c
    ../03_buzzer/buzzer.c
)

# 方案 A: USB Boot 目标
add_executable(clock_usb.elf ${SRCS})
target_link_options(clock_usb.elf PRIVATE
    -T ${USB_LINK}
    -nostdlib
)

add_custom_command(TARGET clock_usb.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary clock_usb.elf ${CMAKE_CURRENT_BINARY_DIR}/clock_usb.bin
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/clock_usb.bin
    COMMENT "Generating raw binary: clock_usb.bin..."
)

add_custom_target(clock_usb
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Entry point (USB):"
    COMMAND sh -c "${CMAKE_READELF} -h clock_usb.elf | grep 'Entry point'"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Running USB flash tool..."
    COMMAND sudo $<TARGET_FILE:usbtool> ${CMAKE_CURRENT_BINARY_DIR}/clock_usb.bin
    DEPENDS clock_usb.elf usbtool
    USES_TERMINAL
    VERBATIM
)

# ============================================================
# 方案 B: SD Boot 目标
# ============================================================
add_executable(clock_sd.elf ${SRCS})
target_link_options(clock_sd.elf PRIVATE
    -T ${SD_LINK}
    -nostdlib
)

add_custom_command(TARGET clock_sd.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary clock_sd.elf ${CMAKE_CURRENT_BINARY_DIR}/clock_sd.bin
    COMMAND $<TARGET_FILE:sigtool> ${CMAKE_CURRENT_BINARY_DIR}/clock_sd.bin ${CMAKE_CURRENT_BINARY_DIR}/210.bin
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/clock_sd.bin ${CMAKE_CURRENT_BINARY_DIR}/210.bin
    COMMENT "Generating signed SD image: 210.bin..."
)

add_dependencies(clock_sd.elf sigtool)

add_custom_target(clock_sd
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Entry point (SD Boot):"
    COMMAND sh -c "${CMAKE_READELF} -h clock_sd.elf | grep 'Entry point'"

    # 确认设备文件存在
    COMMAND sh -c "if [ ! -b '${SD_DEV}' ]; then echo ''; echo '*** [ERROR] Target block device ${SD_DEV} does not exist! Please plug in the SD card. ***'; echo ''; exit 1; fi"

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Burning 210.bin to ${SD_DEV} (seek=1) ..."
    COMMAND sudo dd if=${CMAKE_CURRENT_BINARY_DIR}/210.bin of=${SD_DEV} seek=1 bs=512
    COMMAND sudo sync
    COMMAND ${CMAKE_COMMAND} -E echo "Burn completed successfully!"
    DEPENDS clock_sd.elf sigtool
    USES_TERMINAL
    VERBATIM
)

add_custom_target(clock_dump
    COMMAND ${CMAKE_COMMAND} -E echo "--- USB ELF Dump ---"
    COMMAND ${CMAKE_OBJDUMP} -d clock_usb.elf
    COMMAND ${CMAKE_COMMAND} -E echo "--- SD ELF Dump ---"
    COMMAND ${CMAKE_OBJDUMP} -d clock_sd.elf
    DEPENDS clock_usb.elf clock_sd.elf
)
