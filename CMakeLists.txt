cmake_minimum_required(VERSION 3.15)
project(SosnaUDF C CXX)

# ====================================================================
# 1. ГЛОБАЛЬНЫЕ НАСТРОЙКИ СТАНДАРТОВ И ПУТЕЙ
# ====================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Указываем путь к заголовочным файлам кодека G.723.1
include_directories(g723_1)

# Автоматически собираем все файлы кодека ANSI C в подпапке g723_1
file(GLOB ITU_SOURCES "g723_1/*.C")

# ????????? ?????????? ???????? ??? ????????????? ???????
add_library(g723_stubs STATIC "g723_1/GLOBALS2.C")

# Добавляем определение для совместимости с Windows
add_compile_definitions(_single)

# ====================================================================
# 2. ПЕРЕКЛЮЧАТЕЛИ РЕЖИМОВ СБОРКИ И АНАЛИЗА (OPTIONS)
# ====================================================================
option(ENABLE_ASAN "Включить AddressSanitizer для поиска багов памяти" OFF)
option(ENABLE_CPPCHECK "Включить автоматический статический анализ Cppcheck" ON)
option(STATIC_LINK_CRT "Статическая линковка библиотек времени выполнения (CRT)" ON)

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
        )
        set(CMAKE_C_CPPCHECK ${CMAKE_CXX_CPPCHECK})
        message(STATUS "[CPPCHECK] Успешно интегрирован (Платформа: win32A)")
    else()
        message(WARNING "[CPPCHECK] Исполняемый файл не найден в PATH. Анализ отключен.")
    endif()
endif()

# ====================================================================
# 4. НАСТРОЙКА ADDRESSSANITIZER (ASAN) И ФЛАГОВ СБОРКИ
# ====================================================================
if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address -g -O1)
        add_link_options(-fsanitize=address)
        message(STATUS "[ASAN] Активирован для GCC/Clang")
    elseif(MSVC)
        add_compile_options(/fsanitize=address /Zi /O1)
        message(STATUS "[ASAN] Активирован для MSVC")
    endif()
endif()

# Специфические макросы и флаги оптимизации
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Добавляем строгую сборку х32, генерацию SSE2 и макросы платформы
    add_compile_options(-m32 -msse2 -mfpmath=sse -march=i686 -D__X86__ -D__i386__ -Wall)
    add_link_options(-m32)
elseif(MSVC)
    add_compile_options(/D__X86__ /nologo /W3)
endif()

# ====================================================================
# 5. СБОРКА ФАЙЛА РЕСУРСОВ (version.rc)
# ====================================================================
set(RES_SOURCES "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/version.rc")
    set(RES_SOURCES "version.rc")
endif()

# ====================================================================
# 6. ОПРЕДЕЛЕНИЕ ЦЕЛЕЙ СБОРКИ (TARGETS)
# ====================================================================

# Цель 1: Динамическая UDF-библиотека для InterBase
add_library(sosna_udf SHARED 
    sosna_udf.cpp 
    g723_1_decoder.cpp 
    g711u_coder.cpp 
    ${ITU_SOURCES}
    ${RES_SOURCES}
)

# ??????? ????????
target_link_libraries(sosna_udf PRIVATE g723_stubs)

# Возвращаем критически важный флаг --kill-at для удаления декораций @size в MinGW
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(sosna_udf PRIVATE -Wl,--kill-at)
endif()

# Цель 2: Автономная тестовая программа для проверки кодека
add_executable(test_sosna_udf test_transcode_g723.cpp)

# ====================================================================
# 7. УПРАВЛЕНИЕ ОПТИМИЗАЦИЕЙ КОНКРЕТНЫХ ФАЙЛОВ КОДЕКА (ITU_CFLAGS)
# ====================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(source_file ${ITU_SOURCES})
        set_source_files_properties(${source_file} PROPERTIES COMPILE_FLAGS "-O3 -ftree-vectorize")
        set_source_files_properties(${source_file} PROPERTIES LANGUAGE C)
    endforeach()
endif()

# ====================================================================
# 8. УПРАВЛЕНИЕ ЛИНКОВКОЙ БИБЛИОТЕК ВРЕМЕНИ ВЫПОЛНЕНИЯ (CRT)
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
        # Флаги -static-libgcc и -static-libstdc++ для полной автономности файлов
        target_link_options(test_sosna_udf PRIVATE -static -static-libgcc -static-libstdc++)
        target_link_options(sosna_udf PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()
