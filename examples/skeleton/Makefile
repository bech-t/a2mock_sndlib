# --- Squelette de projet a2mock_sndlib -----------------------------------
#
#   make        construit le binaire
#   make dsk    construit une disquette ProDOS bootable
#   make clean
#
# Copiez ce dossier ou vous voulez et ajustez A2MB.

# Racine de la bibliotheque. Adaptez si vous copiez ce dossier ailleurs.
A2MB     ?= ../..

PROGRAM  := THEME
VOLNAME  := THEME

# Cible "apple2" et non "apple2enh" : la bibliotheque vise un 6502 STRICT,
# donc un II+ de 1979. apple2enh imposerait le 65C02.
TARGET   := apple2
CL65     := cl65

# $4000 : le tampon de modules A2M vit en $0800-$3FFF, juste en dessous.
# Charger le programme plus bas l'ecraserait.
LOADADDR := 0x4000

CFLAGS   := -t $(TARGET) -O -Os -Cl -I $(A2MB)/include
LDFLAGS  := -t $(TARGET) --start-addr $(LOADADDR)

LIB      := $(A2MB)/build/a2mb.lib
AC_JAR   := $(A2MB)/tools/ac/ac.jar
PRODOS   := $(A2MB)/tools/prodos/prodos.dsk
LOADER   := $(A2MB)/tools/prodos/loader.system
A2MCONV  := $(A2MB)/tools/a2mconv/a2mconv.py

SRC      := $(wildcard src/*.c)
SCORES   := $(wildcard music/*.txt)
TUNES    := $(patsubst music/%.txt,build/%.A2M,$(SCORES))
BIN      := build/$(PROGRAM).bin
DSK      := build/$(PROGRAM).dsk

.PHONY: all dsk clean

all: $(BIN)

# La bibliotheque est un prerequis : sans ca, la modifier ne relierait rien.
$(LIB):
	$(MAKE) -C $(A2MB) lib

$(BIN): $(SRC) $(LIB) | build
	$(CL65) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC) $(LIB)

# Les partitions sont converties en modules A2M (profil T par defaut).
build/%.A2M: music/%.txt | build
	@python3 $(A2MCONV) $< -o $@

build:
	@mkdir -p build

dsk: $(DSK)

$(DSK): $(BIN) $(TUNES) $(PRODOS) $(LOADER)
	cp $(PRODOS) $@
	java -jar $(AC_JAR) -n  $@ $(VOLNAME)
	java -jar $(AC_JAR) -as $@ $(PROGRAM)            < $(BIN)
	java -jar $(AC_JAR) -p  $@ $(PROGRAM).SYSTEM sys < $(LOADER)
	@for m in $(TUNES); do \
	  n=$$(basename $$m | tr '[:lower:]' '[:upper:]'); \
	  java -jar $(AC_JAR) -p $@ $$n bin 0 < $$m; \
	done
	@echo
	java -jar $(AC_JAR) -l $@

clean:
	rm -rf build
