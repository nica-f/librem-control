prefix=/usr

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
	install -D $(PRG) $(DESTDIR)$(prefix)/bin/$(PRG)
	install -D org.freedesktop.policykit.librem-control.policy $(DESTDIR)$(prefix)/share/polkit-1/actions/org.freedesktop.policykit.librem-control.policy

clean:
	rm -f $(PRG) $(OBJ)
