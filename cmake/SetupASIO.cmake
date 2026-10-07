if ( NOT TARGET_BINARY_FOR_SETUP )
	message(FATAL_ERROR "TARGET_BINARY_FOR_SETUP is not SET !")
endif ()

message("Enabling ASIO library (header-only) ...")

set(ASIO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/asio/include)

target_include_directories(${TARGET_BINARY_FOR_SETUP} SYSTEM PUBLIC ${ASIO_SOURCE_DIR})
# ASIO_NO_DEPRECATED (owner decision 2026-09-30, plan Ave Robustus): the synchronous operations return void and report
# through their error_code out-parameter only — the deprecated returned copy of that code is gone.
# ASIO_NO_TYPEID (owner decision 2026-10-07, RTTI-free foundation): asio only detects Boost's BOOST_NO_TYPEID, never
# -fno-rtti; without it detail/service_registry, executor.hpp and execution/any_executor.hpp use typeid.
target_compile_definitions(${TARGET_BINARY_FOR_SETUP} PUBLIC ASIO_STANDALONE ASIO_NO_EXCEPTIONS ASIO_DISABLE_CO_AWAIT ASIO_NO_DEPRECATED ASIO_NO_TYPEID)

