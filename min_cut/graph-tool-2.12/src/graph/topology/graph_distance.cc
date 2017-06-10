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

#include "graph.hh"
#include "graph_filtering.hh"
#include "graph_properties.hh"
#include "graph_selectors.hh"
#include "numpy_bind.hh"
#include "hash_map_wrap.hh"

#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/dijkstra_shortest_paths_no_color_map.hpp>
#include <boost/graph/bellman_ford_shortest_paths.hpp>
#include <boost/python/stl_iterator.hpp>
#include <boost/python.hpp>

using namespace std;
using namespace boost;
using namespace graph_tool;

struct stop_search {};

template <class DistMap, class PredMap>
class bfs_max_visitor:
    public boost::bfs_visitor<null_visitor>
{
public:
    bfs_max_visitor(DistMap dist_map, PredMap pred, size_t max_dist,
                    size_t source, size_t target)
        : _dist_map(dist_map), _pred(pred), _max_dist(max_dist),
          _source(source), _target(target), _dist(0) {}

    template <class Graph>
    void initialize_vertex(typename graph_traits<Graph>::vertex_descriptor v,
                           Graph&)
    {
        typedef typename property_traits<DistMap>::value_type dist_t;
        dist_t inf = std::is_floating_point<dist_t>::value ?
            numeric_limits<dist_t>::infinity() :
            numeric_limits<dist_t>::max();
        _dist_map[v] = (v == _source) ? 0 : inf;
        _pred[v] = v;
    }

    template <class Graph>
    void tree_edge(typename graph_traits<Graph>::edge_descriptor e,
                   Graph& g)
    {
        _pred[target(e,g)] = source(e,g);
    }

    template <class Graph>
    void examine_vertex(typename graph_traits<Graph>::vertex_descriptor v,
                        Graph&)
    {
        typedef typename property_traits<DistMap>::value_type val_t;
        if ( _dist_map[v] > val_t(_max_dist))
            throw stop_search();
    }

    template <class Graph>
    void discover_vertex(typename graph_traits<Graph>::vertex_descriptor v,
                         Graph&)
    {
        if (size_t(_pred[v]) == v)
            return;
        _dist_map[v] = _dist_map[_pred[v]] + 1;
        if (v == _target)
            throw stop_search();
    }

private:
    DistMap _dist_map;
    PredMap _pred;
    size_t _max_dist;
    size_t _source;
    size_t _target;
    size_t _dist;
};

template <class DistMap, class PredMap>
class bfs_max_multiple_targets_visitor:
    public boost::bfs_visitor<null_visitor>
{
public:
    bfs_max_multiple_targets_visitor(DistMap dist_map, PredMap pred,
                                     size_t max_dist, size_t source,
                                     gt_hash_set<std::size_t> target)
        : _dist_map(dist_map), _pred(pred), _max_dist(max_dist),
          _source(source), _target(target), _dist(0) {}

    template <class Graph>
    void initialize_vertex(typename graph_traits<Graph>::vertex_descriptor v,
                           Graph&)
    {
        typedef typename property_traits<DistMap>::value_type dist_t;
        dist_t inf = std::is_floating_point<dist_t>::value ?
            numeric_limits<dist_t>::infinity() :
            numeric_limits<dist_t>::max();
        _dist_map[v] = (v == _source) ? 0 : inf;
        _pred[v] = v;
    }

    template <class Graph>
    void tree_edge(typename graph_traits<Graph>::edge_descriptor e,
                   Graph& g)
    {
        _pred[target(e,g)] = source(e,g);
    }

    template <class Graph>
    void examine_vertex(typename graph_traits<Graph>::vertex_descriptor v,
                        Graph&)
    {
        typedef typename property_traits<DistMap>::value_type val_t;
        if ( _dist_map[v] > val_t(_max_dist))
            throw stop_search();
    }

    template <class Graph>
    void discover_vertex(typename graph_traits<Graph>::vertex_descriptor v,
                         Graph&)
    {
        if (size_t(_pred[v]) == v)
            return;
        _dist_map[v] = _dist_map[_pred[v]] + 1;

        auto iter = _target.find(v);
        if (iter != _target.end())
        {
            _target.erase(iter);
            if (_target.empty())
                throw stop_search();
        };
    }

private:
    DistMap _dist_map;
    PredMap _pred;
    size_t _max_dist;
    size_t _source;
    gt_hash_set<std::size_t> _target;
    size_t _dist;
};


template <class DistMap>
class djk_max_visitor:
    public boost::dijkstra_visitor<null_visitor>
{
public:
    djk_max_visitor(DistMap dist_map,
                    typename property_traits<DistMap>::value_type max_dist,
                    size_t target)
        : _dist_map(dist_map), _max_dist(max_dist), _target(target) {}


    template <class Graph>
    void examine_vertex(typename graph_traits<Graph>::vertex_descriptor u,
                        Graph&)
    {
        if (_dist_map[u] > _max_dist)
            throw stop_search();

        if (u == _target)
            throw stop_search();
    }


private:
    DistMap _dist_map;
    typename property_traits<DistMap>::value_type _max_dist;
    size_t _target;
};


template <class DistMap>
class djk_max_multiple_targets_visitor:
    public boost::dijkstra_visitor<null_visitor>
{
public:
    djk_max_multiple_targets_visitor(DistMap dist_map,
                                     typename property_traits<DistMap>::value_type max_dist, 
                                     gt_hash_set<std::size_t> target)
        : _dist_map(dist_map), _max_dist(max_dist), _target(target) {}


    template <class Graph>
    void examine_vertex(typename graph_traits<Graph>::vertex_descriptor u,
                        Graph&)
    {
        if (_dist_map[u] > _max_dist)
            throw stop_search();

        auto iter = _target.find(u);
        if (iter != _target.end())
        {
            _target.erase(iter);
            if (_target.empty())
                throw stop_search();
        };
    }


private:
    DistMap _dist_map;
    typename property_traits<DistMap>::value_type _max_dist;
    gt_hash_set<std::size_t> _target;
};

struct do_bfs_search
{
    template <class Graph, class VertexIndexMap, class DistMap, class PredMap>
    void operator()(const Graph& g, size_t source,
                    boost::python::object otarget_list,
                    VertexIndexMap vertex_index, DistMap dist_map,
                    PredMap pred_map, long double max_dist) const
    {
        typedef typename property_traits<DistMap>::value_type dist_t;

        auto target_list = get_array<int64_t, 1>(otarget_list);
        gt_hash_set<std::size_t> tgt(target_list.begin(),
                                     target_list.end());

        dist_t inf = std::is_floating_point<dist_t>::value ?
            numeric_limits<dist_t>::infinity() :
            numeric_limits<dist_t>::max();

        dist_t max_d = (max_dist > 0) ? max_dist : inf;

        unchecked_vector_property_map<boost::default_color_type, VertexIndexMap>
        color_map(vertex_index, num_vertices(g));
        try
        {
            if (tgt.size() <= 1)
            {
                size_t target = tgt.empty() ?
                    graph_traits<GraphInterface::multigraph_t>::null_vertex() :
                    *tgt.begin();
                breadth_first_search(g, vertex(source, g),
                                     visitor(bfs_max_visitor<DistMap, PredMap>
                                             (dist_map, pred_map, max_d,
                                              source, target)).
                                     vertex_index_map(vertex_index).
                                     color_map(color_map));
            }
            else
            {
                breadth_first_search(g, vertex(source, g),
                                     visitor(bfs_max_multiple_targets_visitor<DistMap, PredMap>
                                             (dist_map, pred_map, max_d,
                                              source, tgt)).
                                     vertex_index_map(vertex_index).
                                     color_map(color_map));
            }

        }
        catch (stop_search&) {}
    }
};

struct do_djk_search
{
    template <class Graph, class VertexIndexMap, class DistMap, class PredMap,
              class WeightMap>
    void operator()(const Graph& g, size_t source,
                    boost::python::object otarget_list,
                    VertexIndexMap vertex_index, DistMap dist_map,
                    PredMap pred_map, WeightMap weight, long double max_dist) const
    {
        auto target_list = get_array<int64_t, 1>(otarget_list);
        typedef typename property_traits<DistMap>::value_type dist_t;
        dist_t max_d = (max_dist > 0) ?
            max_dist : (std::is_floating_point<dist_t>::value ?
                        numeric_limits<dist_t>::infinity() :
                        numeric_limits<dist_t>::max());

        gt_hash_set<std::size_t> tgt(target_list.begin(),
                                     target_list.end());

        dist_t inf = (std::is_floating_point<dist_t>::value) ?
            numeric_limits<dist_t>::infinity() :
            numeric_limits<dist_t>::max();

        int i, N = num_vertices(g);
        #pragma omp parallel for default(shared) private(i) \
            schedule(runtime) if (N > 100)
        for (i = 0; i < N; ++i)
            dist_map[i] = inf;
        dist_map[source] = 0;

        try
        {
            if (tgt.size() <= 1)
            {
                size_t target = tgt.empty() ?
                    graph_traits<GraphInterface::multigraph_t>::null_vertex() :
                    *tgt.begin();
                dijkstra_shortest_paths_no_color_map
                    (g, vertex(source, g),
                     weight_map(weight).
                     distance_map(dist_map).
                     vertex_index_map(vertex_index).
                     predecessor_map(pred_map).
                     distance_inf(inf).
                     visitor(djk_max_visitor<DistMap>
                             (dist_map, max_d, target)));
            }
            else
            {
                dijkstra_shortest_paths_no_color_map
                    (g, vertex(source, g),
                     weight_map(weight).
                     distance_map(dist_map).
                     vertex_index_map(vertex_index).
                     predecessor_map(pred_map).
                     distance_inf(inf).
                     visitor(djk_max_multiple_targets_visitor<DistMap>
                             (dist_map, max_d, tgt)));
            }

        }
        catch (stop_search&) {}
    }
};

struct do_bf_search
{
    template <class Graph, class DistMap, class PredMap, class WeightMap>
    void operator()(const Graph& g, size_t source, DistMap dist_map,
                    PredMap pred_map, WeightMap weight) const
    {
        bool ret = bellman_ford_shortest_paths(g, root_vertex(source).
                                               predecessor_map(pred_map).
                                               distance_map(dist_map).
                                               weight_map(weight));
        if (!ret)
            throw ValueException("Graph contains negative loops");

        // consistency with dijkstra
        typedef typename property_traits<DistMap>::value_type dist_t;
        if (std::is_floating_point<dist_t>::value)
        {
            for (auto v : vertices_range(g))
            {
                if (dist_map[v] == numeric_limits<dist_t>::max())
                    dist_map[v] = numeric_limits<dist_t>::infinity();
            }
        }
    }
};

void get_dists(GraphInterface& gi, size_t source, boost::python::object tgt,
               boost::any dist_map, boost::any weight, boost::any pred_map,
               long double max_dist, bool bf)
{
    typedef property_map_type
        ::apply<int64_t, GraphInterface::vertex_index_map_t>::type pred_map_t;

    pred_map_t pmap = any_cast<pred_map_t>(pred_map);

    if (weight.empty())
    {
        run_action<>()
            (gi, std::bind(do_bfs_search(), placeholders::_1, source, tgt, gi.get_vertex_index(),
                           placeholders::_2, pmap.get_unchecked(num_vertices(gi.get_graph())),
                           max_dist),
             writable_vertex_scalar_properties())
            (dist_map);
    }
    else
    {
        if (bf)
        {
            run_action<>()
                (gi, std::bind(do_bf_search(), placeholders::_1, source,
                               placeholders::_2, pmap.get_unchecked(num_vertices(gi.get_graph())),
                               placeholders::_3),
                 writable_vertex_scalar_properties(),
                 edge_scalar_properties())
                (dist_map, weight);
        }
        else
        {
            run_action<>()
                (gi, std::bind(do_djk_search(), placeholders::_1, source, tgt, gi.get_vertex_index(),
                               placeholders::_2, pmap.get_unchecked(num_vertices(gi.get_graph())),
                               placeholders::_3, max_dist),
                 writable_vertex_scalar_properties(),
                 edge_scalar_properties())
                (dist_map, weight);
        }
    }
}

void export_dists()
{
    python::def("get_dists", &get_dists);
};
