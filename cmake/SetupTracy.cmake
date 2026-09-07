if ( NOT TARGET emeraude_base_flags )
	message(FATAL_ERROR "emeraude_base_flags is not defined !")
endif ()

set(TRACY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tracy)

# An existing checkout does not gain a newly declared submodule on a pull, so initialize it here
# rather than failing every build directory that predates it.
if ( NOT EXISTS ${TRACY_SOURCE_DIR}/CMakeLists.txt )
	find_package(Git REQUIRED)

	message("Initializing the Tracy submodule ...")

	execute_process(
		COMMAND ${GIT_EXECUTABLE} submodule update --init dependencies/tracy
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND_ERROR_IS_FATAL ANY
	)
endif ()

if ( NOT EXISTS ${TRACY_SOURCE_DIR}/CMakeLists.txt )
	message(FATAL_ERROR "The Tracy submodule is still missing at ${TRACY_SOURCE_DIR} !")
endif ()

# Tracy builds its client as an OBJECT library as soon as link-time optimization is active, which
# aggregates a distinct profiler instance into every binary that links it. A multi-DLL process needs
# exactly one, so refuse the combination instead of producing a build that connects on random ports.
if ( EMERAUDE_ENABLE_TRACY AND CMAKE_INTERPROCEDURAL_OPTIMIZATION )
	message(FATAL_ERROR "EMERAUDE_ENABLE_TRACY requires CMAKE_INTERPROCEDURAL_OPTIMIZATION to be off !")
endif ()

# Disabled, the client stays STATIC and compiles down to stubs: no TRACY_ENABLE, no runtime, no
# shared library to ship. Its headers stay on the include path either way, so instrumentation never
# needs a preprocessor guard.
# Enabled, the client becomes SHARED so the executable, emeraude::base, Emeraude and every other
# shared library loaded in the process share a single profiler instance.
if ( EMERAUDE_ENABLE_TRACY )
	message("Enabling Tracy profiler client (shared, on-demand) ...")

	set(TRACY_ENABLE On CACHE BOOL "" FORCE)
	set(TRACY_STATIC Off CACHE BOOL "" FORCE)
	# Record only while the profiler is connected. Without it the client buffers every event from
	# before main() until the first connection, which for a long-running application means gigabytes
	# of resident memory and a single possible session.
	set(TRACY_ON_DEMAND On CACHE BOOL "" FORCE)
else ()
	message("Tracy profiler client disabled (headers only) ...")

	set(TRACY_ENABLE Off CACHE BOOL "" FORCE)
	set(TRACY_STATIC On CACHE BOOL "" FORCE)
	set(TRACY_ON_DEMAND Off CACHE BOOL "" FORCE)
endif ()

set(TRACY_LTO Off CACHE BOOL "" FORCE)

add_subdirectory(${TRACY_SOURCE_DIR} tracy EXCLUDE_FROM_ALL)

# The shared compile requirements are the single hook reaching both the per-module OBJECT libraries
# and, through the umbrella's PUBLIC link, every consumer of emeraude::base — emeraude-engine,
# app_kernel and app_system all get <tracy/Tracy.hpp> and TRACY_ENABLE from here.
target_link_libraries(emeraude_base_flags INTERFACE Tracy::TracyClient)
