#define _WIN32_WINNT 0x0600
#define COBJMACROS
#include <d3d9.h>
#include <dxva2api.h>
#include <windows.h>
int main(void)
{
    IDirectXVideoDecoder *o = NULL;
    IDirectXVideoDecoder_Release(o);
    return 0;
}
