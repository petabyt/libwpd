-include config.mak
CPPFLAGS=-Iwindows/
CC=x86_64-w64-mingw32-gcc
CPP=x86_64-w64-mingw32-c++
FILES=src/dll.o src/main.o
LDFLAGS=-shared -lole32 -loleaut32 -lportabledeviceguids

libwpd_64.dll: $(FILES)
	$(CPP) $(FILES) $(LDFLAGS) -o libwpd_64.dll

libwpd_64.a: $(FILES)
	x86_64-w64-mingw32-ar -rs libwpd_64.a $(FILES)

test.exe: test.o libwpd_64.a
	$(CC) test.o libwpd_64.a -s -lole32 -loleaut32 -lportabledeviceguids -o test.exe

$(FILES): windows/*.h

%.o: %.cpp
	$(CPP) -c $< $(CPPFLAGS) -o $@

%.o: %.c
	$(CC) -c $< $(CPPFLAGS) -o $@


clean:
	$(RM) *.dll *.o *.out src/*.o *.exe *.a
