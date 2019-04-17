PROJECTS:=librfal nfc_reader nfc_writer

all:${PROJECTS}

.PHONY:librfal nfc_reader nfc_writer
librfal:
	make -C librfal

nfc_reader:
	make -C nfc_reader

nfc_writer:
	make -C nfc_writer

.SILENT:clean
clean:
	for proj in ${PROJECTS}; do\
		make -C $${proj} clean;\
	done
