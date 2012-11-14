objects = all_in_one.o demo_instat.o demo_stokes.o myElement.o \
myHOElement.o myIntegrator.o demo_coupling.o demo_coupling_adv.o demo_nonlinear.o  \
myFESpace.o myHOFESpace.o myPreconditioner.o myAssembling.o linhypDG.o 

# linhypDG_par.o 


%.o : %.cpp
	ngscxx -I. -c $? -o $@


libmyngsolve.so : $(objects)
	ngscxx -shared $(objects) -lngsolve -o $@

clean:
	rm *.o */*.o libmyngsolve.so



dist:
	cd ..; tar -czf MyLittleNGSolve-5.0.0.tar.gz --exclude-vcs my_little_ngsolve-5.0/Makefile my_little_ngsolve-5.0/*.cpp my_little_ngsolve-5.0/*.hpp my_little_ngsolve-5.0/*.pde my_little_ngsolve-5.0/*.vol my_little_ngsolve-5.0/*.in2d my_little_ngsolve-5.0/windows/my_little_ngsolve*  my_little_ngsolve-5.0/mixed

