cmake_minimum_required(VERSION 3.15)
project(SosnaUDF LANGUAGES C CXX)

# ====================================================================
# 1. ГЛОБАЛЬНЫЕ НАСТРОЙКИ СТАНДАРТОВ И ПУТЕЙ
# ====================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Указываем путь к заголовочным файлам кодека G.723.1
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/g723_1)

# Автоматически собираем все файлы кодека ANSI C в подпапке g723_1
file(GLOB ITU_SOURCES_ALL "${CMAKE_CURRENT_SOURCE_DIR}/g723_1/*.C")

# Исключаем GLOBALS2.C из ITU_SOURCES, так как он вынесен в отдельную библиотеку g723_stubs
list(FILTER ITU_SOURCES_ALL EXCLUDE REGEX ".*GLOBALS2\\.C$")
set(ITU_SOURCES ${ITU_SOURCES_ALL})

# ====================================================================
# 2. ПЕРЕКЛЮЧАТЕЛИ РЕЖИМОВ СБОРКИ И АНАЛИЗА (OPTIONS)
# ====================================================================
option(ENABLE_ASAN "Включить AddressSanitizer для поиска багов памяти" OFF)
option(ENABLE_CPPCHECK "Включить автоматический статический анализ Cppcheck" ON)
option(STATIC_LINK_CRT "Статическая линковка библиотек времени выполнения (CRT)" ON)
option(BUILD_SHARED_LIBS "Сборка динамической библиотеки (ON) или статической (OFF)" ON)

# ====================================================================
# 3. ИНТЕГРАЦИЯ СТАТИЧЕСКОГО АНАЛИЗАТОРА (CPPCHECK)
# ====================================================================
if(ENABLE_CPPCHECK)
    find_program(CPPCHECK_PROGRAM cppcheck)
    if(CPPCHECK_PROGRAM)
        set(CMAKE_CXX_CPPCHECK
            "${CPPCHECK_PROGRAM}"
            "--platform=win32A"
            "--enable=warning,style,performance,portability"
            "--inline-suppr"
            "--quiet"
            "--suppress=missingIncludeSystem"
        )
        set(CMAKE_C_CPPCHECK ${CMAKE_CXX_CPPCHECK})
        message(STATUS "[CPPCHECK] Успешно интегрирован (Платформа: win32A)")
    else()
        message(WARNING "[CPPCHECK] Исполняемый файл не найден в PATH. Анализ отключен.")
    endif()
endif()

# ====================================================================
# 4. НАСТРОЙКА ПЛАТФОРМЫ (СТРОГО Win32)
# ====================================================================
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING "Обнаружена 64-битная сборка. Принудительно переключаем на 32-битную (Win32).")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-m32 -msse2 -mfpmath=sse -march=i686 -D__X86__ -D__i386__ -Wall -Wextra -Wpedantic)
    add_link_options(-m32)
elseif(MSVC)
    add_compile_options(/D__X86__ /nologo /W3 /arch:SSE2)
endif()

# ====================================================================
# 5. КОНФИГУРАЦИИ СБОРКИ (DEBUG/RELEASE)
# ====================================================================
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Конфигурации сборки" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1" CACHE STRING "Флаги для Debug (MSVC)" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG" CACHE STRING "Флаги для Release (MSVC)" FORCE)
set(CMAKE_C_FLAGS_DEBUG "/MDd /Zi /Ob0 /Od /RTC1" CACHE STRING "Флаги для Debug (MSVC)" FORCE)
set(CMAKE_C_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG" CACHE STRING "Флаги для Release (MSVC)" FORCE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer" CACHE STRING "Флаги для Debug (GCC/Clang)" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ftree-vectorize -fomit-frame-pointer" CACHE STRING "Флаги для Release (GCC/Clang)" FORCE)
    set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer" CACHE STRING "Флаги для Debug (GCC/Clang)" FORCE)
    set(CMAKE_C_FLAGS_RELEASE "-O3 -ftree-vectorize -fomit-frame-pointer" CACHE STRING "Флаги для Release (GCC/Clang)" FORCE)
endif()

# ====================================================================
# 6. НАСТРОЙКА ADDRESSSANITIZER (ASAN)
# ====================================================================
if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address -g -O1 -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
        message(STATUS "[ASAN] Активирован для GCC/Clang")
    elseif(MSVC)
        add_compile_options(/fsanitize=address /Zi /O1 /MDd)
        message(STATUS "[ASAN] Активирован для MSVC")
    endif()
endif()

# ====================================================================
# 7. ОПРЕДЕЛЕНИЯ ПРЕПРОЦЕССОРА
# ====================================================================
add_compile_definitions(_single _CRT_SECURE_NO_WARNINGS)

# ====================================================================
# 8. СБОРКА ФАЙЛА РЕСУРСОВ (version.rc)
# ====================================================================
set(RES_SOURCES "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/version.rc")
    set(RES_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/version.rc")
endif()

# ====================================================================
# 9. ЦЕЛЬ: ITU-T G.723.1 КОДЕК (СТАТИЧЕСКАЯ БИБЛИОТЕКА)
# ====================================================================
# Принудительно компилируем как C (файлы имеют расширение .C, которое CMake воспринимает как C++)
add_library(g723_codec STATIC ${ITU_SOURCES})
target_include_directories(g723_codec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/g723_1)
target_compile_definitions(g723_codec PRIVATE _single __unix__)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(g723_codec PRIVATE -x c -O3 -ftree-vectorize)
elseif(MSVC)
    target_compile_options(g723_codec PRIVATE /TC /O2 /arch:SSE2)
endif()

# ====================================================================
# 10. ЦЕЛЬ: СТУБЫ ДЛЯ ГЛОБАЛЬНЫХ ПЕРЕМЕННЫХ ITU-T
# ====================================================================
add_library(g723_stubs STATIC "${CMAKE_CURRENT_SOURCE_DIR}/g723_1/GLOBALS2.C")
target_include_directories(g723_stubs PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/g723_1")
target_compile_definitions(g723_stubs PRIVATE _single __unix__)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(g723_stubs PRIVATE -x c)
elseif(MSVC)
    target_compile_options(g723_stubs PRIVATE /TC)
endif()

# ====================================================================
# 11. ЦЕЛЬ 1: ДИНАМИЧЕСКАЯ UDF-БИБЛИОТЕКА ДЛЯ INTERBASE
# ====================================================================
add_library(sosna_udf SHARED
    sosna_udf.cpp
    g723_1_decoder.cpp
    g711u_coder.cpp
    ${RES_SOURCES}
)

target_link_libraries(sosna_udf PRIVATE g723_codec g723_stubs)
target_include_directories(sosna_udf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Флаг --kill-at для удаления декораций @size в MinGW
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(sosna_udf PRIVATE -Wl,--kill-at)
endif()

# Выходное имя DLL
set_target_properties(sosna_udf PROPERTIES
    OUTPUT_NAME "sosna_udf"
    PREFIX ""
    SUFFIX ".dll"
    VERSION 1.0.0
    SOVERSION 1
)

# ====================================================================
# 12. ЦЕЛЬ 2: АВТОНОМНАЯ ТЕСТОВАЯ ПРОГРАММА
# ====================================================================
add_executable(test_sosna_udf test_transcode_g723.cpp)
target_link_libraries(test_sosna_udf PRIVATE sosna_udf)

# ====================================================================
# 13. УПРАВЛЕНИЕ ЛИНКОВКОЙ CRT
# ====================================================================
if(MSVC)
    if(STATIC_LINK_CRT)
        set_property(TARGET test_sosna_udf PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET sosna_udf PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
        message(STATUS "[CRT LINK] Настроено: EXE -> /MT (Автономный), DLL -> /MD (Для InterBase)")
    else()
        set_property(TARGET test_sosna_udf PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET sosna_udf PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
    endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(STATIC_LINK_CRT)
        target_link_options(test_sosna_udf PRIVATE -static -static-libgcc -static-libstdc++)
        target_link_options(sosna_udf PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

# ====================================================================
# 14. УСТАНОВКА И ВЫВОД ИНФОРМАЦИИ
# ====================================================================
install(TARGETS sosna_udf
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

message(STATUS "==========================================")
message(STATUS "Sosna UDF Build Configuration")
message(STATUS "==========================================")
message(STATUS "Platform: Win32 (forced)")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "C Standard: ${CMAKE_C_STANDARD}")
message(STATUS "Configurations: ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "ASAN: ${ENABLE_ASAN}")
message(STATUS "Cppcheck: ${ENABLE_CPPCHECK}")
message(STATUS "Static CRT: ${STATIC_LINK_CRT}")
message(STATUS "Shared Libs: ${BUILD_SHARED_LIBS}")
message(STATUS "ITU Sources: ${ITU_SOURCES}")
message(STATUS "==========================================")