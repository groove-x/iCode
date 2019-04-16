ARCH?=arm64

build:
	cd iCodeDemo/build && cmake -DARCH=${ARCH} .. && make

clean:
	rm -rf iCodeDemo/build/*
