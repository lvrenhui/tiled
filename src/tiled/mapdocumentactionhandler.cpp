/*
 * mapdocumentactionhandler.cpp
 * Copyright 2010-2017, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2011, Stefan Beller <stefanbeller@googlemail.com
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mapdocumentactionhandler.h"

#include "addremovelayer.h"
#include "addremovemapobject.h"
#include "changeselectedarea.h"
#include "clipboardmanager.h"
#include "documentmanager.h"
#include "erasetiles.h"
#include "grouplayer.h"
#include "map.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "mapview.h"
#include "movelayer.h"
#include "objectgroup.h"
#include "tilelayer.h"
#include "utils.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QMenu>
#include <QtCore/qmath.h>
#include <QStyle>

#include "qtcompat_p.h"

namespace Tiled {
namespace Internal {

MapDocumentActionHandler *MapDocumentActionHandler::mInstance;

MapDocumentActionHandler::MapDocumentActionHandler(QObject *parent)
    : QObject(parent)
    , mMapDocument(nullptr)
{
    Q_ASSERT(!mInstance);
    mInstance = this;

    mActionSelectAll = new QAction(this);
    mActionSelectAll->setShortcuts(QKeySequence::SelectAll);
    mActionSelectInverse = new QAction(this);
    mActionSelectInverse->setShortcut(tr("Ctrl+I"));
    mActionSelectNone = new QAction(this);
    mActionSelectNone->setShortcut(tr("Ctrl+Shift+A"));

    mActionCropToSelection = new QAction(this);
    mActionAutocrop = new QAction(this);

    QIcon addTileLayerIcon(QLatin1String(":/images/16x16/layer-tile.png"));
    QIcon addObjectLayerIcon(QLatin1String(":/images/16x16/layer-object.png"));
    QIcon addImageLayerIcon(QLatin1String(":/images/16x16/layer-image.png"));

    addTileLayerIcon.addFile(QLatin1String(":/images/32x32/layer-tile.png"));
    addObjectLayerIcon.addFile(QLatin1String(":/images/32x32/layer-object.png"));

    mActionAddTileLayer = new QAction(this);
    mActionAddTileLayer->setIcon(addTileLayerIcon);
    mActionAddObjectGroup = new QAction(this);
    mActionAddObjectGroup->setIcon(addObjectLayerIcon);
    mActionAddImageLayer = new QAction(this);
    mActionAddImageLayer->setIcon(addImageLayerIcon);
    mActionAddGroupLayer = new QAction(this);
    mActionAddGroupLayer->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));

    mActionLayerViaCopy = new QAction(this);
    mActionLayerViaCopy->setShortcut(tr("Ctrl+J"));

    mActionLayerViaCut = new QAction(this);
    mActionLayerViaCut->setShortcut(tr("Ctrl+Shift+J"));

    mActionGroupLayers = new QAction(this);
    mActionUngroupLayers = new QAction(this);

    mActionDuplicateLayers = new QAction(this);
    mActionDuplicateLayers->setShortcut(tr("Ctrl+Shift+D"));
    mActionDuplicateLayers->setIcon(
            QIcon(QLatin1String(":/images/16x16/stock-duplicate-16.png")));

    mActionMergeLayerDown = new QAction(this);

    mActionRemoveLayers = new QAction(this);
    mActionRemoveLayers->setIcon(
            QIcon(QLatin1String(":/images/16x16/edit-delete.png")));

    mActionSelectPreviousLayer = new QAction(this);
    mActionSelectPreviousLayer->setShortcut(tr("Ctrl+PgDown"));

    mActionSelectNextLayer = new QAction(this);
    mActionSelectNextLayer->setShortcut(tr("Ctrl+PgUp"));

    mActionMoveLayersUp = new QAction(this);
    mActionMoveLayersUp->setShortcut(tr("Ctrl+Shift+Up"));
    mActionMoveLayersUp->setIcon(
            QIcon(QLatin1String(":/images/16x16/go-up.png")));

    mActionMoveLayersDown = new QAction(this);
    mActionMoveLayersDown->setShortcut(tr("Ctrl+Shift+Down"));
    mActionMoveLayersDown->setIcon(
            QIcon(QLatin1String(":/images/16x16/go-down.png")));

    mActionToggleOtherLayers = new QAction(this);
    mActionToggleOtherLayers->setShortcut(tr("Ctrl+Shift+H"));
    mActionToggleOtherLayers->setIcon(
            QIcon(QLatin1String(":/images/16x16/show_hide_others.png")));

    mActionToggleLockOtherLayers = new QAction(this);
    mActionToggleLockOtherLayers->setShortcut(tr("Ctrl+Shift+L"));
    mActionToggleLockOtherLayers->setIcon(
        QIcon(QLatin1String(":/images/16x16/locked.png")));

    mActionLayerProperties = new QAction(this);
    mActionLayerProperties->setIcon(
            QIcon(QLatin1String(":images/16x16/document-properties.png")));

    mActionDuplicateObjects = new QAction(this);
    mActionDuplicateObjects->setIcon(QIcon(QLatin1String(":/images/16x16/stock-duplicate-16.png")));

    mActionRemoveObjects = new QAction(this);
    mActionRemoveObjects->setIcon(QIcon(QLatin1String(":/images/16x16/edit-delete.png")));

    Utils::setThemeIcon(mActionRemoveLayers, "edit-delete");
    Utils::setThemeIcon(mActionMoveLayersUp, "go-up");
    Utils::setThemeIcon(mActionMoveLayersDown, "go-down");
    Utils::setThemeIcon(mActionLayerProperties, "document-properties");
    Utils::setThemeIcon(mActionRemoveObjects, "edit-delete");

    connect(mActionSelectAll, &QAction::triggered, this, &MapDocumentActionHandler::selectAll);
    connect(mActionSelectInverse, &QAction::triggered, this, &MapDocumentActionHandler::selectInverse);
    connect(mActionSelectNone, &QAction::triggered, this, &MapDocumentActionHandler::selectNone);
    connect(mActionCropToSelection, &QAction::triggered, this, &MapDocumentActionHandler::cropToSelection);
    connect(mActionAutocrop, &QAction::triggered, this, &MapDocumentActionHandler::autocrop);
    connect(mActionAddTileLayer, &QAction::triggered, this, &MapDocumentActionHandler::addTileLayer);
    connect(mActionAddObjectGroup, &QAction::triggered, this, &MapDocumentActionHandler::addObjectGroup);
    connect(mActionAddImageLayer, &QAction::triggered, this, &MapDocumentActionHandler::addImageLayer);
    connect(mActionAddGroupLayer, &QAction::triggered, this, &MapDocumentActionHandler::addGroupLayer);
    connect(mActionLayerViaCopy, &QAction::triggered, this, &MapDocumentActionHandler::layerViaCopy);
    connect(mActionLayerViaCut, &QAction::triggered, this, &MapDocumentActionHandler::layerViaCut);
    connect(mActionGroupLayers, &QAction::triggered, this, &MapDocumentActionHandler::groupLayers);
    connect(mActionUngroupLayers, &QAction::triggered, this, &MapDocumentActionHandler::ungroupLayers);

    connect(mActionDuplicateLayers, &QAction::triggered, this, &MapDocumentActionHandler::duplicateLayers);
    connect(mActionMergeLayerDown, &QAction::triggered, this, &MapDocumentActionHandler::mergeLayerDown);
    connect(mActionSelectPreviousLayer, &QAction::triggered, this, &MapDocumentActionHandler::selectPreviousLayer);
    connect(mActionSelectNextLayer, &QAction::triggered, this, &MapDocumentActionHandler::selectNextLayer);
    connect(mActionRemoveLayers, &QAction::triggered, this, &MapDocumentActionHandler::removeLayers);
    connect(mActionMoveLayersUp, &QAction::triggered, this, &MapDocumentActionHandler::moveLayersUp);
    connect(mActionMoveLayersDown, &QAction::triggered, this, &MapDocumentActionHandler::moveLayersDown);
    connect(mActionToggleOtherLayers, &QAction::triggered, this, &MapDocumentActionHandler::toggleOtherLayers);
    connect(mActionToggleLockOtherLayers, &QAction::triggered, this, &MapDocumentActionHandler::toggleLockOtherLayers);
    connect(mActionLayerProperties, &QAction::triggered, this, &MapDocumentActionHandler::layerProperties);

    connect(mActionDuplicateObjects, &QAction::triggered, this, &MapDocumentActionHandler::duplicateObjects);
    connect(mActionRemoveObjects, &QAction::triggered, this, &MapDocumentActionHandler::removeObjects);

    updateActions();
    retranslateUi();
}

MapDocumentActionHandler::~MapDocumentActionHandler()
{
    mInstance = nullptr;
}

void MapDocumentActionHandler::retranslateUi()
{
    mActionSelectAll->setText(tr("Select &All"));
    mActionSelectInverse->setText(tr("Invert S&election"));
    mActionSelectNone->setText(tr("Select &None"));
    mActionCropToSelection->setText(tr("&Crop to Selection"));
    mActionAutocrop->setText(tr("Autocrop"));

    mActionAddTileLayer->setText(tr("&Tile Layer"));
    mActionAddObjectGroup->setText(tr("&Object Layer"));
    mActionAddImageLayer->setText(tr("&Image Layer"));
    mActionAddGroupLayer->setText(tr("&Group Layer"));
    mActionLayerViaCopy->setText(tr("Layer via Copy"));
    mActionLayerViaCut->setText(tr("Layer via Cut"));
    mActionGroupLayers->setText(tr("&Group Layers"));
    mActionUngroupLayers->setText(tr("&Ungroup Layers"));

    mActionDuplicateLayers->setText(tr("&Duplicate Layers"));
    mActionMergeLayerDown->setText(tr("&Merge Layer Down"));
    mActionRemoveLayers->setText(tr("&Remove Layers"));
    mActionSelectPreviousLayer->setText(tr("Select Pre&vious Layer"));
    mActionSelectNextLayer->setText(tr("Select &Next Layer"));
    mActionMoveLayersUp->setText(tr("R&aise Layers"));
    mActionMoveLayersDown->setText(tr("&Lower Layers"));
    mActionToggleOtherLayers->setText(tr("Show/&Hide all Other Layers"));
    mActionToggleLockOtherLayers->setText(tr("Lock/&Unlock all Other Layers"));
    mActionLayerProperties->setText(tr("Layer &Properties..."));
}

void MapDocumentActionHandler::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

    if (mMapDocument)
        mMapDocument->disconnect(this);

    mMapDocument = mapDocument;
    updateActions();

    if (mMapDocument) {
        connect(mapDocument, &MapDocument::layerAdded,
                this, &MapDocumentActionHandler::updateActions);
        connect(mapDocument, &MapDocument::layerRemoved,
                this, &MapDocumentActionHandler::updateActions);
        connect(mapDocument, &MapDocument::currentLayerChanged,
                this, &MapDocumentActionHandler::updateActions);
        connect(mapDocument, &MapDocument::selectedLayersChanged,
                this, &MapDocumentActionHandler::updateActions);
        connect(mapDocument, &MapDocument::selectedAreaChanged,
                this, &MapDocumentActionHandler::updateActions);
        connect(mapDocument, &MapDocument::selectedObjectsChanged,
                this, &MapDocumentActionHandler::updateActions);
        connect(mapDocument, &MapDocument::mapChanged,
                this, &MapDocumentActionHandler::updateActions);
    }

    emit mapDocumentChanged(mMapDocument);
}

/**
 * Creates the new layer menu, which is used in several places.
 */
QMenu *MapDocumentActionHandler::createNewLayerMenu(QWidget *parent) const
{
    QMenu *newLayerMenu = new QMenu(tr("&New"), parent);

    newLayerMenu->setIcon(QIcon(QLatin1String(":/images/16x16/document-new.png")));
    Utils::setThemeIcon(newLayerMenu, "document-new");

    newLayerMenu->addAction(actionAddTileLayer());
    newLayerMenu->addAction(actionAddObjectGroup());
    newLayerMenu->addAction(actionAddImageLayer());
    newLayerMenu->addAction(actionAddGroupLayer());
    newLayerMenu->addSeparator();
    newLayerMenu->addAction(actionLayerViaCopy());
    newLayerMenu->addAction(actionLayerViaCut());

    return newLayerMenu;
}

QMenu *MapDocumentActionHandler::createGroupLayerMenu(QWidget *parent) const
{
    QMenu *groupLayerMenu = new QMenu(tr("&Group"), parent);

    groupLayerMenu->addAction(actionGroupLayers());
    groupLayerMenu->addAction(actionUngroupLayers());

    return groupLayerMenu;
}

/**
 * Used to check whether we can cut or delete the current tile selection.
 */
static bool isTileSelectionLocked(const MapDocument &mapDocument)
{
    if (!mapDocument.selectedArea().isEmpty())
        for (Layer *layer : mapDocument.selectedLayers())
            if (layer->isTileLayer() && !layer->isUnlocked())
                return true;

    return false;
}

void MapDocumentActionHandler::cut()
{
    if (!mMapDocument)
        return;

    if (isTileSelectionLocked(*mMapDocument))
        return;

    if (!copy())
        return;

    QUndoStack *stack = mMapDocument->undoStack();
    stack->beginMacro(tr("Cut"));
    delete_();
    stack->endMacro();
}

/**
 * @returns whether anything was copied.
 */
bool MapDocumentActionHandler::copy()
{
    if (mMapDocument)
        return ClipboardManager::instance()->copySelection(*mMapDocument);
    return false;
}

void MapDocumentActionHandler::delete_()
{
    if (!mMapDocument)
        return;

    if (isTileSelectionLocked(*mMapDocument))
        return;

    const QRegion &selectedArea = mMapDocument->selectedArea();

    QList<QUndoCommand*> commands;

    LayerIterator layerIterator(mMapDocument->map(), Layer::TileLayerType);
    for (Layer *layer : mMapDocument->selectedLayers()) {
        if (!layer->isTileLayer())
            continue;

        auto tileLayer = static_cast<TileLayer*>(layer);
        const QRegion area = selectedArea.intersected(tileLayer->bounds());
        if (area.isEmpty())                     // nothing to delete
            continue;

        // Delete the selected part of the layer
        commands.append(new EraseTiles(mMapDocument, tileLayer, area));
    }

    commands.append(new RemoveMapObjects(mMapDocument, mMapDocument->selectedObjects()));

    QUndoStack *undoStack = mMapDocument->undoStack();

    if (!commands.isEmpty()) {
        undoStack->beginMacro(tr("Delete"));
        for (QUndoCommand *command : commands)
            undoStack->push(command);
    }

    selectNone();

    if (!commands.isEmpty())
        undoStack->endMacro();
}

void MapDocumentActionHandler::selectAll()
{
    if (!mMapDocument)
        return;

    const bool infinite = mMapDocument->map()->infinite();

    QRect all;
    QList<MapObject*> objects;

    for (Layer *layer : mMapDocument->selectedLayers()) {
        if (!layer->isUnlocked())
            continue;

        switch (layer->layerType()) {
        case Layer::TileLayerType: {
            auto tileLayer = static_cast<TileLayer*>(layer);
            all |= infinite ? tileLayer->bounds() : tileLayer->rect();
            break;
        }
        case Layer::ObjectGroupType: {
            if (!layer->isUnlocked())
                continue;

            auto objectGroup = static_cast<ObjectGroup*>(layer);
            objects.append(objectGroup->objects());
            break;
        }
        case Layer::ImageLayerType:
        case Layer::GroupLayerType:
            break;
        }
    }

    if (mMapDocument->selectedArea() != all) {
        QUndoCommand *command = new ChangeSelectedArea(mMapDocument, all);
        mMapDocument->undoStack()->push(command);
    }

    if (!objects.isEmpty())
        mMapDocument->setSelectedObjects(objects);
}

void MapDocumentActionHandler::selectInverse()
{
    if (!mMapDocument)
        return;

    Layer *layer = mMapDocument->currentLayer();
    if (!layer)
        return;

    if (TileLayer *tileLayer = layer->asTileLayer()) {
        QRegion all = tileLayer->rect();
        if (mMapDocument->map()->infinite())
            all = tileLayer->bounds();

        QUndoCommand *command = new ChangeSelectedArea(mMapDocument, all - mMapDocument->selectedArea());
        mMapDocument->undoStack()->push(command);
    } else if (ObjectGroup *objectGroup = layer->asObjectGroup()) {
        const auto &allObjects = objectGroup->objects();
        const auto &selectedObjects = mMapDocument->selectedObjects();
        QList<MapObject*> notSelectedObjects;

        for (auto mapObject : allObjects)
            if (!selectedObjects.contains(mapObject))
                notSelectedObjects.append(mapObject);

        mMapDocument->setSelectedObjects(notSelectedObjects);
    }
}

void MapDocumentActionHandler::selectNone()
{
    if (!mMapDocument)
        return;

    if (!mMapDocument->selectedArea().isEmpty()) {
        QUndoCommand *command = new ChangeSelectedArea(mMapDocument, QRegion());
        mMapDocument->undoStack()->push(command);
    }

    if (!mMapDocument->selectedObjects().isEmpty())
        mMapDocument->setSelectedObjects(QList<MapObject*>());
}

void MapDocumentActionHandler::copyPosition()
{
    const MapView *view = DocumentManager::instance()->currentMapView();
    if (!view)
        return;

    const QPoint globalPos = QCursor::pos();
    const QPoint viewportPos = view->viewport()->mapFromGlobal(globalPos);
    const QPointF scenePos = view->mapToScene(viewportPos);

    const MapRenderer *renderer = mapDocument()->renderer();
    const QPointF tilePos = renderer->screenToTileCoords(scenePos);
    const int x = qFloor(tilePos.x());
    const int y = qFloor(tilePos.y());

    QApplication::clipboard()->setText(QString::number(x) +
                                       QLatin1String(", ") +
                                       QString::number(y));
}

void MapDocumentActionHandler::cropToSelection()
{
    if (!mMapDocument)
        return;

    const QRect bounds = mMapDocument->selectedArea().boundingRect();
    if (bounds.isNull())
        return;

    mMapDocument->resizeMap(bounds.size(), -bounds.topLeft(), true);
}

void MapDocumentActionHandler::autocrop()
{
    if (mMapDocument)
        mMapDocument->autocropMap();
}

void MapDocumentActionHandler::addTileLayer()
{
    if (mMapDocument)
        mMapDocument->addLayer(Layer::TileLayerType);
}

void MapDocumentActionHandler::addObjectGroup()
{
    if (mMapDocument)
        mMapDocument->addLayer(Layer::ObjectGroupType);
}

void MapDocumentActionHandler::addImageLayer()
{
     if (mMapDocument)
         mMapDocument->addLayer(Layer::ImageLayerType);
}

void MapDocumentActionHandler::addGroupLayer()
{
    if (mMapDocument)
        mMapDocument->addLayer(Layer::GroupLayerType);
}

void MapDocumentActionHandler::layerVia(MapDocumentActionHandler::LayerViaVariant variant)
{
    if (!mMapDocument)
        return;

    auto *currentLayer = mMapDocument->currentLayer();
    Layer *newLayer = nullptr;
    QRegion selectedArea;
    TileLayer *sourceLayer = nullptr;
    QList<MapObject*> selectedObjects;
    QList<MapObject*> newObjects;
    const QString name = variant == ViaCopy ? tr("Layer via Copy") : tr("Layer via Cut");

    switch (currentLayer->layerType()) {
    case Layer::TileLayerType: {
        selectedArea = mMapDocument->selectedArea();
        if (selectedArea.isEmpty())
            return;

        auto map = mMapDocument->map();
        sourceLayer = static_cast<TileLayer*>(currentLayer);
        auto newTileLayer = new TileLayer(name, 0, 0, map->width(), map->height());
        newTileLayer->setCells(0, 0, sourceLayer, selectedArea);

        newLayer = newTileLayer;
        break;
    }
    case Layer::ObjectGroupType: {
        selectedObjects = mMapDocument->selectedObjectsOrdered();
        if (selectedObjects.isEmpty())
            return;

        auto currentObjectGroup = static_cast<ObjectGroup*>(currentLayer);
        auto newObjectGroup = new ObjectGroup(name, 0, 0);
        newObjectGroup->setDrawOrder(currentObjectGroup->drawOrder());
        newObjectGroup->setColor(currentObjectGroup->color());

        for (MapObject *mapObject : qAsConst(selectedObjects)) {
            MapObject *clone = mapObject->clone();
            if (variant == ViaCopy)
                clone->resetId();
            newObjects.append(clone);
            newObjectGroup->addObject(clone);
        }

        newLayer = newObjectGroup;
        break;
    }
    default:
        return;
    }

    auto parentLayer = currentLayer->parentLayer();
    auto newLayerIndex = mMapDocument->layerIndex(currentLayer) + 1;
    auto addLayer = new AddLayer(mMapDocument, newLayerIndex, newLayer, parentLayer);
    addLayer->setText(name);

    auto undoStack = mMapDocument->undoStack();

    if (variant == ViaCopy) {
        undoStack->push(addLayer);
    } else {
        undoStack->beginMacro(name);
        undoStack->push(addLayer);

        switch (currentLayer->layerType()) {
        case Layer::TileLayerType: {
            undoStack->push(new EraseTiles(mMapDocument, sourceLayer, selectedArea));
            break;
        }
        case Layer::ObjectGroupType:
            undoStack->push(new RemoveMapObjects(mMapDocument, selectedObjects));
            break;
        default:
            Q_ASSERT(false);
            break;
        }

        undoStack->endMacro();
    }

    mMapDocument->setCurrentLayer(newLayer);

    if (!newObjects.isEmpty())
        mMapDocument->setSelectedObjects(newObjects);
}

void MapDocumentActionHandler::groupLayers()
{
    if (mMapDocument)
        mMapDocument->groupLayers(mMapDocument->selectedLayers());
}

void MapDocumentActionHandler::ungroupLayers()
{
    if (mMapDocument)
        mMapDocument->ungroupLayers(mMapDocument->selectedLayers());
}

void MapDocumentActionHandler::duplicateLayers()
{
    if (mMapDocument)
        mMapDocument->duplicateLayers(mMapDocument->selectedLayers());
}

void MapDocumentActionHandler::mergeLayerDown()
{
    if (mMapDocument)
        mMapDocument->mergeLayerDown();
}

void MapDocumentActionHandler::selectPreviousLayer()
{
    if (!mMapDocument)
        return;

    if (Layer *previousLayer = LayerIterator(mMapDocument->currentLayer()).previous()) {
        mMapDocument->setCurrentLayer(previousLayer);
        mMapDocument->setSelectedLayers({ previousLayer });
    }
}

void MapDocumentActionHandler::selectNextLayer()
{
    if (!mMapDocument)
        return;

    if (Layer *nextLayer = LayerIterator(mMapDocument->currentLayer()).next()) {
        mMapDocument->setCurrentLayer(nextLayer);
        mMapDocument->setSelectedLayers({ nextLayer });
    }
}

void MapDocumentActionHandler::moveLayersUp()
{
    if (mMapDocument)
        mMapDocument->moveLayersUp(mMapDocument->selectedLayers());
}

void MapDocumentActionHandler::moveLayersDown()
{
    if (mMapDocument)
        mMapDocument->moveLayersDown(mMapDocument->selectedLayers());
}

void MapDocumentActionHandler::removeLayers()
{
    if (mMapDocument)
        mMapDocument->removeLayers(mMapDocument->selectedLayers());
}

void MapDocumentActionHandler::toggleOtherLayers()
{
    if (mMapDocument)
        mMapDocument->toggleOtherLayers(mMapDocument->selectedLayers());
}

void MapDocumentActionHandler::toggleLockOtherLayers()
{
    if (mMapDocument)
        mMapDocument->toggleLockOtherLayers(mMapDocument->selectedLayers());
}

void MapDocumentActionHandler::layerProperties()
{
    if (mMapDocument) {
        mMapDocument->setCurrentObject(mMapDocument->currentLayer());
        emit mMapDocument->editCurrentObject();
    }
}

void MapDocumentActionHandler::duplicateObjects()
{
    if (mMapDocument)
        mMapDocument->duplicateObjects(mMapDocument->selectedObjects());
}

void MapDocumentActionHandler::removeObjects()
{
    if (mMapDocument)
        mMapDocument->removeObjects(mMapDocument->selectedObjects());
}

void MapDocumentActionHandler::moveObjectsToGroup(ObjectGroup *objectGroup)
{
    if (mMapDocument) {
        mMapDocument->moveObjectsToGroup(mMapDocument->selectedObjects(),
                                         objectGroup);
    }
}

void MapDocumentActionHandler::selectAllInstances(const ObjectTemplate *objectTemplate)
{
    if (mMapDocument)
        mMapDocument->selectAllInstances(objectTemplate);
}

void MapDocumentActionHandler::updateActions()
{
    Map *map = nullptr;
    Layer *currentLayer = nullptr;
    QList<Layer *> selectedLayers;
    QRegion selection;
    int selectedObjectsCount = 0;
    bool canMergeDown = false;

    if (mMapDocument) {
        map = mMapDocument->map();
        currentLayer = mMapDocument->currentLayer();
        selectedLayers = mMapDocument->selectedLayers();
        selection = mMapDocument->selectedArea();
        selectedObjectsCount = mMapDocument->selectedObjects().count();

        if (currentLayer) {
            int currentLayerIndex = currentLayer->siblingIndex();
            if (currentLayerIndex > 0) {
                const auto layers = currentLayer->siblings();
                Layer *upper = layers.at(currentLayerIndex);
                Layer *lower = layers.at(currentLayerIndex - 1);
                canMergeDown = lower->canMergeWith(upper);
            }
        }
    }

    mActionSelectAll->setEnabled(map);
    mActionSelectInverse->setEnabled(map);

    if (currentLayer) {
        if (currentLayer->asTileLayer()) {
            mActionSelectNone->setEnabled(!selection.isEmpty());
        } else if (currentLayer->asObjectGroup()) {
            mActionSelectNone->setEnabled(selectedObjectsCount  > 0);
        } else {
            mActionSelectNone->setEnabled(false);
        }
    } else {
        mActionSelectNone->setEnabled(false);
    }


    mActionCropToSelection->setEnabled(!selection.isEmpty());

    mActionAutocrop->setEnabled(currentLayer && currentLayer->isTileLayer() && !map->infinite());

    mActionAddTileLayer->setEnabled(map);
    mActionAddObjectGroup->setEnabled(map);
    mActionAddImageLayer->setEnabled(map);

    bool usableSelection = currentLayer && ((currentLayer->isObjectGroup() && selectedObjectsCount > 0) ||
                                            (currentLayer->isTileLayer() && !selection.isEmpty()));
    mActionLayerViaCopy->setEnabled(usableSelection);
    mActionLayerViaCut->setEnabled(usableSelection);

    mActionGroupLayers->setEnabled(!selectedLayers.isEmpty());
    mActionUngroupLayers->setEnabled(std::any_of(selectedLayers.begin(), selectedLayers.end(),
                                                 [] (Layer *layer) { return layer->isGroupLayer() || layer->parentLayer(); }));

    const bool hasPreviousLayer = LayerIterator(currentLayer).previous();
    const bool hasNextLayer = LayerIterator(currentLayer).next();
    const bool canMoveLayersUp = !selectedLayers.isEmpty() && MoveLayer::canMoveUp(selectedLayers);
    const bool canMoveLayersDown = !selectedLayers.isEmpty() && MoveLayer::canMoveDown(selectedLayers);

    mActionDuplicateLayers->setEnabled(!selectedLayers.isEmpty());
    mActionMergeLayerDown->setEnabled(canMergeDown);
    mActionSelectPreviousLayer->setEnabled(hasPreviousLayer);
    mActionSelectNextLayer->setEnabled(hasNextLayer);
    mActionMoveLayersUp->setEnabled(canMoveLayersUp);
    mActionMoveLayersDown->setEnabled(canMoveLayersDown);
    mActionToggleOtherLayers->setEnabled(currentLayer && (hasNextLayer || hasPreviousLayer));
    mActionToggleLockOtherLayers->setEnabled(currentLayer && (hasNextLayer || hasPreviousLayer));
    mActionRemoveLayers->setEnabled(!selectedLayers.isEmpty());
    mActionLayerProperties->setEnabled(currentLayer);

    mActionDuplicateObjects->setEnabled(selectedObjectsCount > 0);
    mActionRemoveObjects->setEnabled(selectedObjectsCount > 0);

    QString duplicateText;
    QString removeText;

    if (selectedObjectsCount > 0) {
        duplicateText = tr("Duplicate %n Object(s)", "", selectedObjectsCount);
        removeText = tr("Remove %n Object(s)", "", selectedObjectsCount);
    } else {
        duplicateText = tr("Duplicate Objects");
        removeText = tr("Remove Objects");
    }

    mActionDuplicateObjects->setText(duplicateText);
    mActionRemoveObjects->setText(removeText);
}

} // namespace Internal
} // namespace Tiled
