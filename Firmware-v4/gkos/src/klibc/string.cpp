#include <cstring>
#include <wchar.h>

extern "C" size_t strlen(const char *s)
{
    size_t ret = 0;
    while(*s++)
        ret++;
    return ret;
}

extern "C" size_t wcslen(const wchar_t *s)
{
    size_t ret = 0;
    while(*s++)
        ret++;
    return ret;
}

extern "C" void *memcpy(void *dest, const void *src, size_t n)
{
    auto d = reinterpret_cast<char *>(dest);
    auto s = reinterpret_cast<const char *>(src);
    while(n--)
    {
        *d++ = *s++;
    }
    return dest;
}

extern "C" void *memmove(void *dest, const void *src, size_t n)
{
    auto d = reinterpret_cast<char *>(dest);
    auto s = reinterpret_cast<const char *>(src);
    if(d < s)
    {
        while(n--)
        {
            *d++ = *s++;
        }
    }
    else
    {
        while(n--)
        {
            d[n] = s[n];
        }
    }
    return dest;
}

extern "C" wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n)
{
    return (wchar_t *)memcpy(dest, src, n * sizeof(wchar_t) / sizeof(char));
}

extern "C" wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t n)
{
    return (wchar_t *)memmove(dest, src, n * sizeof(wchar_t) / sizeof(char));
}

extern "C" int strcmp(const char *s1, const char *s2)
{
    while(*s1 || *s2)
    {
        auto diff = *s1++ - *s2++;
        if(diff)
            return (int)diff;
    }

    return 0;
}

extern "C" int wcscmp(const wchar_t *s1, const wchar_t *s2)
{
    while(*s1 || *s2)
    {
        auto diff = *s1++ - *s2++;
        if(diff)
            return (int)diff;
    }

    return 0;
}

extern "C" int strncmp(const char *s1, const char *s2, size_t n)
{
    while((*s1 || *s2) && n--)
    {
        auto diff = *s1++ - *s2++;
        if(diff)
            return (int)diff;
    }

    return 0;
}

extern "C" char *strchr(const char *s, int c)
{
    do
    {
        if(c == *s)
            return (char *)s;
        
        s++;
    } while(*s);

    if(c == 0)
        return (char *)s;
    else
        return nullptr;
}

extern "C" void *memset(void *d, int c, size_t n)
{
    auto dest = (char *)d;
    while(n--)
        *dest++ = (char)c;
    return d;
}

extern "C" wchar_t *wmemset(wchar_t *d, wchar_t c, size_t n)
{
    auto dest = d;
    while(n--)
        *dest++ = c;
    return d;
}

extern "C" char *strcpy(char *dest, const char *src)
{
    auto d = dest;

    while(*src)
        *d++ = *src++;
    *d = 0;
    return dest;
}

extern "C" char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;

    for (i = 0; i < n && src[i] != 0; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = 0;

    return dest;
}

extern "C" wchar_t *wcsncpy(wchar_t *dest, const wchar_t *src, size_t n)
{
    size_t i;

    for (i = 0; i < n && src[i] != 0; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = 0;

    return dest;
}

extern "C" int memcmp(const void *s1, const void *s2, size_t n)
{
    auto _s1 = (const char *)s1;
    auto _s2 = (const char *)s2;
    while(n--)
    {
        auto diff = *_s1++ - *_s2++;
        if(diff)
            return diff;
    }
    return 0;
}

extern "C" void *memchr(const void *s, int c, size_t n)
{
    auto str = (const char *)s;
    while(n--)
    {
        if(*str == (char)c)
            return (void *)str;
        str++;
    }
    return nullptr;
}

extern "C" wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
    auto str = s;
    while(n--)
    {
        if(*str == c)
            return (wchar_t *)str;
        str++;
    }
    return nullptr;
}

extern "C" size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *)
{
    if(!pwc)
        return 0;
    if(!s)
        return 0;
    if(!n)
        return 0;
    if(*s == 0)
        return 0;
    *pwc = (wchar_t)*s;
    return 1;
}

extern "C" size_t wcrtomb(char *s, wchar_t wc, mbstate_t *)
{
    if(!s)
        return 1;
    *s = (char)wc;
    return 1;
}

extern "C" int __locale_mb_cur_max()
{
    return 1;
}

extern "C" int strcoll(const char *s1, const char *s2)
{
    return strcmp(s1, s2);
}

extern "C" char *setlocale(int, const char *locale)
{
    if(!strcmp("C", locale))
        return (char *)locale;
    else if(!strcmp("POSIX", locale))
        return (char *)locale;
    return nullptr;
}

extern "C" int wcscoll(const wchar_t *s1, const wchar_t *s2)
{
    return wcscmp(s1, s2);
}

extern "C" size_t strxfrm(char *dest, const char *src, size_t n)
{
    strncpy(dest, src, n);
    return n;
}

extern "C" size_t wcsxfrm(wchar_t *dest, const wchar_t *src, size_t n)
{
    wcsncpy(dest, src, n);
    return n;
}
