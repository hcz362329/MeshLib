#pragma once
#include "MRMeshFwd.h"
#include "MRLaplacian.h"

namespace MR
{

/// Puts given vertices in such positions to make smooth surface both inside verts-region and on its boundary;
/// \param verts must not include all vertices of a mesh connected component
/// \param fixedSharpVertices in these vertices the surface can be not-smooth
MRMESH_API void positionVertsSmoothly( Mesh& mesh, const VertBitSet& verts,
    Laplacian::EdgeWeights edgeWeightsType = Laplacian::EdgeWeights::Cotan,
    const VertBitSet * fixedSharpVertices = nullptr );

/// Puts given vertices in such positions to make smooth surface inside verts-region, but sharp on its boundary;
/// \param verts must not include all vertices of a mesh connected component
/// \param vertShifts optional additional shifts of each vertex relative to smooth position
MRMESH_API void positionVertsSmoothlySharpBd( Mesh& mesh, const VertBitSet& verts,
    const Vector<Vector3f, VertId>* vertShifts = nullptr );

struct SpacingSettings
{
    /// vertices to be moved by the algorithm, nullptr means all valid vertices
    const VertBitSet* region = nullptr;

    /// target squared distance of each edge in the mesh (for at least one edge's vertex in the region)
    UndirectedEdgeMetric distSq; // must be defined by the caller

    /// the algorithm is iterative, the more iterations the closer result to exact solution
    int numIters = 5;

    /// too small number here can lead to instability, too large - to slow convergence
    float stabilizer = 10;

    /// if this predicated is given, then all inverted faces will be converted in degenerate faces at the end of each iteration
    FacePredicate isInverted;
};

/// Moves given vertices to make the distances between them as specified
MRMESH_API void positionVertsWithSpacing( Mesh& mesh, const SpacingSettings & settings );

struct InflateSettings
{
    /// the amount of pressure applied to mesh region:
    /// positive pressure moves the vertices outside, negative - inside;
    /// please specify a value by magnitude about the region diagonal
    float pressure = 0;
    /// the number of internal iterations (>=1);
    /// larger number of iterations makes the performance slower, but the quality better
    int iterations = 3;
    /// smooths the area before starting inflation;
    /// please set to false only if the region is known to be already smooth
    bool preSmooth = true;
    /// whether to increase the pressure gradually during the iterations (recommended for best quality)
    bool gradualPressureGrowth = true;
};

/// Inflates (in one of two sides) given mesh region,
/// putting given vertices in such positions to make smooth surface inside verts-region, but sharp on its boundary;
/// \param verts must not include all vertices of a mesh connected component
MRMESH_API void inflate( Mesh& mesh, const VertBitSet& verts, const InflateSettings & settings );

} //namespace MR
