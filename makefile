# ====================================================================
# МНОГОМОДУЛЬНЫЙ MAKEFILE ДЛЯ СБОРКИ И ТЕСТИРОВАНИЯ SOSNA_UDF.DLL
# Поддерживаемые платформы: Windows x32 (MinGW-w64 / g++)
# СУБД: InterBase 2009
# ====================================================================

# Название компилятора
# Принудительно используем MinGW GCC/G++ для x32, чтобы не было переопределения
# через переменные среды (например, CC=cc в Windows shell)
override CC := g++
override GCC := gcc

# Путь к подпапке с конечными файлами
TARGET_DIR = release

# Путь к подпапке с декодером ITU-T G.723.1
ITU_DIR = g723_1

# Флаги оптимизации для библиотеки (DLL):
# -m32                : Строгая сборка под 32-битную архитектуру
# -O3                 : Максимальная оптимизация (включает агрессивный инлайнинг)
# -msse2 -mfpmath=sse : Расчет тригонометрии float (sinf) через быстрые регистры SSE2
# -ftree-vectorize    : Векторизация циклов (SIMD) для ускорения обработки аудио-чанков
# -march=i686         : Совместимость со всеми x32 CPU i686+
# -shared             : Сборка динамической библиотеки (.dll)
# -Wl,--kill-at       : Удаление декораций имен функций @size (критично для InterBase)
# -I$(ITU_DIR)        : Подключает путь к заголовочным файлам кодека
CFLAGS = -m32 -O3 -msse2 -mfpmath=sse -march=i686 -ftree-vectorize -Wall -shared -Wl,--kill-at -I$(ITU_DIR)

# Флаги для сборки тестового исполняемого файла
TEST_CFLAGS = -m32 -O2 -Wall -I$(ITU_DIR)

# Сбор всех файлов с расширением .C из подпапки с декодером ITU-T G.723.1
ITU_SOURCES = $(wildcard $(ITU_DIR)/*.C)

# Флаги компиляции старых Си-файлов декодера ITU-T G.723.1 
# -D__X86__ или -D__i386__ : Активирует правильную ветку в TYPEDEF2.H для GCC
# -xc                      : Явно указывает, что это C код, а не C++
ITU_CFLAGS = -x c -m32 -O3 -msse2 -mfpmath=sse -march=i686 -D__X86__ -D__i386__ -I. -I$(ITU_DIR)

# Список Си-объектов (переводим .C файлы в .o объекты)
ITU_OBJECTS = $(patsubst $(ITU_DIR)/%.C,$(ITU_DIR)/%.o,$(ITU_SOURCES))

# Изолированные модули кодеков
CODEC_MODULES = g723_1_decoder.cpp g711u_coder.cpp

# Главный мост интеграции с СУБД InterBase
SRC = sosna_udf.cpp
TARGET = $(TARGET_DIR)/sosna_udf.dll
RES = version.res

# Файлы тестового стенда
TEST_SRC = test_transcode_g723_to_pcmu.cpp
TEST_TARGET = $(TARGET_DIR)/test_runner.exe

.PHONY: all build test clean info

# По умолчанию собираем только UDF-библиотеку
all: build

## Цель build - сборка многомодульной 32-битной UDF-библиотеки для InterBase
build: create_target_dir $(TARGET)

create_target_dir:
	@if not exist $(TARGET_DIR) mkdir $(TARGET_DIR)

# Шаблон правила для компиляции старых Си-файлов в .o объекты
# Использование g++ (C++) компилятора для совместимости с основным кодом
$(ITU_DIR)/%.o: $(ITU_DIR)/%.C
	$(CC) $(ITU_CFLAGS) -c $< -o $@

version.res: version.rc
	windres version.rc -O coff -o version.res

$(TARGET): $(SRC) $(CODEC_MODULES) $(ITU_OBJECTS) $(RES)
	@echo ========================================================================
	@echo  Сборка UDF-библиотеки для InterBase...
	@echo ========================================================================
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(CODEC_MODULES) $(ITU_OBJECTS) $(RES)
	@if exist version.res (del /f /q version.res)
	@if exist $(ITU_DIR)\*.o (del /f /q $(ITU_DIR)\*.o)
	@echo ========================================================================
	@echo  [УСПЕХ] Сборка библиотеки завершена!
	@echo  Перенесите файл $(TARGET) в папку установки Interbase\UDF
	@echo ========================================================================

## Цель test - сборка автономной тестовой программы для проверки функции транскодирования
test: create_target_dir $(TARGET) $(TEST_SRC)
	@echo ========================================================================
	@echo  Сборка автономной тестовой программы...
	@echo ========================================================================
	$(CC) $(TEST_CFLAGS) -o $(TEST_TARGET) $(TEST_SRC)
	@echo ========================================================================
	@echo  [УСПЕХ] Тестовая программа собрана!
	@echo  Запустите файл $(TEST_TARGET) для начала тестирования
	@echo ========================================================================

## Цель clean - очистка папки проекта от всех артефактов предыдущей компиляции и конечных файлов
clean:
	@echo ========================================================================
	@echo  Очистка папки проекта от конечных и временных файлов...
	@echo ========================================================================
	@if exist $(TARGET_DIR)\*.* (del /f /q $(TARGET_DIR)\*.* && echo  [ОК] Конечные файлы удалены)
	@if exist $(ITU_DIR)\*.o (del /f /q $(ITU_DIR)\*.o && echo  [ОК] Си-объекты декодера ITU-T удалены)
	@if exist version.res (del /f /q version.res && echo  [ОК] Скомпилированный файл ресурса удален)
	@if exist $(TARGET) (del /f /q $(TARGET) && echo  [ОК] Конечный файл $(TARGET) удален)
	@if exist $(TEST_TARGET) (del /f /q $(TEST_TARGET) && echo  [ОК] Конечный файл $(TEST_TARGET) удален)
	@if exist test_input.g723 (del /f /q test_input.g723 && echo  [ОК] Тестовый входной файл удален)
	@if exist test_output.pcmu (del /f /q test_output.pcmu && echo  [ОК] Тестовый выходной файл удален)
	@echo  [УСПЕХ] Очистка завершена!
	@echo ========================================================================

## Цель info - вывод текущих флагов компиляции в консоль
info:
	@echo Текущие флаги сборки:
	@echo  CFLAGS:      $(CFLAGS)
	@echo  TEST_CFLAGS: $(TEST_CFLAGS)
	@echo  ITU_CFLAGS:  $(ITU_CFLAGS)