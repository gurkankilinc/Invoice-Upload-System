#include "appConsole.h"

#include <windows.h>
#include <conio.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Program acilirken buldugumuz kod sayfasi; cikarken geri yukluyoruz. */
static UINT g_originalCodePage = 0;

static void restore_code_page(void)
{
    if (0 != g_originalCodePage) {
        SetConsoleOutputCP(g_originalCodePage);
    }
}

void console_setup(void)
{
    /* Tamponlamayi kapat: her printf aninda ekrana gitsin.
       Bu satir olmadan, stdout bir boruysa cikti 4 KB'lik parcalar halinde
       birikiyor ve program tus beklerken ekranda yarim metin kaliyor. */
    setvbuf(stdout, NULL, _IONBF, 0);

    g_originalCodePage = GetConsoleOutputCP();

    if (SetConsoleOutputCP(CP_UTF8)) {
        atexit(restore_code_page);
    }
}

const char *console_to_utf8(const char *ansiText, char *out, size_t outSize)
{
    wchar_t wide[512];
    int wideLength;

    if (NULL == out || 0 == outSize) {
        return "";
    }

    if (NULL == ansiText) {
        out[0] = '\0';
        return out;
    }

    /* Cevrim iki adimda yapiliyor cunku Windows'ta iki kod sayfasi arasinda
       dogrudan gecis yok: once ANSI -> UTF-16 (wide), sonra UTF-16 -> UTF-8.
       UTF-16 burada ortak ara format gorevi goruyor. */
    wideLength = MultiByteToWideChar(CP_ACP, 0, ansiText, -1,
                                     wide, (int)(sizeof(wide) / sizeof(wide[0])));

    if (0 == wideLength ||
        0 == WideCharToMultiByte(CP_UTF8, 0, wide, -1,
                                 out, (int)outSize, NULL, NULL)) {
        /* Cevrilemedi (ornegin metin tampona sigmadi): en azindan okunabilir
           bir sey yazdiralim, cagiran tarafi bos donusle ugrastirmayalim. */
        strncpy(out, ansiText, outSize - 1);
        out[outSize - 1] = '\0';
    }

    return out;
}

int console_is_interactive(void)
{
    /* _isatty: verilen dosya tanimlayicisi bir terminale mi bagli?
       stdin bir boruya ya da dosyaya yonlendirilmisse 0 doner. */
    return _isatty(_fileno(stdin)) ? 1 : 0;
}

int console_read_key(void)
{
    int ch;

    if (0 != console_is_interactive()) {
        return _getch(); /* Enter'a gerek yok, tek tus */
    }

    /* Yonlendirilmis girdi: satir sonlarini atlayip ilk anlamli karakteri al.
       Boylece test betigi "1\n2\n" gibi bir dizi gonderebiliyor. */
    do {
        ch = getchar();
    } while ('\n' == ch || '\r' == ch);

    if (EOF == ch) {
        return CONSOLE_KEY_EOF;
    }

    return ch;
}

void console_read_password(char *buffer, size_t size)
{
    size_t i = 0;

    if (NULL == buffer || 0 == size) {
        return;
    }

    if (0 == console_is_interactive()) {
        /* Yonlendirilmis girdi: maskeleme yapmadan satiri oku */
        if (NULL == fgets(buffer, (int)size, stdin)) {
            buffer[0] = '\0';
            return;
        }

        i = strlen(buffer);
        while (0 < i && ('\n' == buffer[i - 1] || '\r' == buffer[i - 1])) {
            buffer[i - 1] = '\0';
            i--;
        }
        return;
    }

    /* Klavye: her karakteri '*' olarak goster.
       Basit tutmak icin backspace/duzeltme desteklenmiyor. */
    while (i < size - 1) {
        int ch = _getch();

        if ('\r' == ch || '\n' == ch) {
            break;
        }

        buffer[i] = (char)ch;
        i++;
        putchar('*');
    }

    buffer[i] = '\0';
    putchar('\n');
}
