#include <stdio.h>
#include "net_http.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

int main(void)
{
#ifdef _WIN32
   WSADATA wsaData;
#endif
   char   *data;
   http_t *http1, *http2, *http3;
   size_t len, pos = 0, tot = 0;

#ifdef _WIN32
   WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

   http1 = net_http_new("http://buildbot.libretro.com/nightly/win-x86/latest/mednafen_psx_libretro.dll.zip");

   while (!net_http_update(http1, &pos, &tot))
      printf("%.9lu / %.9lu        \r",pos,tot);
	
   http3 = net_http_new("http://www.wikipedia.org/");
   while (!net_http_update(http3, NULL, NULL)) {}
	
   data  = (char*)net_http_data(http3, &len, false);

   printf("%.*s\n", (int)256, data);

   net_http_delete(http1);
   net_http_delete(http3);

   return 0;
}
