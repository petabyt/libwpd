-include config.mak
CPPFLAGS := -Iwindows/ -g -Wall -Wshadow -Wcast-qual -Wpedantic -Werror=deprecated-declarations -Wno-unknown-pragmas
MINGW := x86_64-w64-mingw32
CC := $(MINGW)-gcc
CPP := $(MINGW)-c++
FILES := src/dll.o src/main.o src/parse.o
LDFLAGS := -shared -lole32 -loleaut32 -lportabledeviceguids

libwpd_64.dll: $(FILES)
	$(CPP) $(FILES) $(LDFLAGS) -o libwpd_64.dll

libwpd_64.a: $(FILES)
	x86_64-w64-mingw32-ar -rs libwpd_64.a $(FILES)

test.exe: test.o libwpd_64.a
	$(CC) test.o libwpd_64.a -lole32 -loleaut32 -lportabledeviceguids -o test.exe

install: libwpd_64.a
	cp libwpd_64.a /usr/$(MINGW)/lib/libwpd.a
	cp libwpd.h /usr/$(MINGW)/include

$(FILES): windows/*.hpp

%.o: %.cpp
	$(CPP) -c $< $(CPPFLAGS) -o $@

%.o: %.c
	$(CC) -c $< $(CPPFLAGS) -o $@

clean:
	$(RM) *.dll *.o *.out src/*.o *.exe *.a
