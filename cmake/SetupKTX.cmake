if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling KTX-Software (libktx) library ...")

# KTX::ktx carries dl, Threads::Threads and KTX::astcenc-avx2-static in its
# INTERFACE_LINK_LIBRARIES. Threads::Threads is an IMPORTED target and imported targets
# are directory-scoped, so resolve it here rather than relying on emeraude::base having
# called find_package(Threads) in another scope.
find_package(Threads REQUIRED)

find_package(Ktx CONFIG REQUIRED PATHS ${EMERAUDE_EXT_LIBS_PATH} NO_DEFAULT_PATH)

target_link_libraries(${TARGET_BINARY_FOR_SETUP} PRIVATE KTX::ktx)
