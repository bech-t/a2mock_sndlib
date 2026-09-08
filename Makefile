# --- a2mock_sndlib -------------------------------------------------------
# Bibliotheque son Mockingboard pour Apple II (cc65) + convertisseur hote.
#
# Cible cc65 : "apple2" et pas "apple2enh". apple2enh exige le 65C02 ; la lib
# vise un II+ de 1979, donc 6502 STRICT. C'est aussi pourquoi le banc d'essai
# MAME tourne sur le driver "apple2e" (//e non-enhanced, 6502) et non
# "apple2ee" (65C02) : un opcode interdit se fait voir tout de suite.

TARGET   := apple2
CL65     := cl65
CA65     := ca65
AR65     := ar65

SRCDIR   := src
INCDIR   := include
BUILDDIR := build
LIB      := $(BUILDDIR)/a2mb.lib

# -O -Os -Cl : memes reglages qu'a2adv, ou ils ont ete mesures.
#   -Cl met les locales en statique -> code plus court et plus rapide.
#   /!\ INTERDIT des qu'une fonction est recursive ou reentrante. Aucune ici ;
#       a re-verifier avant d'en introduire une.
# --- Instrumentation ------------------------------------------------------
# A2MB_DEBUG=1 compile le chronometre du handler (pic de duree, depassements).
# Outil de MISE AU POINT : quatre lectures de VIA et une soustraction 16 bits
# a chaque tick, pour un renseignement qu'un jeu fini n'utilise pas.
# Ce depot est un banc de validation, donc 1 par defaut. Un projet reel
# devrait batir la bibliotheque avec 0.
A2MB_DEBUG ?= 1

CFLAGS   := -t $(TARGET) -O -Os -Cl -I $(INCDIR)
AFLAGS   := -t $(TARGET) -I $(INCDIR)
ifeq ($(A2MB_DEBUG),1)
  CFLAGS += -DA2MB_DEBUG
  AFLAGS += -D A2MB_DEBUG=1
endif

CSRC     := $(wildcard $(SRCDIR)/*.c)
ASRC     := $(wildcard $(SRCDIR)/*.s)
OBJS     := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(CSRC)) \
            $(patsubst $(SRCDIR)/%.s,$(BUILDDIR)/%.o,$(ASRC))

.PHONY: all lib demo dsk music scores clean hosttest emu-check

all: lib demo

lib: $(LIB)

$(LIB): $(OBJS)
	@rm -f $@
	$(AR65) a $@ $(OBJS)
	@echo "Bibliotheque : $@"

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CL65) $(CFLAGS) --create-dep $(BUILDDIR)/$*.d -c -o $@ $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.s | $(BUILDDIR)
	$(CA65) $(AFLAGS) -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# --- Disquette de validation ---------------------------------------------
# UN seul programme, a menu : ProDOS ne demarre que le PREMIER fichier .SYSTEM
# de la disquette, donc plusieurs binaires separes seraient indemarrables sans
# un selecteur -- autant que le selecteur soit le programme lui-meme.
DEMOSRC  := $(wildcard demo/src/*.c)
DEMOOBJ  := $(patsubst demo/src/%.c,$(BUILDDIR)/demo_%.o,$(DEMOSRC))
DEMOBIN  := $(BUILDDIR)/mbdemo.bin
PROGRAM  := MBDEMO
VOLNAME  := A2MB
DSK      := $(BUILDDIR)/a2mb-test.dsk

# Adresse de chargement $4000 
LOADADDR ?= 0x4000

# __HIMEM__ releve a $B400 : la demo a grossi (19 bruitages, 10 morceaux,
# l'ecran de detail) et son BSS debordait du $9600 par defaut.
#
# Les tampons ProDOS vivent AU-DESSUS de HIMEM, jusqu'a la page globale
# ($BF00) : a $B400 il en reste deux, et la demo n'ouvre qu'un fichier a la
# fois (le chargement d'un module). On est donc au ras du besoin -- toute
# fonction future qui ouvrirait deux fichiers en meme temps casserait ce
# reglage, et le symptome serait un fopen qui echoue, pas un plantage.
#
# Pile a 1 Ko (defaut 2 Ko)
HIMEM     ?= 0xB400
STACKSIZE ?= 0x0400
LDFLAGS  := -t $(TARGET) --start-addr $(LOADADDR) \
            -Wl -D,__HIMEM__=$(HIMEM) -Wl -D,__STACKSIZE__=$(STACKSIZE)

AC_JAR     ?= tools/ac/ac.jar
PRODOS_TPL ?= tools/prodos/prodos.dsk

# Trois prerequis ne sont pas versionnes -- outils tiers et MIDI tiers, ce ne
# sont pas nos droits a redistribuer. Sans ces regles, make s'arreterait sur
# un « No rule to make target » qui ne dit ni quoi, ni ou. Elles ne se
# declenchent QUE si le fichier manque vraiment.
$(AC_JAR):
	@echo; echo "  MANQUANT : $@"; echo; \
	 echo "  AppleCommander ecrit les fichiers dans l'image ProDOS."; \
	 echo "  Prenez le .jar de la derniere version :"; \
	 echo "      https://github.com/AppleCommander/AppleCommander/releases"; \
	 echo "  et posez-le ici sous le nom ac.jar (cf. tools/ac/README.md)."; \
	 echo; exit 1

$(PRODOS_TPL):
	@echo; echo "  MANQUANT : $@"; echo; \
	 echo "  Il faut une image ProDOS 140 Ko amorcable, qui sert de gabarit :"; \
	 echo "  la disquette de demo en est une copie, reformatee puis remplie."; \
	 echo "      https://prodos8.com/   (ProDOS 2.4, images .dsk)"; \
	 echo "  Posez-la ici sous le nom prodos.dsk (cf. tools/prodos/README.md)."; \
	 echo; exit 1

demo/midi/%.mid:
	@echo; echo "  MANQUANT : $@"; echo; \
	 echo "  Les MIDI sources ne sont pas redistribues (travail de leurs"; \
	 echo "  sequenceurs, cf. demo/midi/SOURCES.md, qui donne la commande"; \
	 echo "  curl de chacun des huit fichiers et credite leurs auteurs)."; \
	 echo; \
	 echo "  Sans eux, 'make dsk' ne peut pas produire les six morceaux qui"; \
	 echo "  en derivent. Les quatre partitions texte de demo/scores/, elles,"; \
	 echo "  se convertissent sans rien telecharger : 'make scores'."; \
	 echo; \
	 echo "  Une disquette deja construite est dans le depot : build/a2mb-test.dsk"; \
	 echo; exit 1
LOADER     ?= tools/prodos/loader.system

demo: $(DEMOBIN)

$(DEMOBIN): $(DEMOOBJ) $(LIB)
	$(CL65) $(LDFLAGS) -o $@ $(DEMOOBJ) $(LIB)

$(BUILDDIR)/demo_%.o: demo/src/%.c | $(BUILDDIR)
	$(CL65) $(CFLAGS) -I demo/src --create-dep $(BUILDDIR)/demo_$*.d -c -o $@ $<

# Disquette ProDOS bootable. Le loader est depose sous le nom du programme
# suivi de .SYSTEM : c'est ainsi qu'il sait quoi charger.
# `ac -as` lit l'exehdr cc65 (adresse de chargement extraite, exehdr strippe).
# --- Musique -------------------------------------------------------------
# Les .A2M sont TOUJOURS regeneres depuis les partitions : les commiter, c'est
# les laisser se perimer en silence des que le convertisseur change.
# Une fausse note se corrige dans le .txt, pas dans le code.
SCORES  := $(wildcard demo/scores/*.txt)
SCORETUNES := $(patsubst demo/scores/%.txt,demo/music/%.A2M,$(SCORES))

# Un module par instrument du catalogue (cf. demo/scores/instruments/) : sert
# a l'ecran TIMBRES de la demo (instr_tunes[] dans demo/src/tests.c), separe
# de la playlist ci-dessus -- ce sont des outils de comparaison, pas des
# morceaux choisis pour la vitrine.
INSTR_SCORES := $(wildcard demo/scores/instruments/*.txt)
INSTRTUNES   := $(patsubst demo/scores/instruments/%.txt,demo/music/instruments/%.A2M,$(INSTR_SCORES))

TUNES      := $(SCORETUNES) $(INSTRTUNES)

# midi2score.py fabrique dragee3.txt et cafe3.txt a partir des MIDI ; une fois
# engendrees, ces partitions se CORRIGENT a la main -- on ne les regenere donc
# pas automatiquement, ce serait ecraser les retouches.
A2MCONV := tools/a2mconv/a2mconv.py

# Pieces tirees d'un MIDI d'orchestre : la reduction a six voies est un choix
# MUSICAL, pas un reglage technique -- elle est donc ecrite ici, en clair.
# Format d'une voie :  pistes:rang:enveloppe:transposition:gain
#   `rang` sert quand une piste joue des accords : 0 = note la plus haute.
#   Voies 0-2 -> AY #1, voies 3-5 -> AY #2, soit un vrai placement stereo.
MIDI2A2M := tools/a2mconv/midi2a2m.py

# Profil T : le lecteur fabrique les enveloppes, le flux ne porte que les
# notes. Les morceaux passent donc EN ENTIER.
#
# --ay1 / --ay2 : ALLOCATION DYNAMIQUE. Chaque AY recoit un groupe de pistes et
# choisit, a chaque trame, les trois notes a jouer parmi tout ce qui sonne --
# la plus haute (la melodie) et la plus basse (l'assise) passent toujours, le
# reste se departage aux poids. La repartition figee « une piste = une voie »
# qu'on avait avant jetait 42 % des notes ; celle-ci en jette 29 %, et surtout
# elle ne perd plus une seule note de melodie.
#
# Format d'un groupe :  pistes:instrument:poids, ...
# Le decoupage en deux groupes reste un placement STEREO : chaque AY a sa
# propre sortie audio.

# --- Regles conservees mais HORS disquette -------------------------------
# DRAGEE, CAFE et ANDROIDS ne sont plus dans la liste du lecteur : les deux
# Casse-Noisette parce que leur identite est dans le TIMBRE (celesta, cordes
# en sourdine) et qu'une onde carree n'en restitue rien ; ANDROIDS parce que
# 12,6 Ko pour cinquante secondes de buzzer mettent mal la carte en valeur.
#
# Les regles restent : elles se batissent a la demande
# (`make demo/music/ANDROIDS.A2M`) et sont le seul cas d'essai des chemins
# « reduction d'orchestre a six voix » et « dump YM ».

# Fee Dragee : le celesta a l'AY #1 pour lui seul (il monte a sept notes
# simultanees), tout l'orchestre a l'AY #2. Transpose d'une octave : au-dessus
# de do6 la quantification 12 bits de l'AY sonne faux de 18 a 36 cents.
demo/music/DRAGEE.A2M: demo/midi/sugarplum.mid $(MIDI2A2M) tools/a2mconv/midi.py tools/a2mconv/a2m.py
	@mkdir -p demo/music
	@python3 $(MIDI2A2M) $< -o $@ --title "FEE DRAGEE" --author TCHAIKOVSKI \
	  --seconds 300 --transpose1 -12 \
	  --ay1 "8:pluck:10" \
	  --ay2 "9:soft:7,10:soft:5,1+2:soft:6,3+4:soft:4,6+7:soft:3,11:pluck:5,12+13:bass:8"

# Cafe (Danse arabe) : vents a gauche (cor anglais et hautbois portent la
# melodie), cordes a droite avec l'ostinato d'alto et le bourdon des graves.
demo/music/CAFE.A2M: demo/midi/arabian.mid $(MIDI2A2M) tools/a2mconv/midi.py tools/a2mconv/a2m.py
	@mkdir -p demo/music
	@python3 $(MIDI2A2M) $< -o $@ --title "CAFE ARABE" --author TCHAIKOVSKI \
	  --seconds 300 \
	  --ay1 "7+3:soft:10,4+5:soft:7,1+2:soft:6" \
	  --ay2 "13:sustain:8,14:sustain:8,12:pluck:6,10+11:soft:5,8+6:soft:4"

# Un dump YM d'Atari ST. Profil R obligatoirement : un YM ne dit rien de ses
# instruments, seulement l'etat des registres. Tronque a 50 s pour tenir dans
# le tampon de 14 Ko -- un YM de tracker remue presque tous les registres a
# chaque trame, d'ou ~300 o/s la ou une reduction MIDI en profil T en fait 30.
YM2A2M := tools/a2mconv/ym2a2m.py

demo/music/ANDROIDS.A2M: demo/ym/androids.ym $(YM2A2M) tools/a2mconv/ym.py tools/a2mconv/a2m.py
	@mkdir -p demo/music
	@python3 $(YM2A2M) $< -o $@ --seconds 50 --report

# --- Le repertoire de demonstration --------------------------------------
# Choisi pour ce que chaque piece fait ressortir de la carte, pas au hasard :
#   BACH    contrepoint a quatre voix, deux a gauche deux a droite : on ENTEND
#           les voix se separer. L'argument de la polyphonie.
#   GRIEG   part pianissimo, ajoute les voix, finit fortissimo. Un arc de demo.
#   JOPLIN  basse stride + melodie syncopee = trois voix naturelles, et l'onde
#           carree va remarquablement au piano bastringue.
#   CANCAN  energie et vitesse.
#   FOSSILE le theme de xylophone EST notre enveloppe `pluck` (et Saint-Saens
#           y cite sa propre Danse macabre).
#   BOURDON doubles croches en continu : epreuve de vitesse pour le tick.
# Toutes dans le domaine public sans ambiguite (auteurs morts avant 1930).

demo/music/BACH.A2M: demo/midi/bachfugue.mid $(MIDI2A2M) tools/a2mconv/midi.py tools/a2mconv/a2m.py
	@mkdir -p demo/music
	@python3 $(MIDI2A2M) $< -o $@ --profile T --title "PETITE FUGUE" --author "J S BACH" --seconds 110 \
	  --voices "1:0:sustain:0:0,1:1:sustain:0:-1,2:0:sustain:0:-1,3:0:sustain:0:-1,4:0:bass:0:0,2:1:sustain:0:-2"

demo/music/GRIEG.A2M: demo/midi/mountainking.mid $(MIDI2A2M) tools/a2mconv/midi.py tools/a2mconv/a2m.py
	@mkdir -p demo/music
	@python3 $(MIDI2A2M) $< -o $@ --profile T --title "ANTRE DU ROI" --author GRIEG --seconds 90 \
	  --voices "1:0:pluck:0:0,1:1:pluck:0:-1,1:2:pluck:0:-2,1:3:pluck:0:-2,1:4:bass:0:-1,1:5:bass:0:0"

demo/music/JOPLIN.A2M: demo/midi/entertainer.mid $(MIDI2A2M) tools/a2mconv/midi.py tools/a2mconv/a2m.py
	@mkdir -p demo/music
	@python3 $(MIDI2A2M) $< -o $@ --profile T --title "THE ENTERTAINER" --author JOPLIN --seconds 100 \
	  --voices "1:0:pluck:0:0,1:1:pluck:0:-2,1:2:pluck:0:-3,1:3:pluck:0:-3,1:4:bass:0:-1,1:5:bass:0:0"

demo/music/CANCAN.A2M: demo/midi/cancan.mid $(MIDI2A2M) tools/a2mconv/midi.py tools/a2mconv/a2m.py
	@mkdir -p demo/music
	@python3 $(MIDI2A2M) $< -o $@ --profile T --title "GALOP INFERNAL" --author OFFENBACH --seconds 90 \
	  --voices "1:0:pluck:0:0,1:1:pluck:0:-2,1:2:pluck:0:-3,2:0:bass:0:0,2:1:pluck:0:-2,2:2:pluck:0:-3"

demo/music/FOSSILE.A2M: demo/midi/fossiles.mid $(MIDI2A2M) tools/a2mconv/midi.py tools/a2mconv/a2m.py
	@mkdir -p demo/music
	@python3 $(MIDI2A2M) $< -o $@ --title "FOSSILES" --author "SAINT-SAENS" --seconds 100 \
	  --ay1 "6:pluck:10,7:pluck:7,4:soft:6" \
	  --ay2 "12+13:soft:7,14:soft:6,15+16:bass:9,8+9+10:pluck:5"

demo/music/BOURDON.A2M: demo/midi/bumblebee.mid $(MIDI2A2M) tools/a2mconv/midi.py tools/a2mconv/a2m.py
	@mkdir -p demo/music
	@python3 $(MIDI2A2M) $< -o $@ --title "VOL DU BOURDON" --author "RIMSKI-KORSAKOV" --seconds 95 \
	  --ay1 "1:pluck:10,2:pluck:7,14:pluck:6" \
	  --ay2 "7:soft:8,4:soft:6,5+9+11:soft:5"

MIDITUNES := demo/music/BACH.A2M demo/music/GRIEG.A2M demo/music/JOPLIN.A2M \
             demo/music/CANCAN.A2M demo/music/FOSSILE.A2M demo/music/BOURDON.A2M
TUNES     += $(MIDITUNES)

music: $(TUNES)

# Les seules partitions texte : convertibles sur un clone nu, sans un seul
# telechargement. C'est la cible a proposer a qui decouvre le depot.
scores: $(SCORETUNES) $(INSTRTUNES)

# Les partitions texte donnent des modules a TROIS voix (un seul AY) en profil
# T : quelques centaines d'octets, et l'AY #2 reste libre pour les bruitages.
# Le nom du fichier suit celui de la partition, en majuscules.
demo/music/%.A2M: demo/scores/%.txt $(A2MCONV) tools/a2mconv/a2m.py tools/a2mconv/render.py
	@mkdir -p demo/music
	@python3 $(A2MCONV) $< -o $@

demo/music/instruments/%.A2M: demo/scores/instruments/%.txt $(A2MCONV) tools/a2mconv/a2m.py tools/a2mconv/render.py
	@mkdir -p demo/music/instruments
	@python3 $(A2MCONV) $< -o $@

dsk: $(DSK)

$(DSK): $(DEMOBIN) $(TUNES) $(PRODOS_TPL) $(AC_JAR) $(LOADER) | $(BUILDDIR)
	cp $(PRODOS_TPL) $@
	java -jar $(AC_JAR) -n  $@ $(VOLNAME)
	java -jar $(AC_JAR) -as $@ $(PROGRAM)            < $(DEMOBIN)
	java -jar $(AC_JAR) -p  $@ $(PROGRAM).SYSTEM sys < $(LOADER)
	@# Les modules, en type BIN/aux 0 : donnee brute, pas une image a charger
	@# telle quelle. Le nom sur la disquette est celui qu'attend tests.c.
	@for m in $(SCORETUNES) $(MIDITUNES); do \
	  n=$$(basename $$m); \
	  echo "  + $$n"; \
	  java -jar $(AC_JAR) -p $@ $$n bin 0 < $$m; \
	done
	@# TIMBRES/ : sous-repertoire ProDOS, pour ne compter que pour UNE entree
	@# dans la racine (25 max sur ce gabarit -- programme + 10 morceaux +
	@# 15 timbres a plat l'auraient depassee). `-p` avec un "/" dans le nom
	@# cree le sous-repertoire tout seul. Les chemins ici doivent correspondre
	@# EXACTEMENT a instr_tunes[] dans demo/src/tests.c.
	@for m in $(INSTRTUNES); do \
	  n=TIMBRES/$$(basename $$m); \
	  echo "  + $$n"; \
	  java -jar $(AC_JAR) -p $@ $$n bin 0 < $$m; \
	done
	@echo
	java -jar $(AC_JAR) -l $@
	@echo "Disquette prete : $@   (volume /$(VOLNAME)/)"

# --- Tests hote ----------------------------------------------------------
# Compile la lib avec gcc (les acces materiel se replient sur des no-op) pour
# exercer la logique portable. Ne teste evidemment pas le pilotage de la carte.
hosttest: | $(BUILDDIR)
	gcc -std=c99 -Wall -Wextra -I$(INCDIR) -o $(BUILDDIR)/hosttest \
	    test/hosttest.c $(CSRC) -lm
	$(BUILDDIR)/hosttest

# --- Banc d'essai --------------------------------------------------------
emu-check:
	@echo "== MAME =="; mame -verifyroms apple2e 2>&1 | tail -8 || true
	@echo "== AppleWin =="; \
	  [ -x "$${APPLEWIN:-/mnt/h/Thomas/dev2/apple2/AppleWin1.30.13.0/AppleWin.exe}" ] \
	    && echo "  present" || echo "  ABSENT"

# L'image disque est VERSIONNEE (cf. .gitignore) : la supprimer ici la ferait
# disparaitre du depot au premier commit suivant un `make clean`. On vide donc
# tout le reste et on la laisse ; `make dsk` la reecrira.
clean:
	find $(BUILDDIR) -mindepth 1 ! -name '$(notdir $(DSK))' -delete

-include $(wildcard $(BUILDDIR)/*.d)
