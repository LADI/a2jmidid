TOP:=$(shell pwd)

.PHONY: a2jmidi
a2jmidi:
	rm -rf jack2
	git clone -b $(shell git symbolic-ref --short HEAD) --depth=1 --recurse-submodules --shallow-submodules https://github.com/LADI/jack2 build/jack2
	cd build/jack2 && python3 ./waf configure --prefix=$(TOP)/build/destdir/usr
	cd build/jack2 && python3 ./waf
	cd build/jack2 && python3 ./waf install
	PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:$(TOP)/build/destdir/usr/lib/pkgconfig python3 ./waf configure --prefix=$(TOP)/build/destdir/usr
	python3 ./waf
	python3 ./waf install
