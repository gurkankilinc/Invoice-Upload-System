#include "appXml.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stdlib.h>
#include <string.h>

/* Spesifikasyonun istedigi sabit metin ve node/oznitelik adlari.
   Tek yerde durmalari, ileride degisirse tek satir duzeltmek icin. */
#define NODE_INVOICE_STATUS "invoiceStatus"
#define NODE_MESSAGE        "message"
#define NODE_SIGNATURE      "signature"
#define ATTR_DATE           "date"
#define TEXT_MESSAGE        "Fatura Kaydedildi"

/* Belge icinde daha once eklenmis bir invoiceStatus varsa siler.
   Ayni faturayi ikinci kez imzalarsak iki tane invoiceStatus olusmasin diye.
   "static": sadece bu dosyanin ic yardimcisi. */
static void remove_existing_status(xmlNodePtr root)
{
    xmlNodePtr child = root->children;

    while (NULL != child) {
        /* Simdiki node'u silersek child->next'e erisemeyiz,
           o yuzden bir sonrakini onceden aliyoruz. */
        xmlNodePtr next = child->next;

        if (XML_ELEMENT_NODE == child->type &&
            0 == xmlStrcmp(child->name, BAD_CAST NODE_INVOICE_STATUS)) {
            xmlUnlinkNode(child);
            xmlFreeNode(child);
        }

        child = next;
    }
}

int xml_sign_invoice(const char *invoiceXml, size_t invoiceLength,
                     const char *timestamp, const char *signatureHex,
                     char **outXml, size_t *outLength)
{
    xmlDocPtr doc;
    xmlNodePtr root;
    xmlNodePtr status;
    xmlChar *dumped = NULL;
    int dumpedLength = 0;
    char *result;

    if (NULL == invoiceXml || NULL == timestamp || NULL == signatureHex ||
        NULL == outXml || NULL == outLength) {
        return -1;
    }

    *outXml = NULL;
    *outLength = 0;

    /* XML_PARSE_NOERROR/NOWARNING: bozuk fatura gelirse libxml2 kendi hata
       metnini stderr'e basmasin, hatayi biz kendi mesajimizla bildirelim. */
    doc = xmlReadMemory(invoiceXml, (int)invoiceLength, "invoice.xml", NULL,
                        XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (NULL == doc) {
        return -1;
    }

    root = xmlDocGetRootElement(doc);
    if (NULL == root) {
        xmlFreeDoc(doc);
        return -1;
    }

    remove_existing_status(root);

    /* root->ns vererek node'u kokun namespace'ine bagliyoruz. Boylece
       ciktida invoiceStatus'a ayrica bir xmlns yazilmiyor, fatura
       belgesinin varsayilan namespace'ini devraliyor. */
    status = xmlNewChild(root, root->ns, BAD_CAST NODE_INVOICE_STATUS, NULL);
    if (NULL == status) {
        xmlFreeDoc(doc);
        return -1;
    }

    xmlNewProp(status, BAD_CAST ATTR_DATE, BAD_CAST timestamp);
    xmlNewChild(status, root->ns, BAD_CAST NODE_MESSAGE, BAD_CAST TEXT_MESSAGE);
    xmlNewChild(status, root->ns, BAD_CAST NODE_SIGNATURE, BAD_CAST signatureHex);

    /* Son parametre 1 = girintili (format) cikti; imzali dosya insan
       tarafindan da okunabilir olsun diye. */
    xmlDocDumpFormatMemoryEnc(doc, &dumped, &dumpedLength, "UTF-8", 1);
    xmlFreeDoc(doc);

    if (NULL == dumped || 0 >= dumpedLength) {
        xmlFree(dumped);
        return -1;
    }

    /* Ciktiyi libxml2'nin ayirdigi tampondan kendi tamponumuza aliyoruz ki
       cagiran taraf serbest birakmak icin libxml2'yi bilmek zorunda kalmasin.
       memcpy kullaniyoruz cunku uzunlugu zaten biliyoruz. */
    result = (char *)malloc((size_t)dumpedLength + 1);
    if (NULL == result) {
        xmlFree(dumped);
        return -1;
    }

    memcpy(result, dumped, (size_t)dumpedLength);
    result[dumpedLength] = '\0';
    xmlFree(dumped);

    *outXml = result;
    *outLength = (size_t)dumpedLength;
    return 0;
}

void xml_free_buffer(char *buffer)
{
    free(buffer);
}

int xml_is_well_formed(const char *xml, size_t length)
{
    xmlDocPtr doc;

    if (NULL == xml || 0 == length) {
        return 0;
    }

    doc = xmlReadMemory(xml, (int)length, "check.xml", NULL,
                        XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (NULL == doc) {
        return 0;
    }

    xmlFreeDoc(doc);
    return 1;
}

void xml_shutdown(void)
{
    xmlCleanupParser();
}
