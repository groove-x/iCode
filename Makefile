all:librfal nfc_writer

PROJECTS:=librfal nfc_writer

.PHONY:librfal nfc_writer
librfal:
	make -C librfal

nfc_writer:
	make -C nfc_writer

.SILENT:clean
clean:
	for proj in ${PROJECTS}; do\
		make -C $${proj} clean;\
	done
