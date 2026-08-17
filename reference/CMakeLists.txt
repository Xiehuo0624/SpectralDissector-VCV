# Spectral Dissector 26.08.13 golden model（平台无关, 纯 C++, 无外部依赖）
# 另含旧版基线 (legacy/) 与新旧 A/B 对比 (compare_old_new)。
cmake_minimum_required(VERSION 3.16)
project(spectral_dissector_ref LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------- 26.08.13 golden ----------
add_executable(test_golden
    test_golden.cpp
    STFT.cpp
    Cepstrum.cpp
    HpssCore.cpp
    MaskGen.cpp
    SpectralSeparator.cpp
)
target_include_directories(test_golden PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(test_golden PRIVATE -O2 -Wall -Wextra)

# ---------- 旧版基线（namespace sd_legacy, 独立可跑） ----------
add_executable(test_golden_legacy
    legacy/test_golden.cpp
    legacy/STFT.cpp
    legacy/Cepstrum.cpp
    legacy/GenDsp.cpp
    legacy/SpectralSeparator.cpp
)
target_include_directories(test_golden_legacy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/legacy)
target_compile_options(test_golden_legacy PRIVATE -O2 -Wall -Wextra)

# ---------- 新旧 A/B 证据 ----------
add_executable(compare_old_new
    compare_old_new.cpp
    STFT.cpp
    Cepstrum.cpp
    HpssCore.cpp
    MaskGen.cpp
    SpectralSeparator.cpp
    legacy/STFT.cpp
    legacy/Cepstrum.cpp
    legacy/GenDsp.cpp
    legacy/SpectralSeparator.cpp
)
target_include_directories(compare_old_new PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/legacy
)
target_compile_options(compare_old_new PRIVATE -O2 -Wall -Wextra)
