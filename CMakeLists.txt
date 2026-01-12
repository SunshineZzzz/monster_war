# ============================================
# 项目主配置文件
# ============================================
cmake_minimum_required(VERSION 3.10.0)
project(MonsterWar VERSION 0.1.0 LANGUAGES C CXX)

# ============================================
# 基础配置
# ============================================

# 指定搜索路径（用于find_package在编译期查找库）
set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/prebuilt)

# 指定目标名称
set(TARGET ${PROJECT_NAME}-${CMAKE_SYSTEM_NAME})

# 依赖库默认链接类型：ON = 动态链接(.dll/.so/.dylib)，OFF = 静态链接(.lib/.a)
# 注意：可以在Dependencies.cmake中为每个库单独指定
option(BUILD_SHARED_LIBS "依赖库默认编译为动态库" OFF)

# ============================================
# 引入模块化配置
# ============================================

# 编译器设置（C++标准、编译选项等）
include(cmake/CompilerSettings.cmake)

# 运行时路径配置（RPATH）
include(cmake/RuntimePath.cmake)

# 依赖管理
include(cmake/Dependencies.cmake)

# ImGui配置
include(cmake/ImGui.cmake)

# 构建辅助函数
include(cmake/BuildHelpers.cmake)

# 项目信息打印
include(cmake/ProjectInfo.cmake)

# ============================================
# 设置项目依赖
# ============================================

# 调用依赖配置函数（定义在Dependencies.cmake中）
setup_project_dependencies()

# ============================================
# 项目源文件
# ============================================

# 使用 CONFIGURE_DEPENDS 自动感应文件增删
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.h" "${CMAKE_SOURCE_DIR}/src/*.hpp")

source_group(TREE "${CMAKE_SOURCE_DIR}/src" PREFIX "src" FILES ${SOURCES} ${HEADERS})

if(IMGUI_SOURCES)
    source_group("3rd/ImGui" FILES ${IMGUI_SOURCES})
endif()

# Windows平台添加资源文件
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/resources.rc")
    list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/resources.rc")
    source_group("res" FILES "${CMAKE_SOURCE_DIR}/resources.rc")
endif()

# ============================================
# 可执行文件配置
# ============================================

# 创建可执行文件
add_executable(${TARGET} ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})

# 设置包含路径，让包含可以从 src/ 开始
target_include_directories(${TARGET} PRIVATE src)

# 链接所有依赖库
target_link_libraries(${TARGET}
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_mixer::SDL3_mixer
    SDL3_ttf::SDL3_ttf
    glm::glm
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    EnTT::EnTT
)

# ============================================
# 应用配置
# ============================================

# 设置编译选项（定义在CompilerSettings.cmake中）
setup_compiler_options(${TARGET})

# 配置资源文件复制（定义在BuildHelpers.cmake中）
setup_asset_copy(${TARGET})

# 配置Windows DLL复制（定义在BuildHelpers.cmake中）
setup_windows_dll_copy(${TARGET})

# ============================================
# 打印配置信息
# ============================================

# 打印项目配置完成信息（定义在ProjectInfo.cmake中）
print_project_info(${TARGET})
