demo_instat.so: demo_instat.cpp
	gcc -O2 -shared -fpic -DNETGEN_ELTRANS -I. -I$(NETGENDIR)/../include  $? -o $@

install: 
	-cp demo_instat.so $(NETGENDIR)/../lib

