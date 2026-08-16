# ====================================================================
# МНОГОМОДУЛЬНЫЙ MAKEFILE ДЛЯ СБОРКИ И ТЕСТИРОВАНИЯ SOSNA_UDF.DLL
# Поддерживаемые платформы: Windows x32 (MinGW-w64 / g++)
# СУБД: InterBase 2009
# ====================================================================

# Название компилятора
CC = g++
GCC = gcc

# Путь к подпапке с декодером ITU-T G.723.1
ITU_DIR = g723_1

# Флаги оптимизации для библиотеки (DLL):
# -m32             : Строгая сборка под 32-битную архитектуру InterBase 2009
# -O3              : Максимальная оптимизация (включает агрессивный инлайнинг)
# -msse2
# -mfpmath=sse     : Расчет тригонометрии float (sinf) через быстрые регистры SSE2
# -ftree-vectorize : Векторизация циклов (SIMD) для ускорения обработки аудио-чанков
# -march=pentium4  : Совместимость со всеми CPU (исключает сбои Illegal Instruction)
# -shared          : Сборка динамической библиотеки (.dll)
# -Wl,--kill-at    : Удаление декораций имен функций @size (критично для InterBase)
# -I$(ITU_DIR)     : Подключает путь к заголовочным файлам кодека
CFLAGS = -m32 -O3 -msse2 -mfpmath=sse -march=pentium4 -ftree-vectorize -Wall -shared -Wl,--kill-at -I$(ITU_DIR)

# Флаги для сборки тестового исполняемого файла
TEST_CFLAGS = -m32 -O2 -Wall -I$(ITU_DIR)

# Сбор всех файлов с расширением .C из подпапки с декодером ITU-T G.723.1
ITU_SOURCES = $(wildcard $(ITU_DIR)/*.C)

# Флаги компиляции старых Си-файлов декодера ITU-T G.723.1 
# -D__X86__ или -D__i386__ : активирует правильную ветку в TYPEDEF2.H для GCC
ITU_CFLAGS = -m32 -O3 -msse2 -mfpmath=sse -march=pentium4 -D__X86__ -I. -I$(ITU_DIR)

# Список Си-объектов
ITU_OBJECTS = $(wildcard $(ITU_DIR)/*.o)

# Изолированные модули кодеков
CODEC_MODULES = g723_1_decoder.cpp g711u_coder.cpp

# Главный мост интеграции с СУБД InterBase
SRC = sosna_udf.cpp
TARGET = sosna_udf.dll

# Файлы тестового стенда
TEST_SRC = test_transcode_g723_to_pcmu.cpp
TEST_TARGET = test_runner.exe

.PHONY: all build test run clean info

# По умолчанию собираем только целевую DLL для СУБД
all: build

## build: Сборка многомодульной 32-битной UDF-библиотеки для InterBase
build: $(TARGET)

# Шаблон правила для компиляции старых Си-файлов в .o объекты
$(ITU_DIR)/%.o: $(ITU_DIR)/%.C
	$(GCC) $(ITU_CFLAGS) -c $< -o $@

$(TARGET): $(SRC) $(CODEC_MODULES) $(ITU_OBJECTS)
	@echo ==============================================================
	@echo  Начинается сборка UDF-библиотеки для InterBase...
	@echo ==============================================================
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(CODEC_MODULES) $(ITU_OBJECTS)
	@echo  
	@echo  [УСПЕХ] Сборка библиотеки завершена!
	@echo  Перенесите файл $(TARGET) в папку установки Interbase\UDF
	@echo ==============================================================

## test: Сборка автономного тестового стенда под типы данных Annex B
test: $(TARGET) $(TEST_SRC)
	@echo ====================================================
	@echo  Сборка автономного тестового стенда...
	@echo ====================================================
	$(CC) $(TEST_CFLAGS) -o $(TEST_TARGET) $(TEST_SRC)
	@echo  [ОК] Тестовый стенд собран: $(TEST_TARGET)
	@echo ====================================================

## run: Автоматическая сборка, создание фейковых аудио-кадров и запуск тестов без крашей
run: test
	@echo ====================================================
	@echo  Запуск симуляции транскодирования...
	@echo ====================================================
	@./$(TEST_TARGET)

## clean: Полная очистка всех артефактов компиляции, логов и сгенерированных аудио-файлов
clean:
	@echo  Очистка временных файлов и бинарников...
	@if exist $(TARGET) (del /f /q $(TARGET) && echo  Конечный файл $(TARGET) удален.)
	@if exist $(TEST_TARGET) (del /f /q $(TEST_TARGET) && echo  Конечный файл $(TEST_TARGET) удален.)
	@if exist $(ITU_DIR)\*.o del /f /q $(ITU_DIR)\*.o && echo  Си-объекты декодера ITU-T удалены.)
	@if exist test_input.g723 (del /f /q test_input.g723 && echo  Тестовый входной файл удален.)
	@if exist test_output.pcmu (del /f /q test_output.pcmu && echo  Тестовый выходной файл удален.)
	@echo  [ОК] Очистка успешно завершена.

## info: Вывод текущих флагов компиляции в консоль для проверки
info:
	@echo Текущие флаги сборки:
	@echo  CFLAGS:      $(CFLAGS)
	@echo  TEST_CFLAGS: $(TEST_CFLAGS)
	@echo  ITU_CFLAGS:  $(ITU_CFLAGS)