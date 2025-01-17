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

#ifndef GRAPH_LAPLACIAN_HH
#define GRAPH_LAPLACIAN_HH

#include "graph.hh"
#include "graph_filtering.hh"
#include "graph_util.hh"

namespace graph_tool
{
using namespace boost;

enum deg_t
{
    IN_DEG,
    OUT_DEG,
    TOTAL_DEG
};

template <class Graph, class Weight, class EdgeSelector>
typename property_traits<Weight>::value_type
sum_degree(Graph& g, typename graph_traits<Graph>::vertex_descriptor v,
           Weight w, EdgeSelector)
{
    typename property_traits<Weight>::value_type sum = 0;
    typename EdgeSelector::type e, e_end;
    for(tie(e, e_end) = EdgeSelector::get_edges(v, g); e != e_end; ++e)
        sum += get(w, *e);
    return sum;
}

template <class Graph, class EdgeSelector>
double
sum_degree(Graph& g, typename graph_traits<Graph>::vertex_descriptor v,
           ConstantPropertyMap<double, GraphInterface::edge_t>, all_edges_iteratorS<Graph>)
{
    return total_degreeS()(v, g);
}

template <class Graph, class EdgeSelector>
double
sum_degree(Graph& g, typename graph_traits<Graph>::vertex_descriptor v,
           ConstantPropertyMap<double, GraphInterface::edge_t>, in_edge_iteratorS<Graph>)
{
    return in_degreeS()(v, g);
}

template <class Graph, class EdgeSelector>
double
sum_degree(Graph& g, typename graph_traits<Graph>::vertex_descriptor v,
           ConstantPropertyMap<double, GraphInterface::edge_t>, out_edge_iteratorS<Graph>)
{
    return out_degreeS()(v, g);
}

struct get_laplacian
{
    template <class Graph, class Index, class Weight>
    void operator()(const Graph& g, Index index, Weight weight, deg_t deg,
                    multi_array_ref<double,1>& data,
                    multi_array_ref<int32_t,1>& i,
                    multi_array_ref<int32_t,1>& j) const
    {
        int pos = 0;
        for (const auto& e : edges_range(g))
        {
            if (source(e, g) == target(e, g))
                continue;

            data[pos] = -get(weight, e);
            i[pos] = get(index, target(e, g));
            j[pos] = get(index, source(e, g));

            ++pos;
            if (!is_directed::apply<Graph>::type::value)
            {
                data[pos] = -get(weight, e);
                i[pos] = get(index, source(e, g));
                j[pos] = get(index, target(e, g));
                ++pos;
            }
        }

        for (auto v : vertices_range(g))
        {
            double k = 0;
            switch (deg)
            {
            case OUT_DEG:
                k = sum_degree(g, v, weight, out_edge_iteratorS<Graph>());
                break;
            case IN_DEG:
                k = sum_degree(g, v, weight, in_edge_iteratorS<Graph>());
                break;
            case TOTAL_DEG:
                k = sum_degree(g, v, weight, all_edges_iteratorS<Graph>());
            }
            data[pos] = k;
            i[pos] = j[pos] = get(index, v);
            ++pos;
        }

    }
};



struct get_norm_laplacian
{
    template <class Graph, class Index, class Weight>
    void operator()(const Graph& g, Index index, Weight weight, deg_t deg,
                    multi_array_ref<double,1>& data,
                    multi_array_ref<int32_t,1>& i,
                    multi_array_ref<int32_t,1>& j) const
    {
        int pos = 0;
        for (auto v : vertices_range(g))
        {
            double ks = 0;
            switch (deg)
            {
            case OUT_DEG:
                ks = sum_degree(g, v, weight, out_edge_iteratorS<Graph>());
                break;
            case IN_DEG:
                ks = sum_degree(g, v, weight, in_edge_iteratorS<Graph>());
                break;
            case TOTAL_DEG:
                ks = sum_degree(g, v, weight, all_edges_iteratorS<Graph>());
            }

            for(const auto& e : out_edges_range(v, g))
            {
                if (source(e, g) == target(e, g))
                    continue;
                double kt = 0;

                switch (deg)
                {
                case OUT_DEG:
                    kt = sum_degree(g, target(e, g), weight, out_edge_iteratorS<Graph>());
                    break;
                case IN_DEG:
                    kt = sum_degree(g, target(e, g), weight, in_edge_iteratorS<Graph>());
                    break;
                case TOTAL_DEG:
                    kt = sum_degree(g, target(e, g), weight, all_edges_iteratorS<Graph>());
                }

                if (ks * kt > 0)
                    data[pos] = -get(weight, e) / sqrt(ks * kt);
                i[pos] = get(index, target(e, g));
                j[pos] = get(index, source(e, g));

                ++pos;
            }

            if (ks > 0)
                data[pos] = 1;
            i[pos] = j[pos] = get(index, v);
            ++pos;
        }

    }
};


} // namespace graph_tool

#endif // GRAPH_LAPLACIAN_HH
