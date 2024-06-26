#include "MRSceneObjectsListDrawer.h"
#include "MRSceneCache.h"
#include "MRMesh/MRObject.h"
#include "MRMesh/MRObjectsAccess.h"
#include "MRViewer.h"
#include "MRViewerInstance.h"
#include "MRViewport.h"
#include "MRUIStyle.h"
#include "MRMesh/MRSceneRoot.h"
#include "MRRibbonConstants.h"
#include "MRColorTheme.h"
#include "ImGuiMenu.h"
#include "MRMesh/MRChangeSceneAction.h"
#include "MRAppendHistory.h"
#include "MRRibbonButtonDrawer.h"
#include "MRRibbonFontManager.h"
#include "MRRibbonSchema.h"
#include "MRUITestEngine.h"
#include "MRRibbonMenu.h"
#include "MRPch/MRSpdlog.h"
#include "imgui_internal.h"
#include "imgui.h"
#include <stack>

namespace MR
{

void SceneObjectsListDrawer::draw( float height, float scaling )
{
    menuScaling_ = scaling;

    ImGui::BeginChild( "SceneObjectsList", ImVec2( -1, height ), false );
    updateSceneWindowScrollIfNeeded_();
    drawObjectsList_();
    // any click on empty space below Scene Tree removes object selection
    const auto& selected = SceneCache::getSelectedObjects();
    ImGui::BeginChild( "EmptySpace" );
    if ( ImGui::IsWindowHovered() && ImGui::IsMouseClicked( 0 ) )
    {
        for ( const auto& s : selected )
            if ( s )
                s->select( false );
    }
    ImGui::EndChild();

    ImGui::EndChild();
    sceneOpenCommands_.clear();
    reorderSceneIfNeeded_();
}

float SceneObjectsListDrawer::drawCustomTreeObjectProperties_( Object&, bool )
{
    return 0.f;
}

void SceneObjectsListDrawer::setObjectTreeState( const Object* obj, bool open )
{
    if ( obj )
        sceneOpenCommands_[obj] = open;
}

void SceneObjectsListDrawer::allowSceneReorder( bool allow )
{
    allowSceneReorder_ = allow;
}

void SceneObjectsListDrawer::drawObjectsList_()
{
    const auto& viewerRef = getViewerInstance();

    const auto& all = SceneCache::getAllObjects();
    const auto& depth = SceneCache::getAllObjectsDepth();
    const auto& selected = SceneCache::getSelectedObjects();

    int curentDepth = 0;
    std::stack<std::shared_ptr<Object>> objDepthStack;

    const float drawBoxYmin = ImGui::GetScrollY();
    const float drawBoxYmax = drawBoxYmin + ImGui::GetWindowHeight();

    int collapsedHeaderDepth = -1;
    float skipedCursorPosY = ImGui::GetCursorPosY();
    float lastSpacingY = 0;
    const float dragDropTargetHeight = 4.f * menuScaling_; // see makeDragDropTarget_(...)
    const float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
    const float frameHeight = ImGui::GetFrameHeight();
    for ( int i = 0; i < all.size(); ++i )
    {
        // skip child elements after collapsed header
        if ( collapsedHeaderDepth >= 0 )
        {
            if ( depth[i] > collapsedHeaderDepth )
                continue;
            else
                collapsedHeaderDepth = -1;
        }

        auto& object = *all[i];
        if ( curentDepth < depth[i] )
        {
            ImGui::Indent();
            if ( i > 0 )
                objDepthStack.push( all[i - 1] );
            ++curentDepth;
            assert( curentDepth == depth[i] );
        }

        std::string uniqueStr = std::to_string( intptr_t( &object ) );
        const bool isObjSelectable = !object.isAncillary();

        // has selectable children
        bool hasRealChildren = objectHasSelectableChildren( object );
        bool isOpen{ false };
        if ( ( hasRealChildren || isObjSelectable ) )
        {
            if ( needDragDropTarget_() )
            {
                if ( skipedCursorPosY + dragDropTargetHeight <= drawBoxYmin || skipedCursorPosY > drawBoxYmax )
                {
                    skipedCursorPosY += dragDropTargetHeight + itemSpacingY;
                    lastSpacingY = itemSpacingY;
                }
                else
                {
                    if ( skipedCursorPosY > ImGui::GetCursorPosY() )
                        ImGui::Dummy( ImVec2( 0, skipedCursorPosY - ImGui::GetCursorPosY() - lastSpacingY ) );
                    makeDragDropTarget_( object, true, true, uniqueStr );
                    skipedCursorPosY = ImGui::GetCursorPosY();
                }
            }
            if ( skipedCursorPosY + frameHeight <= drawBoxYmin || skipedCursorPosY > drawBoxYmax )
            {
                skipedCursorPosY += frameHeight + itemSpacingY;
                lastSpacingY = itemSpacingY;
                isOpen = ImGui::TreeNodeUpdateNextOpen( ImGui::GetCurrentWindow()->GetID( ( object.name() + " ##" + uniqueStr ).c_str() ),
                    ImGuiTreeNodeFlags_None );
            }
            else
            {
                if ( skipedCursorPosY > ImGui::GetCursorPosY() )
                    ImGui::Dummy( ImVec2( 0, skipedCursorPosY - ImGui::GetCursorPosY() - lastSpacingY ) );
                {
                    // Visibility checkbox
                    bool isVisible = object.isVisible( viewerRef.viewport().id );
                    auto ctx = ImGui::GetCurrentContext();
                    assert( ctx );
                    auto window = ctx->CurrentWindow;
                    assert( window );
                    auto diff = ImGui::GetStyle().FramePadding.y - cCheckboxPadding * menuScaling_;
                    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + diff );
                    if ( UI::checkbox( ( "##VisibilityCheckbox" + uniqueStr ).c_str(), &isVisible ) )
                    {
                        object.setVisible( isVisible, viewerRef.viewport().id );
                        if ( deselectNewHiddenObjects_ && !object.isVisible( viewerRef.getPresentViewports() ) )
                            object.select( false );
                    }
                    window->DC.CursorPosPrevLine.y -= diff;
                    ImGui::SameLine();
                }
                {
                    // custom prefix
                    drawCustomObjectPrefixInScene_( object );
                }

                const bool isSelected = object.isSelected();

                auto openCommandIt = sceneOpenCommands_.find( &object );
                if ( openCommandIt != sceneOpenCommands_.end() )
                    ImGui::SetNextItemOpen( openCommandIt->second );

                if ( !isSelected )
                    ImGui::PushStyleColor( ImGuiCol_Header, ImVec4( 0, 0, 0, 0 ) );
                else
                {
                    ImGui::PushStyleColor( ImGuiCol_Header, ColorTheme::getRibbonColor( ColorTheme::RibbonColorsType::SelectedObjectFrame ).getUInt32() );
                    ImGui::PushStyleColor( ImGuiCol_Text, ColorTheme::getRibbonColor( ColorTheme::RibbonColorsType::SelectedObjectText ).getUInt32() );
                }

                ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );

                isOpen = ImGui::CollapsingHeader( ( object.name() + " ##" + uniqueStr ).c_str(),
                                            ( hasRealChildren ? ImGuiTreeNodeFlags_DefaultOpen : 0 ) |
                                            ImGuiTreeNodeFlags_SpanAvailWidth |
                                            ImGuiTreeNodeFlags_Framed |
                                            ImGuiTreeNodeFlags_OpenOnArrow |
                                            ( isSelected ? ImGuiTreeNodeFlags_Selected : 0 ) );

                if ( ImGui::IsMouseDoubleClicked( 0 ) && ImGui::IsItemHovered() )
                {
                    if ( auto menu = viewerRef.getMenuPlugin() )
                        menu->tryRenameSelectedObject();
                }

                ImGui::PopStyleColor( isSelected ? 2 : 1 );
                ImGui::PopStyleVar();

                makeDragDropSource_( selected );
                makeDragDropTarget_( object, false, false, "0" );

                if ( isObjSelectable && ImGui::IsItemHovered() )
                {
                    bool pressed = !isSelected && ( ImGui::IsMouseClicked( 0 ) || ImGui::IsMouseClicked( 1 ) );
                    bool released = isSelected && !dragTrigger_ && !clickTrigger_ && ImGui::IsMouseReleased( 0 );

                    if ( pressed )
                        clickTrigger_ = true;
                    if ( isSelected && clickTrigger_ && ImGui::IsMouseReleased( 0 ) )
                        clickTrigger_ = false;

                    if ( pressed || released )
                    {

                        auto newSelection = getPreSelection_( &object, ImGui::GetIO().KeyShift, ImGui::GetIO().KeyCtrl, selected, all );
                        if ( ImGui::GetIO().KeyCtrl )
                        {
                            for ( auto& sel : newSelection )
                            {
                                const bool select = ImGui::GetIO().KeyShift || !sel->isSelected();
                                sel->select( select );
                                if ( showNewSelectedObjects_ && select )
                                    sel->setGlobalVisibility( true );
                            }
                        }
                        else
                        {
                            for ( const auto& data : selected )
                            {
                                auto inNewSelList = std::find( newSelection.begin(), newSelection.end(), data.get() );
                                if ( inNewSelList == newSelection.end() )
                                    data->select( false );
                            }
                            for ( auto& sel : newSelection )
                            {
                                sel->select( true );
                                if ( showNewSelectedObjects_ )
                                    sel->setGlobalVisibility( true );
                            }
                        }
                    }
                }

                if ( isSelected )
                    drawSceneContextMenu_( selected );

                skipedCursorPosY = ImGui::GetCursorPosY();
            }

            if ( !isOpen )
                collapsedHeaderDepth = curentDepth;
        }
        if ( isOpen )
        {
            const float drawPropertiesHeight = drawCustomTreeObjectProperties_( object, true );
            if ( drawPropertiesHeight > 0.f )
            {
                if ( skipedCursorPosY + drawPropertiesHeight <= drawBoxYmin || skipedCursorPosY > drawBoxYmax )
                {
                    skipedCursorPosY += frameHeight + itemSpacingY;
                    lastSpacingY = itemSpacingY;
                }
                else
                {
                    if ( skipedCursorPosY > ImGui::GetCursorPosY() )
                        ImGui::Dummy( ImVec2( 0, skipedCursorPosY - ImGui::GetCursorPosY() - lastSpacingY ) );
                    drawCustomTreeObjectProperties_( object, false );
                    skipedCursorPosY = ImGui::GetCursorPosY();
                }
            }
            bool infoOpen = false;
            auto lines = object.getInfoLines();
            if ( showInfoInObjectTree_ && hasRealChildren && !lines.empty() )
            {
                auto infoId = std::string( "Info: ##" ) + uniqueStr;

                if ( skipedCursorPosY + frameHeight <= drawBoxYmin || skipedCursorPosY > drawBoxYmax )
                {
                    skipedCursorPosY += frameHeight + itemSpacingY;
                    lastSpacingY = itemSpacingY;
                    infoOpen = ImGui::TreeNodeUpdateNextOpen( ImGui::GetCurrentWindow()->GetID( infoId.c_str() ),
                        ImGuiTreeNodeFlags_None );
                }
                else
                {
                    if ( skipedCursorPosY > ImGui::GetCursorPosY() )
                        ImGui::Dummy( ImVec2( 0, skipedCursorPosY - ImGui::GetCursorPosY() - lastSpacingY ) );
                    infoOpen = ImGui::CollapsingHeader( infoId.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed );
                    skipedCursorPosY = ImGui::GetCursorPosY();
                }
            }

            if ( infoOpen || !hasRealChildren )
            {
                auto itemSpacing = ImGui::GetStyle().ItemSpacing;
                auto framePadding = ImGui::GetStyle().FramePadding;
                framePadding.y = 2.0f * menuScaling_;
                itemSpacing.y = 2.0f * menuScaling_;

                ImGui::PushStyleColor( ImGuiCol_Header, ImVec4( 0, 0, 0, 0 ) );
                ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, framePadding );
                ImGui::PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0.0f );
                ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, itemSpacing );
                ImGui::PushStyleVar( ImGuiStyleVar_IndentSpacing, cItemInfoIndent * menuScaling_ );
                ImGui::Indent();

                const float infoFrameHeight = ImGui::GetFrameHeight();

                for ( const auto& str : lines )
                {
                    if ( skipedCursorPosY + infoFrameHeight <= drawBoxYmin || skipedCursorPosY > drawBoxYmax )
                    {
                        skipedCursorPosY += infoFrameHeight + itemSpacing.y;
                        lastSpacingY = itemSpacing.y;
                    }
                    else
                    {
                        if ( skipedCursorPosY > ImGui::GetCursorPosY() )
                            ImGui::Dummy( ImVec2( 0, skipedCursorPosY - ImGui::GetCursorPosY() - lastSpacingY ) );
                        ImGui::TreeNodeEx( str.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_DefaultOpen |
                            ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_Framed );
                        ImGui::TreePop();
                        skipedCursorPosY = ImGui::GetCursorPosY();
                    }
                }

                ImGui::Unindent();
                ImGui::PopStyleVar( 4 );
                ImGui::PopStyleColor();
            }
        }

        const bool isLast = i == int( all.size() ) - 1;
        int newDepth = isLast ? 0 : depth[i + 1];
        for ( ; curentDepth > newDepth; --curentDepth )
        {
            if ( needDragDropTarget_() )
            {
                if ( skipedCursorPosY + dragDropTargetHeight <= drawBoxYmin || skipedCursorPosY > drawBoxYmax )
                {
                    skipedCursorPosY += dragDropTargetHeight + itemSpacingY;
                    lastSpacingY = itemSpacingY;
                }
                else
                {
                    if ( skipedCursorPosY > ImGui::GetCursorPosY() )
                        ImGui::Dummy( ImVec2( 0, skipedCursorPosY - ImGui::GetCursorPosY() - lastSpacingY ) );
                    makeDragDropTarget_( *objDepthStack.top(), false, true, "0" );
                    skipedCursorPosY = ImGui::GetCursorPosY();
                }
            }
            objDepthStack.pop();
            ImGui::Unindent();
        }
        if ( isLast && needDragDropTarget_() )
        {
            if ( skipedCursorPosY + dragDropTargetHeight <= drawBoxYmin || skipedCursorPosY > drawBoxYmax )
            {
                skipedCursorPosY += dragDropTargetHeight + itemSpacingY;
                lastSpacingY = itemSpacingY;
            }
            else
            {
                if ( skipedCursorPosY > ImGui::GetCursorPosY() )
                    ImGui::Dummy( ImVec2( 0, skipedCursorPosY - ImGui::GetCursorPosY() - lastSpacingY ) );
                makeDragDropTarget_( SceneRoot::get(), false, true, "" );
                skipedCursorPosY = ImGui::GetCursorPosY();
            }
        }
    }
    if ( skipedCursorPosY > ImGui::GetCursorPosY() )
        ImGui::Dummy( ImVec2( 0, skipedCursorPosY - ImGui::GetCursorPosY() - lastSpacingY ) );
}

void SceneObjectsListDrawer::makeDragDropSource_( const std::vector<std::shared_ptr<Object>>& payload )
{
    if ( !allowSceneReorder_ || payload.empty() )
        return;

    if ( std::any_of( payload.begin(), payload.end(), std::mem_fn( &Object::isParentLocked ) ) )
        return; // Those objects don't want their parents to be changed.

    if ( ImGui::BeginDragDropSource( ImGuiDragDropFlags_AcceptNoDrawDefaultRect ) )
    {
        dragTrigger_ = true;

        std::vector<Object*> vectorObjPtr;
        for ( auto& ptr : payload )
            vectorObjPtr.push_back( ptr.get() );

        ImGui::SetDragDropPayload( "_TREENODE", vectorObjPtr.data(), sizeof( Object* ) * vectorObjPtr.size() );
        std::string allNames;
        allNames = payload[0]->name();
        for ( int i = 1; i < payload.size(); ++i )
            allNames += "\n" + payload[i]->name();
        ImGui::Text( "%s", allNames.c_str() );
        ImGui::EndDragDropSource();
    }
}

bool SceneObjectsListDrawer::needDragDropTarget_()
{
    if ( !allowSceneReorder_ )
        return false;
    const ImGuiPayload* payloadCheck = ImGui::GetDragDropPayload();
    return payloadCheck && std::string_view( payloadCheck->DataType ) == "_TREENODE";
}

void SceneObjectsListDrawer::makeDragDropTarget_( Object& target, bool before, bool betweenLine, const std::string& uniqueStr )
{
    if ( !allowSceneReorder_ )
        return;
    const ImGuiPayload* payloadCheck = ImGui::GetDragDropPayload();
    ImVec2 curPos{};
    const bool lineDrawed = payloadCheck && std::string_view( payloadCheck->DataType ) == "_TREENODE" && betweenLine;
    if ( lineDrawed )
    {
        curPos = ImGui::GetCursorPos();
        auto width = ImGui::GetContentRegionAvail().x;
        ImGui::ColorButton( ( "##InternalDragDropArea" + uniqueStr ).c_str(),
            ImVec4( 0, 0, 0, 0 ),
            0, ImVec2( width, 4 * menuScaling_ ) );
    }
    if ( ImGui::BeginDragDropTarget() )
    {
        if ( lineDrawed )
        {
            ImGui::SetCursorPos( curPos );
            auto width = ImGui::GetContentRegionAvail().x;
            ImGui::ColorButton( ( "##ColoredInternalDragDropArea" + uniqueStr ).c_str(),
                ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered],
                0, ImVec2( width, 4 * menuScaling_ ) );
        }
        if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( "_TREENODE" ) )
        {
            assert( payload->DataSize % sizeof( Object* ) == 0 );
            Object** objArray = ( Object** )payload->Data;
            const int size = payload->DataSize / sizeof( Object* );
            std::vector<Object*> vectorObj( size );
            for ( int i = 0; i < size; ++i )
                vectorObj[i] = objArray[i];
            sceneReorderCommand_ = { vectorObj, &target, before };
        }
        ImGui::EndDragDropTarget();
    }
}

void SceneObjectsListDrawer::reorderSceneIfNeeded_()
{
    if ( !allowSceneReorder_ )
        return;

    const bool filledReorderCommand = !sceneReorderCommand_.who.empty() && sceneReorderCommand_.to;
    const bool sourceNotTarget = std::all_of( sceneReorderCommand_.who.begin(), sceneReorderCommand_.who.end(), [target = sceneReorderCommand_.to] ( auto it )
    {
        return it != target;
    } );
    const bool trueTarget = !sceneReorderCommand_.before || sceneReorderCommand_.to->parent();
    const bool trueSource = std::all_of( sceneReorderCommand_.who.begin(), sceneReorderCommand_.who.end(), [] ( auto it )
    {
        return bool( it->parent() );
    } );
    if ( !( filledReorderCommand && sourceNotTarget && trueSource && trueTarget ) )
    {
        sceneReorderCommand_ = {};
        return;
    }

    bool dragOrDropFailed = false;
    std::shared_ptr<Object> childTo = nullptr;
    if ( sceneReorderCommand_.before )
    {
        for ( auto childToItem : sceneReorderCommand_.to->parent()->children() )
            if ( childToItem.get() == sceneReorderCommand_.to )
            {
                childTo = childToItem;
                break;
            }
        assert( childTo );
    }

    struct MoveAction
    {
        std::shared_ptr<ChangeSceneAction> detachAction;
        std::shared_ptr<ChangeSceneAction> attachAction;
    };
    std::vector<MoveAction> actionList;
    for ( const auto& source : sceneReorderCommand_.who )
    {
        std::shared_ptr<Object> sourcePtr = nullptr;
        for ( auto child : source->parent()->children() )
            if ( child.get() == source )
            {
                sourcePtr = child;
                break;
            }
        assert( sourcePtr );

        auto detachAction = std::make_shared<ChangeSceneAction>( "Detach object", sourcePtr, ChangeSceneAction::Type::RemoveObject );
        bool detachSuccess = sourcePtr->detachFromParent();
        if ( !detachSuccess )
        {
            //showModalMessage( "Cannot perform such reorder", NotificationType::Error );
            showModal( "Cannot perform such reorder", NotificationType::Error );
            dragOrDropFailed = true;
            break;
        }

        auto attachAction = std::make_shared<ChangeSceneAction>( "Attach object", sourcePtr, ChangeSceneAction::Type::AddObject );
        bool attachSucess{ false };
        if ( !sceneReorderCommand_.before )
            attachSucess = sceneReorderCommand_.to->addChild( sourcePtr );
        else
            attachSucess = sceneReorderCommand_.to->parent()->addChildBefore( sourcePtr, childTo );
        if ( !attachSucess )
        {
            detachAction->action( HistoryAction::Type::Undo );
            //showModalMessage( "Cannot perform such reorder", NotificationType::Error );
            showModal( "Cannot perform such reorder", NotificationType::Error );
            dragOrDropFailed = true;
            break;
        }

        actionList.push_back( { detachAction, attachAction } );
    }

    if ( dragOrDropFailed )
    {
        for ( int i = int( actionList.size() ) - 1; i >= 0; --i )
        {
            actionList[i].attachAction->action( HistoryAction::Type::Undo );
            actionList[i].detachAction->action( HistoryAction::Type::Undo );
        }
    }
    else
    {
        SCOPED_HISTORY( "Reorder scene" );
        for ( const auto& moveAction : actionList )
        {
            AppendHistory( moveAction.detachAction );
            AppendHistory( moveAction.attachAction );
        }
    }
    sceneReorderCommand_ = {};
    dragTrigger_ = false;
}

void SceneObjectsListDrawer::updateSceneWindowScrollIfNeeded_()
{
    auto window = ImGui::GetCurrentContext()->CurrentWindow;
    if ( !window )
        return;

    ScrollPositionPreservation scrollInfo;
    scrollInfo.relativeMousePos = ImGui::GetMousePos().y - window->Pos.y;
    scrollInfo.absLinePosRatio = window->ContentSize.y == 0.0f ? 0.0f : ( scrollInfo.relativeMousePos + window->Scroll.y ) / window->ContentSize.y;

    if ( nextFrameFixScroll_ )
    {
        nextFrameFixScroll_ = false;
        window->Scroll.y = std::clamp( prevScrollInfo_.absLinePosRatio * window->ContentSize.y - prevScrollInfo_.relativeMousePos, 0.0f, window->ScrollMax.y );
    }
    else if ( dragObjectsMode_ )
    {
        float relativeMousePosRatio = window->Size.y == 0.0f ? 0.0f : scrollInfo.relativeMousePos / window->Size.y;
        float shift = 0.0f;
        if ( relativeMousePosRatio < 0.05f )
            shift = ( relativeMousePosRatio - 0.05f ) * 25.0f - 1.0f;
        else if ( relativeMousePosRatio > 0.95f )
            shift = ( relativeMousePosRatio - 0.95f ) * 25.0f + 1.0f;

        auto newScroll = std::clamp( window->Scroll.y + shift, 0.0f, window->ScrollMax.y );
        if ( newScroll != window->Scroll.y )
        {
            window->Scroll.y = newScroll;
            getViewerInstance().incrementForceRedrawFrames();
        }
    }

    const ImGuiPayload* payloadCheck = ImGui::GetDragDropPayload();
    bool dragModeNow = payloadCheck && std::string_view( payloadCheck->DataType ) == "_TREENODE";
    if ( dragModeNow && !dragObjectsMode_ )
    {
        dragObjectsMode_ = true;
        nextFrameFixScroll_ = true;
        getViewerInstance().incrementForceRedrawFrames( 2, true );
    }
    else if ( !dragModeNow && dragObjectsMode_ )
    {
        dragObjectsMode_ = false;
        nextFrameFixScroll_ = true;
        getViewerInstance().incrementForceRedrawFrames( 2, true );
    }

    if ( !nextFrameFixScroll_ )
        prevScrollInfo_ = scrollInfo;
}

std::vector<Object*> SceneObjectsListDrawer::getPreSelection_( Object* meshclicked, bool isShift, bool isCtrl,
    const std::vector<std::shared_ptr<Object>>& selected, const std::vector<std::shared_ptr<Object>>& all_objects )
{
    if ( selected.empty() || !isShift )
        return { meshclicked };

    const auto& first = isCtrl ? selected.back().get() : selected.front().get();

    auto firstIt = std::find_if( all_objects.begin(), all_objects.end(), [first] ( const std::shared_ptr<Object>& obj )
    {
        return obj.get() == first;
    } );
    auto clickedIt = std::find_if( all_objects.begin(), all_objects.end(), [meshclicked] ( const std::shared_ptr<Object>& obj )
    {
        return obj.get() == meshclicked;
    } );

    size_t start{ 0 };
    std::vector<Object*> res;
    if ( firstIt < clickedIt )
    {
        start = std::distance( all_objects.begin(), firstIt );
        res.resize( std::distance( firstIt, clickedIt + 1 ) );
    }
    else
    {
        start = std::distance( all_objects.begin(), clickedIt );
        res.resize( std::distance( clickedIt, firstIt + 1 ) );
    }
    for ( int i = 0; i < res.size(); ++i )
    {
        res[i] = all_objects[start + i].get();
    }
    return res;
}

}
