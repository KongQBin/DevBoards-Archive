file(GLOB SRCS CONFIGURE_DEPENDS
    ${BM_SRCS}
    *.c
)

# 方案 A: USB Boot 目标
add_executable(led_usb.elf ${SRCS})
target_link_options(led_usb.elf PRIVATE
    -T ${USB_LINK}
    -nostdlib
)

add_custom_command(TARGET led_usb.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary led_usb.elf ${CMAKE_CURRENT_BINARY_DIR}/led_usb.bin
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/led_usb.bin
    COMMENT "Generating raw binary: led_usb.bin..."
)

add_custom_target(led_usb
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Entry point (USB):"
    COMMAND sh -c "${CMAKE_READELF} -h led_usb.elf | grep 'Entry point'"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Running USB flash tool..."
    COMMAND sudo $<TARGET_FILE:usbtool> ${CMAKE_CURRENT_BINARY_DIR}/led_usb.bin
    DEPENDS led_usb.elf usbtool
    USES_TERMINAL
    VERBATIM
)

# ============================================================
# 方案 B: SD Boot 目标
# ============================================================
add_executable(led_sd.elf ${SRCS})
target_link_options(led_sd.elf PRIVATE
    -T ${SD_LINK}
    -nostdlib
)

add_custom_command(TARGET led_sd.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary led_sd.elf ${CMAKE_CURRENT_BINARY_DIR}/led_sd.bin
    COMMAND $<TARGET_FILE:sigtool> ${CMAKE_CURRENT_BINARY_DIR}/led_sd.bin ${CMAKE_CURRENT_BINARY_DIR}/210.bin
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/led_sd.bin ${CMAKE_CURRENT_BINARY_DIR}/210.bin
    COMMENT "Generating signed SD image: 210.bin..."
)

add_dependencies(led_sd.elf sigtool)

add_custom_target(led_sd
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Entry point (SD Boot):"
    COMMAND sh -c "${CMAKE_READELF} -h led_sd.elf | grep 'Entry point'"

    # 确认设备文件存在
    COMMAND sh -c "if [ ! -b '${SD_DEV}' ]; then echo ''; echo '*** [ERROR] Target block device ${SD_DEV} does not exist! Please plug in the SD card. ***'; echo ''; exit 1; fi"

    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Burning 210.bin to ${SD_DEV} (seek=1) ..."
    COMMAND sudo dd if=${CMAKE_CURRENT_BINARY_DIR}/210.bin of=${SD_DEV} seek=1 bs=512
    COMMAND sudo sync
    COMMAND ${CMAKE_COMMAND} -E echo "Burn completed successfully!"
    DEPENDS led_sd.elf sigtool
    USES_TERMINAL
    VERBATIM
)

add_custom_target(led_dump
    COMMAND ${CMAKE_COMMAND} -E echo "--- USB ELF Dump ---"
    COMMAND ${CMAKE_OBJDUMP} -d led_usb.elf
    COMMAND ${CMAKE_COMMAND} -E echo "--- SD ELF Dump ---"
    COMMAND ${CMAKE_OBJDUMP} -d led_sd.elf
    DEPENDS led_usb.elf led_sd.elf
)
