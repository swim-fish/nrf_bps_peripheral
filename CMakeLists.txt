cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(citizen_bps_peripheral)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE
  ${app_sources}
  )

zephyr_include_directories(
    src
    configuration/${BOARD}
    )
    
target_sources_ifdef(CONFIG_CAF_SAMPLE_BUTTON_STATE
    app PRIVATE src/modules/button_state.c)

