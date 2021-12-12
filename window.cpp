#include "window.h"

#include <QtGui/private/qrhivulkan_p.h>
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

    if (m_renderPass) {
        m_renderPass->destroy();
        m_renderPass = nullptr;
    }

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

void Window::initialize()
{
    m_swapChain = m_rhi->newSwapChain();
    m_swapChain->setWindow(this);

    m_renderPass = m_swapChain->newCompatibleRenderPassDescriptor();
    m_swapChain->setRenderPassDescriptor(m_renderPass);
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

    QRhiCommandBuffer *cmd = m_swapChain->currentFrameCommandBuffer();

    cmd->beginPass(m_swapChain->currentFrameRenderTarget(), Qt::cyan, QRhiDepthStencilClearValue{});
    cmd->endPass();

    result = m_rhi->endFrame(m_swapChain);
    if (result != QRhi::FrameOpSuccess) {
        qDebug() << "Failed to end a frame";
    }
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
