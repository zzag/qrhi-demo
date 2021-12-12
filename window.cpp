#include "window.h"

#include <QtGui/private/qrhivulkan_p.h>
#include <QFile>
#include <QPlatformSurfaceEvent>

Window::Window(QVulkanInstance *vulkanInstance)
{
    setSurfaceType(QSurface::VulkanSurface);
    setVulkanInstance(vulkanInstance);

    // Force creation of the platform window, needed to initialize QRhi.
    winId();

    QRhiVulkanInitParams rhiInitParams {};
    rhiInitParams.inst = vulkanInstance;
    rhiInitParams.window = this;

    m_rhi = QRhi::create(QRhi::Implementation::Vulkan, &rhiInitParams);
    if (!m_rhi) {
        qFatal("Failed to initialize QRhi implementation");
    }
}

Window::~Window()
{
    releaseResources();
}

void Window::releaseResources()
{
    releaseSwapChain();

    for (QRhiResource *resource : qAsConst(m_releasePool)) {
        resource->destroy();
    }
    m_releasePool.clear();

    delete m_rhi;
    m_rhi = nullptr;
}

bool Window::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest:
        render();
        break;
    case QEvent::PlatformSurface:
        if (static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            releaseSwapChain();
        }
        break;
    default:
        break;
    }

    return QWindow::event(event);
}

void Window::exposeEvent(QExposeEvent *)
{
    if (isExposed()) {
        ensureReady();
    } else {
        ensurePaused();
    }
}

void Window::ensureReady()
{
    if (m_status == Status::Uninitialized) {
        initialize();
        m_status = Status::Initialized;
    }

    if ((m_status == Status::Initialized || m_status == Status::Paused) && !m_swapChain->surfacePixelSize().isEmpty()) {
        if (resizeSwapChain()) {
            m_status = Status::Ready;
        }
    }
    if (m_status == Status::Ready) {
        requestUpdate();
    }
}

void Window::ensurePaused()
{
    if (m_status == Status::Ready && m_swapChain && m_swapChain->surfacePixelSize().isEmpty()) {
        m_status = Status::Paused;
    }
}

static QShader loadShader(const QString &fileName)
{
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly)) {
        return QShader::fromSerialized(file.readAll());
    }
    return QShader{};
}

void Window::initialize()
{
    m_swapChain = m_rhi->newSwapChain();
    m_swapChain->setWindow(this);

    m_renderPass = m_swapChain->newCompatibleRenderPassDescriptor();
    m_releasePool << m_renderPass;
    m_swapChain->setRenderPassDescriptor(m_renderPass);

    QImage textureData(QStringLiteral(":/pexels-petr-ganaj-4032582.jpg"));
    const QVector<float> vertexData {
        // First triangle
        -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 1.0f,

        // Second triangle
        -1.0f, -1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
    };

    const QVector<float> onscreenVertexData {
        // First triangle
        0, 0, 0.0f, 0.0f,
        300, 200, 1.0f, 1.0f,
        0, 200, 0.0f, 1.0f,

        // Second triangle
        0, 0, 0.0f, 0.0f,
        300, 0, 1.0f, 0.0f,
        300, 200, 1.0f, 1.0f,
    };

    {
        m_offscreen.targetTexture = m_rhi->newTexture(QRhiTexture::RGBA8, m_swapChain->surfacePixelSize(), 1, QRhiTexture::RenderTarget);
        m_releasePool << m_offscreen.targetTexture;
        m_offscreen.targetTexture->create();

        m_offscreen.renderTarget = m_rhi->newTextureRenderTarget(QRhiColorAttachment(m_offscreen.targetTexture));
        m_releasePool << m_offscreen.renderTarget;

        m_offscreen.renderPass = m_offscreen.renderTarget->newCompatibleRenderPassDescriptor();
        m_releasePool << m_offscreen.renderPass;
        m_offscreen.renderTarget->setRenderPassDescriptor(m_offscreen.renderPass);
        m_offscreen.renderTarget->create();

        m_offscreen.vbo = m_rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::UsageFlag::VertexBuffer, sizeof(float) * vertexData.size());
        m_releasePool << m_offscreen.vbo;
        m_offscreen.vbo->create();

        m_offscreen.ubo = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UsageFlag::VertexBuffer | QRhiBuffer::UsageFlag::UniformBuffer, sizeof(QMatrix4x4));
        m_releasePool << m_offscreen.ubo;
        m_offscreen.ubo->create();

        m_offscreen.texture = m_rhi->newTexture(QRhiTexture::BGRA8, textureData.size());
        m_releasePool << m_offscreen.texture;
        m_offscreen.texture->create();

        m_offscreen.sampler = m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, QRhiSampler::AddressMode::ClampToEdge, QRhiSampler::AddressMode::ClampToEdge);
        m_releasePool << m_offscreen.sampler;
        m_offscreen.sampler->create();

        m_offscreen.shaderBindings = m_rhi->newShaderResourceBindings();
        m_releasePool << m_offscreen.shaderBindings;
        m_offscreen.shaderBindings->setBindings({
            QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, m_offscreen.texture, m_offscreen.sampler),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, m_offscreen.ubo),
        });
        m_offscreen.shaderBindings->create();

        m_offscreen.pipeline = m_rhi->newGraphicsPipeline();
        m_releasePool << m_offscreen.pipeline;

        const QShader vertexShader = loadShader(QStringLiteral(":/shaders/offscreen.vert.qsb"));
        if (!vertexShader.isValid()) {
            qFatal("Failed to load vertex shader");
        }

        const QShader fragmentShader = loadShader(QStringLiteral(":/shaders/offscreen.frag.qsb"));
        if (!fragmentShader.isValid()) {
            qFatal("Failed to load fragment shader");
        }

        m_offscreen.pipeline->setShaderStages({
            QRhiShaderStage(QRhiShaderStage::Vertex, vertexShader),
            QRhiShaderStage(QRhiShaderStage::Fragment, fragmentShader),
        });

        QRhiVertexInputLayout vertexInputLayout;
        vertexInputLayout.setBindings({
            QRhiVertexInputBinding(4 * sizeof(float)),
        });
        vertexInputLayout.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0 * sizeof(float)),
            QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)),
        });

        m_offscreen.pipeline->setVertexInputLayout(vertexInputLayout);
        m_offscreen.pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_offscreen.pipeline->setShaderResourceBindings(m_offscreen.shaderBindings);
        m_offscreen.pipeline->setRenderPassDescriptor(m_offscreen.renderPass);
        m_offscreen.pipeline->create();
    }

    {
        m_onscreen.vbo = m_rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::VertexBuffer, sizeof(float) * onscreenVertexData.size());
        m_releasePool << m_onscreen.vbo;
        m_onscreen.vbo->create();

        m_onscreen.ubo = m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer | QRhiBuffer::UniformBuffer, sizeof(QMatrix4x4));
        m_releasePool << m_onscreen.ubo;
        m_onscreen.ubo->create();

        m_onscreen.sampler = m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, QRhiSampler::AddressMode::ClampToEdge, QRhiSampler::AddressMode::ClampToEdge);
        m_releasePool << m_onscreen.sampler;
        m_onscreen.sampler->create();

        m_onscreen.shaderBindings = m_rhi->newShaderResourceBindings();
        m_releasePool << m_onscreen.shaderBindings;
        m_onscreen.shaderBindings->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_onscreen.ubo),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_offscreen.targetTexture, m_onscreen.sampler),
        });
        m_onscreen.shaderBindings->create();

        m_onscreen.pipeline = m_rhi->newGraphicsPipeline();
        m_releasePool << m_onscreen.pipeline;

        const QShader vertexShader = loadShader(QStringLiteral(":/shaders/texture.vert.qsb"));
        if (!vertexShader.isValid()) {
            qFatal("Failed to load vertex shader");
        }

        const QShader fragmentShader = loadShader(QStringLiteral(":/shaders/texture.frag.qsb"));
        if (!fragmentShader.isValid()) {
            qFatal("Failed to load fragment shader");
        }

        m_onscreen.pipeline->setShaderStages({
            QRhiShaderStage(QRhiShaderStage::Vertex, vertexShader),
            QRhiShaderStage(QRhiShaderStage::Fragment, fragmentShader),
        });

        QRhiVertexInputLayout vertexInputLayout;
        vertexInputLayout.setBindings({
            QRhiVertexInputBinding(4 * sizeof(float)),
        });
        vertexInputLayout.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0 * sizeof(float)),
            QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)),
        });

        m_onscreen.pipeline->setVertexInputLayout(vertexInputLayout);
        m_onscreen.pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_onscreen.pipeline->setShaderResourceBindings(m_onscreen.shaderBindings);
        m_onscreen.pipeline->setRenderPassDescriptor(m_renderPass);
        m_onscreen.pipeline->create();
    }

    m_initialUpdates = m_rhi->nextResourceUpdateBatch();
    m_initialUpdates->uploadStaticBuffer(m_offscreen.vbo, 0, sizeof(float) * vertexData.size(), vertexData.constData());
    m_initialUpdates->uploadStaticBuffer(m_onscreen.vbo, 0, sizeof(float) * onscreenVertexData.size(), onscreenVertexData.constData());
    m_initialUpdates->uploadTexture(m_offscreen.texture, textureData);
}

void Window::render()
{
    if (m_status != Status::Ready) {
        return;
    }

    if (m_swapChain->currentPixelSize() != m_swapChain->surfacePixelSize()) {
        if (!resizeSwapChain()) {
            return;
        }
    }

    QRhi::FrameOpResult result = m_rhi->beginFrame(m_swapChain);
    switch (result) {
    case QRhi::FrameOpSwapChainOutOfDate:
        if (resizeSwapChain()) {
            requestUpdate();
        }
        break;
    case QRhi::FrameOpSuccess:
        break;
    default:
        qFatal("Failed to start a frame");
        break;
    }

    QRhiResourceUpdateBatch *updates = m_rhi->nextResourceUpdateBatch();
    if (m_initialUpdates) {
        updates->merge(m_initialUpdates);
        m_initialUpdates->release();
        m_initialUpdates = nullptr;
    }

    QRhiCommandBuffer *cmd = m_swapChain->currentFrameCommandBuffer();

    {
        QMatrix4x4 modelViewProjectionMatrix;
        modelViewProjectionMatrix.rotate(m_frameCount, 0, 0, 1);
        updates->updateDynamicBuffer(m_offscreen.ubo, 0, sizeof(modelViewProjectionMatrix), modelViewProjectionMatrix.constData());

        cmd->beginPass(m_offscreen.renderTarget, Qt::cyan, QRhiDepthStencilClearValue{}, updates);
        cmd->setGraphicsPipeline(m_offscreen.pipeline);
        cmd->setViewport(QRhiViewport(0, 0, m_offscreen.targetTexture->pixelSize().width(), m_offscreen.targetTexture->pixelSize().height()));
        cmd->setShaderResources();
        const QRhiCommandBuffer::VertexInput vertexBindings[] = {
            QRhiCommandBuffer::VertexInput{m_offscreen.vbo, 0},
        };
        cmd->setVertexInput(0, 1, vertexBindings);
        cmd->draw(6);
        cmd->endPass();
    }

    {
        QMatrix4x4 modelViewProjectionMatrix;
        modelViewProjectionMatrix.ortho(0, width(), 0, height(), -1, 1);
        updates->updateDynamicBuffer(m_onscreen.ubo, 0, sizeof(modelViewProjectionMatrix), modelViewProjectionMatrix.constData());

        cmd->beginPass(m_swapChain->currentFrameRenderTarget(), Qt::black, QRhiDepthStencilClearValue{}, updates);
        cmd->setGraphicsPipeline(m_onscreen.pipeline);
        cmd->setViewport(QRhiViewport(0, 0, m_swapChain->currentPixelSize().width(), m_swapChain->currentPixelSize().height()));
        cmd->setShaderResources();
        const QRhiCommandBuffer::VertexInput vertexBindings[] = {
            QRhiCommandBuffer::VertexInput{m_onscreen.vbo, 0},
        };
        cmd->setVertexInput(0, 1, vertexBindings);
        cmd->draw(6);
        cmd->endPass();
    }

    result = m_rhi->endFrame(m_swapChain);
    if (result != QRhi::FrameOpSuccess) {
        qDebug() << "Failed to end a frame";
    }

    m_frameCount++;
    requestUpdate();
}

bool Window::resizeSwapChain()
{
    return m_swapChain->createOrResize();
}

void Window::releaseSwapChain()
{
    if (m_swapChain) {
        m_swapChain->destroy();
        m_swapChain = nullptr;
    }
}
