# PSn00bSDK project build script - DualShock tester

cmake_minimum_required(VERSION 3.21)

project(
	dstest
	LANGUAGES    C ASM
	VERSION      1.0.0
	DESCRIPTION  "DualShock controller tester"
	HOMEPAGE_URL "https://github.com/Lameguy64/PSn00bSDK"
)

psn00bsdk_add_executable(dstest GPREL main.c spi.c)

psn00bsdk_add_cd_image(
	iso      # Target name
	dstest   # Output file name (= dstest.bin + dstest.cue)
	iso.xml  # Path to config file
	DEPENDS dstest system.cnf
)

# Additionally create a plain .iso image (raw 2352-byte sectors, CD-ROM XA
# mode 2) using mkpsxiso. The custom command below configures iso.xml on its
# own because the SDK's psn00bsdk_add_cd_image() only emits .bin/.cue pairs.
configure_file(iso.xml ${CMAKE_CURRENT_BINARY_DIR}/ds_iso.xml)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dstest.iso
	COMMAND ${MKPSXISO} -y -o dstest.iso ${CMAKE_CURRENT_BINARY_DIR}/ds_iso.xml
	DEPENDS dstest system.cnf ${CMAKE_CURRENT_BINARY_DIR}/ds_iso.xml
	COMMENT "Building CD image dstest.iso"
	VERBATIM
)

add_custom_target(iso_image ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dstest.iso)
