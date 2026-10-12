cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD 23)
project(CityExplorer)

include(FetchContent)

set(fetchTargets)

find_package(Vulkan REQUIRED)

find_package(SDL3 QUIET CONFIG COMPONENTS SDL3-static)
if(NOT SDL3_FOUND)
	set(SDL_TESTS OFF CACHE BOOL "" FORCE)
	set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

	set(SDL_SHARED OFF CACHE BOOL "" FORCE)
	set(SDL_STATIC ON CACHE BOOL "" FORCE)

	FetchContent_Declare(
		SDL3
		GIT_REPOSITORY	https://github.com/libsdl-org/SDL.git
		GIT_TAG			release-3.4.18
	)
	list(APPEND fetchTargets SDL3)
endif()

find_package(glm QUIET CONFIG)
if(NOT glm_FOUND)
	set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

	FetchContent_Declare(
		glm
		GIT_REPOSITORY	https://github.com/g-truc/glm.git
		GIT_TAG			1.0.3
	)
	list(APPEND fetchTargets glm)
endif()

FetchContent_MakeAvailable(${fetchTargets})

# Dear ImGui doesn't have a cmake file, so lets construct a library in place
add_library(DearImGui STATIC
	vendor/DearImGui/imgui.cpp

	vendor/DearImGui/imgui_demo.cpp
	vendor/DearImGui/imgui_draw.cpp
	vendor/DearImGui/imgui_tables.cpp
	vendor/DearImGui/imgui_widgets.cpp

	vendor/DearImGui/backends/imgui_impl_vulkan.cpp
	vendor/DearImGui/backends/imgui_impl_sdl3.cpp

	vendor/DearImGui/misc/cpp/imgui_stdlib.cpp)
target_link_libraries(DearImGui PRIVATE SDL3::SDL3 Vulkan::Vulkan)
target_include_directories(DearImGui PUBLIC vendor/DearImGui vendor/DearImGui/misc/cpp)

add_executable(${PROJECT_NAME} main.cpp)

file(GLOB_RECURSE sources CONFIGURE_DEPENDS "source/**/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE ${sources})

target_include_directories(${PROJECT_NAME} PRIVATE include)

# load stb as include dir, nothing to build with single header lib
target_include_directories(${PROJECT_NAME} PRIVATE vendor/stb)

target_link_libraries(${PROJECT_NAME} PRIVATE
	Vulkan::Vulkan DearImGui SDL3::SDL3 glm::glm)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	$<$<CONFIG:Debug>:BUILD_DEBUG>
	$<$<CONFIG:RelWithDebInfo>:BUILD_DEBINFO>
	$<$<CONFIG:Release>:BUILD_RELEASE>

	$<$<NOT:$<CONFIG:Debug>>:BUILD_NDEBUG>
	$<$<NOT:$<CONFIG:RelWithDebInfo>>:BUILD_NDEBINFO>
	$<$<NOT:$<CONFIG:Release>>:BUILD_NRELEASE>
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
