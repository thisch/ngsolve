#include <solve.hpp>

namespace netgen
{
  int h_argc;
  char ** h_argv;
}

extern int dummy_bvp;

int main(int argc, char ** argv)
{
  if (argc < 2)
    {
      std::cout << "Usage:  ngs filename" << std::endl;
      exit(1);
    }

  netgen::h_argc = argc;
  netgen::h_argv = argv;
  dummy_bvp = 17;

  ngsolve::MyMPI mympi(argc, argv);

  ngsolve::PDE pde; 

  try
    {
      pde.LoadPDE (argv[argc-1]); 
      pde.Solve();
    }

  catch(ngstd::Exception & e)
    {
      std::cout << "Caught exception: " << std::endl
                << e.What() << std::endl;
    };


#ifdef PARALLEL
  int id;
  MPI_Comm_rank(MPI_COMM_WORLD, &id);
  char filename[100];
  sprintf (filename, "ngs.prof.%d", id);
  FILE *prof = fopen(filename,"w");
  NgProfiler::Print (prof);
  fclose(prof);
#endif


  return 0;
}

