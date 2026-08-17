# Spectral Dissector 参考实现 (平台无关, golden model)
# 无任何外部依赖, 仅需 C++17。
cmake_minimum_required(VERSION 3.16)
project(spectral_dissector_ref LANGUAGES CXX)

add_executable(test_golden
    test_golden.cpp
    STFT.cpp
    Cepstrum.cpp
    GenDsp.cpp
    SpectralSeparator.cpp
)
target_include_directories(test_golden PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_golden PRIVATE cxx_std_17)
target_compile_options(test_golden PRIVATE -O2 -Wall -Wextra)
