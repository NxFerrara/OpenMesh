#include <Python.h>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/bind.hpp>
#include <boost/python.hpp>
#include <boost/python/iterator.hpp>
#include <boost/python/return_internal_reference.hpp>
#include <boost/python/copy_const_reference.hpp>
#include "OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh"
#include "OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh"
#include "OpenMesh/Core/Utils/PropertyManager.hh"

using namespace boost::python;

struct MeshTraits : public OpenMesh::DefaultTraits
{
	/// Use double precision points
	typedef OpenMesh::Vec3d Point;

	/// Use double precision Normals
	typedef OpenMesh::Vec3d Normal;

	/// Use RGBA Color
	typedef OpenMesh::Vec4f Color;
};

typedef OpenMesh::TriMesh_ArrayKernelT<MeshTraits> TriMesh;
typedef OpenMesh::PolyMesh_ArrayKernelT<MeshTraits> PolyMesh;

typedef OpenMesh::BaseHandle BaseHandle;
typedef OpenMesh::VertexHandle VertexHandle;
typedef OpenMesh::HalfedgeHandle HalfedgeHandle;
typedef OpenMesh::EdgeHandle EdgeHandle;
typedef OpenMesh::FaceHandle FaceHandle;

template <class Vector, class Scalar>
void set_item(Vector& _vec, int _index, Scalar _value) {
	if (_index < 0) {
		_index += _vec.size();
	}

	if ((size_t)_index < _vec.size()) {
		_vec[_index] = _value;
	}
	else {
		PyErr_SetString(PyExc_IndexError, "index out of range");
		throw_error_already_set();
	}
}

template <class Vector, class Scalar>
Scalar get_item(Vector& _vec, int _index) {
	if (_index < 0) {
		_index += _vec.size();
	}

	if ((size_t)_index < _vec.size()) {
		return _vec[_index];
	}
	else {
		PyErr_SetString(PyExc_IndexError, "index out of range");
		throw_error_already_set();
	}

	return 0.0;
}

template<class Iterator, size_t (OpenMesh::ArrayKernel::*n_items)() const>
class IteratorWrapperT {
	public:
		IteratorWrapperT(const OpenMesh::PolyConnectivity& _mesh) :
			mesh_(_mesh), n_items_(n_items),
			iterator_(_mesh, typename Iterator::value_type(0)),
			iterator_end_(_mesh, typename Iterator::value_type(int((_mesh.*n_items)()))) {
		}

		IteratorWrapperT iter() const {
			return *this;
		}

		typename Iterator::value_type next() {
			if (iterator_ != iterator_end_) {
				typename Iterator::value_type res = *iterator_;
				++iterator_;
				return res;
			}
			else {
				PyErr_SetString(PyExc_StopIteration, "No more data.");
				throw_error_already_set();
			}
			return typename Iterator::value_type();
		}

		unsigned int len() const {
			return (mesh_.*n_items_)();
		}

	private:
		const OpenMesh::PolyConnectivity& mesh_;
		size_t (OpenMesh::ArrayKernel::*n_items_)() const;
		Iterator iterator_;
		Iterator iterator_end_;
};

template<class Circulator>
class CirculatorWrapperT {
	public:
		CirculatorWrapperT(const typename Circulator::mesh_type& _mesh, typename Circulator::center_type _center) :
			circulator_(_mesh, _center) {
		}

		CirculatorWrapperT iter() const {
			return *this;
		}

		typename Circulator::value_type next() {
			if (circulator_.is_valid()) {
				typename Circulator::value_type res = *circulator_;
				++circulator_;
				return res;
			}
			else {
				PyErr_SetString(PyExc_StopIteration, "No more data.");
				throw_error_already_set();
			}
			return typename Circulator::value_type();
		}

	private:
		Circulator circulator_;
};

template<class Mesh>
class MeshWrapperT : public Mesh {
	public:
		IteratorWrapperT<OpenMesh::PolyConnectivity::VertexIter, &OpenMesh::ArrayKernel::n_vertices> vertices() const {
			return IteratorWrapperT<OpenMesh::PolyConnectivity::VertexIter, &OpenMesh::ArrayKernel::n_vertices>(*this);
		}

		IteratorWrapperT<OpenMesh::PolyConnectivity::HalfedgeIter, &OpenMesh::ArrayKernel::n_halfedges> halfedges() const {
			return IteratorWrapperT<OpenMesh::PolyConnectivity::HalfedgeIter, &OpenMesh::ArrayKernel::n_halfedges>(*this);
		}

		IteratorWrapperT<OpenMesh::PolyConnectivity::EdgeIter, &OpenMesh::ArrayKernel::n_edges> edges() const {
			return IteratorWrapperT<OpenMesh::PolyConnectivity::EdgeIter, &OpenMesh::ArrayKernel::n_edges>(*this);
		}

		IteratorWrapperT<OpenMesh::PolyConnectivity::FaceIter, &OpenMesh::ArrayKernel::n_faces> faces() const {
			return IteratorWrapperT<OpenMesh::PolyConnectivity::FaceIter, &OpenMesh::ArrayKernel::n_faces>(*this);
		}

		CirculatorWrapperT<typename Mesh::VertexVertexIter> vv(VertexHandle _handle) const {
			return CirculatorWrapperT<typename Mesh::VertexVertexIter>(*this, _handle);
		}

		CirculatorWrapperT<typename Mesh::VertexFaceIter> vf(VertexHandle _handle) const {
			return CirculatorWrapperT<typename Mesh::VertexFaceIter>(*this, _handle);
		}

		CirculatorWrapperT<typename Mesh::FaceVertexIter> fv(FaceHandle _handle) const {
			return CirculatorWrapperT<typename Mesh::FaceVertexIter>(*this, _handle);
		}

		void garbage_collection() {
			Mesh::garbage_collection();
		}
};

template<class PropertyManager, class IndexHandle>
class PropertyManagerWrapperT : public PropertyManager {
	public:
		PropertyManagerWrapperT(OpenMesh::PolyConnectivity &_mesh, const char *_propname, bool _existing = false) :
			PropertyManager(_mesh, _propname, _existing) {
		}

		PropertyManagerWrapperT() : PropertyManager() { }

		object getitem(IndexHandle _handle) const {
			return (*this)[_handle];
		}

		void setitem(IndexHandle _handle, object _item) {
			(*this)[_handle] = _item;
		}
};

void expose_vec3d() {
	struct VecFn {
		static OpenMesh::Vec3d *makeNullPoint() { return new OpenMesh::Vec3d(0, 0, 0); }
	};

	double (OpenMesh::Vec3d::*max1)() const = &OpenMesh::Vec3d::max;
	OpenMesh::Vec3d (OpenMesh::Vec3d::*max2)(const OpenMesh::Vec3d&) const = &OpenMesh::Vec3d::max;
	double (OpenMesh::Vec3d::*min1)() const = &OpenMesh::Vec3d::min;
	OpenMesh::Vec3d (OpenMesh::Vec3d::*min2)(const OpenMesh::Vec3d&) const = &OpenMesh::Vec3d::min;

	OpenMesh::Vec3d& (OpenMesh::Vec3d::*maximize)(const OpenMesh::Vec3d&) = &OpenMesh::Vec3d::maximize;
	OpenMesh::Vec3d& (OpenMesh::Vec3d::*minimize)(const OpenMesh::Vec3d&) = &OpenMesh::Vec3d::minimize;
	OpenMesh::Vec3d& (OpenMesh::Vec3d::*normalize)() = &OpenMesh::Vec3d::normalize;
	OpenMesh::Vec3d& (OpenMesh::Vec3d::*normalize_cond)() = &OpenMesh::Vec3d::normalize_cond;

	class_<OpenMesh::Vec3d>("Vec3d", init<double, double, double>())
		.def("__init__", make_constructor(&VecFn::makeNullPoint))
		.def(self_ns::str(self))
		.def(self == self)
		.def(self != self)
		.def(self < self)
		.def(self * self)
		.def(self / self)
		.def(self + self)
		.def(self - self)
		.def(self *= self)
		.def(self /= self)
		.def(self += self)
		.def(self -= self)
		.def(double() * self)
		.def(self * double())
		.def(self / double())
		.def(self *= double())
		.def(self /= double())
		.def("dot", &OpenMesh::Vec3d::operator|)
		.def("cross", &OpenMesh::Vec3d::operator%)
		.def("length", &OpenMesh::Vec3d::length)
		.def("norm", &OpenMesh::Vec3d::norm)
		.def("sqrnorm", &OpenMesh::Vec3d::sqrnorm)
		.def("l1_norm", &OpenMesh::Vec3d::l1_norm)
		.def("l8_norm", &OpenMesh::Vec3d::l8_norm)
		.def("max", max1)
		.def("max", max2)
		.def("min", min1)
		.def("min", min2)
		.def("maximize", maximize, return_internal_reference<>())
		.def("minimize", minimize, return_internal_reference<>())
		.def("mean", &OpenMesh::Vec3d::mean)
		.def("mean_abs", &OpenMesh::Vec3d::mean_abs)
		.def("max_abs", &OpenMesh::Vec3d::max_abs)
		.def("min_abs", &OpenMesh::Vec3d::min_abs)
		.def("maximized", &OpenMesh::Vec3d::maximized)
		.def("minimized", &OpenMesh::Vec3d::minimized)
		.def("normalize", normalize, return_internal_reference<>())
		.def("normalize_cond", normalize_cond, return_internal_reference<>())
		.def("normalized", &OpenMesh::Vec3d::normalized)
		.def("__setitem__", &set_item<OpenMesh::Vec3d, double>)
		.def("__getitem__", &get_item<OpenMesh::Vec3d, double>)
		;
}

void expose_handles() {
	class_<BaseHandle>("BaseHandle")
		.def("idx", &BaseHandle::idx)
		.def("is_valid", &BaseHandle::is_valid)
		.def("reset", &BaseHandle::reset)
		.def("invalidate", &BaseHandle::invalidate)
		.def(self == self)
		.def(self != self)
		.def(self < self)
		;
	class_<VertexHandle, bases<BaseHandle> >("VertexHandle");
	class_<HalfedgeHandle, bases<BaseHandle> >("HalfedgeHandle");
	class_<EdgeHandle, bases<BaseHandle> >("EdgeHandle");
	class_<FaceHandle, bases<BaseHandle> >("FaceHandle");
}

template<typename Mesh>
void expose_openmesh_type(const char *_name) {
	VertexHandle   (Mesh::*vertex_handle_uint  )(unsigned int  ) const = &Mesh::vertex_handle;
	HalfedgeHandle (Mesh::*halfedge_handle_uint)(unsigned int  ) const = &Mesh::halfedge_handle;
	EdgeHandle     (Mesh::*edge_handle_uint    )(unsigned int  ) const = &Mesh::edge_handle;
	FaceHandle     (Mesh::*face_handle_uint    )(unsigned int  ) const = &Mesh::face_handle;
	HalfedgeHandle (Mesh::*halfedge_handle_vh  )(VertexHandle  ) const = &Mesh::halfedge_handle;
	HalfedgeHandle (Mesh::*halfedge_handle_fh  )(FaceHandle    ) const = &Mesh::halfedge_handle;

	EdgeHandle     (Mesh::*edge_handle_heh     )(HalfedgeHandle) const = &Mesh::edge_handle;
	FaceHandle     (Mesh::*face_handle_heh     )(HalfedgeHandle) const = &Mesh::face_handle;

	HalfedgeHandle (Mesh::*halfedge_handle_eh_uint)(EdgeHandle, unsigned int) const = &Mesh::halfedge_handle;
	HalfedgeHandle (Mesh::*prev_halfedge_handle_heh)(HalfedgeHandle) const = &Mesh::prev_halfedge_handle;

	void (Mesh::*set_halfedge_handle_vh_heh)(VertexHandle, HalfedgeHandle) = &Mesh::set_halfedge_handle;
	void (Mesh::*set_halfedge_handle_fh_heh)(FaceHandle, HalfedgeHandle  ) = &Mesh::set_halfedge_handle;

	const typename Mesh::Point& (Mesh::*point_vh)(VertexHandle) const = &Mesh::point;
	FaceHandle (Mesh::*add_face)(VertexHandle, VertexHandle, VertexHandle) = &Mesh::add_face;
	class_<Mesh> classMeshT = class_<Mesh>(_name);

	/*
	 * It is important that we enter the scope before we add
	 * the definitions because in some of the builders classes
	 * which are supposed to be inside the scope are defined.
	 */
	scope scope_MeshT = classMeshT;

	classMeshT
		.def("vertex_handle", vertex_handle_uint)
		.def("halfedge_handle", halfedge_handle_uint)
		.def("edge_handle", edge_handle_uint)
		.def("face_handle", face_handle_uint)

		.def("clear", &Mesh::clear)
		.def("clean", &Mesh::clean)
		.def("garbage_collection", &Mesh::garbage_collection)

		.def("n_vertices", &Mesh::n_vertices)
		.def("n_halfedges", &Mesh::n_halfedges)
		.def("n_edges", &Mesh::n_edges)
		.def("n_faces", &Mesh::n_faces)
		.def("vertices_empty", &Mesh::vertices_empty)
		.def("halfedges_empty", &Mesh::halfedges_empty)
		.def("edges_empty", &Mesh::edges_empty)
		.def("faces_empty", &Mesh::faces_empty)

		.def("halfedge_handle", halfedge_handle_vh)
		.def("set_halfedge_handle", set_halfedge_handle_vh_heh)
		.def("point", point_vh, return_value_policy<copy_const_reference>())
		.def("set_point", &Mesh::set_point)

		.def("vertices", &Mesh::vertices)
		.def("halfedges", &Mesh::halfedges)
		.def("edges", &Mesh::edges)
		.def("faces", &Mesh::faces)

		.def("to_vertex_handle", &Mesh::to_vertex_handle)
		.def("from_vertex_handle", &Mesh::from_vertex_handle)
		.def("set_vertex_handle", &Mesh::set_vertex_handle)
		.def("face_handle", face_handle_heh)
		.def("set_face_handle", &Mesh::set_face_handle)
		.def("next_halfedge_handle", &Mesh::next_halfedge_handle)
		.def("set_next_halfedge_handle", &Mesh::set_next_halfedge_handle)
		.def("prev_halfedge_handle", prev_halfedge_handle_heh)
		.def("opposite_halfedge_handle", &Mesh::opposite_halfedge_handle)
		.def("ccw_rotated_halfedge_handle", &Mesh::ccw_rotated_halfedge_handle)
		.def("cw_rotated_halfedge_handle", &Mesh::cw_rotated_halfedge_handle)
		.def("edge_handle", edge_handle_heh)

		.def("halfedge_handle", halfedge_handle_eh_uint)

		.def("halfedge_handle", halfedge_handle_fh)
		.def("set_halfedge_handle", set_halfedge_handle_fh_heh)

		.def("request_vertex_normals", &Mesh::request_vertex_normals)
		.def("request_vertex_colors", &Mesh::request_vertex_colors)
		.def("request_vertex_texcoords1D", &Mesh::request_vertex_texcoords1D)
		.def("request_vertex_texcoords2D", &Mesh::request_vertex_texcoords2D)
		.def("request_vertex_texcoords3D", &Mesh::request_vertex_texcoords3D)
		.def("request_vertex_status", &Mesh::request_vertex_status)
		.def("request_halfedge_status", &Mesh::request_halfedge_status)
		.def("request_halfedge_normals", &Mesh::request_halfedge_normals)
		.def("request_halfedge_colors", &Mesh::request_halfedge_colors)
		.def("request_halfedge_texcoords1D", &Mesh::request_halfedge_texcoords1D)
		.def("request_halfedge_texcoords2D", &Mesh::request_halfedge_texcoords2D)
		.def("request_halfedge_texcoords3D", &Mesh::request_halfedge_texcoords3D)
		.def("request_edge_status", &Mesh::request_edge_status)
		.def("request_edge_colors", &Mesh::request_edge_colors)
		.def("request_face_normals", &Mesh::request_face_normals)
		.def("request_face_colors", &Mesh::request_face_colors)
		.def("request_face_status", &Mesh::request_face_status)
		.def("request_face_texture_index", &Mesh::request_face_texture_index)

		.def("release_vertex_normals", &Mesh::release_vertex_normals)
		.def("release_vertex_colors", &Mesh::release_vertex_colors)
		.def("release_vertex_texcoords1D", &Mesh::release_vertex_texcoords1D)
		.def("release_vertex_texcoords2D", &Mesh::release_vertex_texcoords2D)
		.def("release_vertex_texcoords3D", &Mesh::release_vertex_texcoords3D)
		.def("release_vertex_status", &Mesh::release_vertex_status)
		.def("release_halfedge_status", &Mesh::release_halfedge_status)
		.def("release_halfedge_normals", &Mesh::release_halfedge_normals)
		.def("release_halfedge_colors", &Mesh::release_halfedge_colors)
		.def("release_halfedge_texcoords1D", &Mesh::release_halfedge_texcoords1D)
		.def("release_halfedge_texcoords2D", &Mesh::release_halfedge_texcoords2D)
		.def("release_halfedge_texcoords3D", &Mesh::release_halfedge_texcoords3D)
		.def("release_edge_status", &Mesh::release_edge_status)
		.def("release_edge_colors", &Mesh::release_edge_colors)
		.def("release_face_normals", &Mesh::release_face_normals)
		.def("release_face_colors", &Mesh::release_face_colors)
		.def("release_face_status", &Mesh::release_face_status)
		.def("release_face_texture_index", &Mesh::release_face_texture_index)

		.def("has_vertex_normals", &Mesh::has_vertex_normals)
		.def("has_vertex_colors", &Mesh::has_vertex_colors)
		.def("has_vertex_texcoords1D", &Mesh::has_vertex_texcoords1D)
		.def("has_vertex_texcoords2D", &Mesh::has_vertex_texcoords2D)
		.def("has_vertex_texcoords3D", &Mesh::has_vertex_texcoords3D)
		.def("has_vertex_status", &Mesh::has_vertex_status)
		.def("has_halfedge_status", &Mesh::has_halfedge_status)
		.def("has_halfedge_normals", &Mesh::has_halfedge_normals)
		.def("has_halfedge_colors", &Mesh::has_halfedge_colors)
		.def("has_halfedge_texcoords1D", &Mesh::has_halfedge_texcoords1D)
		.def("has_halfedge_texcoords2D", &Mesh::has_halfedge_texcoords2D)
		.def("has_halfedge_texcoords3D", &Mesh::has_halfedge_texcoords3D)
		.def("has_edge_status", &Mesh::has_edge_status)
		.def("has_edge_colors", &Mesh::has_edge_colors)
		.def("has_face_normals", &Mesh::has_face_normals)
		.def("has_face_colors", &Mesh::has_face_colors)
		.def("has_face_status", &Mesh::has_face_status)
		.def("has_face_texture_index", &Mesh::has_face_texture_index)

		.def("add_vertex", &Mesh::add_vertex)
		.def("add_face", add_face)
		.def("vertex_handle", &Mesh::vertex_handle)

		.def("vv", &Mesh::vv)
		.def("vf", &Mesh::vf)
		.def("fv", &Mesh::fv)
		;
}

template<class Iterator, size_t (OpenMesh::ArrayKernel::*n_items)() const>
void expose_iterator(const char *_name) {
	class_<IteratorWrapperT<Iterator, n_items> >(_name, init<MeshWrapperT<TriMesh>&>())
		.def(init<MeshWrapperT<PolyMesh>&>())
		.def("__iter__", &IteratorWrapperT<Iterator, n_items>::iter)
		.def("__next__", &IteratorWrapperT<Iterator, n_items>::next)
		.def("__len__", &IteratorWrapperT<Iterator, n_items>::len)
		.def("next", &IteratorWrapperT<Iterator, n_items>::next)
		;
}

template<class Circulator>
void expose_circulator(const char *_name) {
	class_<CirculatorWrapperT<Circulator> >(_name, init<MeshWrapperT<TriMesh>&, typename Circulator::center_type>())
		.def(init<MeshWrapperT<PolyMesh>&, typename Circulator::center_type>())
		.def("__iter__", &CirculatorWrapperT<Circulator>::iter)
		.def("__next__", &CirculatorWrapperT<Circulator>::next)
		.def("next", &CirculatorWrapperT<Circulator>::next)
		;
}

template<class PropHandle, class IndexHandle>
void expose_property_manager(const char *_name) {
	typedef OpenMesh::PropertyManager<PropHandle, OpenMesh::PolyConnectivity > PropertyManager;
	typedef PropertyManagerWrapperT<PropertyManager, IndexHandle> PropertyManagerWrapper;

	class_<PropertyManagerWrapper, boost::noncopyable>(_name)
		.def(init<MeshWrapperT<TriMesh>&, const char *, optional<bool> >())
		.def(init<MeshWrapperT<PolyMesh>&, const char *, optional<bool> >())
		.def("__getitem__", &PropertyManagerWrapper::getitem)
		.def("__setitem__", &PropertyManagerWrapper::setitem)
		;
}

BOOST_PYTHON_MODULE(openmesh) {
	expose_vec3d();
	expose_handles();

	expose_openmesh_type<MeshWrapperT<TriMesh> >("TriMesh");
	expose_openmesh_type<MeshWrapperT<PolyMesh> >("PolyMesh");

	expose_iterator<OpenMesh::PolyConnectivity::VertexIter, &OpenMesh::ArrayKernel::n_vertices>("VertexIter");
	expose_iterator<OpenMesh::PolyConnectivity::HalfedgeIter, &OpenMesh::ArrayKernel::n_halfedges>("HalfedgeIter");
	expose_iterator<OpenMesh::PolyConnectivity::EdgeIter, &OpenMesh::ArrayKernel::n_edges>("EdgeIter");
	expose_iterator<OpenMesh::PolyConnectivity::FaceIter, &OpenMesh::ArrayKernel::n_faces>("FaceIter");

	expose_circulator<OpenMesh::PolyConnectivity::VertexVertexIter>("VertexVertexIter");
	expose_circulator<OpenMesh::PolyConnectivity::VertexIHalfedgeIter>("VertexIHalfedgeIter");
	expose_circulator<OpenMesh::PolyConnectivity::VertexOHalfedgeIter>("VertexOHalfedgeIter");
	expose_circulator<OpenMesh::PolyConnectivity::VertexEdgeIter>("VertexEdgeIter");
	expose_circulator<OpenMesh::PolyConnectivity::VertexFaceIter>("VertexFaceIter");

	expose_circulator<OpenMesh::PolyConnectivity::FaceVertexIter>("FaceVertexIter");
	expose_circulator<OpenMesh::PolyConnectivity::FaceHalfedgeIter>("FaceHalfedgeIter");
	expose_circulator<OpenMesh::PolyConnectivity::FaceEdgeIter>("FaceEdgeIter");
	expose_circulator<OpenMesh::PolyConnectivity::FaceFaceIter>("FaceFaceIter");

	// TODO expose_circulator<OpenMesh::PolyConnectivity::HalfedgeLoopIter>("HalfedgeLoopIter");

	expose_property_manager<OpenMesh::VPropHandleT<object>, VertexHandle>("VPropertyManager");
}