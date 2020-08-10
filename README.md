# DnsServer
Test DNS server modified for google captive portal on ESP8266

The code is from the original ESP8266 based DnsServer. The purpose is that some versions of android seem to have a hard-coded DNS lookup that will use the google DNS servers for finding the IP address of the 'online check' used by android when signing in. One workaround for this is to set a non-internet-connected ESP8266 working as a local access point to emulate something like 8.8.8.8 as it's IP address so it can properly catch the hard-coded DNS request and spoof it back to the esp8266 for a proper captive re-direct.
