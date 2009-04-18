build:
	g++ -o libfi libfi.cpp `xml2-config --cflags` `xml2-config --libs`

