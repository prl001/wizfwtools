CC=gcc
CFLAGS=-g -Wall -c
PREFIX=/usr/local

all: wiz_unpack wiz_genromfs wiz_pack wiz_svctool

wiz_svctool: src/svctool.o
	$(CC) $< -o $@

wiz_unpack: src/wiz_unpack.o
	$(CC) $< -o $@

src/wiz_unpack.o: src/wiz_unpack.c
	$(CC) $(CFLAGS) $< -o $@

wiz_genromfs: src/genromfs.o
	$(CC) $< -o $@

src/genromfs.o: src/genromfs.c
	$(CC) $(CFLAGS) $< -o $@

wiz_pack: src/wiz_pack/wiz_pack.o
	$(CC) $< -o $@
	
src/wiz_pack/wiz_pack.o: src/wiz_pack/wiz_pack.c src/wiz_pack/md5.c src/wiz_pack/md5.h src/wiz_pack/hdr.h
	$(CC) $(CFLAGS) -I src/wiz_pack src/wiz_pack/wiz_pack.c -o $@

install: all
	cp wiz_unpack wiz_pack wiz_genromfs wiz_svctool $(PREFIX)/bin

clean:
	rm -r src/*.o src/wiz_pack/*.o wiz_unpack wiz_genromfs wiz_pack wiz_svctool
