# Render schematic as SVG
#
# Dependencies:
# - https://github.com/INTI-CMNB/KiBot
# - https://www.gnu.org/software/sed/

.DELETE_ON_ERROR:
.SECONDARY:
.PHONY: all

all: \
	youshiko-yc8091-xxl-radio-controlled-wall-clock-sch.svg

build/%-schematic.svg: %.kicad_sch default.kibot.yaml
	kibot -e $<

%-sch.svg: build/%-schematic.svg Makefile
	sed \
		-e 's:<title>[^<]*</title>::' \
		< $< > $@
