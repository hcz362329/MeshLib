#pragma once
#include "MRMesh/MRMeshFwd.h"
#ifndef MRMESH_NO_OPENVDB
#include "MRViewer/MRStatePlugin.h"
#include "MRMesh/MRVoxelsLoad.h"

namespace MR
{
class OpenRawVoxelsPlugin : public StatePlugin
{
public:
    OpenRawVoxelsPlugin();

    virtual void drawDialog( float menuScaling, ImGuiContext* ) override;

private:
    virtual bool onEnable_() override;
    virtual bool onDisable_() override;

    bool autoMode_{ false };
    VoxelsLoad::RawParameters parameters_;
};
}
#endif
