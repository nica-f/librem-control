PREFIX?=/usr

CC=gcc
#CFLAGS=-g -O2 -Wall -D_REENTRANT `pkg-config --cflags libadwaita-1`
#LIBS=`pkg-config --libs libadwaita-1`
CFLAGS=-g -O2 -Wall -D_REENTRANT `pkg-config --cflags gtk4`
LIBS=`pkg-config --libs gtk4`

OBJ=librem-control.o ec-tool.o
PRG=librem-control

all: $(PRG)

$(PRG): $(OBJ)
	$(CC) $(OBJ) -o $(PRG) $(LIBS)

install:
	install -D $(PRG) $(DESTDIR)$(PREFIX)/bin/$(PRG)
	install -D org.freedesktop.policykit.librem-control.policy $(DESTDIR)$(PREFIX)/share/polkit-1/actions/org.freedesktop.policykit.librem-control.policy
	install -D librem-control.desktop $(DESTDIR)$(PREFIX)/$(prefix)/share/applications/librem-control.desktop
	install -D librem-control.svg $(DESTDIR)$(PREFIX)/$(prefix)/share/icons/hicolor/scalable/apps/librem-control.svg

deb:
	fakeroot debian/rules binary

clean:
	rm -f $(PRG) $(OBJ)
	rm -rf debian/.debhelper debian/librem-control debian/librem-control.substvars debian/files debian/debhelper-build-stamp
