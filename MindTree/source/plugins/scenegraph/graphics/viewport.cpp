/*
    FRG Shader Editor, a Node-based Renderman Shading Language Editor
    Copyright (C) 2011  Sascha Fricke

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define GLM_SWIZZLE
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "GL/glew.h"

#include "QMenu"
#include "QGraphicsSceneContextMenuEvent"
#include "QMouseEvent"
#include "QGLShaderProgram"
#include "QGLFramebufferObject"
#include "QPointF"
#include "QMetaObject"
#include "GL/glut.h"

#include "iostream"
#include "ctime"

#include "math.h"

#include "graphics/viewport_widget.h"
#include "source/data/base/raytracing/ray.h"
#include "data/project.h"
#include "../../render/render.h"
#include "../../render/primitive_renderer.h"

#include "viewport.h"

using namespace MindTree;

Viewport::Viewport()
    : QGLWidget(MindTree::GL::QtContext::getSharedContext()->getNativeContext()), 
    rotate(false), 
    zoom(false), 
    pan(false), 
    _showGrid(true), 
    defaultCamera(std::make_shared<Camera>()),
    selectionMode(false), 
    transformMode(0), 
    selectedNode(0)
{
    activeCamera = defaultCamera;
    DNode *snode = 0;
    setAutoBufferSwap(false);

    rendermanager = std::unique_ptr<MindTree::GL::RenderManager>(new MindTree::GL::RenderManager());

    auto pass = rendermanager->addPass();
    pass->setCamera(activeCamera);

    grid = new GL::GridRenderer(100, 100, 100, 100);
    auto trans = glm::rotate(glm::mat4(), 90.f, glm::vec3(1, 0, 0));

    grid->setTransformation(trans);
    grid->setColor(glm::vec4(.5, .5, .5, .5));

    pass->addRenderer(grid);
    pass->addOutput("outcolor");

    auto pixelpass = rendermanager->addPass();
    pixelpass->addRenderer(new GL::FullscreenQuadRenderer(pass.get()));
}

Viewport::~Viewport()
{
}

void Viewport::setData(std::shared_ptr<Group> value)
{
    if(!value) return;

    rendermanager->getPass(0)->setRenderersFromGroup(value);
}

void Viewport::changeCamera(QString cam)    
{
    CameraNode* cnode = (CameraNode*)Project::instance()->getItem(cam.toStdString());
    if(cnode) {
        activeCamera = std::static_pointer_cast<Camera>(cnode->getObject());
        if(selectedNode == cnode)
            selectedNode = 0;
    }
}

void Viewport::resizeEvent(QResizeEvent *)
{
    activeCamera->setProjection((GLdouble)width()/(GLdouble)height());
    rendermanager->setSize(width(), height());
    rendermanager->setSize(width(), height());
}

void Viewport::paintEvent(QPaintEvent *)
{
    //do nothing here
}

void Viewport::showEvent(QShowEvent *)
{
    rendermanager->start();
}

void Viewport::hideEvent(QHideEvent *)
{
    rendermanager->stop();
}

void Viewport::setShowPoints(bool b)    
{
    auto config = rendermanager->getConfig();
    config.setDrawPoints(b);
    rendermanager->setConfig(config);
}

void Viewport::setShowEdges(bool b)    
{
    auto config = rendermanager->getConfig();
    config.setDrawEdges(b);
    rendermanager->setConfig(config);
}

void Viewport::setShowPolygons(bool b)    
{
    auto config = rendermanager->getConfig();
    config.setDrawPolygons(b);
    rendermanager->setConfig(config);
}

void Viewport::setShowFlatShading(bool b)
{
    auto config = rendermanager->getConfig();
    config.setShowFlatShaded(b);
    rendermanager->setConfig(config);
}

void Viewport::setShowGrid(bool b)
{
    grid->setVisible(b);
}

void Viewport::mousePressEvent(QMouseEvent *event)    
{
    lastpos = event->posF();
    if(event->modifiers() & Qt::ControlModifier)
        zoom = true;
    else if (event->modifiers() & Qt::ShiftModifier)
        pan = true;
    else
        rotate = true;
}

void Viewport::mouseReleaseEvent(QMouseEvent *event)    
{
    rotate = false;
    pan = false;
    zoom = false;
    selectionMode =false;
    lastpos = QPointF();
}

void Viewport::mouseMoveEvent(QMouseEvent *event)    
{
    float xdist = lastpos.x()  - event->posF().x();
    float ydist = event->posF().y() - lastpos.y();
    if(rotate)
        rotateView(xdist, ydist);
    else if(pan)
        panView(xdist, ydist);
    else if(zoom)
        zoomView(xdist, ydist);
    lastpos = event->posF();
}

void Viewport::wheelEvent(QWheelEvent *event)    
{
    zoomView(0, event->delta());
}

AbstractTransformableNode* Viewport::getSelectedNode()    
{
    if(selectedNode && selectedNode->getObject() == activeCamera)
        return 0;
    else return selectedNode;
}

glm::vec3 Viewport::getCamPivot()    
{
    if(getSelectedNode())
        return glm::vec4(getSelectedNode()->getObject()->getTransformation() * glm::vec4()).xyz();
    else
        return glm::vec3(0, 0, 0);
}

void Viewport::rotateView(float xdist, float ydist)
{
    if(!activeCamera)
        return;
    glm::mat4 mat;
    glm::vec3 campos = activeCamera->getPosition();
    glm::vec3 center = activeCamera->getCenter();
    glm::vec3 lookat = campos - center;

    mat = glm::rotate(mat, (float)ydist, glm::cross(glm::vec3(0, 1, 0), center - campos));
    mat = glm::rotate(mat, (float)xdist, glm::vec3(0, 1, 0));
    lookat = (mat * glm::vec4(lookat, 1)).xyz();
    activeCamera->posAroundCenter(lookat + center);
}

void Viewport::panView(float xdist, float ydist)    
{
    if(!activeCamera)
        return;
    float lalen = glm::length(activeCamera->getCenter() - activeCamera->getPosition());
    float xtrans = xdist * lalen * 0.005;
    float ytrans = ydist * lalen * 0.005;
    activeCamera->translate(glm::vec3(1, 0, 0) * xtrans);
    activeCamera->translate(glm::vec3(0, 1, 0) * ytrans);
}

void Viewport::zoomView(float xdist, float ydist)    
{
    if(!activeCamera)
        return;
    //glm::vec3 zvec = activeCamera->getCenter() - activeCamera->getPosition();
    activeCamera->moveToCenter(ydist/height());
}
