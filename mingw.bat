@echo off
REM
REM Batch file to compile Minimum Profit with the MinGW compiler under Windows
REM (Tested with the one included in the Bloodshed Dev-C++ IDE
REM  and the one included with the Qt Creator IDE)
REM

copy mpdm\VERSION mpdm\config.h
echo #define CONFOPT_PREFIX "/usr/local" >> mpdm\config.h
echo #define CONFOPT_WIN32 1 >> mpdm\config.h
echo #define HAVE_STRING_H 1 >> mpdm\config.h
echo #define REGEX 1 >> mpdm\config.h
echo #define CONFOPT_INCLUDED_REGEX 1 >> mpdm\config.h
echo #define CONFOPT_UNISTD_H 1 >> mpdm\config.h
echo #define CONFOPT_SYS_TYPES_H 1 >> mpdm\config.h
echo #define CONFOPT_SYS_STAT_H 1 >> mpdm\config.h
echo #define CONFOPT_FULLPATH 1 >> mpdm\config.h

copy mpsl\VERSION mpsl\config.h
echo #define CONFOPT_PREFIX "/usr/local" >> mpsl\config.h

copy VERSION config.h
echo #define CONFOPT_PREFIX "/usr/local" >> config.h
echo #define CONFOPT_APPNAME "mp-5" >> config.h
echo #define CONFOPT_WIN32 1 >> config.h
echo int win32_drv_detect(int * argc, char *** argv); >> config.h
echo #define TRY_DRIVERS() (win32_drv_detect(^&argc,^&argv)^|^|0) >> config.h

cd mpdm
gcc -c mpdm_v.c
gcc -c mpdm_a.c
gcc -c mpdm_h.c
gcc -c mpdm_r.c
gcc -c mpdm_s.c
gcc -c mpdm_d.c
gcc -c mpdm_f.c
gcc -c gnu_regex.c
ar rv libmpdm.a mpdm_*.o gnu_regex.o
cd ..

cd mpsl
gcc -I../mpdm -c lex.yy.c -o mpsl_l.o
gcc -I../mpdm -c y.tab.c -o mpsl_y.o
gcc -I../mpdm -c mpsl_c.c
gcc -I../mpdm -c mpsl_f.c
gcc -I../mpdm -c mpsl_d.c
ar rv libmpsl.a mpsl_*.o
cd ..

gcc -Impdm -Impsl -c mp_core.c
gcc -Impdm -Impsl -c mpv_win32.c
windres mp_res.rc mp_res.o

gcc mp_core.o mpv_win32.o mp_res.o -Lmpdm -Lmpsl -lmpsl -lmpdm -mwindows -lcomctl32 -o mp-5.exe
