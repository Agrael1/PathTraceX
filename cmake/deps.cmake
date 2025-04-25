set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}")
set(PROJECT_NAME_STORE ${PROJECT_NAME})

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(CPM_DONT_UPDATE_MODULE_PATH ON)
set(GET_CPM_FILE "${CMAKE_CURRENT_LIST_DIR}/get_cpm.cmake")

if (NOT EXISTS ${GET_CPM_FILE})
  file(DOWNLOAD
      https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
      "${GET_CPM_FILE}"
  )
endif()
include(${GET_CPM_FILE})

# Add CPM dependencies here
# Wisdom
CPMAddPackage(
  NAME Wisdom
  GITHUB_REPOSITORY Agrael1/Wisdom
  GIT_TAG 0.6.8

  OPTIONS
  "WISDOM_BUILD_TESTS OFF"
  "WISDOM_BUILD_EXAMPLES OFF"
  "WISDOM_BUILD_DOCS OFF"
)

# SDL3
CPMAddPackage(
  NAME SDL3
  GITHUB_REPOSITORY libsdl-org/SDL
  GIT_TAG preview-3.1.3
  OPTIONS
  "SDL_WERROR OFF"
)

# DirectXMath
CPMAddPackage(
  NAME DirectXMath
  GITHUB_REPOSITORY microsoft/DirectXMath
  GIT_TAG main
)

# imgui
CPMAddPackage(
  NAME imgui
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v1.91.8
  DOWNLOAD_ONLY TRUE
)
set(IMGUI_SRC ${imgui_SOURCE_DIR})
set(IMGUI_BUILD_SDL3_BINDING ON)
include(imgui)

set(PROJECT_NAME ${PROJECT_NAME_STORE})