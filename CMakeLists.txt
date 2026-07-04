zephyr_library()

zephyr_library_sources_ifdef(CONFIG_TCA9555_TEST
    src/tca9555_test.c
)
