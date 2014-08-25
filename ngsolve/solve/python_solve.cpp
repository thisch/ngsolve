#ifdef NGS_PYTHON
#include "../ngstd/python_ngstd.hpp"
#include <solve.hpp>
using namespace ngsolve;

/*
template <typename T> 
auto_ptr<T> make_auto_ptr(T * ptr)
{
  return auto_ptr<T> (ptr);
}
*/

BOOST_PYTHON_MODULE(Ngsolve) {
  cout << "init py - ngsolve" << endl;
  PyExportSymbolTable<shared_ptr<FESpace>> ();
  PyExportSymbolTable<shared_ptr<GridFunction>> ();
  PyExportSymbolTable<shared_ptr<BilinearForm>> ();
  PyExportSymbolTable<shared_ptr<LinearForm>> ();
  PyExportSymbolTable<shared_ptr<NumProc>> ();
  PyExportSymbolTable<double> ();
  PyExportSymbolTable<shared_ptr<double>> ();


  bp::class_<PDE> ("PDE", bp::init<>())
    .def(bp::init<const string&>())
    .def("Load", static_cast<void(ngsolve::PDE::*)(const string &, const bool, const bool)> 
         (&ngsolve::PDE::LoadPDE),
         (boost::python::arg("filename"), 
          boost::python::arg("meshload")=0, 
          boost::python::arg("nogeometryload")=0))
    .def("Mesh",  static_cast<MeshAccess&(ngsolve::PDE::* const)(int)>(&PDE::GetMeshAccess),
         bp::return_value_policy<bp::reference_existing_object>(),
         (bp::arg("nr")=0))
    .def("Solve", &ngsolve::PDE::Solve)

    .def("Add", FunctionPointer([](PDE & self, shared_ptr<GridFunction> gf)
                                {
                                  self.AddGridFunction (gf->GetName(), gf);
                                }))

    .add_property ("constants", FunctionPointer([](PDE & self) { return self.GetConstantTable(); }))
    .add_property ("variables", FunctionPointer([](PDE & self) { return self.GetVariableTable(); }))
    .add_property ("spaces", FunctionPointer([](PDE & self) { return self.GetSpaceTable(); }))
    .add_property ("gridfunctions", FunctionPointer([](PDE & self) { return self.GetGridFunctionTable(); }))
    .add_property ("bilinearforms", FunctionPointer([](PDE & self) { return self.GetBilinearFormTable(); }))
    .add_property ("linearforms", FunctionPointer([](PDE & self) { return self.GetLinearFormTable(); }))
    .add_property ("numprocs", FunctionPointer([](PDE & self) { return self.GetNumProcTable(); }))
    ;
}









#endif
