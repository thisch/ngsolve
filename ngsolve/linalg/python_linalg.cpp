#ifdef NGS_PYTHON
#include "../ngstd/python_ngstd.hpp"
#include <boost/python/slice.hpp>
#include <la.hpp>
using namespace ngla;





void ExportNgla() {
    std::string nested_name = "la";
    if( bp::scope() )
         nested_name = bp::extract<std::string>(bp::scope().attr("__name__") + ".la");
    
    bp::object module(bp::handle<>(bp::borrowed(PyImport_AddModule(nested_name.c_str()))));

    cout << "exporting la as " << nested_name << endl;
    bp::object parent = bp::scope() ? bp::scope() : bp::import("__main__");
    parent.attr("la") = module ;

    bp::scope ngla_scope(module);

    bp::object expr_module = bp::import("ngsolve.__expr");
    bp::object expr_namespace = expr_module.attr("__dict__");


  
  bp::class_<BaseVector, shared_ptr<BaseVector>, boost::noncopyable>("BaseVector", bp::no_init)
    .def("__str__", &ToString<BaseVector>)
    .add_property("size", &BaseVector::Size)
    .def("CreateVector", FunctionPointer( [] ( BaseVector & self)
        { return shared_ptr<BaseVector>(self.CreateVector()); } ))

    .def("Assign", FunctionPointer([](BaseVector & self, BaseVector & v2, double s)->void { self.Set(s, v2); }))
    .def("Add", FunctionPointer([](BaseVector & self, BaseVector & v2, double s)->void { self.Add(s, v2); }))
    .add_property("expr", bp::object(expr_namespace["VecExpr"]) )
    .add_property("data", bp::object(expr_namespace["VecExpr"]), bp::object(expr_namespace["expr_data"] ))
    .def("__add__" , bp::object(expr_namespace["expr_add"]) )
    .def("__sub__" , bp::object(expr_namespace["expr_sub"]) )
    .def("__rmul__" , bp::object(expr_namespace["expr_rmul"]) )
    .def("__getitem__", FunctionPointer( [](BaseVector & self,  bp::slice inds ) {
        return self.FVDouble();
    } ))
    .def(bp::self+=bp::self)
    .def(bp::self-=bp::self)
    .def(bp::self*=double())
    .def("InnerProduct", FunctionPointer( [](BaseVector & self, BaseVector & other)
                                          {
                                            return bp::object (InnerProduct (self, other));
                                          }))
    ;       

  // bp::def("InnerProduct", FunctionPointer([](BaseVector & v1, BaseVector & v2)->double { return InnerProduct(v1,v2); }))
  bp::def ("InnerProduct",
           FunctionPointer( [] (bp::object x, bp::object y) -> bp::object
                            { return x.attr("InnerProduct") (y); }));
  ;
  

  typedef BaseMatrix BM;
  typedef BaseVector BV;

  bp::class_<BaseMatrix, shared_ptr<BaseMatrix>, boost::noncopyable>("BaseMatrix", bp::no_init)
    .def("__str__", &ToString<BaseMatrix>)
    .add_property("height", &BaseMatrix::Height)
    .add_property("width", &BaseMatrix::Width)

    .def("CreateMatrix", &BaseMatrix::CreateMatrix)

    .def("CreateRowVector", FunctionPointer( [] ( BaseMatrix & self)
        { return shared_ptr<BaseVector>(self.CreateRowVector()); } ))
    .def("CreateColVector", FunctionPointer( [] ( BaseMatrix & self)
        { return shared_ptr<BaseVector>(self.CreateColVector()); } ))

    .def("AsVector", FunctionPointer( [] (BM & m)
                                      {
                                        return shared_ptr<BaseVector> (&m.AsVector(), NOOP_Deleter);
                                      }))
    .def("Mult",        FunctionPointer( [](BM &m, BV &x, BV &y, double s) { m.Mult (x,y); y *= s; }) )
    .def("MultAdd",     FunctionPointer( [](BM &m, BV &x, BV &y, double s) { m.MultAdd (s, x, y); }))
    // .def("MultTrans",   FunctionPointer( [](BM &m, BV &x, BV &y, double s) { y  = s*Trans(m)*x; }) )
    // .def("MultTransAdd",FunctionPointer( [](BM &m, BV &x, BV &y, double s) { y += s*Trans(m)*x; }) )

    .add_property("expr", bp::object(expr_namespace["MatExpr"]) )
    .def("__mul__" , bp::object(expr_namespace["expr_mul"]) )
    .def("__rmul__" , bp::object(expr_namespace["expr_rmul"]) )

    .def("__iadd__", FunctionPointer( [] (BM &m, BM &m2) { 
        m.AsVector()+=m2.AsVector();
    }))

    .def("Inverse", FunctionPointer( [](BM &m, BitArray & freedofs)->shared_ptr<BaseMatrix>
                                     { return m.InverseMatrix(&freedofs); }))
    // bp::return_value_policy<bp::manage_new_object>(),
    // (bp::arg("self"), bp::arg("freedofs")))
    .def("Inverse", FunctionPointer( [](BM &m)->shared_ptr<BaseMatrix>
                                     { return m.InverseMatrix(); }))
    // bp::return_value_policy<bp::manage_new_object>())
    ;
  
}



void ExportNgbla();

BOOST_PYTHON_MODULE(libngla) {
  // ExportNgbla();
  ExportNgla();
}






#endif // NGS_PYTHON