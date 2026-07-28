#include "appProto.h"

#include <stdlib.h>
#include <string.h>

/* Istenen kadar bayti, gerekirse birden fazla recv() cagirarak okur.
   TCP tek recv()'de istenenden az bayt verebilir; "kismi okuma" denen bu durum
   hata degil, normal davranistir.
   PROTO_OK / PROTO_TIMEOUT / PROTO_CLOSED / PROTO_ERROR doner. */
static int recv_exactly(SOCKET sock, char *buffer, size_t length)
{
    size_t received = 0;

    while (received < length) {
        int chunk = recv(sock, buffer + received, (int)(length - received), 0);

        if (0 == chunk) {
            /* Karsi taraf baglantiyi duzgunce kapatti */
            return PROTO_CLOSED;
        }

        if (0 > chunk) {
            /* SO_RCVTIMEO ile ayarlanan sure dolduysa Winsock WSAETIMEDOUT
               veriyor. Bunu diger soket hatalarindan ayiriyoruz: zaman
               asiminda sunucu ayakta ama gec kaliyor, digerinde baglanti
               gercekten bozulmus demektir. */
            if (WSAETIMEDOUT == WSAGetLastError()) {
                return PROTO_TIMEOUT;
            }
            return PROTO_ERROR;
        }

        received += (size_t)chunk;
    }

    return PROTO_OK;
}

/* send() de kismi yazabilir: 900 bayt istedik, 500'unu yazdi diyebilir.
   Kalani gonderene kadar tekrarliyoruz. */
static int send_exactly(SOCKET sock, const char *buffer, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        int chunk = send(sock, buffer + sent, (int)(length - sent), 0);

        if (0 == chunk) {
            return PROTO_CLOSED;
        }

        if (0 > chunk) {
            return PROTO_ERROR;
        }

        sent += (size_t)chunk;
    }

    return PROTO_OK;
}

int proto_send(SOCKET sock, const char *message, size_t length)
{
    unsigned char header[4];

    if (NULL == message || PROTO_MAX_MESSAGE < length) {
        return PROTO_ERROR;
    }

    /* Uzunlugu big-endian olarak elle yaziyoruz. htonl() de kullanilabilirdi
       ama bayt bayt yazmak hangi sirayla gittigini kodda gorunur kiliyor. */
    header[0] = (unsigned char)((length >> 24) & 0xFF);
    header[1] = (unsigned char)((length >> 16) & 0xFF);
    header[2] = (unsigned char)((length >> 8) & 0xFF);
    header[3] = (unsigned char)(length & 0xFF);

    {
        int result = send_exactly(sock, (const char *)header, sizeof(header));
        if (PROTO_OK != result) {
            return result;
        }
    }

    return send_exactly(sock, message, length);
}

int proto_recv(SOCKET sock, char **outMessage, size_t *outLength)
{
    unsigned char header[4];
    size_t length;
    char *buffer;

    int result;

    if (NULL == outMessage || NULL == outLength) {
        return PROTO_ERROR;
    }

    *outMessage = NULL;
    *outLength = 0;

    result = recv_exactly(sock, (char *)header, sizeof(header));
    if (PROTO_OK != result) {
        return result;
    }

    length = ((size_t)header[0] << 24) |
             ((size_t)header[1] << 16) |
             ((size_t)header[2] << 8) |
             ((size_t)header[3]);

    if (PROTO_MAX_MESSAGE < length) {
        return PROTO_ERROR; /* bozuk ya da makul olmayan uzunluk */
    }

    /* +1: icerigin sonuna '\0' koyuyoruz ki cagiran taraf metin fonksiyonlarini
       (strstr, strncmp...) guvenle kullanabilsin. */
    buffer = (char *)malloc(length + 1);
    if (NULL == buffer) {
        return PROTO_ERROR;
    }

    if (0 < length) {
        result = recv_exactly(sock, buffer, length);
        if (PROTO_OK != result) {
            free(buffer);
            return result;
        }
    }

    buffer[length] = '\0';

    *outMessage = buffer;
    *outLength = length;
    return PROTO_OK;
}

void proto_free(char *message)
{
    free(message); /* free(NULL) zaten guvenli, ayrica kontrol gerekmiyor */
}
