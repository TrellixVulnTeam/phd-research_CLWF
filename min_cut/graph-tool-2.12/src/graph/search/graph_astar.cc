// graph-tool -- a general graph modification and manipulation thingy
//
// Copyright (C) 2006-2015 Tiago de Paula Peixoto <tiago@skewed.de>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 3
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "graph_filtering.hh"
#include "graph_python_interface.hh"

#include <boost/python.hpp>
#include <boost/graph/astar_search.hpp>

#include "graph.hh"
#include "graph_selectors.hh"
#include "graph_util.hh"

#include "graph_astar.hh"

#ifdef HAVE_BOOST_COROUTINE
#include <boost/coroutine/all.hpp>
#endif // HAVE_BOOST_COROUTINE

using namespace std;
using namespace boost;
using namespace graph_tool;

template <class T>
python::object operator |(const python::object& a, const T& b)
{
    return a / b;
}

struct do_astar_search
{
    template <class Graph, class DistanceMap, class PredMap, class Visitor>
    void operator()(Graph& g, size_t s, DistanceMap dist, PredMap pred,
                    boost::any aweight, Visitor vis, pair<AStarCmp, AStarCmb> cmp,
                    pair<python::object, python::object> range,
                    python::object h, GraphInterface& gi) const
    {
        typedef typename graph_traits<Graph>::edge_descriptor edge_t;
        typedef typename property_traits<DistanceMap>::value_type dtype_t;
        dtype_t z = python::extract<dtype_t>(range.first);
        dtype_t i = python::extract<dtype_t>(range.second);
        checked_vector_property_map<default_color_type,
                                    decltype(get(vertex_index, g))>
            color(get(vertex_index, g));
        checked_vector_property_map<dtype_t,
                                    decltype(get(vertex_index, g))>
            cost(get(vertex_index, g));
        DynamicPropertyMapWrap<dtype_t, edge_t> weight(aweight,
                                                       edge_properties());
        astar_search(g, vertex(s, g), AStarH<Graph, dtype_t>(gi, g, h),
                     vis, pred, cost, dist, weight, get(vertex_index, g), color,
                     cmp.first, cmp.second, i, z);
   }
};

struct do_astar_search_fast
{
    template <class Graph, class DistanceMap, class WeightMap,
              class Visitor>
    void operator()(Graph& g, size_t s, DistanceMap dist,
                    WeightMap weight, Visitor vis,
                    pair<python::object, python::object> range,
                    python::object h, GraphInterface& gi) const
    {
        typedef typename property_traits<DistanceMap>::value_type dtype_t;
        dtype_t z = python::extract<dtype_t>(range.first);
        dtype_t i = python::extract<dtype_t>(range.second);
        astar_search(g, vertex(s, g), AStarH<Graph, dtype_t>(gi, g, h),
                     weight_map(weight).distance_map(dist).distance_zero(z).
                     distance_inf(i).visitor(vis));
   }
};


void a_star_search(GraphInterface& g, size_t source, boost::any dist_map,
                   boost::any pred_map, boost::any weight, python::object vis,
                   python::object cmp, python::object cmb, python::object zero,
                   python::object inf, python::object h)
{
    typedef typename property_map_type::
        apply<int64_t, GraphInterface::vertex_index_map_t>::type pred_t;
    pred_t pred = any_cast<pred_t>(pred_map);
    run_action<graph_tool::detail::all_graph_views,mpl::true_>()
        (g, std::bind(do_astar_search(),  placeholders::_1, source,
                      placeholders::_2, pred, weight,
                      AStarVisitorWrapper(g, vis), make_pair(AStarCmp(cmp),
                                                             AStarCmb(cmb)),
                      make_pair(zero, inf), h, std::ref(g)),
         writable_vertex_properties())(dist_map);
}

#ifdef HAVE_BOOST_COROUTINE

typedef boost::coroutines::asymmetric_coroutine<boost::python::object> coro_t;

class AStarGeneratorVisitor : public astar_visitor<>
{
public:
    AStarGeneratorVisitor(GraphInterface& gi,
                          coro_t::push_type& yield)
        : _gi(gi), _yield(yield) {}

    template <class Edge, class Graph>
    void edge_relaxed(const Edge& e, Graph& g)
    {
        std::shared_ptr<Graph> gp = retrieve_graph_view<Graph>(_gi, g);
        _yield(boost::python::object(PythonEdge<Graph>(gp, e)));
    }

private:
    GraphInterface& _gi;
    coro_t::push_type& _yield;
};

class AStarGenerator
{
public:
    template <class Dispatch>
    AStarGenerator(Dispatch& dispatch)
        : _coro(std::make_shared<coro_t::pull_type>(dispatch)),
          _iter(begin(*_coro)), _end(end(*_coro)) {}
    boost::python::object next()
    {
        if (_iter == _end)
            boost::python::objects::stop_iteration_error();
        boost::python::object oe = *_iter;
        ++_iter;
        return oe;
    }
private:
    std::shared_ptr<coro_t::pull_type> _coro;
    coro_t::pull_type::iterator _iter;
    coro_t::pull_type::iterator _end;
};

#endif // HAVE_BOOST_COROUTINE

boost::python::object astar_search_generator(GraphInterface& g,
                                             size_t source,
                                             boost::any dist_map,
                                             boost::any weight,
                                             python::object cmp,
                                             python::object cmb,
                                             python::object zero,
                                             python::object inf,
                                             python::object h)
{
#ifdef HAVE_BOOST_COROUTINE
    auto dispatch = [&](auto& yield)
        {
            AStarGeneratorVisitor vis(g, yield);
            run_action<graph_tool::detail::all_graph_views,mpl::true_>()
               (g, std::bind(do_astar_search(),  placeholders::_1, source,
                             placeholders::_2, dummy_property_map(), weight,
                             vis, make_pair(AStarCmp(cmp), AStarCmb(cmb)),
                             make_pair(zero, inf), h, std::ref(g)),
                writable_vertex_properties())(dist_map);
        };
    return boost::python::object(AStarGenerator(dispatch));
#else
    throw GraphException("This functionality is not available because boost::coroutine was not found at compile-time");
#endif
}

boost::python::object astar_search_generator_fast(GraphInterface& g,
                                                  size_t source,
                                                  boost::any dist_map,
                                                  boost::any weight,
                                                  python::object zero,
                                                  python::object inf,
                                                  python::object h)
{
#ifdef HAVE_BOOST_COROUTINE
    auto dispatch = [&](auto& yield)
        {
            AStarGeneratorVisitor vis(g, yield);
            run_action<graph_tool::detail::all_graph_views,mpl::true_>()
               (g, std::bind(do_astar_search_fast(),  placeholders::_1, source,
                             placeholders::_2, placeholders::_3,
                             vis, make_pair(zero, inf), h, std::ref(g)),
                writable_vertex_scalar_properties(),
                edge_scalar_properties())(dist_map, weight);
        };
    return boost::python::object(AStarGenerator(dispatch));
#else
    throw GraphException("This functionality is not available because boost::coroutine was not found at compile-time");
#endif
}


void export_astar()
{
    using namespace boost::python;
    def("astar_search", &a_star_search);
    def("astar_generator", &astar_search_generator);
    def("astar_generator_fast", &astar_search_generator_fast);
#ifdef HAVE_BOOST_COROUTINE
    class_<AStarGenerator>("AStarGenerator", no_init)
        .def("__iter__", objects::identity_function())
        .def("next", &AStarGenerator::next)
        .def("__next__", &AStarGenerator::next);
#endif
}
