# Sosna UDF — G.723.1 to PCMU Transcoder

UDF-библиотека для InterBase 2009 (Win32), выполняющая транскодирование аудиоданных из кодека **G.723.1** (Annex B, 6.3/5.3 kbps) в **G.711 μ-law (PCMU)**.

Использует референсный код **ITU-T G.723.1 Floating Point Release 2 (June 2006)**.

---

## Файлы проекта

| Файл | Назначение |
|------|------------|
| `sosna_udf.dll` | Основная UDF-библиотека для InterBase |
| `test_sosna_udf.exe` | Автономная тестовая программа (работает без СУБД) |
| `install/init_sosna_udf.sql` | Скрипт регистрации UDF в базе |
| `test/test_transcode_g723.sql` | Пример пакетного обновления таблицы `speech` |

---

## Экспортируемые функции

### 1. `transcode_g723` — основная UDF-функция

```c
void __cdecl transcode_g723(BLOB_CB in_blob, BLOB_CB out_blob);
```

**Параметры:**
- `in_blob` — входной BLOB с данными G.723.1 (поле `rec` таблицы `speech`)
- `out_blob` — выходной BLOB для записи PCMU (возвращается в `rec`)

**Регистрация в InterBase:**
```sql
DECLARE EXTERNAL FUNCTION UDF_TRANSCODE_G723
    BLOB, BLOB
    RETURNS PARAMETER 2
    ENTRY_POINT 'transcode_g723'
    MODULE_NAME 'sosna_udf';

CREATE PROCEDURE TRANSCODE_G723 (rec_in BLOB SUB_TYPE 0)
RETURNS (rec_out BLOB SUB_TYPE 0)
AS
BEGIN
    IF (rec_in IS NOT NULL) THEN
        rec_out = UDF_TRANSCODE_G723(rec_in);
    ELSE
        rec_out = NULL;
    SUSPEND;
END
```

**Использование:**
```sql
UPDATE speech
   SET rec = (SELECT rec_out FROM TRANSCODE_G723(rec)),
       rectype = 'PCMU'
 WHERE rectype = 'G723.1';
```

---

### 2. `transcode_g723_ib_util` — расширенная версия с `ib_util_malloc`

```c
void __cdecl transcode_g723_ib_util(
    BLOB_CB in_blob,
    BLOB_CB out_blob,
    void* (*ib_util_malloc)(size_t),
    void (*ib_util_free)(void*)
);
```

**Назначение:**  
Версия для совместимости с InterBase API, где СУБД передает свои функции управления памятью (`ib_util_malloc`, `ib_util_free`) как 3-й и 4-й параметры.

**Параметры:**
- `in_blob` — входной BLOB (G.723.1)
- `out_blob` — выходной BLOB (PCMU)
- `ib_util_malloc` — функция выделения памяти InterBase (передается СУБД автоматически)
- `ib_util_free` — функция освобождения памяти InterBase (передается СУБД автоматически)

**Регистрация в InterBase:**
```sql
DECLARE EXTERNAL FUNCTION UDF_TRANSCODE_G723_IB
    BLOB, BLOB
    RETURNS PARAMETER 2
    ENTRY_POINT 'transcode_g723_ib_util'
    MODULE_NAME 'sosna_udf';
```

> **Важно:** В текущей реализации параметры `ib_util_malloc`/`ib_util_free` **не используются** — выходные данные передаются через callback `out_blob->blob_put_segment`, который вызывается самой СУБД. Функция существует для будущей совместимости (например, если потребуется выделять большие буферы через `ib_util_malloc` перед `blob_put_segment`).

**Тестирование без СУБД:**
```cpp
// Передавайте NULL — функция их игнорирует
transcode_g723_ib_util(&in_blob, &out_blob, nullptr, nullptr);

// Или свои аллокаторы для тестов
void* my_malloc(size_t size) { return malloc(size); }
void my_free(void* ptr) { free(ptr); }
transcode_g723_ib_util(&in_blob, &out_blob, my_malloc, my_free);
```

---

## Установка

1. Скопируйте `sosna_udf.dll` в папку `UDF` сервера InterBase (например: `C:\InterBase\UDF\`)
2. Выполните скрипт регистрации:
   ```bash
   isql -user sysdba -password masterkey db.gdb -i install/init_sosna_udf.sql
   ```

---

## Тестирование без СУБД

```bash
cd build
test_sosna_udf.exe
```

Программа:
- Генерирует тестовые файлы `test_input_hi.g723` (6.3 kbps) и `test_input_low.g723` (5.3 kbps)
- Транскодирует их через загруженную `sosna_udf.dll`
- Сохраняет результат в `test_input_hi__[6.3].pcmu` и `test_input_low__[5.3].pcmu`
- Проверяет граничные случаи (пустой BLOB, неполные кадры, SID, маркеры потерь)

---

## Структура таблицы `speech`

```sql
CREATE TABLE speech (
    id      INTEGER NOT NULL PRIMARY KEY,
    rectype VARCHAR(20) NOT NULL,  -- 'G723.1' или 'PCMU'
    rec     BLOB SUB_TYPE 0        -- аудиоданные
);
```

- Исходные данные: `rectype = 'G723.1'`
- После транскодирования: `rectype = 'PCMU'`

---

## Требования

- **InterBase 2009** (Win32)
- **Windows** (x86 / x64 с WoW64)
- Библиотека собрана как **32-битная** (Win32)
- CRT: динамическая линковка (`/MD`) для совместимости с InterBase

### Зависимости рантайма (runtime dependencies)

При сборке с **MinGW** (GCC) библиотека `sosna_udf.dll` зависит от следующих DLL, которые должны быть доступны в PATH или в той же папке:

| DLL | Назначение | Откуда взять |
|-----|------------|--------------|
| `libwinpthread-1.dll` | POSIX threads поддержка (MinGW) | `d:\Programs\MinGW\mingw32\bin\` |

> **Важно:** `msvcrt.dll` и `KERNEL32.dll` — системные библиотеки Windows, они **всегда присутствуют** в системе. Дополнительно копировать их **не нужно**.

При использовании опции `STATIC_LINK_CRT=ON` (по умолчанию) тестовая программа `test_sosna_udf.exe` линкуется статически с `libgcc`, `libstdc++`, `libwinpthread` и зависит только от системных `msvcrt.dll` / `KERNEL32.dll`.

При сборке с **MSVC** (Visual Studio) зависимостей MinGW нет, используется только системный `msvcrt.dll` (или `vcruntime140.dll` при динамической линковке `/MD`).

**Рекомендация для продакшена:** используйте MSVC-сборку (`cmake -G "Visual Studio 17 2022" -A Win32 ...`), чтобы избежать необходимости распространять MinGW DLL.

---

## Сборка

```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DSTATIC_LINK_CRT=ON ..
mingw32-make -j4
```

Опции CMake:
| Опция | Значение по умолчанию | Описание |
|-------|----------------------|----------|
| `STATIC_LINK_CRT` | `ON` | Статическая линковка CRT для тестовой программы |
| `ENABLE_ASAN` | `OFF` | AddressSanitizer (только GCC/Clang) |
| `ENABLE_CPPCHECK` | `ON` | Статический анализ Cppcheck |
| `BUILD_SHARED_LIBS` | `ON` | Сборка DLL (OFF = статическая библиотека) |

---

## Лицензия

ITU-T G.723.1 reference code © 1995-2006 Intel Corporation, France Telecom, AudioCodes, DSP Group, Universite de Sherbrooke.

Обертка UDF © 2026 PineCode Lab.