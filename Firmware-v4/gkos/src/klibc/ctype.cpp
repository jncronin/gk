#include <ctype.h>
#include <wctype.h>
#include <cstring>
#include <stdio.h>

extern "C" int toupper(int c)
{
    if((c >= 'a') && (c <= 'z'))
    {
        return c - 'a' + 'A';
    }
    else
    {
        return c;
    }
}

extern "C" int tolower(int c)
{
    if((c >= 'A') && (c <= 'Z'))
    {
        return c - 'A' + 'a';
    }
    else
    {
        return c;
    }
}

#define WCTYPE_ALNUM        1
#define WCTYPE_ALPHA        2
#define WCTYPE_BLANK        3
#define WCTYPE_CNTRL        4
#define WCTYPE_DIGIT        5
#define WCTYPE_GRAPH        6
#define WCTYPE_LOWER        7
#define WCTYPE_PRINT        8
#define WCTYPE_PUNCT        9
#define WCTYPE_SPACE        10
#define WCTYPE_UPPER        11
#define WCTYPE_XDIGIT       12

extern "C" wctype_t wctype(const char *str)
{
    if(!strcmp("alnum", str))
        return (wctype_t)WCTYPE_ALNUM;
    if(!strcmp("alpha", str))
        return (wctype_t)WCTYPE_ALPHA;
    if(!strcmp("blank", str))
        return (wctype_t)WCTYPE_BLANK;
    if(!strcmp("cntrl", str))
        return (wctype_t)WCTYPE_CNTRL;
    if(!strcmp("digit", str))
        return (wctype_t)WCTYPE_DIGIT;
    if(!strcmp("graph", str))
        return (wctype_t)WCTYPE_GRAPH;
    if(!strcmp("lower", str))
        return (wctype_t)WCTYPE_LOWER;
    if(!strcmp("print", str))
        return (wctype_t)WCTYPE_PRINT;
    if(!strcmp("punct", str))
        return (wctype_t)WCTYPE_PUNCT;
    if(!strcmp("space", str))
        return (wctype_t)WCTYPE_SPACE;
    if(!strcmp("upper", str))
        return (wctype_t)WCTYPE_UPPER;
    if(!strcmp("xdigit", str))
        return (wctype_t)WCTYPE_XDIGIT;
    return (wctype_t)0;
}

extern "C" int iswctype(wint_t wc, wctype_t desc)
{
    switch(desc)
    {
        case WCTYPE_ALNUM:
            return iswalnum(wc);
        case WCTYPE_ALPHA:
            return iswalpha(wc);
        case WCTYPE_BLANK:
            return iswblank(wc);
        case WCTYPE_CNTRL:
            return iswcntrl(wc);
        case WCTYPE_DIGIT:
            return iswdigit(wc);
        case WCTYPE_GRAPH:
            return iswgraph(wc);
        case WCTYPE_LOWER:
            return iswlower(wc);
        case WCTYPE_PRINT:
            return iswprint(wc);
        case WCTYPE_PUNCT:
            return iswpunct(wc);
        case WCTYPE_SPACE:
            return iswspace(wc);
        case WCTYPE_UPPER:
            return iswupper(wc);
        case WCTYPE_XDIGIT:
            return iswxdigit(wc);
        default:
            return 0;
    }
}

extern "C" int wctob(wint_t c)
{
    return (c <= 255) ? (int)c : EOF;
}

extern "C" wint_t btowc(int c)
{
    return (wint_t)c;
}

extern "C" int isalnum(int c)
{
    return iswalnum(btowc(c));
}

extern "C" int isalpha(int c)
{
    return iswalpha(btowc(c));
}

extern "C" int isblank(int c)
{
    return iswblank(btowc(c));
}

extern "C" int iscntrl(int c)
{
    return iswcntrl(btowc(c));
}

extern "C" int isdigit(int c)
{
    return iswdigit(btowc(c));
}

extern "C" int isgraph(int c)
{
    return iswgraph(btowc(c));
}

extern "C" int islower(int c)
{
    return iswlower(btowc(c));
}

extern "C" int isprint(int c)
{
    return iswprint(btowc(c));
}

extern "C" int ispunct(int c)
{
    return iswpunct(btowc(c));
}

extern "C" int isspace(int c)
{
    return iswspace(btowc(c));
}

extern "C" int isupper(int c)
{
    return iswupper(btowc(c));
}

extern "C" int isxdigit(int c)
{
    return iswxdigit(btowc(c));
}

extern "C" int iswalnum(wint_t c)
{
    return iswalpha(c) || isdigit(c);
}

extern "C" int iswalpha(wint_t c)
{
    return iswupper(c) || iswlower(c);
}

extern "C" int iswdigit(wint_t c)
{
    return (c >= '0') && (c <= '9');
}

extern "C" int iswlower(wint_t c)
{
    return (c >= 'a') && (c <= 'z');
}

extern "C" int iswupper(wint_t c)
{
    return (c >= 'A') && (c <= 'Z');
}

extern "C" int iswcntrl(wint_t c)
{
    if(iswspace(c))
        return 0;
    return c < 32;
}

extern "C" int iswpunct(wint_t c)
{
    return iswprint(c) && !iswspace(c) && !iswalnum(c);
}

extern "C" int iswprint(wint_t c)
{
    return !iswcntrl(c);
}

extern "C" int iswspace(wint_t c)
{
    return (c == ' ') || (c == '\f') || (c == '\n') || (c == 'r') ||
        (c == '\t') || (c == '\v');
}

extern "C" int iswblank(wint_t c)
{
    return (c == ' ') || (c == '\t');
}

extern "C" int iswgraph(wint_t c)
{
    return iswprint(c) && !iswspace(c);
}

extern "C" int iswxdigit(wint_t c)
{
    return iswdigit(c) ||
        ((c >= 'a') && (c <= 'f')) |
        ((c >= 'A') && (c <= 'F'));
}

extern "C" wint_t towlower(wint_t c)
{
    if((c >= 'A') && (c <= 'Z'))
        return c - 'A' + 'a';
    return c;
}

extern "C" wint_t towupper(wint_t c)
{
    if((c >= 'a') && (c <= 'z'))
        return c - 'a' + 'A';
    return c;
}

