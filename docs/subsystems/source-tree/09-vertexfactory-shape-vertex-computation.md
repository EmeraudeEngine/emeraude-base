## VertexFactory: Shape Vertex Computation (Adjacency Optimization)

### O(V+T) Vertex Normal/Tangent Computation

`Shape::computeVertexNormal()`, `computeVertexTangent()`, and `computeVertexTBNSpace()` use a **vertex-to-triangle adjacency table** for O(V+T) performance instead of the previous O(V×T) brute-force scan.

**Algorithm:**
1. `buildVertexToTriangleAdjacency()` builds a lookup table: for each vertex, the list of triangles it belongs to. Cost: O(T).
2. Each vertex accumulates normals/tangents only from its adjacent triangles. Cost: O(V + average fan size).

```cpp
const auto adjacency = this->buildVertexToTriangleAdjacency();  // O(T)
for ( size_t v = 0; v < m_vertices.size(); ++v )
{
    Vector< 3, vertex_data_t > normal;
    for ( const auto triIndex : adjacency[v] )
        normal += m_triangles[triIndex].surfaceNormal();
    m_vertices[v].setNormal(normal.normalize());
}
```

**OpenMP was removed** from all three functions. With the adjacency table, each vertex iteration is lightweight (typically 4-8 triangle lookups). The OMP fork/join overhead exceeds the benefit for such small work units, especially when the engine already uses the thread pool for parallelism.

**Code references:**
- `VertexFactory/Shape.hpp:buildVertexToTriangleAdjacency()` — Private O(T) adjacency builder
- `VertexFactory/Shape.hpp:computeVertexNormal()` — O(V+T) normal computation
- `VertexFactory/Shape.hpp:computeVertexTangent()` — O(V+T) tangent computation
- `VertexFactory/Shape.hpp:computeVertexTBNSpace()` — O(V+T) combined TBN computation
