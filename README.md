#### Panasonic-AC-8266

This is firmware for ESP8266 use to control panasonic air condition using smart phone.  
It can be use for other brand with small changes(just because I lazy to make it.).  

create a wifi.h with content like

``` c
#ifndef __SWIFI__
#define __SWIFI__
#define WIFI_SSID "my_ssid"
#define PASSWORD "mysecret"
#endif
```

compile and write firmware to ESP8266, then you can go to control UI at [http://ac-remote.local](http://ac-remote.local)

enjoy!

#### License

This software is licensed under MIT license.