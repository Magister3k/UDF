# Sosna UDF

UDF-библиотека для InterBase 2009 (Win32), преобразующая G.723.1 в G.711 mu-law (PCMU).

Используется референсная реализация ITU-T G.723.1 Floating Point Release 2. Поддерживаются кадры 6.3 и 5.3 кбит/с, SID и маркеры потери кадров.

## Возможности

- потоковая обработка BLOB без загрузки всего файла в память;
- поддержка файлов больше 200 КБ;
- запись результата сегментами непосредственно в выходной BLOB InterBase;
- сериализованный доступ к глобальному состоянию декодера;
- входной буфер содержит один сегмент и незавершенный кадр;
- выходной буфер не превышает 32767 байт;
- Win32 ABI совместим с InterBase 2009.

## Структура проекта

| Файл | Назначение |
| --- | --- |
| `sosna_udf.cpp` | UDF-обертка, BLOB ABI и потоковая обработка |
| `g723_1_decoder.cpp` | C-совместимый адаптер декодера |
| `g723_decoder.cpp` / `g723_decoder.hpp` | C++-обертка G.723.1 |
| `g723_1_mod/` | Референсное ядро G.723.1 |
| `g711u_coder.cpp` | Кодирование PCM в PCMU |
| `install/init_sosna_udf.sql` | Регистрация UDF и процедуры |
| `test_transcode_g723.cpp` | Автономный тест DLL |

## Регистрация в InterBase

В InterBase регистрируется двухаргументный экспорт `transcode_g723`:

```sql
DECLARE EXTERNAL FUNCTION UDF_TRANSCODE_G723
    BLOB,
    BLOB
    RETURNS PARAMETER 2
    ENTRY_POINT 'transcode_g723'
    MODULE_NAME 'sosna_udf';
```

Используйте `install/init_sosna_udf.sql`. Пример преобразования записи:

```sql
UPDATE speech
SET rec = UDF_TRANSCODE_G723(
    (SELECT rec FROM speech WHERE id = 0)
)
WHERE id = 1;
```

Для массовой обработки используйте процедуру `TRANSCODE_G723` из установочного скрипта.

Важно: `transcode_g723_ib_util` имеет четыре параметра и не должен регистрироваться как двухаргументная SQL-функция. Он оставлен только для специальных прямых тестов.

## Установка

1. Соберите DLL в конфигурации Win32.
2. Остановите InterBase Server, если он загрузил предыдущую версию DLL.
3. Скопируйте `sosna_udf.dll` в каталог UDF, например `C:\InterBase\UDF\`.
4. Запустите InterBase Server.
5. Выполните `install/init_sosna_udf.sql` для нужной базы.

InterBase удерживает DLL загруженной до остановки сервера. Поэтому при пересборке DLL сервер и автономный тест должны быть закрыты.

## Сборка MSVC x86

Проект рассчитан на 32-битную сборку из-за Win32 ABI InterBase 2009.

```bat
call "c:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -B build_msvc .
cmake --build build_msvc --config Release
```

Артефакты:

- `build_msvc\sosna_udf.dll` — UDF-библиотека;
- `build_msvc\test_sosna_udf.exe` — автономный тест.

DLL собирается с динамичесным CRT `/MD`. Автономный тест при `STATIC_LINK_CRT=ON` может использовать статический CRT.

## Автономный тест

Положите файлы `*.g723` рядом с `test_sosna_udf.exe` и запустите:

```bat
test_sosna_udf.exe
```

Тест проверяет транскодирование файлов, создание PCMU-файлов, пустой BLOB, неполный кадр, SID и маркеры потери кадров.

## Ограничения и безопасность

- входные данные читаются только через callback InterBase;
- длина каждого сегмента проверяется до использования;
- незавершенный кадр переносится в начало небольшого входного буфера;
- исключения C++ не пересекают границу UDF ABI;
- глобальное состояние декодера защищено критической секцией;
- память выходного BLOB управляется InterBase через `blob_put_segment`;
- `ib_util_malloc` для основной функции не используется.

## Проверки качества

Проект поддерживает CMake-сборку, Cppcheck и AddressSanitizer для совместимых компиляторов. Рабочий production-вариант для InterBase 2009 — MSVC Win32 Release.

## Лицензия

Референсный код ITU-T G.723.1: Intel Corporation, France Telecom, AudioCodes, DSP Group, Universite de Sherbrooke, 1995-2006.

Обертка UDF: PineCode Lab, 2026.