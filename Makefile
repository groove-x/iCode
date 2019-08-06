PROJECTS:=librfal $(wildcard nfc_*)

.PHONY:all ${PROJECTS}
all:${PROJECTS}

${PROJECTS}:
	make -C $@

.SILENT:clean
clean:
	for proj in ${PROJECTS}; do\
		make -C $${proj} clean;\
	done
