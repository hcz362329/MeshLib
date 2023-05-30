#if !defined( __EMSCRIPTEN__) && !defined( MRMESH_NO_VOXEL )
#include "MRPartialOffset.h"
#include "MRMesh.h"
#include "MRMeshBoolean.h"
#include "MRPch/MRSpdlog.h"

namespace MR
{

tl::expected<Mesh, std::string> partialOffsetMesh( const MeshPart& mp, float offset, const OffsetParameters& params /*= {} */ )
{
    auto realParams = params;
    realParams.type = OffsetParameters::Type::Shell; // for now only shell can be in partial offset
    realParams.callBack = subprogress( params.callBack, 0.0f, 0.5f );
    auto offsetPart = offsetMesh( mp, offset, realParams );
    if ( params.callBack && !params.callBack( 0.5f ) )
        return tl::make_unexpected( "Operation was canceled." );
    if ( !offsetPart.has_value() )
        return offsetPart;
    auto res = boolean( mp.mesh, *offsetPart, BooleanOperation::Union, nullptr, nullptr, subprogress( params.callBack, 0.5f, 1.0f ) );
    if ( res.errorString == "Operation was canceled." )
        return tl::make_unexpected( "Operation was canceled." );
    if ( !res.valid() )
        return tl::make_unexpected("Partial offset failed: " + res.errorString );
    return std::move( res.mesh );
}

} //namespace MR
#endif //!__EMSCRIPTEN__
