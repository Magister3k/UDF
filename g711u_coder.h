#ifndef G711U_CODER_H
#define G711U_CODER_H

// Инициализирует внутреннюю LUT-таблицу в памяти (вызывается в DllMain)
void g711u_init_encoder();

// Сверхбыстрое преобразование одного 16-битного отсчета звука в 8-битный PCMU байт
unsigned char g711u_linear_to_pcmu(short sample);

#endif // G711U_CODER_H
