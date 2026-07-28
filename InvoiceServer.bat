@echo off
rem Proje kokunden kisa yoldan calistirmak icin: .\InvoiceServer
rem Asil exe server\ klasorunde duruyor (kaynaklarinin yaninda).
rem %~dp0 = bu .bat dosyasinin bulundugu klasor, yani proje koku.
rem %* = yazilan tum parametreleri oldugu gibi aktarir (--port, --delay ...)
"%~dp0server\InvoiceServer.exe" %*
