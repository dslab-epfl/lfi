build:
	g++ -Wall -o libfi libfi.cpp `xml2-config --cflags` `xml2-config --libs`

clean:
	rm -f inter.c.* intercept.stub*