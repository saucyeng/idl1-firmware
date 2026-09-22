# TI TLA2528 ADC / GPIO expander driver (IDL1 board LEDs + pod analog).
# Takes the I2C bus handle and address from the caller; no dependency on main.

idf_component_register(
    SRCS "tla2528.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c
)
