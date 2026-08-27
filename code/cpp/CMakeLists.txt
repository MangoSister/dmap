cmake_minimum_required(VERSION 3.23)

project(dmap LANGUAGES CUDA CXX C)
include(CheckLanguage)
check_language(CUDA)

set(CMAKE_MODULE_PATH  
"${CMAKE_CURRENT_SOURCE_DIR}/cmake"
"${OptiX_INSTALL_DIR}/SDK/CMake" 
${CMAKE_MODULE_PATH})
message("CMAKE_MODULE_PATH = " "${CMAKE_MODULE_PATH}")

find_package(Eigen3 3.3 REQUIRED NO_MODULE)
find_package(TBB CONFIG REQUIRED)
find_package(embree 3.0 REQUIRED)
find_package(lz4 CONFIG REQUIRED)

find_package(Vulkan)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(VulkanUtilityLibraries CONFIG REQUIRED)
find_package(Freetype REQUIRED)
# Still need this for driver API...
find_package(CUDAToolkit)

find_package(slang REQUIRED PATHS src/ks/deps/slang/cmake/ NO_DEFAULT_PATH)
find_package(tinyply CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(npy CONFIG REQUIRED)

##############################################################################

set(COMMON_INCLUDE_DIRS
  src/ks/deps/cxxopts/include
  src/ks/deps/stb
  src/ks/deps/tinyexr
  src/ks/deps/tinyobjloader
  src/ks/deps/tinygltf
  src/ks/deps/tomlplusplus
  src/ks/deps/spdlog/include
  src/ks/deps/imgui/
  src/ks/deps/
  deps/
)

set(KS_SHADER_DIR ${CMAKE_SOURCE_DIR}/src/ks/gpu/shaders)
set(DMAP_SHADER_DIR ${CMAKE_SOURCE_DIR}/src/shaders)

set(COMMON_COMPILE_DEFS
  -DVK_NO_PROTOTYPES
  -DPPK_ASSERT_ENABLED=1
  -DDATA_DIR=\"${CMAKE_SOURCE_DIR}/data/\"
  -DKS_SHADER_DIR=\"${KS_SHADER_DIR}\"
  -DSHADER_DIR=\"${DMAP_SHADER_DIR}\"
)

set(CPP_LIBS
  Eigen3::Eigen
  TBB::tbb 
  TBB::tbbmalloc # used by parallel kd tree
  embree
  lz4::lz4
  Vulkan::Vulkan
  GPUOpen::VulkanMemoryAllocator
  glfw
  volk::volk_headers
  Vulkan::UtilityHeaders
  Freetype::Freetype
  tinyply
  slang::slang
  nlohmann_json::nlohmann_json
  npy::npy
)

set(CUDA_LIBS
  CUDA::cuda_driver # Still need to explicitly link to driver API...
)

set(KS_SRC
  src/ks/assertion.h
  src/ks/assertion.cpp
  src/ks/maths.h
  src/ks/aabb.h
  src/ks/barray.h
  src/ks/bsdf.h
  src/ks/bsdf.cpp
  src/ks/principled_bsdf.h
  src/ks/principled_bsdf.cpp
  src/ks/vmf.h
  src/ks/vmf.cpp
  src/ks/camera.h
  src/ks/camera.cpp
  src/ks/config.h
  src/ks/config.cpp
  src/ks/distrib.h
  src/ks/distrib.cpp
  src/ks/embree_util.h
  src/ks/embree_util.cpp
  src/ks/file_util.h
  src/ks/geometry.h
  src/ks/geometry.cpp
  src/ks/hash.h
  src/ks/keyframe.h
  src/ks/keyframe.cpp
  src/ks/light.h
  src/ks/light.cpp
  src/ks/log_util.h
  src/ks/log_util.cpp
  src/ks/memory_util.h
  src/ks/memory_util.cpp
  src/ks/material.h
  src/ks/material.cpp
  src/ks/mesh_asset.h
  src/ks/mesh_asset.cpp
  src/ks/nee.h
  src/ks/nee.cpp
  src/ks/normal_map.h
  src/ks/normal_map.cpp
  src/ks/opacity_map.h
  src/ks/opacity_map.cpp
  src/ks/parallel.h
  src/ks/parallel.cpp
  src/ks/render_target.h
  src/ks/render_target.cpp
  src/ks/tonemap.h
  src/ks/tonemap.cpp
  src/ks/scene.h
  src/ks/scene.cpp
  src/ks/subsurface.h
  src/ks/subsurface.cpp
  src/ks/texture.h
  src/ks/texture.cpp
  src/ks/ray.h
  src/ks/ray.cpp
  src/ks/rng.h
  src/ks/image_util.h
  src/ks/image_util.cpp
  src/ks/sobol.h
  src/ks/sobol.cpp
  src/ks/sat.h
  src/ks/small_pt.h
  src/ks/small_pt.cpp
  src/ks/colormap.h
  src/ks/colormap.cpp
  #
  src/ks/gpu/ksvk.h
  src/ks/gpu/ksvk.cpp
  src/ks/gpu/light.h
  src/ks/gpu/light.cpp
  src/ks/gpu/vk_cuda_interop.h
  src/ks/gpu/vk_cuda_interop.cpp
  src/ks/cuda/api_error.h
  src/ks/cuda/basic.cuh
  src/ks/cuda/vecmath.cuh
  src/ks/cuda/vecmath.cu
  src/ks/cuda/aabb.cuh
  # src/ks/cuda/cuda_parallel_kd_tree.h
  # src/ks/cuda/cuda_parallel_kd_tree.cu
  # src/ks/cuda/cuda_parallel_kd_tree.cpp
  # src/ks/cuda/cuda_parallel_kd_tree_test.cpp
  src/ks/cuda/memory_low_level.h
  src/ks/cuda/memory_low_level.cpp
)

set(IMGUI_SRC
  src/ks/deps/imgui/imconfig.h
  src/ks/deps/imgui/imgui.h
  src/ks/deps/imgui/imgui.cpp
  src/ks/deps/imgui/imgui_demo.cpp
  src/ks/deps/imgui/imgui_draw.cpp
  src/ks/deps/imgui/imgui_internal.h
  src/ks/deps/imgui/imgui_tables.cpp
  src/ks/deps/imgui/imgui_widgets.cpp
  src/ks/deps/imgui/backends/imgui_impl_glfw.h
  src/ks/deps/imgui/backends/imgui_impl_glfw.cpp
  src/ks/deps/imgui/backends/imgui_impl_vulkan.h
  src/ks/deps/imgui/backends/imgui_impl_vulkan.cpp
  src/ks/deps/imgui/misc/freetype/imgui_freetype.h
  src/ks/deps/imgui/misc/freetype/imgui_freetype.cpp
)

set(DMAP_SRC
  src/main.cpp  
  src/test_weighted_area_sampling.cpp
)

#-----------------------------------------------------------------------------
# DMAP main target
#-----------------------------------------------------------------------------

add_executable(dmap)
target_sources(dmap PRIVATE ${KS_SRC})
target_sources(dmap PRIVATE ${DMAP_SRC})
target_sources(dmap PRIVATE ${IMGUI_SRC})

target_compile_features(dmap PRIVATE cxx_std_20)
target_include_directories(dmap PRIVATE ${COMMON_INCLUDE_DIRS})

# NOTE: for CUDA debugger in debug. This will affect performance for debug build.
# target_compile_options(dmap PRIVATE "$<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CUDA>>:-G>")
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(dmap PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/MP /bigobj> )
    # I don't know if this is the best fix...
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:libcmt.lib")
endif()
target_compile_options(dmap PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${COMMON_COMPILE_DEFS}> )
# Allow device lambdas
target_compile_options(dmap PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:--extended-lambda>)

target_link_libraries(dmap PRIVATE ${CPP_LIBS} ${CUDA_LIBS})
set_target_properties(dmap PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Windows: manually copy dlls to executable folder
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  # slang
  add_custom_command(TARGET dmap POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/src/ks/deps/slang/bin/slang.dll $<TARGET_FILE_DIR:dmap>
  )
endif()