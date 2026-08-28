cmake_minimum_required(VERSION 3.16)

project(tux-dock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0
)

FetchContent_MakeAvailable(ftxui)

include(FetchContent)

FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

add_executable(tux-dock
  main.cpp
  src/process_runner.cpp
  src/docker_engine_client.cpp
  src/http_response_parser.cpp
  src/container_parser.cpp
  src/operation_state.cpp
  src/stop_waiter.cpp
  src/docker_manager.cpp
)

target_link_libraries(tux-dock
  PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
    nlohmann_json::nlohmann_json
)

target_compile_options(tux-dock PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
  add_executable(tux-dock-http-tests
    tests/test_http_response_parser.cpp
    src/http_response_parser.cpp
  )
  target_include_directories(tux-dock-http-tests PRIVATE src)
  target_compile_options(tux-dock-http-tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME tux-dock-http-tests COMMAND tux-dock-http-tests)

  add_executable(tux-dock-container-tests
    tests/test_container_parser.cpp
    src/container_parser.cpp
  )
  target_include_directories(tux-dock-container-tests PRIVATE src)
  target_link_libraries(tux-dock-container-tests PRIVATE nlohmann_json::nlohmann_json)
  target_compile_options(tux-dock-container-tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME tux-dock-container-tests COMMAND tux-dock-container-tests)

  add_executable(tux-dock-operation-tests
    tests/test_operation_state.cpp
    src/operation_state.cpp
  )
  target_include_directories(tux-dock-operation-tests PRIVATE src)
  target_compile_options(tux-dock-operation-tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME tux-dock-operation-tests COMMAND tux-dock-operation-tests)

  add_executable(tux-dock-stop-tests
    tests/test_stop_waiter.cpp
    src/stop_waiter.cpp
  )
  target_include_directories(tux-dock-stop-tests PRIVATE src)
  target_compile_options(tux-dock-stop-tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME tux-dock-stop-tests COMMAND tux-dock-stop-tests)

  add_executable(tux-dock-stop-sequence-tests
    tests/test_stop_sequence.cpp
    src/stop_waiter.cpp
  )
  target_include_directories(tux-dock-stop-sequence-tests PRIVATE src)
  target_compile_options(tux-dock-stop-sequence-tests PRIVATE -Wall -Wextra -Wpedantic)
  add_test(NAME tux-dock-stop-sequence-tests COMMAND tux-dock-stop-sequence-tests)

  add_test(NAME tux-dock-docker-integration-tests
           COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_docker_integration.sh)

  add_test(NAME tuxreaperd-zombie-tests
           COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tuxreaperd.sh
                   ${CMAKE_CURRENT_BINARY_DIR}/tuxreaperd
                   ${CMAKE_CURRENT_SOURCE_DIR}/tuxreaperdgnu.c)
endif()
