# Benchmark spike: can NAM inference fit MilodikFX's 0.33 ms block budget?
#
# Builds NeuralAmpModelerCore (pointed at by -DNAM_DIR, a clone that lives
# outside version control) into a single benchmark executable, with the same
# flags the real integration would use: MSVC, /MT static runtime, /arch:AVX2,
# NAM_SAMPLE_FLOAT. The numbers this produces are the go/no-go for P2-1;
# nothing here ships.
cmake_minimum_required(VERSION 3.25)

project(nambench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED NAM_DIR)
  message(FATAL_ERROR "Pass -DNAM_DIR=<path to a NeuralAmpModelerCore clone>")
endif()

# Same runtime as the shipping exe, so the measurement is of the build we
# would actually ship, not a friendlier one.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

file(GLOB NAM_SOURCES
  "${NAM_DIR}/NAM/*.cpp"
  "${NAM_DIR}/NAM/*/*.cpp"
)

add_executable(nambench main.cpp ${NAM_SOURCES})

target_include_directories(nambench PRIVATE
  "${NAM_DIR}"
  "${NAM_DIR}/Dependencies/eigen"
  "${NAM_DIR}/Dependencies/nlohmann"
)

target_compile_definitions(nambench PRIVATE
  # float I/O so JUCE buffers could be handed over without conversion;
  # must be identical across every NAM translation unit (ODR).
  NAM_SAMPLE_FLOAT
  NOMINMAX
  WIN32_LEAN_AND_MEAN
  _CRT_SECURE_NO_WARNINGS
  # Only MPL2-licensed parts of Eigen, matching the AGPLv3 host's licence audit.
  EIGEN_MPL2_ONLY
)

if(MSVC)
  # AVX2 is what makes Eigen's kernels vectorise; without it the README's own
  # guidance says performance drops severalfold. The real integration would
  # scope this flag to the NAM static library, not the whole app.
  target_compile_options(nambench PRIVATE /O2 /arch:AVX2 /fp:fast)
endif()
