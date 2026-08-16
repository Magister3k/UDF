#ifndef G723_1_DECODER_H
#define G723_1_DECODER_H

// Инициализация статических структур декодера (вызывается при старте DLL)
void g723_init_decoder();

// Сброс состояния для новой аудиозаписи BLOB
void g723_reset_decoder();

// Декодирование одного фрейма. 
// Принимает: указатель на входной кадр, массив для записи 240 отсчетов float
// Возвращает: размер обработанного кадра в байтах (24, 20, 4 или 1 для PLC)
int g723_decode_frame(const unsigned char* input, double* output_pcm);

#endif // G723_1_DECODER_H
