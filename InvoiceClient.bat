@echo off
rem Proje kokunden kisa yoldan calistirmak icin: .\InvoiceClient
rem Asil exe client\ klasorunde duruyor; fatura dosyalari (*.inv) ve
rem upload_system.db da orada oldugu icin exe kendi klasorune geciyor.
"%~dp0client\InvoiceClient.exe" %*
