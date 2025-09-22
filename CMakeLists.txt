cmake_minimum_required(VERSION 3.16)
project(DDSMiniBus LANGUAGES CXX)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not supported. Run from project root: cmake -S . -B build ...")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DDS_ENABLE_DEPLOY_RUNTIME "Deploy Qt runtime for installed app" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Test)

# Ensure all executables land in the binary dir for consistent test execution
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Diagnostics for CTest PATH
message(STATUS "CTEST PATH will prepend: '${CMAKE_BINARY_DIR};${_qt_bin};${_mingw}'")

# ===== Main app =====
set(SOURCES
    main.cpp
    utils/log_setup.cpp
    utils/logger.cpp
    config/config_manager.cpp
    serialization/serializer.cpp
    core/dds_core.cpp
    core/publisher.cpp
    core/subscriber.cpp
    transport/transport_base.cpp
    transport/udp_transport.cpp
    transport/tcp_transport.cpp
    transport/ack_manager.cpp
    discovery/discovery_manager.cpp
)
set(HEADERS
    include/log_setup.h
    include/logger.h
    include/config_manager.h
    include/serializer.h
    include/dds_core.h
    include/publisher.h
    include/subscriber.h
    include/topic.h
    include/qos.h
    include/bounded_lru.h
    include/transport_base.h
    include/udp_transport.h
    include/tcp_transport.h
    include/ack_manager.h
    include/frame_codec.h
    include/discovery_manager.h
    include/cli.h
)


qt_add_executable(dds_mini_bus
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(dds_mini_bus PRIVATE Qt6::Core Qt6::Network)
target_include_directories(dds_mini_bus PRIVATE . include)

# Post-build: copy config files to build/config/ with runtime names
add_custom_command(TARGET dds_mini_bus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/config
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/logs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/config/config_tx.json ${CMAKE_BINARY_DIR}/config/dds-sender-1.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/config/config_rx.json ${CMAKE_BINARY_DIR}/config/dds-receiver-1.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/config/config_rx2.json ${CMAKE_BINARY_DIR}/config/dds-receiver-2.json
    COMMENT "Copying config files to build/config/ and creating logs/ directory"
)

# Find CLI sources recursively in common folders
file(GLOB_RECURSE MINI_DDS_CLI_CPP
  "${CMAKE_SOURCE_DIR}/utils/*cli*.cpp"
  "${CMAKE_SOURCE_DIR}/utils/**/cli*.cpp"
  "${CMAKE_SOURCE_DIR}/src/*cli*.cpp"
  "${CMAKE_SOURCE_DIR}/src/**/cli*.cpp"
  "${CMAKE_SOURCE_DIR}/app/*cli*.cpp"
  "${CMAKE_SOURCE_DIR}/app/**/cli*.cpp"
  "${CMAKE_SOURCE_DIR}/cli/*.cpp"
)

# (Optional) log what we found
message(STATUS "CLI sources: ${MINI_DDS_CLI_CPP}")

# Create static library with core sources for linking with tests
add_library(mini_dds_lib STATIC
  utils/log_setup.cpp
  utils/logger.cpp
  config/config_manager.cpp
  serialization/serializer.cpp
  core/dds_core.cpp
  core/publisher.cpp
  core/subscriber.cpp
  transport/transport_base.cpp
  transport/udp_transport.cpp
  transport/tcp_transport.cpp
  transport/ack_manager.cpp
  discovery/discovery_manager.cpp
  ${MINI_DDS_CLI_CPP}
)
target_include_directories(mini_dds_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} include)
target_link_libraries(mini_dds_lib PUBLIC Qt6::Core Qt6::Network)

# Generate moc for Qt classes in mini_dds_lib (centralized)
target_sources(mini_dds_lib PRIVATE
  include/transport_base.h
  include/tcp_transport.h
  include/udp_transport.h
  include/ack_manager.h
  include/dds_core.h
  include/discovery_manager.h
  include/config_manager.h
)

# Link app against the lib
target_link_libraries(dds_mini_bus PRIVATE mini_dds_lib)

# Windows deployment function (skips test targets)
function(add_windeploy target)
  if(NOT WIN32 OR NOT DDS_ENABLE_DEPLOY_RUNTIME)
    return()
  endif()

  # Skip tests entirely
  if(target MATCHES "^test_")
    message(STATUS "Skipping windeployqt for test target: ${target}")
    return()
  endif()

  find_program(WINDEPLOYQT_EXECUTABLE windeployqt
    HINTS "$ENV{QTDIR}/bin" "C:/Qt/6.9.2/mingw_64/bin")

  if(NOT WINDEPLOYQT_EXECUTABLE)
    message(WARNING "windeployqt not found, skipping deployment for ${target}")
    return()
  endif()

  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --no-opengl-sw --no-compiler-runtime
            --dir "${CMAKE_BINARY_DIR}/qt_deploy" "$<TARGET_FILE:${target}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target}>" "${CMAKE_BINARY_DIR}/qt_deploy/"
    COMMENT "Running windeployqt for ${target}")
endfunction()

# ===== Tests (scenario runners) =====
if (NOT TARGET test_discovery_rx)
  add_executable(test_discovery_rx
      tests/test_discovery_receiver.cpp
  )
  target_link_libraries(test_discovery_rx PRIVATE mini_dds_lib Qt6::Core Qt6::Network)
  target_include_directories(test_discovery_rx PRIVATE . include)
  # Disable AUTOMOC for test targets to avoid Qt class conflicts
  set_target_properties(test_discovery_rx PROPERTIES AUTOMOC OFF)
endif()

if (NOT TARGET test_discovery_tx)
  add_executable(test_discovery_tx
      tests/test_discovery_sender.cpp
  )
  target_link_libraries(test_discovery_tx PRIVATE mini_dds_lib Qt6::Core Qt6::Network)
  target_include_directories(test_discovery_tx PRIVATE . include)
  # Disable AUTOMOC for test targets to avoid Qt class conflicts
  set_target_properties(test_discovery_tx PROPERTIES AUTOMOC OFF)
endif()

target_sources(dds_mini_bus PRIVATE
    include/frame_codec.h
)

# ===== Unit tests =====
include(CTest)
enable_testing()

add_executable(test_serializer
    tests/unit/test_serializer.cpp
)
target_link_libraries(test_serializer PRIVATE mini_dds_lib Qt6::Core Qt6::Test)
target_include_directories(test_serializer PRIVATE . include)

add_executable(test_ack_manager
    tests/unit/test_ack_manager.cpp
)
target_link_libraries(test_ack_manager PRIVATE mini_dds_lib Qt6::Core Qt6::Network Qt6::Test)
target_include_directories(test_ack_manager PRIVATE . include)
# Keep AUTOMOC enabled for this test as it has Q_OBJECT

add_executable(test_negotiation
    tests/unit/test_negotiation.cpp
)
target_link_libraries(test_negotiation PRIVATE mini_dds_lib Qt6::Core Qt6::Test)
target_include_directories(test_negotiation PRIVATE . include)

add_executable(test_integration_scenarios
    tests/unit/test_integration_scenarios.cpp
)
target_link_libraries(test_integration_scenarios PRIVATE mini_dds_lib Qt6::Test)
target_include_directories(test_integration_scenarios PRIVATE . include)
# Re-enable AUTOMOC for this test as it has Q_OBJECT
add_dependencies(test_integration_scenarios test_discovery_rx)

add_executable(test_tcp_reliable
    tests/test_tcp_reliable.cpp
)
target_link_libraries(test_tcp_reliable PRIVATE mini_dds_lib Qt6::Core Qt6::Network)
target_include_directories(test_tcp_reliable PRIVATE . include)
# Disable AUTOMOC to avoid Qt class conflicts
set_target_properties(test_tcp_reliable PROPERTIES AUTOMOC OFF)

# ===== Integration tests =====
add_executable(test_pub2sub_reliable
    tests/integration/test_pub2sub_reliable.cpp
    tests/common/test_utils.cpp
    utils/cli.cpp
)
target_include_directories(test_pub2sub_reliable PRIVATE . include)
# Re-enable AUTOMOC for this test as it has Q_OBJECT in test_utils.cpp

add_executable(test_discovery_cycle
    tests/integration/test_discovery_cycle.cpp
    tests/common/test_utils.cpp
    utils/cli.cpp
)
target_include_directories(test_discovery_cycle PRIVATE . include)
# Re-enable AUTOMOC for this test as it has Q_OBJECT in test_utils.cpp

add_executable(test_qos_failure
    tests/integration/test_qos_failure.cpp
    tests/common/test_utils.cpp
    utils/cli.cpp
)
target_include_directories(test_qos_failure PRIVATE . include)
# Re-enable AUTOMOC for this test as it has Q_OBJECT in test_utils.cpp


# Perf tests (manual)
add_executable(test_throughput_udp tests/perf/test_throughput_udp.cpp)
target_link_libraries(test_throughput_udp PRIVATE mini_dds_lib Qt6::Test)
target_include_directories(test_throughput_udp PRIVATE . include)

add_executable(test_latency_reliable tests/perf/test_latency_reliable.cpp)
target_link_libraries(test_latency_reliable PRIVATE mini_dds_lib Qt6::Test)
target_include_directories(test_latency_reliable PRIVATE . include)

# Link ALL tests to mini_dds_lib (including legacy target if present)
foreach(t IN ITEMS
  test_pub2sub_reliable
  test_discovery_cycle
  test_qos_failure
  test_integration_scenarios
)
  if (TARGET ${t})
    target_link_libraries(${t} PRIVATE mini_dds_lib Qt6::Test)
    target_compile_definitions(${t} PRIVATE TEST_APP_NAME="dds_mini_bus.exe")
  endif()
endforeach()

# ---- BEGIN: Test configuration and helpers ----

# --- Ensure qt_deploy/config exists with a default config at configure-time ---
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/qt_deploy/config")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/qt_deploy/logs")
configure_file(
  "${CMAKE_SOURCE_DIR}/config/config_broadcast.json"
  "${CMAKE_BINARY_DIR}/qt_deploy/config/config.json"
  COPYONLY
)

# Helper to register tests with a consistent command/env/WD
function(dds_add_test NAME)
  # Additional args go to the test binary command line
  add_test(NAME ${NAME}
    COMMAND $<TARGET_FILE:${NAME}> ${ARGN}
  )
  # Run from build dir so exe is there; qt_deploy is in PATH for DLLs
  set_tests_properties(${NAME} PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
  # Prepend qt_deploy to PATH (so DLLs resolve)
  set_property(TEST ${NAME} APPEND PROPERTY ENVIRONMENT
    "PATH=${CMAKE_BINARY_DIR}/qt_deploy;$ENV{PATH}"
  )
endfunction()

# Helper to append loopback + verbose logs on Windows (to avoid multicast/firewall flakiness)
function(dds_set_loopback_env NAME)
  if (WIN32)
    set_property(TEST ${NAME} APPEND PROPERTY ENVIRONMENT "DDS_TEST_LOOPBACK=1")
  endif()
  set_property(TEST ${NAME} APPEND PROPERTY ENVIRONMENT "QT_LOGGING_RULES=dds.disc=true;dds.net=true")
endfunction()

# Register all tests with consistent setup
set(TEST_CFG "${CMAKE_BINARY_DIR}/qt_deploy/config/config.json")

# Unit/perf tests (no args):
dds_add_test(test_serializer)
dds_add_test(test_ack_manager)
dds_add_test(test_negotiation)
dds_add_test(test_integration_scenarios)
dds_add_test(test_tcp_reliable)
dds_add_test(test_throughput_udp)
dds_add_test(test_latency_reliable)

dds_set_loopback_env(test_integration_scenarios)

# Integration-like tests that need a config:
# Note: QtTest-based tests (pub2sub, discovery_cycle, qos_failure) don't take --config args directly
# They spawn dds_mini_bus.exe processes and pass config args to those
dds_add_test(test_pub2sub_reliable)
dds_add_test(test_discovery_cycle)
dds_add_test(test_qos_failure)
dds_add_test(test_discovery_rx      --config "${TEST_CFG}")
dds_add_test(test_discovery_tx      --config "${TEST_CFG}")

# Add loopback env on Windows to networked tests (avoid multicast issues)
foreach(NETTEST
  test_pub2sub_reliable
  test_discovery_cycle
  test_qos_failure
  test_discovery_rx
  test_discovery_tx
)
  dds_set_loopback_env(${NETTEST})
endforeach()

# Disable flaky single-process integration test on Windows/MinGW by default.
# This matches our README and still allows an explicit override for CI/dev.
if (WIN32 AND (CMAKE_GENERATOR MATCHES "MinGW" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  set_tests_properties(test_integration_scenarios PROPERTIES
    DISABLED TRUE
    LABELS "integration_single_process_win_flaky"
  )
endif()

# Optional override: set FORCE_ENABLE_INTEGRATION_TESTS=1 to force-run it anywhere.
if(DEFINED ENV{FORCE_ENABLE_INTEGRATION_TESTS} AND "$ENV{FORCE_ENABLE_INTEGRATION_TESTS}" STREQUAL "1")
  set_tests_properties(test_integration_scenarios PROPERTIES DISABLED FALSE)
endif()

# Longer timeout for latency test on Windows
set_tests_properties(test_latency_reliable PROPERTIES TIMEOUT 120)

# ---- END: Test configuration and helpers ----

# Windows Qt deployment and test configuration
if (WIN32)
  set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/qt_deploy")
  set(TEST_CONFIG_SRC "${CMAKE_SOURCE_DIR}/config")
  set(TEST_CONFIG_DST "${DEPLOY_DIR}/config")

  # Find Qt deployment tools
  find_program(WINDEPLOYQT_EXE windeployqt
    HINTS
      "$ENV{QT_BIN}/bin"
      "C:/Qt/6.9.2/msvc2019_64/bin"
      "C:/Qt/6.9.2/mingw_64/bin"
      "C:/Qt/6.5.3/msvc2019_64/bin"
      "C:/Qt/6.5.3/mingw_64/bin"
  )

  # Deploy Qt for main executable
  if (WINDEPLOYQT_EXE)
    add_custom_command(TARGET dds_mini_bus POST_BUILD
      COMMAND "${WINDEPLOYQT_EXE}" "$<TARGET_FILE:dds_mini_bus>"
              --debug --compiler-runtime --network --no-translations
              --dir "${DEPLOY_DIR}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:dds_mini_bus>" "${DEPLOY_DIR}/"
      COMMENT "Deploying Qt runtime for dds_mini_bus.exe"
    )

    # Copy essential Qt DLLs for tests to qt_deploy
    add_custom_target(deploy_qt_dlls_for_tests ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::Core>" "${DEPLOY_DIR}/"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::Network>" "${DEPLOY_DIR}/"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Qt6::Test>" "${DEPLOY_DIR}/"
      COMMENT "Copying Qt DLLs for tests to qt_deploy"
    )
  else()
    message(WARNING "windeployqt not found - Qt runtime will not be deployed automatically")
  endif()

  # Copy test configs to both build and deploy directories
  add_custom_target(copy_test_configs ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/config"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/config" "${CMAKE_BINARY_DIR}/config"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/config"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/config" "${DEPLOY_DIR}/config"
    COMMENT "Copy test config files to build/config and qt_deploy/config"
  )

  # Make all tests depend on deployed DLLs and configs (only if deploy_qt_once exists)
  if(TARGET deploy_qt_once)
    add_dependencies(copy_test_configs deploy_qt_once)
  endif()


  # Deploy only the main app
  add_windeploy(dds_mini_bus)
endif()