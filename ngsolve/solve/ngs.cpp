#include <solve.hpp>
#include <cstdlib>

namespace netgen
{
  int h_argc;
  char ** h_argv;
}

int main(int argc, char ** argv)
{
  int retcode = EXIT_SUCCESS;
  if (argc < 2)
    {
      std::cout << "Usage:  ngs filename" << std::endl;
      exit(EXIT_FAILURE);
    }

  netgen::h_argc = argc;
  netgen::h_argv = argv;


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

      retcode = EXIT_FAILURE;
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

  return retcode;
}
