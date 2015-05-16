ALL: epgread

epgread: epgread.cpp
	g++ -pthread  epgread.cpp dvbcfg_common.c dvbcfg_zapchannel.c -o epgread -lucsi

clean:
	rm epgread