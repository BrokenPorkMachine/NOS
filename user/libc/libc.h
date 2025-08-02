#pragma once
#include <stddef.h>

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
char *strncpy(char *dest, const char *src, size_t n);
size_t strlcpy(char *dst, const char *src, size_t size);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
char *strstr(const char *haystack, const char *needle);

typedef struct {
    int handle;
    unsigned int pos;
} FILE;

FILE *fopen(const char *path, const char *mode);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fclose(FILE *stream);
int rename(const char *old, const char *new);

int abs(int x);
long labs(long x);
long long llabs(long long x);
double sqrt(double x);

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void free(void *ptr);
void *__memcpy_chk(void *dest, const void *src, size_t n, size_t destlen);
char *__strncpy_chk(char *dest, const char *src, size_t n, size_t destlen);

int fork(void);
int exec(const char *path);
void *sbrk(long inc);

