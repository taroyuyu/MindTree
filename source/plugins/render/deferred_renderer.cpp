#define GLM_SWIZZLE
#include "render.h"
#include "renderpass.h"
#include "rendertree.h"
#include "../3dwidgets/widgets.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/string_cast.hpp"
#include "data/debuglog.h"
#include "light_renderer.h"
#include "light_accumulation_plane.h"
#include "pixel_plane.h"
#include "../datatypes/Object/lights.h"
#include "polygon_renderer.h"
#include "camera_renderer.h"
#include "empty_renderer.h"
#include "shader_render_node.h"
#include "rsm_computation_plane.h"

#include "deferred_renderer.h"

using namespace MindTree;
using namespace GL;

struct Compositor : public PixelPlane::ShaderProvider {
    std::shared_ptr<ShaderProgram> provideProgram() {
        auto prog = std::make_shared<ShaderProgram>();

        prog
            ->addShaderFromFile("../plugins/render/defaultShaders/fullscreenquad.vert", 
                                ShaderProgram::VERTEX);
        prog
            ->addShaderFromFile("../plugins/render/defaultShaders/fullscreenquad.frag", 
                                ShaderProgram::FRAGMENT);

        return prog;
    }
};

struct FinalOut : public PixelPlane::ShaderProvider {
    std::shared_ptr<ShaderProgram> provideProgram() {
        auto prog = std::make_shared<ShaderProgram>();

        prog
            ->addShaderFromFile("../plugins/render/defaultShaders/fullscreenquad.vert", 
                                ShaderProgram::VERTEX);
        prog
            ->addShaderFromFile("../plugins/render/defaultShaders/finalout.frag", 
                                ShaderProgram::FRAGMENT);

        return prog;
    }
};

DeferredRenderer::DeferredRenderer(QGLContext *context, CameraPtr camera, Widget3DManager *widgetManager) :
    RenderConfigurator(context, camera)
{
    auto manager = getManager();
    auto config = manager->getConfig();
    config.setProperty("defaultLighting", true);
    manager->setConfig(config);

    addRenderBlock(std::make_shared<GBufferRenderBlock>(_geometryPass));
    auto rsmGenerationBlock = std::make_shared<RSMGenerationBlock>();
    addRenderBlock(rsmGenerationBlock);
    addRenderBlock(std::make_shared<DeferredLightingRenderBlock>(rsmGenerationBlock.get()));
    addRenderBlock(std::make_shared<RSMEvaluationBlock>(rsmGenerationBlock.get()));

    auto overlayPass = std::make_shared<RenderPass>();
    _overlayPass = overlayPass;
    manager->addPass(overlayPass);
    auto overlay = _overlayPass.lock();
    overlay
        ->setDepthOutput(std::make_shared<Renderbuffer>("depth",
                                                        Renderbuffer::DEPTH));
    overlay->addOutput(std::make_shared<Texture2D>("overlay"));
    overlay->setCamera(camera);
    _viewCenter = new SinglePointRenderer();
    _viewCenter->setVisible(false);
    overlay->addRenderer(_viewCenter);

    if(widgetManager) widgetManager->insertWidgetsIntoRenderPass(overlay);

    auto pixelPass = std::make_shared<RenderPass>();
    _pixelPass = pixelPass;
    manager->addPass(pixelPass);
    pixelPass->setCamera(camera);
    auto pplane = new PixelPlane();
    pplane->setProvider<Compositor>();
    pixelPass->addRenderer(pplane);
    float value = 70.0 / 255;
    pixelPass->setBackgroundColor(glm::vec4(value, value, value, 1.));
    pixelPass->addOutput(std::make_shared<Texture2D>("final_out"));

    auto finalPass = std::make_shared<RenderPass>();
    _finalPass = finalPass;
    finalPass->setCamera(camera);
    manager->addPass(finalPass);
    pplane = new PixelPlane();
    pplane->setProvider<FinalOut>();
    finalPass->addRenderer(pplane);

    setCamera(camera);
}

void DeferredRenderer::setCamera(std::shared_ptr<Camera> cam)
{
    RenderConfigurator::setCamera(cam);
    _overlayPass.lock()->setCamera(cam);
    _pixelPass.lock()->setCamera(cam);
}

glm::vec4 DeferredRenderer::getPosition(glm::vec2 pixel) const
{
    std::vector<std::string> values = {"worldposition"};
    return _geometryPass.lock()->readPixel(values, pixel)[0];
}

void DeferredRenderer::setProperty(std::string name, Property prop)
{
    Object::setProperty(name, prop);
    
    if (name == "GL:showgrid") {
        auto value = prop.getData<bool>();
        _grid->setVisible(value);
    }
    if (name == "GL:camera:showcenter") {
        auto value = prop.getData<bool>();
        _viewCenter->setVisible(value);
    }
    if (name == "GL:camera:center") {
        auto value = prop.getData<glm::vec3>();
        _viewCenter->setPosition(value);
    }
}

void DeferredRenderer::setOverrideOutput(std::string output)
{
    _finalPass.lock()->setCustomTextureNameMapping(output, "final_out");
}

void DeferredRenderer::clearOverrideOutput()
{
    _finalPass.lock()->clearCustomTextureNameMapping();
}

GBufferRenderBlock::GBufferRenderBlock(std::weak_ptr<RenderPass> geopass)
    : GeometryRenderBlock(geopass)
{
}

GBufferRenderBlock::~GBufferRenderBlock()
{
}

void GBufferRenderBlock::init()
{
    auto geopass = _geometryPass.lock();

    geopass->setEnableBlending(false);

    geopass->setDepthOutput(std::make_shared<Texture2D>("depth", 
                                                        Texture::DEPTH));
    geopass->addOutput(std::make_shared<Texture2D>("outdiffusecolor"));
    geopass->addOutput(std::make_shared<Texture2D>("outcolor"));
    geopass->addOutput(std::make_shared<Texture2D>("outdiffuseintensity"));
    geopass->addOutput(std::make_shared<Texture2D>("outspecintensity"));
    geopass->addOutput(std::make_shared<Texture2D>("outnormal", 
                                                   Texture::RGBA16F));
    geopass->addOutput(std::make_shared<Texture2D>("outposition", 
                                                   Texture::RGBA16F));
    geopass->addOutput(std::make_shared<Texture2D>("worldposition", 
                                                   Texture::RGBA16F));

    setupGBuffer();
}

void GBufferRenderBlock::setupGBuffer()
{
    auto gbufferShader = std::make_shared<ShaderProgram>();
    gbufferShader
        ->addShaderFromFile("../plugins/render/defaultShaders/polygons.vert", 
                            ShaderProgram::VERTEX);
    gbufferShader
        ->addShaderFromFile("../plugins/render/defaultShaders/gbuffer.frag", 
                            ShaderProgram::FRAGMENT);
    auto gbufferNode = std::make_shared<ShaderRenderNode>(gbufferShader);
    _gbufferNode = gbufferNode;

    _geometryPass.lock()->addGeometryShaderNode(gbufferNode);
}

void GBufferRenderBlock::addRendererFromObject(std::shared_ptr<GeoObject> obj)
{
    auto data = obj->getData();
    switch(data->getType()){
        case ObjectData::MESH:
            _gbufferNode->addRenderer(new PolygonRenderer(obj));
            _geometryPass.lock()->addGeometryRenderer(new EdgeRenderer(obj));
            _geometryPass.lock()->addGeometryRenderer(new PointRenderer(obj));
            break;
    }
}

DeferredLightingRenderBlock::DeferredLightingRenderBlock(ShadowMappingRenderBlock *shadowBlock) :
   _shadowBlock(shadowBlock)
{
}

void DeferredLightingRenderBlock::setGeometry(std::shared_ptr<Group> grp)
{
    auto config = _config->getManager()->getConfig();
    if(config.hasProperty("defaultLighting") &&
       config["defaultLighting"].getData<bool>()) {
        setupDefaultLights();
    }
    else {
        _deferredRenderer->setLights(grp->getLights());
    }
    setRenderersFromGroup(grp);
    if(_shadowBlock)
        _deferredRenderer->setShadowPasses(_shadowBlock->getShadowPasses());
}

glm::mat4 createTransFromZVec(glm::vec3 z)
{
    glm::mat4 trans;
    z = glm::normalize(z);
    glm::vec3 x = glm::normalize(glm::cross(z + glm::vec3(1, 0, 0), z));
    glm::vec3 y = glm::normalize(glm::cross(x, z));

    trans[0] = glm::vec4(x, 0);
    trans[1] = glm::vec4(y, 0);
    trans[2] = glm::vec4(z, 0);

    return trans;
}

void DeferredLightingRenderBlock::init()
{
    auto deferredPass = addPass();
    _deferredPass = deferredPass;
    deferredPass
        ->addOutput(std::make_shared<Texture2D>("shading_out",
                                                Texture::RGB16F));
    _deferredRenderer = new LightAccumulationPlane();
    deferredPass->addRenderer(_deferredRenderer);
    deferredPass->setBlendFunc(GL_ONE, GL_ONE);

    setupDefaultLights();
}

void DeferredLightingRenderBlock::setupDefaultLights()
{
    static const double coneangle = 2 * 3.14159265359;
    auto light1 = std::make_shared<DistantLight>(.8, glm::vec4(1));
    light1->setTransformation(createTransFromZVec(glm::vec3(-1, -1, -1)));

    auto light2 = std::make_shared<DistantLight>(.3, glm::vec4(1));
    light2->setTransformation(createTransFromZVec(glm::vec3(5, 1, -1)));

    auto light3 = std::make_shared<DistantLight>(.1, glm::vec4(1));
    light3->setTransformation(createTransFromZVec(glm::vec3(0, 0, -1)));

    std::vector<LightPtr> lights = {light1, light2, light3};
    _deferredRenderer->setLights(lights);
}

