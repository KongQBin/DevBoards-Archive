set(SD_LINK ${CMAKE_CURRENT_SOURCE_DIR}/link.lds)
set(USB_LINK ${CMAKE_CURRENT_SOURCE_DIR}/usb_link.lds)
file(GLOB BM_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.S")

add_subdirectory(01_led)        # LED灯
add_subdirectory(02_uart)       # 串口
add_subdirectory(03_buzzer)     # 蜂鸣器
add_subdirectory(04_clock)      # 时钟
add_subdirectory(05_timer)      # 定时器
add_subdirectory(06_ddr)        # 定时器

add_custom_target(bm_bash SOURCES
    ${SD_LINK}
    ${USB_LINK}
    ${BM_SRCS}  # 如果 start.S 在子目录没有正确挂载，可以直接在这里暴露
)
